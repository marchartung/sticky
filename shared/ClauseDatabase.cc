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

#include "shared/SharedTypes.h"
#include "shared/ClauseTypes.h"
#include "shared/ClauseDatabase.h"
#include "shared/ClauseBucketArray.h"
#include "shared/CoreSolver.h"
#include "shared/Heuristic.h"
#include "shared/ClauseWatcher.h"
#include "shared/ClauseReducer.h"

namespace Sticky
{

ClauseDatabase::ClauseDatabase()
      : completeViviInProgress(0),
        abortSolving(false),
        numRunningThreads(0),
        finishedSolver(nullptr),
        numStartVivis(0),
        numStartClVivs(1),
        sharingHeuristic(),
        reduceWatchVersion(0),
        numReducesSinceCleanUp(0),
        completeClauseNum(0),
        buckets(sharingHeuristic),
        solvers(buckets.getHeuristic().numThreads, nullptr)
{
   assert(buckets.getHeuristic().numThreads > 0);
   pthread_barrier_init(&startVivBarrier, 0, buckets.getHeuristic().numThreads);
}

ClauseDatabase::~ClauseDatabase()
{
   pthread_barrier_destroy(&startVivBarrier);
}

void ClauseDatabase::setLbd(CoreSolver & s, const VarSet & vs, const unsigned lbd)
{
   assert(vs.getWatcherType() == WatcherType::TWO);
   Watcher & w = s.getThreadState().twoWatched.getWatcher(vs);
   Watcher & wr = (w.isHeader()) ? w : s.getThreadState().twoWatched.getOtherWatcher(w);
   wr.setLbd(lbd);
}

void ClauseDatabase::updateLBD(CoreSolver & s, VarSet & vs, const unsigned lbd)
{
   assert(vs.getWatcherType() == WatcherType::TWO);
   setLbd(s, vs, lbd);
   //setProtected(s, vs);
}

inline void ClauseDatabase::setProtected(CoreSolver & s, const VarSet & vs)
{
   if (vs.getWatcherType() == WatcherType::TWO && !isProtected(s, vs))
   {
      auto & w = s.getThreadState().twoWatched.getWatcher(vs);
      if (w.isHeader())
         s.getThreadState().twoWatched.getOtherWatcher(vs).setProtected(true);
      else
         w.setProtected(true);
   }
}

void ClauseDatabase::unsetProtected(CoreSolver & s, const VarSet & vs)
{
   if (vs.getWatcherType() == WatcherType::TWO)
   {
      auto & w = s.getThreadState().twoWatched.getWatcher(vs);
      if (w.isHeader())
         s.getThreadState().twoWatched.getOtherWatcher(vs).setProtected(false);
      else
         w.setProtected(false);
   }
}

void ClauseDatabase::markAsToVivify(CoreSolver & s, const VarSet & vs, const bool enforce)
{
   const BaseClause & c = getClause(s, vs);
   if (vs.getWatcherType() == WatcherType::TWO && (!c.isVivified() || enforce))
   {
      Watcher & w = s.getThreadState().twoWatched.getWatcher(vs);
//      if (w.getCRef() == 1089288)
//         std::cout << "ohoh\n";
      unsigned lbd = getLbd(s, vs);
      if (c.isPermanentClause() || (lbd <= sharingHeuristic.maxVivificationLbd && static_cast<unsigned>(c.size() - 1) > lbd && !buckets.getClause(w.getCRef()).isPrivateClause()))
      {
         BaseClause & c = buckets.getClause(w.getCRef());
         c.setVivified();
//         if (w.getCRef() == 634024)
//            std::cout << "ihhh\n";
         s.getThreadState().toVivifyRefs.push(w.getCRef());
//         std::cout << s.getThreadId() << " entered vivify '" << w.getCRef() << "'\n";
//         std::cout.flush();
      }
   }
}

CRef ClauseDatabase::getCRef(const CoreSolver & s, const VarSet & w) const
{
   CRef res = CRef_Undef;
   switch (w.getWatcherType())
   {
      case WatcherType::BINARY:
         res = s.getThreadState().binWatched.getCRef(w);
         break;
      case WatcherType::ONE:
         res = s.getThreadState().oneWatched.getCRef(w);
         break;
      case WatcherType::TWO:
         res = s.getThreadState().twoWatched.getCRef(w);
         break;
      default:
         assert(false);
   }
   return res;
}

bool ClauseDatabase::replaceClause(CoreSolver & s, const CRef cref, const vec<Lit> & newC, const unsigned lbd)
{
   const BaseClause & c = buckets.getClause(cref);
   VarSet pos = s.getThreadState().twoWatched.findWatcher(cref);
   bool res = pos.isValid();
   if (res)
   {
      assert(this->getCRef(s, pos) == cref);
      assert(s.getThreadState().twoWatched.getWatcher(pos).getCRef() == cref);
      Header h;
      if (sharingHeuristic.isPermanentClause(lbd, newC.size()) || c.isPermanentClause())
         h = BaseClause::getPermanentClauseHeader(c, lbd, newC.size(),c.isVivified());
      else if (sharingHeuristic.isSharedClause(lbd, newC.size()) || c.isSharedClause())
         h = BaseClause::getSharedClauseHeader(c, lbd, newC.size(),c.isVivified());
      else
         h = BaseClause::getPrivateClauseHeader(c, lbd, newC.size(),c.isVivified());
      res = replaceClause(s, cref, pos, newC, h);
   }
   return res;
}

bool ClauseDatabase::tryShareClause(CoreSolver & s, VarSet & vs)
{
   bool res = false;
   const CRef cref = getCRef(s, vs);
   const SharingHeuristic & sh = sharingHeuristic;
   const BaseClause & c = buckets.getClause(cref);
   const int lbd = getLbd(s, vs);
   const int cSize = c.size();

   assert(cSize > 2);
   assert(!c.isPermanentClause());
   Header h;
   if (sh.isPermanentClause(lbd, cSize, getActivity(s, vs)))
      h = BaseClause::getPermanentClauseHeader(c, lbd);
   else if (sharingHeuristic.isSharedClause(lbd, cSize) || c.isSharedClause())
      h = BaseClause::getSharedClauseHeader(c, lbd);
   else
      h = BaseClause::getPrivateClauseHeader(c, lbd);
   if (!h.sameClauseType(c.getHeader()))
   {
      res = replaceClause(s, cref, vs, c, h);
      assert(!res || !s.getThreadState().twoWatched.findWatcher(cref).isValid());
      if (res && c.isPrivateClause())
         ++s.getStatistic().nExportedCl;
   }
   return res;
}

void ClauseDatabase::setBudgetTillNextReduce(CoreSolver & s)
{
   DatabaseThreadState & state = s.getThreadState();
   SolverStatistic & stat = s.getStatistic();
   const SolverHeuristic& heur = s.getHeuristic();
   stat.nLastReduceConflicts = stat.nConflicts;
   state.restartFactor = (stat.nConflicts / state.nConflictsBeforeReduce) + 1;
   state.nConflictsBeforeReduce += heur.incReduceDB;
}

bool ClauseDatabase::shouldReduce(const CoreSolver & s) const
{
   const DatabaseThreadState & state = s.getThreadState();
   return s.getLiteralSetting().decisionLevel() == 0 && s.getStatistic().nConflicts >= state.restartFactor * state.nConflictsBeforeReduce;
}

void ClauseDatabase::reduce(CoreSolver & s)
{
   ClauseReducer cred(*this, buckets, s);
   cred.reduce();
   setBudgetTillNextReduce(s);
}

void ClauseDatabase::addUnit(CoreSolver & s, const Lit u)
{
   buckets.addUnit(s, u);
}

VarSet ClauseDatabase::addClause(CoreSolver & s, const vec<Lit> & c, const unsigned lbd)
{
   VarSet res;
   res.setUnit();
   ++s.getStatistic().nExportedCl;
   unsigned sz = c.size();
   if (c.size() == 1)
      addUnit(s, c[0]);
   else
   {
      const SharingHeuristic & sh = sharingHeuristic;
      CRef cref = CRef_Undef;
      if (sh.isPermanentClause(lbd, sz, 0))
      {
         cref = buckets.addSharedClause(s, c, BaseClause::getPermanentClauseHeader(lbd, sz), true);
      } else if (sh.isSharedClause(lbd, sz, 0))
      {
         cref = buckets.addSharedClause(s, c, BaseClause::getSharedClauseHeader(lbd, sz), true);
         ++s.getStatistic().nSharedCl;
      } else
      {
         cref = buckets.addPrivateClause(s, c, BaseClause::getPrivateClauseHeader(lbd, sz));
         ++s.getStatistic().nPrivateCl;
         --s.getStatistic().nExportedCl;
      }
      BaseClause & c = buckets.getClause(cref);
      if (c.size() > 2)
      {
         res = s.getThreadState().twoWatched.attachOnFirst(cref, s, lbd);
      } else
      {
         assert(c.size() == 2);
         auto tmp = s.getThreadState().binWatched.attach(cref, s);
         assert(!tmp.isConflict());
         res = tmp.getVarSet();
      }
   }
   return res;
}

CRef ClauseDatabase::addInitialClause(const Glucose::Clause & c)
{
   assert(c.size() > 1);
   CRef cref = buckets.addInitialClause(c);
   return cref;
}

void ClauseDatabase::notifyClauseUsedInConflict(CoreSolver & s, VarSet & vs)
{
   bool shouldbeVivi = false;
   BaseClause & c = getClause(s, vs);
   unsigned nblevels = c.getLbd();
   if (vs.getWatcherType() == WatcherType::ONE)
   {
      DatabaseThreadState & ts = s.getThreadState();
      VarSet tmp = vs;
      vs = ts.twoWatched.attachOneWatched(ts.oneWatched.getWatcher(vs), s, vs.getWatchedLit(), nblevels);
      ts.oneWatched.detach(s, tmp);
      ++s.getStatistic().nPromotedCl;
   }
   if (c.getLbd() > 2)
   {
      increaseActivity(s, vs);
      // DYNAMIC NBLEVEL trick (see competition'09 companion paper)
      nblevels = s.computeLBD(c);
      if (nblevels < c.getLbd())
      {
         updateLBD(s, vs, nblevels);  // improve the LBD
         shouldbeVivi = sharingHeuristic.useLBDImproveVivification && nblevels <= s.getStatistic().medianLbd && static_cast<int>(nblevels) < getClause(s, vs).size();
      }
   }
   if (!c.isPermanentClause())
      shouldbeVivi |= tryShareClause(s, vs) && sharingHeuristic.useExportVivification;

   if (shouldbeVivi)
      markAsToVivify(s, vs);
}

bool ClauseDatabase::shouldImportClauses(const CoreSolver & s) const
{
   return s.getLiteralSetting().decisionLevel() == 0
         || (sharingHeuristic.useEarlyImport && (s.getStatistic().nConflicts & sharingHeuristic.importAfterConflicts) == 0
               && sharingHeuristic.importAfterConflicts < buckets.getReferenceSharer().getNumNewCRefs(s.getThreadState()));
}
void ClauseDatabase::importCRefs(CoreSolver & s)
{
   if (buckets.getReferenceSharer().getNumNewCRefs(s.getThreadState()) > 0)
   {
      bool wasAttached;
      DatabaseThreadState & ts = s.getThreadState();
      LiteralSetting & ls = s.getLiteralSetting();
      PropagateResult confl;

      buckets.getReferenceSharer().getNewCRefs(s.getThreadState());
// always insert non-units first. A permanent clause could be reduced due to units, which could lead to wrong SAT solutions, when inserted without the unit clause
      for (int i = 0; i < ts.crefBuffer.size(); ++i)
      {
         if (ts.crefBuffer[i].tid != s.getThreadId())
         {
            assert(!s.getThreadState().twoWatched.findWatcher(ts.crefBuffer[i].cref).isValid());
            if (buckets.shouldBeRemoved(ts.crefBuffer[i].cref))
            {
               buckets.removeClause(s, ts.crefBuffer[i].cref);
            } else
            {
               CRef cref = ts.crefBuffer[i].cref;
               if (buckets.shouldBeReplaced(cref))
               {
                  cref = buckets.getReplacement(s, cref);
               }
               //std::string str;
               wasAttached = false;
               const BaseClause & c = buckets.getClause(cref);
               if (!c.isPermanentClause())
               {
                  assert(c.size() > 2);
                  assert(!c.isPrivateClause());
                  ++s.getStatistic().nSharedCl;
                  if (sharingHeuristic.isOneWatchedClause(c))
                  {
                     wasAttached = ts.oneWatched.attach(cref, c, s);
                     if (!wasAttached)
                        ++s.getStatistic().nPromotedCl;
                     else
                     {
//                        std::cout << s.getThreadId() << " imported '" << cref << "'\n";
//                        std::cout.flush();
                     }
                  }
               }

               if (!wasAttached)
               {
                  if (c.size() == 2)
                  {
                     confl = ts.binWatched.attach(cref, s);
//                     std::cout << s.getThreadId() << " imported '" << cref << "'\n";
//                     std::cout.flush();
                     assert(getCRef(s, confl.getVarSet()) == cref);
                     if (confl.isConflict())
                     {
                        Lit minLvlLit = (ls.level(c[0]) < ls.level(c[1])) ? c[0] : c[1], maxLvlLit = (minLvlLit == c[0]) ? c[1] : c[0];
                        int minLvl = ls.level(minLvlLit), maxLvl = ls.level(maxLvlLit);
                        if (maxLvl == 0)
                           s.setResult(l_False, "Conflict through clause import");
                        else if (minLvl == maxLvl)
                           s.cancelUntil(ls.level(minLvlLit) - 1);
                        else
                        {
                           s.cancelUntil(ls.level(minLvlLit));
                           assert(ls.value(minLvlLit) == l_False && ls.value(maxLvlLit) == l_Undef);
                           confl.getVarSet().setPropagated(WatcherType::BINARY);
                           s.uncheckedEnqueue(maxLvlLit, confl.getVarSet());
                        }
                     }
                  } else
                  {
                     assert(c.size() > 2);
                     confl = ts.twoWatched.attach(cref, s, (c.isPermanentClause()) ? c.getLbd() : c.size());
                     ts.twoWatched.getStateWatcher(confl.getVarSet()).setImported(true);
//                     std::cout << s.getThreadId() << " imported '" << cref << "'\n";
//                     std::cout.flush();
                     assert(getCRef(s, confl.getVarSet()) == cref);

                     if (confl.isConflict())
                     {
                        ts.twoWatched.detach(s, confl.getVarSet());
                        Lit a = c[0], b = lit_Undef;
                        for (int i = 1; i < c.size(); ++i)
                        {
                           assert(ls.value(c[i]) == l_False);
                           if (ls.level(c[i]) > ls.level(a))
                           {
                              b = a;
                              a = c[i];
                           } else if ((b == lit_Undef || ls.level(c[i]) > ls.level(b)) && ls.level(a) != ls.level(c[i]))
                           {
                              b = c[i];
                           }
                        }
                        if (ls.level(a) == 0)  // Clause is unsat in level 0 -> conflict
                           s.setResult(l_False, "Conflict through clause import");
                        else if (b == lit_Undef)  // every lit was propagated in the same level, TODO I think we cannot use anything here, so currently just backtrack, so the clause is unset
                        {
                           for (int i = 0; i < c.size(); ++i)
                              assert(ls.level(a) == ls.level(c[i]));
                           assert(ls.level(a) > 0);
                           s.cancelUntil(ls.level(a) - 1);
                           assert(ls.value(a) == l_Undef);

                        } else  // clause can be used to propagate, lazy attach or conflict
                        {
                           // possibility 1: backtrack to the propagation level (second highest level of all lits, here 'b') and propagate highest level lit, here 'a', TODO buggy
//                        s.cancelUntil(ls.level(b));
//                        lbool va = ls.value(a), vb = ls.value(b);
//                        assert(vb == l_False && va == l_Undef);
//                        assert(ls.level(b) == ls.decisionLevel());
//                        confl.getVarSet() = ts.twoWatched.plainAttach(s, ts.crefBuffer[i].cref, a, b, (c.isPermanentClause()) ? c.getLbd() : c.size());
//                        confl.getVarSet().setPropagated(WatcherType::TWO);
//                        assert(confl.getVarSet().getWatchedLit() == a);
//                        s.uncheckedEnqueue(a, confl.getVarSet());

                           // possibility 2 (lazy): backtrack so highest level lit is unset and keep one watcher on a false value i.e. one watched clause
                           s.cancelUntil(ls.level(a) - 1);
                           lbool va = ls.value(a), vb = ls.value(b);
                           assert(vb == l_False && va == l_Undef);
                           ts.twoWatched.plainAttach(s, cref, a, b, (c.isPermanentClause()) ? c.getLbd() : c.size());
                           ts.twoWatched.getStateWatcher(confl.getVarSet()).setImported(true);

                           // possibility 3 (conflict): attach clause and call resolveConflict
                        }
                     }
                  }
               }
               ++s.getStatistic().nImportedCl;
            }
         }
      }
      ts.crefBuffer.clear();
   }
}

void ClauseDatabase::importUnits(CoreSolver & s)
{
   DatabaseThreadState & ts = s.getThreadState();
   LiteralSetting & ls = s.getLiteralSetting();
   buckets.getReferenceSharer().getNewUnaries(s.getThreadState());
   for (int i = 0; i < ts.litBuffer.size(); ++i)
   {
      const Lit l = ts.litBuffer[i];
      if (ls.value(l) == l_Undef || ls.level(l) > 0)
      {
         if (ls.decisionLevel() > 0)
            s.cancelUntil(0);  // currently not possible to insert unit clauses when decision level is higher than zero

         bool check = s.enqueue(l, VarSet());
         assert(check);
         s.getLiteralSetting().newUnits.push(l);
      } else if (ls.value(l) == l_False && ls.level(l) == 0)
      {
         s.setResult(l_False, "Conflict through unit import");
         break;
      }
   }
   ts.litBuffer.clear();
}

void ClauseDatabase::notifyNewConflict(CoreSolver & s)
{
   if (s.getLiteralSetting().decisionLevel() == 0)
      notifyRestart(s);
   else if (shouldImportClauses(s))
   {
      importCRefs(s);
      importUnits(s);
   }
}

void ClauseDatabase::notifyRestart(CoreSolver & s)
{
   assert(s.getLiteralSetting().decisionLevel() == 0);
   checkCompleteVivification(s, false);
   if (shouldReduce(s))
   {
//         std::cout << s.getThreadId() << " impro:\n";
//         std::cout.flush();
      improveClauses(s);
      //         std::cout << s.getThreadId() << " reduce:\n";
      //         std::cout.flush();
      reduce(s);
      //         std::cout << s.getThreadId() << " end:\n";
      //         std::cout.flush();
      ++s.getStatistic().nReduces;
   }

   importCRefs(s);  // always import clauses after reduce
   importUnits(s);
}

void ClauseDatabase::notifySolverStart(CoreSolver & s)
{
   ++numRunningThreads;
   while (numStartVivis < sharingHeuristic.numStartUpVivification && numStartClVivs > 0)
   {
      pthread_barrier_wait(&startVivBarrier);
      unsigned numLocalVivis = 0;
      if (s.getThreadId() == 0)
      {
         completeViviRefs.writeLock();
         completeViviRefs = s.getThreadState().twoWatched.getAllCRefs();
         completeViviRefs.releaseWriteLock();
         ++numStartVivis;
         numStartClVivs = 0;
      }
      pthread_barrier_wait(&startVivBarrier);
      numLocalVivis = checkCompleteVivification(s, true);
      numStartClVivs += numLocalVivis;
      pthread_barrier_wait(&startVivBarrier);
      ClauseReducer(*this, buckets, s).cleanBinaryWatched();
      s.getThreadState().twoWatched.removeMarkedClauses(s);
      buckets.garbageCollection(s);
      assert(s.getThreadState().toVivifyRefs.size() == 0);

      importCRefs(s);
      importUnits(s);
   }

   if (s.getThreadId() == 0)
      completeViviRefs.clear();
}
void ClauseDatabase::notifySolverEnd(CoreSolver & s)
{
   --numRunningThreads;
}

void ClauseDatabase::improveClauses(CoreSolver & s)
{
   assert(s.getLiteralSetting().decisionLevel() == 0);

   ClauseReducer cred(*this, buckets, s);

   if (sharingHeuristic.usePrivateVivification || sharingHeuristic.useDynamicVivi)
      cred.collectPrivateGoodClauses(s.getThreadState().toVivifyRefs);
   cred.vivifyClauses(RefVec<CRef>(s.getThreadState().toVivifyRefs));
   s.getThreadState().toVivifyRefs.clear();
   s.getThreadState().twoWatched.assertCorrectWatchers(s);
}

unsigned ClauseDatabase::checkCompleteVivification(CoreSolver & s, const bool enforce)
{
   unsigned res = 0;
   if ((completeViviRefs.size() > 0 && completeViviInProgress == 2) || enforce)
   {
      ClauseReducer cred(*this, buckets, s);
      RefVec<CRef> refs;
      bool wasLast = false;
      while ((refs = completeViviRefs.getElements()).size() > 0 && !wasLast)
      {
         assert(refs.size() < 100);
         res += cred.vivifyClauses(refs);
         wasLast = completeViviRefs.notifyProccessed(refs);
      }
      if (wasLast && !enforce)
      {
         unsigned expected = 2;
         bool succ = completeViviInProgress.compare_exchange_strong(expected, 0);
         assert(succ);
      }
   }
   return res;
}

void ClauseDatabase::setAbortSolving(const bool b)
{
   abortSolving = b;
}

bool ClauseDatabase::jobFinished() const
{
   return abortSolving || finishedSolver != nullptr;
}

const CoreSolver& ClauseDatabase::winner() const
{
   assert(jobFinished());
   return *(finishedSolver);
}

bool ClauseDatabase::foundSolution(CoreSolver & s, lbool result, const std::string & msg)
{
   const CoreSolver * tmp = nullptr;
   assert(s.getResult() == result);
   bool res = finishedSolver.compare_exchange_strong(tmp, &s);
   if (res && msg.size() > 0)
   {
      std::cout << "c " << msg << std::endl;
      std::cout.flush();
   }
   assert(finishedSolver != nullptr);
   return res;
}

lbool ClauseDatabase::getResult() const
{
   assert(jobFinished());
   return (abortSolving) ? l_Undef : winner().getResult();
}

}
