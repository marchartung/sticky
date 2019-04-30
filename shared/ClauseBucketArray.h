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

#ifndef SHARED_DYNAMICSTACKBUCKETARRAY_H_
#define SHARED_DYNAMICSTACKBUCKETARRAY_H_

#include "shared/SharedTypes.h"
#include "shared/ClauseTypes.h"
#include "shared/CoreSolver.h"
#include "shared/Heuristic.h"
#include "shared/ReferenceSharer.h"
#include "shared/Statistic.h"
#include "shared/DatabaseThreadState.h"
#include "parallel_utils/LockStack.h"

#include <atomic>
#include <algorithm>
#include <memory>
#include <limits>
#include <cstdlib>
#include <cassert>
#include <utility>

#include <unordered_map>
#include <pthread.h>
#include <shared/DatabaseThreadState.h>

namespace Sticky
{
class ClauseBucketArray;

// 1MB bucket size: 1024*1024=1048576
//#define BUCKET_SIZE 1048576

// 256KB bucket size: 1024*256=262144
#ifndef BUCKET_SIZE
#define BUCKET_SIZE 262144
#endif

class ClauseBucket
{

   friend class ClauseBucketArray;

   ClauseBucket(const ClauseBucket &) = delete;
   ClauseBucket(ClauseBucket &&) = delete;

 public:
   typedef BaseAllocatorType value_type;
   typedef uint32_t size_type;

   ClauseBucket();

   ~ClauseBucket();

   static constexpr size_type capacity()
   {
      return _capacity;
   }
   static constexpr size_type npos()
   {
      return std::numeric_limits<size_type>::max();
   }
   static constexpr size_type numBytes()
   {
      return BUCKET_SIZE;
   }
   static constexpr size_type headerSize()
   {
      static_assert(sizeof(Header)%sizeof(value_type) == 0,"Header size is invalid");
      return sizeof(Header) / sizeof(value_type);
   }

   size_type numWasted() const;
   bool isFull() const;
   CRef alloc(const size_type & num);

   //returns true when bucket is completely wasted
   bool remove(const size_type & num);
   void reset();
   bool wasteRest();

 private:

   struct Header
   {
      Header();
      std::atomic<size_type> sz;
      std::atomic<size_type> wastedEntries;

   };
   static constexpr size_type _capacity = (BUCKET_SIZE - sizeof(Header)) / sizeof(value_type);

   Header header;
   value_type mem[_capacity];
};

struct ClauseUpdate
{
   const CRef cref;
   const BaseClause * c;

   bool isReallocation() const
   {
      return isValidRef(cref) && c == nullptr;
   }
   bool isReplacement() const
   {
      return isValidRef(cref) && c != nullptr;
   }
};

class ClauseBucketArray
{
   ClauseBucketArray(const ClauseBucketArray &) = delete;
   ClauseBucketArray(ClauseBucketArray &&) = delete;
   ClauseBucketArray & operator=(const ClauseBucketArray &) = delete;

 public:
   typedef ClauseBucket::value_type value_type;
   typedef ClauseBucket::size_type size_type;
   static_assert(BUCKET_SIZE == sizeof(ClauseBucket), "Mismatch of allocation sizes for buckets.");

   ClauseBucketArray(const SharingHeuristic & sharingHeuristic);
   ~ClauseBucketArray();

   CRef getCRefBucketOffset(const size_type & bId) const;

   size_type getNewBucket();

   static size_type npos();

   size_type numFreeBuckets() const;
   double getNumMBFreeBuckets() const;
   double getNumMBBuckets() const;
   double getNumUsedMB() const;

   BaseClause & getClause(const CRef & cref);
   const BaseClause & getClause(const CRef & cref) const;

   const void * getPrefetchClauseAddr(const CRef & cref) const;

   ClauseUpdate getClauseUpdate(CoreSolver & ts, const CRef cref);

   void addUnit(CoreSolver & ts, const Lit & l);
   template<typename VecType>
   CRef addPrivateClause(CoreSolver & ts, const VecType & lits, const Header & h);
   template<typename VecType>
   CRef addSharedClause(CoreSolver & ts, const VecType & lits, const Header & h, const bool shareClause);

   template<typename VecType>
   CRef makeShared(CoreSolver & ts, const CRef & prevCRef, const VecType & v, const Header & h);
   template<typename VecType>
   CRef makePermanent(CoreSolver & ts, const CRef & prevCRef, const VecType & v, const Header & h);

   template<typename VecType>
   void replacePrivate(CoreSolver & ts, const CRef & prevCRef, const VecType & c, const Header & h);
   template<typename VecType, typename ReplaceChecker>
   std::tuple<bool, CRef> replaceShared(CoreSolver & ts, const CRef & prevCRef, const VecType & c, const Header & h, const ReplaceChecker checker);
   template<typename ReplaceChecker>
   CRef peekLast(CoreSolver & ts, const CRef prevCRef, const ReplaceChecker checker);
   CRef peekLastValid( const CRef prevCRef) const;


   CRef addInitialClause(const Glucose::Clause & c);

   void garbageCollection(CoreSolver & ts);

   /** return a bucket id, when the bucket can be deleted
    */
   void removeClause(CoreSolver & ts, const CRef & cref);

   DatabaseHeuristic & getHeuristic();
   const DatabaseHeuristic & getHeuristic() const;

   DatabaseStatistic & getStatistic();
   const DatabaseStatistic & getStatistic() const;

   CRef getCRef(const BaseClause & c) const
   {
      return static_cast<CRef>(reinterpret_cast<const value_type *>(&c) - reinterpret_cast<const value_type *>(buckets));
   }

   bool shouldBeReplaced(const CRef & cref) const;
   bool shouldBeRemoved(const CRef & cref) const;

   CRef peekReplacement(const CRef & cref) const;

   CRef getReplacement(CoreSolver & ts, const CRef & cref);

   ReferenceSharer & getReferenceSharer();
   const ReferenceSharer & getReferenceSharer() const;

   bool markClauseAsDeleted(const CRef & cref);

   void simpleRemoveClause(CoreSolver & ts, const CRef & cref);

   bool hasSucc(const CRef lookFor, const CRef start, const CRef end);

   void removeLater(CoreSolver & ts, const CRef & startCRef, const CRef & endCRef = CRef_Undef);

 private:
   DatabaseHeuristic heuristic;
   DatabaseStatistic statistic;
   const size_type sz;
   size_type masterBucketId;
   ClauseBucket * buckets;
   ReferenceSharer refShare;
   LockStack<size_type> freeBuckets;

   ClauseBucket & getBucket(const size_type & bIdx);
   const ClauseBucket & getBucket(const size_type & bIdx) const;
   size_type getBucketIdxFromCRef(const CRef & idx) const;
   ClauseBucket & getBucketFromCRef(const CRef & idx);
   const ClauseBucket & getBucketFromCRef(const CRef & idx) const;

   ClauseBucket * getBucketPtr();
   const ClauseBucket * getBucketPtr() const;

   inline bool shouldReallocateClause(const CRef & cref) const
   {
      const ClauseBucket & b = getBucketFromCRef(cref);
      return b.isFull() && b.numWasted() > heuristic.garbageWasteFrac * ClauseBucket::capacity();
   }

   template<typename T>
   const T & getAs(const size_type & idx) const
   {
      const value_type * ptr = reinterpret_cast<const value_type*>(getBucketPtr());
      return *(reinterpret_cast<const T*>(&ptr[idx]));
   }
   template<typename T>
   T & getAs(const size_type & idx)
   {
      value_type * ptr = reinterpret_cast<value_type*>(getBucketPtr());
      return *(reinterpret_cast<T*>(&ptr[idx]));
   }

   void recursiveRemove(CoreSolver & ts, const CRef & startCRef, const CRef & endCRef);

   template<typename VecType>
   CRef insertClause(CoreSolver & ts, const VecType & c, const Header & header);

   template<typename VecType>
   CRef insertClauseIntoBucket(const size_type & bId, const VecType & c, const Header & header);

   inline bool is64BitAligned(const size_type & cref)
   {
      return ((uintptr_t) &(buckets[cref])) % 8 == 0;
   }

   void initialize();

   void removeFromBucket(const size_type & bId, const unsigned & num);
};

template<typename VecType>
void ClauseBucketArray::replacePrivate(CoreSolver & ts, const CRef & prevCRef, const VecType & c, const Header & h)
{
   BaseClause & oldC = getClause(prevCRef);
   assert(oldC.size() > c.size());
   ClauseBucket & b = this->getBucketFromCRef(prevCRef);
   static_assert(sizeof(value_type) == sizeof(Lit),"Data types have wrong sizes");
   b.remove(oldC.size() - c.size());
   oldC.set(c, h);
   assert(oldC.size() == c.size());
}

inline CRef ClauseBucketArray::peekLastValid( const CRef prevCRef) const
{
   assert(!getClause(prevCRef).isPrivateClause());
   CRef res = prevCRef, tmp = getClause(res).shared().getReplaceCRef();
   while(isValidRef(tmp))
   {
      res = tmp;
      tmp = getClause(res).shared().getReplaceCRef();
   }
   return res;
}

inline bool ClauseBucketArray::shouldBeReplaced(const CRef & cref) const
{
   assert(isValidRef(cref));
   const BaseClause & c = getClause(cref);
   return (c.isPermanentClause() || c.isSharedClause()) && c.size() > getClause(peekLastValid(cref)).size();
}

inline CRef ClauseBucketArray::getReplacement(CoreSolver & ts, const CRef & cref)
{
   assert(isValidRef(cref));
   assert(!getClause(cref).isPrivateClause());
   BaseClause & c = getClause(cref);
   assert(c.size() > 2);
   CRef res = cref;
   while (isValidRef(getClause(res).shared().getReplaceCRef()))
   {
      res = getClause(res).shared().getReplaceCRef();
   }
   assert(isValidRef(res));
   assert(cref != res);
   assert(getClause(cref).size() > getClause(res).size());
   removeLater(ts, cref, res);
   return res;
}

inline CRef ClauseBucketArray::peekReplacement(const CRef & cref) const
{
   assert(shouldBeReplaced(cref));
   CRef cur = cref;
   while (isValidRef(cur))
   {
      const BaseClause & c = getClause(cur);
      assert(!c.isPrivateClause());
      cur = c.shared().getReplaceCRef();
   }
   return cur;
}

inline void ClauseBucketArray::addUnit(CoreSolver & ts, const Lit & l)
{
   refShare.addUnary(ts.getThreadState(), l);
}
template<typename VecType>
inline CRef ClauseBucketArray::addPrivateClause(CoreSolver & ts, const VecType & lits, const Header & h)
{
   assert(h.isPrivateClause());
   CRef res = insertClause(ts, lits, h);
   return res;
}

template<typename VecType>
inline CRef ClauseBucketArray::addSharedClause(CoreSolver & ts, const VecType & lits, const Header & h, const bool shareClause)
{
   assert(lits[0] != lit_Undef);
   assert(h.isPermanentClause() || h.isSharedClause());
   CRef res = insertClause(ts, lits, h);
   getClause(res).shared().initialize(heuristic.numThreads);
   if (shareClause)
      refShare.addCRef(ts.getThreadState(), res);
   return res;
}

inline bool ClauseBucketArray::shouldBeRemoved(const CRef & cref) const
{
   bool res;
   assert(isValidRef(cref));
   CRef cur = cref;
   if (!getClause(cref).isPrivateClause())
   {
      while (isValidRef(cur))
      {
         cur = getClause(cur).shared().getReplaceCRef();
      }
      res = cur == CRef_Del;
   } else
      res = getClause(cref).isPrivDel();
   return res;
}

inline bool ClauseBucketArray::markClauseAsDeleted(const CRef & cref)
{
   BaseClause & c = getClause(cref);
   bool res = false;
   if (!c.isPrivateClause())
   {
      CRef curRef = cref;
      while (isValidRef(curRef))
      {
         SharedClause & c = getClause(curRef).shared();
         curRef = c.getReplaceCRef();
         if (curRef == CRef_Undef)
         {
            res = c.markDeleted();
            curRef = c.getReplaceCRef();
         }
      }
      assert(curRef == CRef_Del);
   } else
   {
      res = true;
      c.setPrivDel();
   }
   return res;
}

template<typename VecType>
inline CRef ClauseBucketArray::makeShared(CoreSolver & ts, const CRef & prevCRef, const VecType & v, const Header & h)
{
   assert(h.isSharedClause() && getClause(prevCRef).isPrivateClause());
   CRef res = addSharedClause(ts, v, h, true);
   removeLater(ts, prevCRef);

   return res;
}
template<typename VecType>
inline CRef ClauseBucketArray::makePermanent(CoreSolver & ts, const CRef & prevCRef, const VecType & v, const Header & h)
{
   assert(h.isPermanentClause());
   CRef res = CRef_Undef;
   // first mark clause as deleted
   if (markClauseAsDeleted(prevCRef))
   {
      // add and return new permanent clause and delete clause prevCRef
      res = addSharedClause(ts, v, h, true);
      removeLater(ts, prevCRef);
   } else
      // when already deleted, just return prefCRef without doing anything
      res = prevCRef;
   return res;
}

inline CRef ClauseBucketArray::addInitialClause(const Glucose::Clause & c)
{
   size_type & bId = masterBucketId;
   CRef res = CRef_Undef;
   while (res == CRef_Undef)  // while loop could be replaced by two ifs
   {
      res = insertClauseIntoBucket(bId, c, BaseClause::getPermanentClauseHeader(0, c.size()));
      if (res == CRef_Undef)
         bId = getNewBucket();
   }
   getClause(res).shared().initialize(heuristic.numThreads);
   assert(res != CRef_Undef);
   ++statistic.numInitCl;
   return res;
}

inline ClauseBucket * ClauseBucketArray::getBucketPtr()
{
   return buckets;
}
inline const ClauseBucket * ClauseBucketArray::getBucketPtr() const
{
   return buckets;
}

inline BaseClause & ClauseBucketArray::getClause(const CRef & cref)
{
   BaseClause & res = getAs<BaseClause>(cref);
   return res;
}
inline const BaseClause & ClauseBucketArray::getClause(const CRef & cref) const
{
   const BaseClause & res = getAs<BaseClause>(cref);
   return res;
}

inline DatabaseHeuristic & ClauseBucketArray::getHeuristic()
{
   return heuristic;
}
inline const DatabaseHeuristic & ClauseBucketArray::getHeuristic() const
{
   return heuristic;
}

inline DatabaseStatistic & ClauseBucketArray::getStatistic()
{
   return statistic;
}
inline const DatabaseStatistic & ClauseBucketArray::getStatistic() const
{
   return statistic;
}

template<typename VecType>
CRef ClauseBucketArray::insertClause(CoreSolver & ts, const VecType & c, const Header & header)
{
   size_type & bId = ts.getThreadState().getBucketId(header);
   CRef res = insertClauseIntoBucket(bId, c, header);
   if (res == CRef_Undef)
   {
      bId = getNewBucket();
      res = insertClauseIntoBucket(bId, c, header);
   }
   assert(res != CRef_Undef);

   if (header.isPrivateClause())
   {
//      std::cout << ts.getThreadId() << " '" << res << "'pr\n";
      ++ts.getStatistic().nAllocPrivateCl;
   } else if (header.isSharedClause())
   {
//      std::cout << ts.getThreadId() << " '" << res << "'sh\n";
      ++ts.getStatistic().nAllocSharedCl;
   } else
   {
//      std::cout << ts.getThreadId() << " '" << res << "'pe\n";
      ++ts.getStatistic().nAllocPermanentCl;
   }
//   std::cout.flush();
   return res;
}

template<typename VecType>
CRef ClauseBucketArray::insertClauseIntoBucket(const ClauseBucketArray::size_type & bId, const VecType & c, const Header & header)
{
   CRef res = CRef_Undef;
   ClauseBucket & bucket = getBucket(bId);
   unsigned memory = BaseClause::getMemory(header) + BaseClause::needsAlignment(header), offset = BaseClause::getOffset(header);
   if (memory > ClauseBucket::capacity())
   {
      std::cout << "Error: Clause is too big for buckets. Increase bucket size!" << std::endl;
      std::cout << "Info: Clause needs " << memory * sizeof(value_type) << " bytes, buckets provide maximal " << ClauseBucket::capacity() * sizeof(value_type) << " bytes"
                << std::endl;
      throw OutOfMemoryException();
   }
   if ((res = bucket.alloc(memory)) != ClauseBucket::npos())
   {
      res += getCRefBucketOffset(bId);
      if (BaseClause::needsAlignment(header) && !is64BitAligned(res))
         ++offset;

      res += offset;  // offset for different clause types
      bucket.remove(BaseClause::getOffset(header));  // remove offset now so later no check for clause type is necessary
      assert(!BaseClause::needsAlignment(header) || is64BitAligned(res));
//      std::cout << "alloc '" << res << "'\n";
//      std::cout.flush();
      BaseClause & C = getAs<BaseClause>(res);
      C.set(c, header);
      assert(C.size() == c.size());
   } else
   {
      if (bucket.wasteRest())
         removeFromBucket(bId, 0);
   }

   return res;
}
template<typename ReplaceChecker>
CRef ClauseBucketArray::peekLast(CoreSolver & ts, const CRef prevCRef, const ReplaceChecker checker)
{
   CRef cur = prevCRef;
   if (!getClause(prevCRef).isPrivateClause())
   {
      while (isValidRef(getClause(cur).shared().getReplaceCRef()) && checker(getClause(cur).shared().getReplaceCRef()))
         cur = getClause(cur).shared().getReplaceCRef();
   }
   return cur;
}
/*
 * Returns the CRef of the last in the replace chain. When checker returned true for the last part in the chain, a new clause with c and h was appended.
 */
template<typename VecType, typename ReplaceChecker>
std::tuple<bool, CRef> ClauseBucketArray::replaceShared(CoreSolver & ts, const CRef & prevCRef, const VecType & c, const Header & h, const ReplaceChecker checker)
{
   bool replaced = false;
   CRef replacy = prevCRef, succCRef, newClause = CRef_Undef;
   while (isValidRef(replacy))
   {
      succCRef = getClause(replacy).shared().getReplaceCRef();
      if (succCRef == CRef_Undef && checker(replacy))  // found empty spot, where we are allowed to insert our new clause
      {
         BaseClause & prevC = getClause(replacy);
         if (newClause == CRef_Undef)
            newClause = addSharedClause(ts, c, h, false);
         const ReferenceStateChange refChange = prevC.shared().markReallocated(newClause);
         if (!refChange.isAlreadyReallocated())
         {
            // replacement successful
            getClause(newClause).shared().correctRealloc(heuristic.numThreads, refChange);
            prevC.setReplaced();
            assert(getClause(prevCRef).shared().isReplaced());
            replacy = newClause;
            replaced = true;
            break;
         }
      } else if (isValidRef(succCRef))
      {
         replacy = succCRef;
         assert(h.sameClauseType(getClause(replacy).getHeader()));
      }
      else
         break;
   }
   if (newClause != CRef_Undef && !replaced)
   {
      getClause(newClause).shared().initialize(1);
      removeClause(ts, newClause);
   }
   if (replacy != prevCRef)
   {
      removeLater(ts, prevCRef, replacy);
//      std::cout << ts.getThreadId() << " replaceShared " << prevCRef << " -> " << replacy << "\n";
//      std::cout.flush();
   }
   return std::make_tuple(replaced, replacy);
}

inline ReferenceSharer & ClauseBucketArray::getReferenceSharer()
{
   return refShare;
}
inline const ReferenceSharer & ClauseBucketArray::getReferenceSharer() const
{
   return refShare;
}

} /* namespace Glucose */

#endif /* SHARED_DYNAMICSTACKBUCKETARRAY_H_ */
