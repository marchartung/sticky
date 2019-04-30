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

#include "glucose/mtl/XAlloc.h"
#include "shared/ClauseBucketArray.h"
#include "shared/ClauseDatabase.h"
#include "shared/Heuristic.h"
#include "shared/SharedTypes.h"
#include "shared/ClauseTypes.h"

#include <iostream>
#include <memory>
#include <set>

namespace Sticky
{

ClauseBucket::Header::Header()
      : sz(0),
        wastedEntries(0)
{
}

ClauseBucket::ClauseBucket()
{
}

ClauseBucketArray::ClauseBucketArray(const SharingHeuristic & sharingHeuristic)
      : heuristic(),
        statistic(),
        sz((heuristic.maxAllocBytes * (1 - heuristic.fracSolverMem)) / ClauseBucket::numBytes()),
        masterBucketId(0),
        buckets(nullptr),
        refShare(sharingHeuristic.sizeExchangeCRefBuffer() * heuristic.numThreads, sharingHeuristic.sizeExchangeUnaryBuffer() * heuristic.numThreads),
        freeBuckets(sz)
{
   initialize();
}

ClauseBucket::size_type ClauseBucket::numWasted() const
{
   return header.wastedEntries;
}

CRef ClauseBucket::alloc(const size_type & num)
{
   size_type expected, desired;
   CRef res = CRef_Undef;
   do
   {
      expected = header.sz;
      desired = header.sz + num;
      if (desired > capacity())
         break;

   } while (!header.sz.compare_exchange_weak(expected, desired));

   if (desired <= capacity())
      res = expected;

   return res;
}

bool ClauseBucket::isFull() const
{
   return header.sz >= capacity();
}

//returns true when bucket is completely wasted
bool ClauseBucket::remove(const size_type & num)
{
   assert(num <= capacity());
   size_type res = header.wastedEntries.fetch_add(num);
   assert(header.wastedEntries <= header.sz);
   return res + num == capacity();
}

void ClauseBucket::reset()
{
   header.sz = 0;
   header.wastedEntries = 0;
   for (unsigned i = 0; i < capacity(); ++i)
      mem[i] = 0;
}

bool ClauseBucket::wasteRest()
{
   size_type expected = header.sz;

   // set sz to max capacity if possible
   while (expected < capacity() && !header.sz.compare_exchange_weak(expected, capacity()))
      ;

   // if capacity was set, add wasted portion to wastedEntries
   if (expected < capacity())
   {
      return remove(capacity() - expected);
   } else
      return false;
}

ClauseBucketArray::~ClauseBucketArray()
{
   if (buckets != nullptr)
      Glucose::xfree(buckets);
}

inline const void * ClauseBucketArray::getPrefetchClauseAddr(const CRef & cref) const
{
   return reinterpret_cast<const void*>(reinterpret_cast<const value_type*>(&buckets[0]) + cref);
}

ClauseBucket & ClauseBucketArray::getBucket(const ClauseBucketArray::size_type & bIdx)
{
   assert(bIdx < sz);
   return buckets[bIdx];
}

const ClauseBucket& ClauseBucketArray::getBucket(const ClauseBucketArray::size_type & bIdx) const
{
   assert(bIdx < sz);
   return buckets[bIdx];
}

unsigned ClauseBucketArray::getNewBucket()
{
   auto res = freeBuckets.pop();
   if (!res.has_value())
   {
      std::cout << "Error: No buckets available." << std::endl;
      throw OutOfMemoryException();
   }
   buckets[res.value()].reset();
   --statistic.numFreeBuckets;
   return res.value();
}

ClauseBucketArray::size_type ClauseBucketArray::getBucketIdxFromCRef(const CRef & idx) const
{
   unsigned res = idx / (BUCKET_SIZE / sizeof(value_type));
   return res;  // TODO: too expansive ?!?
}

const ClauseBucket& ClauseBucketArray::getBucketFromCRef(const CRef & idx) const
{
   return buckets[getBucketIdxFromCRef(idx)];
}

ClauseBucket& ClauseBucketArray::getBucketFromCRef(const CRef & idx)
{
   return buckets[getBucketIdxFromCRef(idx)];
}

CRef ClauseBucketArray::getCRefBucketOffset(const uint32_t & bId) const
{
   return (BUCKET_SIZE / sizeof(value_type)) * bId + ClauseBucket::headerSize();
}

void ClauseBucketArray::initialize()
{
   assert(buckets == nullptr);
   buckets = reinterpret_cast<ClauseBucket*>(Glucose::xrealloc(nullptr, sz * sizeof(ClauseBucket)));
   unsigned i = sz - 1;
   do
   {
      freeBuckets.push(i);
      --i;
   } while (i != 0);
}

void ClauseBucketArray::removeClause(CoreSolver & ts, const CRef & cref)
{
   if (getClause(cref).isPrivateClause())
   {
      for (int i = 0; i < ts.getThreadState().deleteRefs.size(); ++i)
         assert(std::get<0>(ts.getThreadState().deleteRefs[i]) != cref);
      simpleRemoveClause(ts, cref);
   } else
   {
      for (int i = 0; i < ts.getThreadState().deleteRefs.size(); ++i)
         assert(!hasSucc(cref, std::get<0>(ts.getThreadState().deleteRefs[i]), std::get<1>(ts.getThreadState().deleteRefs[i])));
      recursiveRemove(ts, cref, CRef_Undef);
   }
}

unsigned ClauseBucketArray::npos()
{
   return std::numeric_limits<uint32_t>::max();
}

uint32_t ClauseBucketArray::numFreeBuckets() const
{
   return freeBuckets.size();
}

double ClauseBucketArray::getNumMBFreeBuckets() const
{
   double res = static_cast<double>(numFreeBuckets() * ClauseBucket::capacity() * sizeof(value_type)) / (1024.0 * 1024.0);
   return res;
}
double ClauseBucketArray::getNumMBBuckets() const
{
   return static_cast<double>(sz * ClauseBucket::capacity() * sizeof(value_type)) / (1024.0 * 1024.0);
}
double ClauseBucketArray::getNumUsedMB() const
{
   assert(getNumMBBuckets() >= getNumMBFreeBuckets());
   return getNumMBBuckets() - getNumMBFreeBuckets();
}

void ClauseBucketArray::removeFromBucket(const ClauseBucketArray::size_type & bId, const unsigned & num)
{
   ClauseBucket & b = getBucket(bId);
   if (b.remove(num))
      this->freeBuckets.push(bId);
   else
      assert(num > 0);  // would be a senseless remove, indicates some kind of error in usage
}

bool ClauseBucketArray::hasSucc(const CRef lookFor, const CRef start, const CRef end)
{
   bool res = false;
   if (getClause(start).isPrivateClause() || getClause(lookFor).isPrivateClause())
   {
      res = lookFor == start;
   } else
   {
      CRef cur = start;
      while (isValidRef(cur) && cur != end)
      {
         if (cur == lookFor)
         {
            res = true;
            break;
         }
         cur = getClause(cur).shared().getReplaceCRef();
      }
   }
//   if (res)
//      std::cout << "uuhu\n";
   return res;
}

void ClauseBucketArray::simpleRemoveClause(CoreSolver & ts, const CRef & cref)
{
   assert(cref != CRef_Undef);
   size_type bId = getBucketIdxFromCRef(cref);
   BaseClause & c = getClause(cref);
   bool shouldRemove = true;
//   if (cref == 156526)
//      std::cout << "aha\n";
   if (c.isSharedClause() || c.isPermanentClause())
   {
      SharedClause & rc = c.shared();
      unsigned numRefs1 = rc.getNumRefs();
//      std::cout << ts.getThreadId() << " '" << cref << "' r" << numRefs1 << "\n";
//      std::cout.flush();
//      if (numRefs1 == 0 && isValidRef(rc.getReplaceCRef()))
//      {
//         const auto & c2 = getClause(rc.getReplaceCRef());
//         std::cout << "ohoh\n";
//         std::cout.flush();
//      }
      auto refChange = rc.dereference(cref);
      unsigned numRefs2 = refChange.refs();
      assert(numRefs1 > numRefs2);
      shouldRemove = refChange.isDereferenced();
//      std::cout << ts.getThreadId() << " cref " << cref << " dereferenced (" << refChange.refs() << ")\n";
//      std::cout.flush();
   }
   if (shouldRemove)
   {
      const Header & header = c.getHeader();
      if (header.isPrivateClause())
      {
         --ts.getStatistic().nAllocPrivateCl;
      } else if (header.isSharedClause())
      {
         --ts.getStatistic().nAllocSharedCl;
      } else
      {
         --ts.getStatistic().nAllocPermanentCl;
      }
      assert(getClause(cref)[0] != lit_Undef);
      getClause(cref)[0] = lit_Undef;
//      std::cout << "remove '" << cref << "'\n";
      removeFromBucket(bId, BaseClause::getMemory(c.size()));

   }
}

void ClauseBucketArray::recursiveRemove(CoreSolver & ts, const CRef & startCRef, const CRef & endCRef)
{
   assert(!getClause(startCRef).isPrivateClause());
   CRef cur = startCRef;
   unsigned tripCounter = 0;
   while (isValidRef(cur) && cur != endCRef)
   {
      ++tripCounter;
      CRef tmp = getClause(cur).shared().getReplaceCRef();
      simpleRemoveClause(ts, cur);
      cur = tmp;
   }
}

ClauseUpdate ClauseBucketArray::getClauseUpdate(CoreSolver & ts, const CRef cref)
{
   CRef res = this->peekLast(ts, cref, [](const CRef r)
   {  return true;});
   assert(isValidRef(res));
   if (res == cref)
   {
      if (this->shouldReallocateClause(cref))
      {
         const BaseClause & c = getClause(res);
         if (c.isPrivateClause())
         {
            res = this->addPrivateClause(ts, c, c.getHeader());
            removeLater(ts, cref);
         } else
         {
            std::tuple<bool, CRef> replacement = replaceShared(ts, cref, getClause(cref), getClause(cref).getHeader(), [this,&c](const CRef & cref)
            {  return this->shouldReallocateClause(cref) && getClause(cref).size() == c.size();});

            assert(isValidRef(std::get<1>(replacement)));
            res = std::get<1>(replacement);
         }
      } else
         res = CRef_Undef;
   }

   return
   {  res,(isValidRef(res) && getClause(res).size() < getClause(cref).size()) ? &getClause(res) : nullptr};
}

void ClauseBucketArray::removeLater(CoreSolver & ts, const CRef & startCRef, const CRef & endCRef)
{
//   if (startCRef == 716551)
//      std::cout << "aha\n";

//   std::cout << ts.getThreadId() << " later " << startCRef << " till " << endCRef << "\n";
//   std::cout.flush();
   const BaseClause & c = getClause(startCRef);
   for (int i = 0; i < ts.getThreadState().deleteRefs.size(); ++i)
      assert(!hasSucc(startCRef, std::get<0>(ts.getThreadState().deleteRefs[i]), std::get<1>(ts.getThreadState().deleteRefs[i])));
   assert(c.isPrivateClause() || c.shared().getNumRefs() > 0);
   if (!isValidRef(endCRef))
      this->markClauseAsDeleted(startCRef);
   ts.getThreadState().deleteRefs.push( std::make_tuple( startCRef, endCRef));
}

void ClauseBucketArray::garbageCollection(CoreSolver & ts)
{
   vec<std::tuple<CRef, CRef>> & dels = ts.getThreadState().deleteRefs;
   for (int i = 0; i < dels.size(); ++i)
   {
      if (getClause(std::get<0>(dels[i])).isPrivateClause())
      {
         assert(std::get<1>(dels[i]) == CRef_Undef);
         simpleRemoveClause(ts, std::get<0>(dels[i]));
      } else
      {
         recursiveRemove(ts, std::get<0>(dels[i]), std::get<1>(dels[i]));
      }
   }
   dels.clear();
}

}
