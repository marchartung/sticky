/***************************************************************************************[Solver.cc]

 Sticky Sat -- Copyright (c) 2018-2019, Marc Hartung, Zuse Institute Berlin

Sticky Sat sources are based on Glucose Syrup and is therefore also restricted by all
copyright notices below.

Sticky Sat sources are based on another copyright. Permissions and copyrights for the parallel
version of Glucose-Syrup (the "Software") are granted, free of charge, to deal with the Software
without restriction, including the rights to use, copy, modify, merge, publish, distribute,
sublicence, and/or sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

- The above and below copyrights notices and this permission notice shall be included in all
copies or substantial portions of the Software;

--------------- Original Glucose Copyrights

 Glucose -- Copyright (c) 2009-2014, Gilles Audemard, Laurent Simon
 CRIL - Univ. Artois, France
 LRI  - Univ. Paris Sud, France (2009-2013)
 Labri - Univ. Bordeaux, France

 Syrup (Glucose Parallel) -- Copyright (c) 2013-2014, Gilles Audemard, Laurent Simon
 CRIL - Univ. Artois, France
 Labri - Univ. Bordeaux, France

 Glucose sources are based on MiniSat (see below MiniSat copyrights). Permissions and copyrights of
 Glucose (sources until 2013, Glucose 3.0, single core) are exactly the same as Minisat on which it
 is based on. (see below).

 Glucose-Syrup sources are based on another copyright. Permissions and copyrights for the parallel
 version of Glucose-Syrup (the "Software") are granted, free of charge, to deal with the Software
 without restriction, including the rights to use, copy, modify, merge, publish, distribute,
 sublicence, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 - The above and below copyrights notices and this permission notice shall be included in all
 copies or substantial portions of the Software;
 - The parallel version of Glucose (all files modified since Glucose 3.0 releases, 2013) cannot
 be used in any competitive event (sat competitions/evaluations) without the express permission of
 the authors (Gilles Audemard / Laurent Simon). This is also the case for any competitive event
 using Glucose Parallel as an embedded SAT engine (single core or not).


 --------------- Original Minisat Copyrights

 Copyright (c) 2003-2006, Niklas Een, Niklas Sorensson
 Copyright (c) 2007-2010, Niklas Sorensson

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
 associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
 NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
 OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 **************************************************************************************************/

#include "shared/ClauseTypes.h"
namespace Sticky
{

Header BaseClause::getPrivateClauseHeader(const BaseType & lbd, const BaseType & size)
{
   return
   {  0,1,0,0,lbd,size};
}

Header BaseClause::getSharedClauseHeader(const BaseType & lbd, const BaseType & size)
{
   return
   {  1,1,0,0,lbd,size};
}

Header BaseClause::getPermanentClauseHeader(const BaseType & lbd, const BaseType & size)
{
   return
   {  1,0,0,0,lbd,size};
}

Header BaseClause::getPrivateClauseHeader(const BaseClause & c, const BaseType lbd, const BaseType size, const bool vived)
{
   Header res = c.h;
   res.clauseType = 0;
   res.deletable = 1;
   res.vivified = vived;
   res.lbd = (lbd == LBD_UNDEF_VALUE) ? res.lbd : lbd;
   res.sz = (size == 0) ? res.sz : size;
   return res;
}
Header BaseClause::getSharedClauseHeader(const BaseClause & c, const BaseType lbd, const BaseType size, const bool vived)
{
   Header res = c.h;
   res.clauseType = 1;
   res.deletable = 1;
   res.vivified = vived;
   res.lbd = (lbd == LBD_UNDEF_VALUE) ? res.lbd : lbd;
   res.sz = (size == 0) ? res.sz : size;
   return res;
}
Header BaseClause::getPermanentClauseHeader(const BaseClause & c, const BaseType lbd, const BaseType size, const bool vived)
{
   Header res = c.h;
   res.clauseType = 1;
   res.deletable = 0;
   res.vivified = vived;
   res.lbd = (lbd == LBD_UNDEF_VALUE) ? res.lbd : lbd;
   res.sz = (size == 0) ? res.sz : size;
   return res;
}

unsigned BaseClause::getMemory(const int & sz)
{
   return BaseClause::get32BitMemory(sz);
}

unsigned BaseClause::getMemory(const Header & h)
{
   return (h.isPrivateClause()) ? BaseClause::get32BitMemory(h.sz) : SharedClause::get32BitMemory(h.sz);
}
unsigned BaseClause::getOffset(const Header & h)
{
   return (h.isPrivateClause()) ? BaseClause::get32BitOffset() : SharedClause::get32BitOffset();
}
bool BaseClause::needsAlignment(const Header & h)
{
   return (h.isPrivateClause()) ? BaseClause::need64BitAlignment() : SharedClause::need64BitAlignment();
}

bool BaseClause::need64BitAlignment()
{
   return false;
}
unsigned BaseClause::get32BitMemory(const int & numLits)
{
   return (sizeof(Header) + (numLits * sizeof(Lit))) / sizeof(BaseAllocatorType);
}

unsigned BaseClause::get32BitOffset()
{
   return 0;
}
bool BaseClause::contains(const Var & v) const
{
   bool res = false;
   for (int i = 0; i < size(); ++i)
      if (var(data[i]) == v)
      {
         res = true;
         break;
      }
   return res;
}

bool BaseClause::contains(const Lit & l) const
{
   bool res = false;
   for (int i = 0; i < size(); ++i)
      if (data[i] == l)
      {
         res = true;
         break;
      }
   if (!res)
      std::cout << "oha\n";
   return res;
}

bool BaseClause::isSatisfied(const vec<lbool> & assigns)
{
   bool res = false;
   for (int i = 0; i < size(); ++i)
      if (toLbool(data[i]) == assigns[var(data[i])])
      {
         res = true;
         break;
      }
   return res;
}

ReferenceStateChange::ReferenceStateChange()
      : state(0)
{
}

ReferenceStateChange::ReferenceStateChange(const uint64_t & in)
      : state(in)
{
}

ReferenceStateChange::ReferenceStateChange(const int32_t & refs, const CRef & cref)
      : state(0)
{
   set(refs, cref);
}

void ReferenceStateChange::set(const int32_t & r, const CRef & c)
{
   refs() = r;
   cref() = c;
}

const int32_t & ReferenceStateChange::refs() const
{
   return reinterpret_cast<const int32_t*>(&state)[0];
}

int32_t & ReferenceStateChange::refs()
{
   return reinterpret_cast<int32_t*>(&state)[0];
}

static_assert(sizeof(CRef)==sizeof(int32_t), "Change of CRef won't work for reference clauses");
const CRef & ReferenceStateChange::cref() const
{
   return reinterpret_cast<const CRef*>(&state)[1];
}
CRef & ReferenceStateChange::cref()
{
   CRef & res = reinterpret_cast<CRef*>(&state)[1];
   return res;
}

bool ReferenceStateChange::isAlreadyReallocated() const
{
   return cref() != CRef_Undef && cref() != CRef_Del;
}

bool ReferenceStateChange::shouldBeDeleted() const
{
   return cref() == CRef_Del;
}

bool ReferenceStateChange::isDereferenced() const
{
   return refs() == 0;
}

unsigned SharedClause::get32BitMemory(const int & numLits)
{
   return BaseClause::get32BitMemory(numLits) + get32BitOffset();
}

unsigned SharedClause::get32BitOffset()
{
   assert(BaseClause::get32BitOffset() == 0);
   return sizeof(std::atomic<ReferenceStateChange::StateChangeType>) / sizeof(BaseAllocatorType);
}
bool SharedClause::need64BitAlignment()
{
   return true;
}

const std::atomic<ReferenceStateChange::StateChangeType> & SharedClause::state() const
{
   return *(reinterpret_cast<const std::atomic<ReferenceStateChange::StateChangeType> *>(this) - 1);
}
std::atomic<ReferenceStateChange::StateChangeType> & SharedClause::state()
{
   return *(reinterpret_cast<std::atomic<ReferenceStateChange::StateChangeType> *>(this) - 1);
}

void SharedClause::forceCompleteDereference()
{
   initialize(1);
}

CRef SharedClause::getReplaceCRef() const
{
   ReferenceStateChange res(state());
   return res.cref();
}

void SharedClause::initialize(const unsigned & numRefs)
{
   ReferenceStateChange tmp;
   tmp.set(numRefs, CRef_Undef);
   state() = tmp.state;
}

void SharedClause::correctRealloc(const unsigned & numMaxRefs, const ReferenceStateChange & appliedChange)
{
   int32_t minusRefs = numMaxRefs - appliedChange.refs();
   ReferenceStateChange expected(state()), desired;
   do
   {
      assert(expected.refs() > minusRefs);
      desired.set(expected.refs() - minusRefs, expected.cref());
   } while (!state().compare_exchange_weak(expected.state, desired.state));
}

ReferenceStateChange SharedClause::markReallocated(const CRef & posForClause)
{
   ReferenceStateChange expected(state()), desired;
   do
   {
      desired.set(expected.refs(), posForClause);
   } while (expected.cref() == CRef_Undef && !state().compare_exchange_weak(expected.state, desired.state));
   return expected;
}

bool SharedClause::markDeleted()
{
   ReferenceStateChange expected(state()), desired;
   do
   {
      desired.set(expected.refs(), CRef_Del);
   } while (expected.cref() == CRef_Undef && !state().compare_exchange_weak(expected.state, desired.state));
   return this->getReplaceCRef() == CRef_Del && expected.cref() == CRef_Undef;
}

unsigned SharedClause::getNumRefs() const
{
   return ReferenceStateChange(state()).refs();
}

void SharedClause::referenceAdditional()
{
   ReferenceStateChange expected(state()), desired;
   do
   {
      desired.set(expected.refs() + 1, expected.cref());
   } while (!state().compare_exchange_weak(expected.state, desired.state));
}

ReferenceStateChange SharedClause::dereference(CRef cref)
{
   ReferenceStateChange expected(state()), desired;
   do
   {
      assert(expected.refs() > 0);
      assert(expected.refs() < 500);
      desired.set(expected.refs() - 1, expected.cref());
   } while (!state().compare_exchange_weak(expected.state, desired.state));

   //std::cout << "dereferenced to alloc: " << getAllocCount(desired) << " ref: " << getRefCount(desired) << std::endl;
   return desired;
}
} /* namespace Concusat */
