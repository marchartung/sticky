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

#ifndef SHARED_CLAUSETYPES_H_
#define SHARED_CLAUSETYPES_H_

#include "shared/SharedTypes.h"
#include <cstdint>

namespace Sticky
{

typedef uint32_t BaseType;
typedef uint32_t ActivityType;

#define LBD_BIT_SIZE 5
#define SIZE_BIT_SIZE 23

#define LBD_UNDEF_VALUE (1 << (LBD_BIT_SIZE - 1))
#define MAX_LBD_VALUE LBD_UNDEF_VALUE-1

inline bool isValidRef(const CRef & cref)
{
   return cref != CRef_Undef && cref != CRef_Del;
}

class SharedClause;

struct Header
{
   BaseType clauseType :1;
   BaseType deletable :1;
   BaseType replaced :1;
   BaseType vivified :1;
   BaseType lbd : LBD_BIT_SIZE;
   BaseType sz : SIZE_BIT_SIZE;

   /*Header(const BaseType type, const BaseType deletable, const BaseType replaced, const BaseType lbd, const BaseType sz)
         : clauseType(type),
           deletable(deletable),
           replaced(replaced),
           vivified(false),
           lbd(lbd),
           sz(sz)
   {
   }*/


   inline bool isPrivateClause() const
   {
      return clauseType == 0;
   }
   inline bool sameClauseType(const Header & h) const
   {
      return clauseType == h.clauseType && (isPrivateClause() || deletable == h.deletable);
   }
   inline bool isSharedClause() const
   {
      return !isPrivateClause() == 1 && deletable == 1;
   }
   inline bool isPermanentClause() const
   {
      return !isPrivateClause() && deletable == 0;
   }

   Header & operator=(const Header & in)
   {
      static_assert(sizeof(Header) == sizeof(uint32_t), "Clause header is not correctly sized");
      *reinterpret_cast<uint32_t*>(this) = *reinterpret_cast<const uint32_t*>(&in);
      /*clauseType = in.clauseType;
      deletable = in.deletable;
      replaced = in.replaced;
      lbd = in.lbd;
      sz = in.sz;*/
      return *this;
   }
};

static_assert(sizeof(Header) == sizeof(BaseType), "Bad struct alignment");

class BaseClause
{
   BaseClause() = delete;
   BaseClause(const BaseClause &) = delete;
   BaseClause(BaseClause &&) = delete;
   BaseClause & operator=(const BaseClause &) = delete;
   BaseClause & operator=(BaseClause &&) = delete;

 protected:
   Header h;
   Lit data[2];

   static bool need64BitAlignment();
   static unsigned get32BitOffset();
   static unsigned get32BitMemory(const int & numLits);
 public:

   static unsigned getMemory(const int & sz);
   static unsigned getMemory(const Header & h);
   static unsigned getOffset(const Header & h);
   static bool needsAlignment(const Header & h);

   static Header getPrivateClauseHeader(const BaseType & lbd, const BaseType & size);
   static Header getSharedClauseHeader(const BaseType & lbd, const BaseType & size);
   static Header getPermanentClauseHeader(const BaseType & lbd, const BaseType & size);

   static Header getPrivateClauseHeader(const BaseClause & c, const BaseType lbd = LBD_UNDEF_VALUE, const BaseType size = 0, const bool vived = false);
   static Header getSharedClauseHeader(const BaseClause & c, const BaseType lbd = LBD_UNDEF_VALUE, const BaseType size = 0, const bool vived = false);
   static Header getPermanentClauseHeader(const BaseClause & c, const BaseType lbd = LBD_UNDEF_VALUE, const BaseType size = 0, const bool vived = false);

   inline void setReplaced()
   {
      h.replaced = 1;
   }
   inline void setVivified()
   {
      h.vivified = 1;
   }
   inline bool isReplaced() const // for low overhead check, this race is ok (sync through shared clause calls)
   {
      return h.replaced;
   }

   inline bool isVivified() const // for low overhead check, this race is ok (sync through shared clause calls)
   {
      return h.vivified;
   }

   inline int size() const
   {
      return h.sz;
   }
   inline uint32_t getLbd() const
   {
      return h.lbd;
   }
   inline void setLbd(const unsigned & inLbd)
   {
      h.lbd = (inLbd <= MAX_LBD_VALUE) ? inLbd : MAX_LBD_VALUE;
   }

   inline void setPrivDel()
   {
      h.lbd = LBD_UNDEF_VALUE;
   }

   inline bool isPrivDel() const
   {
      return h.lbd == LBD_UNDEF_VALUE;
   }

   inline bool isPrivateClause() const
   {
      return h.isPrivateClause();
   }
   inline bool isSharedClause() const
   {
      return h.isSharedClause();
   }
   inline bool isPermanentClause() const
   {
      return h.isPermanentClause();
   }

   inline SharedClause & shared()
   {
      return *reinterpret_cast<SharedClause*>(this);
   }
   inline const SharedClause & shared() const
   {
      return *reinterpret_cast<const SharedClause*>(this);
   }

   inline const Header & getHeader() const
   {
      return h;
   }

   template<typename VecType>
   inline void set(const VecType & lits, const Header & inHeader)
   {
      h = inHeader;
      for (int i = 0; i < lits.size(); ++i)
      {
         data[i] = lits[i];
      }
      h.sz = lits.size();
   }

   bool contains(const Var & v) const;
   bool contains(const Lit & l) const;

   inline Lit& operator [](int i)
   {
      return data[i];
   }
   inline const Lit & operator [](int i) const
   {
      return data[i];
   }

   Lit subsumes(const BaseClause& other) const;
   bool isSatisfied(const vec<lbool> & assigns);

   inline bool operator==(const BaseClause & c) const
   {
      bool res = c.size() == size();
      int i = 0;
      while (res && i < size())
      {
         res = (c.data[i] == data[i]);
         ++i;
      }
      return res;
   }

   bool hasUniqueLits() const
   {
      bool res = true;
      for(int i=0;i<size();++i)
         for(int j=i+1;j<size();++j)
            if(data[i] == data[j])
            {
               res = false;
               break;
            }
      return res;
   }

};

struct ReferenceStateChange
{
   typedef uint64_t StateChangeType;
   StateChangeType state;

   ReferenceStateChange();
   ReferenceStateChange(const uint64_t & in);
   ReferenceStateChange(const int32_t & refs, const CRef & cref);

   void set(const int32_t & refs, const CRef & cref);

   int32_t & refs();
   const int32_t & refs() const;
   CRef & cref();
   const CRef & cref() const;

   bool isAlreadyReallocated() const;
   bool shouldBeDeleted() const;
   bool isDereferenced() const;
};

class SharedClause : public BaseClause
{
   friend class BaseClause;
   SharedClause() = delete;
   SharedClause(const SharedClause &) = delete;
   SharedClause(SharedClause &&) = delete;
   SharedClause & operator=(const SharedClause &) = delete;
   SharedClause & operator=(SharedClause &&) = delete;

   static unsigned get32BitMemory(const int & numLits);
   static unsigned get32BitOffset();
   static bool need64BitAlignment();

 public:

   CRef getReplaceCRef() const;

   void initialize(const unsigned & numExpectedAllocs);
   void correctRealloc(const unsigned & numExpectedAllocs, const ReferenceStateChange & appliedChange);

   // returns true if the reallocation was successful
   ReferenceStateChange markReallocated(const CRef & posForClause);
   bool markDeleted();

   unsigned getNumRefs() const;

   // when function returns referenced, the clause still reference by other solver, when dereferenced or reallocted than it should be deleted.
   // also when reallocated is returned, the reallocation should be deleted too
   ReferenceStateChange dereference(CRef cref = CRef_Undef);
   void referenceAdditional();
   void forceCompleteDereference();

 private:

   const std::atomic<ReferenceStateChange::StateChangeType> & state() const;
   std::atomic<ReferenceStateChange::StateChangeType> & state();
};
} /* namespace Concusat */

#endif /* SHARED_CLAUSETYPES_H_ */
