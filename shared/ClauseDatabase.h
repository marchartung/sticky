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

#ifndef SHARED_CLAUSEDATABASE_H_
#define SHARED_CLAUSEDATABASE_H_

#include "shared/DatabaseThreadState.h"
#include "shared/ClauseBucketArray.h"
#include "shared/CoreSolver.h"
#include "shared/Heuristic.h"
#include "shared/PropagateResult.h"
#include "shared/ReferenceSharer.h"
#include "shared/Statistic.h"
#include "parallel_utils/SWriterMReaderVec.h"
#include <atomic>
namespace Sticky
{

class ClauseDatabase
{
 public:

   ClauseDatabase();

   ~ClauseDatabase();

   // Access functions:
   unsigned getNumSolverThreads() const;
   unsigned getNumRunningThreads() const;
   const BaseClause & operator[](const CRef cref) const;

   ClauseBucketArray & getBuckets();
   const ClauseBucketArray & getBuckets() const;

   const CoreSolver & getSolver(const unsigned idx) const;

   const SharingHeuristic getSharingHeuristic() const;

   // Clause Adders:
   CRef addInitialClause(const Glucose::Clause & c);
   VarSet addClause(CoreSolver & s, const vec<Lit> & c, const unsigned lbd);
   void addUnit(CoreSolver & s, const Lit u);

   // Clause Access

   // fast:
   template<typename WType>
   const BaseClause & getClause(const WType & w) const;
   template<typename WType>
   BaseClause & getClause(const WType & w);

   template<typename WType>
   const BaseClause & operator[](const WType & w) const;
   template<typename WType>
   BaseClause & operator[](const WType & w);

   bool isProtected(CoreSolver & s, const Watcher & w) const;
   bool isProtected(CoreSolver & s, const VarSet & vs) const;

   unsigned getLbd(const CoreSolver & s, const Watcher & w) const;
   unsigned getLbd(const OneWatcher & w) const;
   unsigned getLbd(const BinaryWatcher & w) const;
   template<typename WType>
   CRef getCRef(const WType & w) const;
   unsigned getActivity(const CoreSolver & s, const Watcher & w) const;
   unsigned getActivity(const CoreSolver & s, const OneWatcher & w) const;
   unsigned getActivity(const CoreSolver & s, const BinaryWatcher & w) const;

   Lit getPropagatedLit(const CoreSolver & s, const VarSet & vs) const;

   // slower:
   const BaseClause & getClause(const CoreSolver & s, const VarSet & vs) const;
   BaseClause & getClause(const CoreSolver & s, const VarSet & vs);
   const BaseClause & operator()(const CoreSolver & s, const VarSet & w) const;
   BaseClause & operator()(const CoreSolver & s, const VarSet & w);

   unsigned getLbd(const CoreSolver & s, const VarSet & vs) const;
   CRef getCRef(const CoreSolver & s, const VarSet & vs) const;
   unsigned getActivity(const CoreSolver & s, const VarSet & vs) const;
   void increaseActivity(CoreSolver & s, const VarSet & vs);

   void setProtected(CoreSolver & s, const VarSet & vs);
   void unsetProtected(CoreSolver & s, const VarSet & vs);

   void markAsToVivify(CoreSolver & s, const VarSet & vs, const bool enforce = false);
   // Notifiers:
   void notifySolverStart(CoreSolver & s);
   void notifySolverEnd(CoreSolver & s);
   void notifyClauseUsedInConflict(CoreSolver & s, VarSet & vs);
   void notifyNewConflict(CoreSolver & s);
   void notifyRestart(CoreSolver & s);

   void importUnits(CoreSolver & s);
   void importCRefs(CoreSolver & s);

   bool shouldCollectCRefs();
   bool replaceClause(CoreSolver & s, const CRef cref, const vec<Lit> & newC, const unsigned lbd);
   template<typename VecType>
   bool replaceClause(CoreSolver & s, const CRef cref, const VarSet & wpos, const VecType & newC, const Header & h);

// Solve state related:

   bool foundSolution(CoreSolver & s, lbool result, const std::string & msg);
   const CoreSolver& winner() const;        // Gets the first solver that called IFinished()
   bool jobFinished() const;                // True if the job is over
   lbool getResult() const;

   SWriterMReaderVec<CRef> & getCompleteViviRefs();

 private:

   std::atomic<unsigned> completeViviInProgress;
   SWriterMReaderVec<CRef> completeViviRefs;

   std::atomic<bool> abortSolving;
   std::atomic<unsigned> numRunningThreads;
   std::atomic<const CoreSolver*> finishedSolver;

   std::atomic<unsigned> numStartVivis;
   std::atomic<unsigned> numStartClVivs;
   pthread_barrier_t startVivBarrier;
 protected:

   SharingHeuristic sharingHeuristic;
   unsigned reduceWatchVersion;
   unsigned numReducesSinceCleanUp;
   unsigned completeClauseNum;

   ClauseBucketArray buckets;
   vec<CoreSolver*> solvers;

   void checkVivifyComplete(const GlobalStatistic & gstat);
   void setAbortSolving(const bool b);
 private:

   void updateLBD(CoreSolver & s, VarSet & vs, const unsigned lbd);
   bool tryShareClause(CoreSolver & s, VarSet & vs);

   bool shouldReduce(const CoreSolver & s) const;
   bool shouldImportClauses(const CoreSolver & s) const;
   void reduce(CoreSolver & s);
   void improveClauses(CoreSolver & s);

   void setBudgetTillNextReduce(CoreSolver & s);

   void setLbd(CoreSolver & s, const VarSet & vs, const unsigned lbd);

   unsigned checkCompleteVivification(CoreSolver & s, const bool enforce);

};

template<typename VecType>
bool ClauseDatabase::replaceClause(CoreSolver & s, const CRef cref, const VarSet & wpos, const VecType & newC, const Header & h)
{
   assert(wpos.isValid());
   bool res = false;
   CRef newRef = CRef_Undef;
   if (newC.size() == 1)
   {
      s.getThreadState().twoWatched.detach(s, wpos);
      buckets.removeLater(s, cref);
      res = true;
   } else
   {
      BaseClause & c = buckets.getClause(cref);
      const bool inplace = newC.size() == c.size();
      if (!c.isPrivateClause() && h.isSharedClause() == c.isSharedClause())  // inplace clause exchange possible
         std::tie(res, newRef) = buckets.replaceShared(s, cref, newC, h, [this,&newC](const CRef & cref)
         {  return buckets.getClause(cref).size() > newC.size();});
      else if (!c.getHeader().sameClauseType(h))  // new clause must be exchanged
      {
         if (h.isPermanentClause())
            newRef = buckets.makePermanent(s, cref, newC, h);
         else
         {
            assert(h.isSharedClause());
            newRef = buckets.makeShared(s, cref, newC, h);
         }
         res = isValidRef(newRef) && newRef != cref;
      } else
      {
         assert(c.isPrivateClause() && h.isPrivateClause());
         if (c.isPrivDel())
            res = false;
         else
         {
            buckets.replacePrivate(s, cref, newC, h);
            newRef = cref;
            res = true;
         }
      }
      if (res)
      {
//         std::cout << s.getThreadId() << " replaced '" << cref << "' -> '" << newRef << "'\n";
//         std::cout.flush();
         TwoWatcherLists & two = s.getThreadState().twoWatched;
         if (buckets.getClause(newRef).size() == 2)
         {
            assert(buckets.getClause(cref).size() > newC.size());
            two.detach(s, wpos);
            s.getThreadState().binWatched.attach(newRef, s);

         } else
         {
            const Watcher & w = two.getWatcher(wpos);
            assert(isValidRef(newRef));
            if (inplace)
               two.changeCRef(s, wpos, newRef);
            else
            {
               const CRef lbd = (w.isHeader()) ? w.getLbd() : two.getOtherWatcher(w).getLbd();
               two.detach(s, wpos);
               two.attachOnFirst(newRef, s, lbd);
            }
         }
      }
   }
   const BaseClause & oldC = buckets.getClause(cref);
   if (!h.sameClauseType(oldC.getHeader()))
   {
      if (oldC.isPrivateClause())
         --s.getStatistic().nPrivateCl;
      else if (oldC.isSharedClause())
         --s.getStatistic().nSharedCl;
      if (isValidRef(newRef) && buckets.getClause(newRef).isSharedClause())
         ++s.getStatistic().nSharedCl;
   }
   return res;
}

inline SWriterMReaderVec<CRef> & ClauseDatabase::getCompleteViviRefs()
{
   return completeViviRefs;
}

inline void ClauseDatabase::checkVivifyComplete(const GlobalStatistic & gstat)
{
   if (sharingHeuristic.useCompleteVivification && completeViviInProgress == 0)
   {
      // only improve permanent clauses when the number of permanent and the number of 'mutual used' shared clauses doubled
      unsigned numPer = gstat.numPermanent.value();
      unsigned numSh = std::max(gstat.numAllocatedSharedClauses.sum(), 1l);
      unsigned numPr = gstat.numAllocatedPrivateClauses.sum();
      double percentSharedWatched = static_cast<double>(gstat.numSharedClauses.average()) / numSh;
      unsigned numRelevantClauses = numPer + (percentSharedWatched * numSh) + static_cast<double>(numPr) / gstat.numSharedClauses.size();
      if (numRelevantClauses > completeClauseNum)
      {
         std::cout << "c starting complete vivification" << std::endl;
         completeClauseNum = 2 * numRelevantClauses;
         unsigned expected = 0;
         completeViviInProgress.compare_exchange_strong(expected, 1);
      } else
         ;  //std::cout << "c rel cl: " << numRelevantClauses << " needed: " << completeClauseNum << std::endl;
   }
}

inline bool ClauseDatabase::shouldCollectCRefs()
{
   bool res = false;
   if (completeViviInProgress == 1)
   {
      unsigned expected = 1;
      res = completeViviInProgress.compare_exchange_strong(expected, 2);
   }
   return res;
}

inline const SharingHeuristic ClauseDatabase::getSharingHeuristic() const
{
   return sharingHeuristic;
}

inline const CoreSolver & ClauseDatabase::getSolver(const unsigned idx) const
{
   assert(solvers[idx] != nullptr);
   return *solvers[idx];
}
inline unsigned ClauseDatabase::getActivity(const CoreSolver & s, const Watcher & w) const
{
   return (w.isHeader()) ? w.getActivity() : s.getThreadState().twoWatched.getOtherWatcher(w).getActivity();
}
inline unsigned ClauseDatabase::getActivity(const CoreSolver & s, const OneWatcher & w) const
{
   return 0;
}
inline unsigned ClauseDatabase::getActivity(const CoreSolver & s, const BinaryWatcher & w) const
{
   return std::numeric_limits<unsigned>::max();
}

inline unsigned ClauseDatabase::getActivity(const CoreSolver & s, const VarSet & vs) const
{
   unsigned res = 0;
   switch (vs.getWatcherType())
   {
      case WatcherType::TWO:
         res = getActivity(s, s.getThreadState().twoWatched.getWatcher(vs));
         break;
      case WatcherType::BINARY:
         res = getActivity(s, s.getThreadState().binWatched.getWatcher(vs));
         break;
      case WatcherType::ONE:
         res = getActivity(s, s.getThreadState().oneWatched.getWatcher(vs));
         break;
      default:
         assert(false);
   }
   return res;
}
inline Lit ClauseDatabase::getPropagatedLit(const CoreSolver & s, const VarSet & vs) const
{
   Lit res = lit_Undef;
   switch (vs.getWatcherType())
   {
      case WatcherType::TWO:
         res = s.getThreadState().twoWatched.getWatcher(vs).getBlocker();
         break;
      case WatcherType::BINARY:
         res = s.getThreadState().binWatched.getWatcher(vs).getBlocker();
         break;
      default:
         ;
   }
   return res;
}

inline void ClauseDatabase::increaseActivity(CoreSolver & s, const VarSet & vs)
{
   assert(vs.getWatcherType() == WatcherType::TWO);
   DatabaseThreadState & ts = s.getThreadState();
   Watcher & w = ts.twoWatched.getWatcher(vs);
   Watcher & wr = (w.isHeader()) ? w : ts.twoWatched.getOtherWatcher(w);
   wr.increaseActivity();
}

inline const BaseClause & ClauseDatabase::getClause(const CoreSolver & s, const VarSet & vs) const
{
   switch (vs.getWatcherType())
   {
      case WatcherType::ONE:
         return buckets.getClause(s.getThreadState().oneWatched.getCRef(vs));
      case WatcherType::TWO:
         return buckets.getClause(s.getThreadState().twoWatched.getCRef(vs));
      case WatcherType::BINARY:
         return buckets.getClause(s.getThreadState().binWatched.getCRef(vs));
      default:
         assert(false);
         return *(reinterpret_cast<const BaseClause*>(0));
   }
}

inline BaseClause & ClauseDatabase::getClause(const CoreSolver & s, const VarSet & vs)
{
   switch (vs.getWatcherType())
   {
      case WatcherType::ONE:
         return buckets.getClause(s.getThreadState().oneWatched.getCRef(vs));
      case WatcherType::TWO:
         return buckets.getClause(s.getThreadState().twoWatched.getCRef(vs));
      case WatcherType::BINARY:
         return buckets.getClause(s.getThreadState().binWatched.getCRef(vs));
      default:
         assert(false);
         return *(reinterpret_cast<BaseClause*>(0));
   }
}

inline const BaseClause & ClauseDatabase::operator()(const CoreSolver & s, const VarSet & w) const
{
   return getClause(s, w);
}

inline BaseClause & ClauseDatabase::operator()(const CoreSolver & s, const VarSet & w)
{
   return getClause(s, w);
}

inline ClauseBucketArray & ClauseDatabase::getBuckets()
{
   return buckets;
}
inline const ClauseBucketArray & ClauseDatabase::getBuckets() const
{
   return buckets;
}

inline unsigned ClauseDatabase::getLbd(const CoreSolver & s, const VarSet & vs) const
{
   unsigned res = 0;
   switch (vs.getWatcherType())
   {
      case WatcherType::ONE:
         res = getLbd(s.getThreadState().oneWatched.getWatcher(vs));
         break;
      case WatcherType::TWO:
         res = getLbd(s, s.getThreadState().twoWatched.getWatcher(vs));
         break;
      case WatcherType::BINARY:
         res = getLbd(s.getThreadState().binWatched.getWatcher(vs));
         break;
      default:
         assert(false);
   }
   return res;
}

inline unsigned ClauseDatabase::getLbd(const OneWatcher & w) const
{
   return getClause(w).size();
}

inline unsigned ClauseDatabase::getLbd(const CoreSolver & s, const Watcher & w) const
{
   const Watcher & wr = (w.isHeader()) ? w : s.getThreadState().twoWatched.getOtherWatcher(w);
   return wr.getLbd();
}

inline unsigned ClauseDatabase::getLbd(const BinaryWatcher & w) const
{
   return 1;
}

template<typename WType>
inline CRef ClauseDatabase::getCRef(const WType & w) const
{
   return w.getCRef();
}

template<typename WType>
inline const BaseClause & ClauseDatabase::getClause(const WType & w) const
{
   return buckets.getClause(w.getCRef());
}
template<typename WType>
inline BaseClause & ClauseDatabase::getClause(const WType & w)
{
   return buckets.getClause(w.getCRef());
}

template<typename WType>
inline const BaseClause & ClauseDatabase::operator[](const WType & w) const
{
   return getClause(w);
}
template<typename WType>
inline BaseClause & ClauseDatabase::operator[](const WType & w)
{
   return getClause(w);
}

inline bool ClauseDatabase::isProtected(CoreSolver & s, const Watcher & w) const
{
   return (w.isHeader()) ? s.getThreadState().twoWatched.getOtherWatcher(w).isProtected() : w.isProtected();
}

inline bool ClauseDatabase::isProtected(CoreSolver & s, const VarSet & vs) const
{
   if (vs.getWatcherType() == WatcherType::TWO)
      return isProtected(s, s.getThreadState().twoWatched.getWatcher(vs));
   else
      return true;
}

inline unsigned ClauseDatabase::getNumSolverThreads() const
{
   return buckets.getHeuristic().numThreads;
}

inline unsigned ClauseDatabase::getNumRunningThreads() const
{
   return numRunningThreads;
}

inline const BaseClause & ClauseDatabase::operator[](const CRef cref) const
{
   return buckets.getClause(cref);
}

} /* namespace Glucose */

#endif /* SHARED_CLAUSEDATABASE_H_ */
