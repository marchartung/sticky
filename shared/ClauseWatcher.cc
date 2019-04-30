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

#include "shared/ClauseWatcher.h"
#include "shared/CoreSolver.h"
#include "core/SolverTypes.h"
#include "shared/ClauseDatabase.h"
#include "shared/PropagateResult.h"

#include <set>
#include <unordered_map>

namespace Sticky
{

BinaryWatcher::BinaryWatcher(const CRef & cr, const Lit & p)
      : blocker(p),
        cref(cr)
{
}
BinaryWatcher::BinaryWatcher()
      : blocker(lit_Undef),
        cref(CRef_Undef)
{
}

Watcher::Watcher(const CRef & cr, const Lit & p, const int & watchRef)
      : activity(0),
        _isProtected(false),
        _isImported(false),
        lbd(MaxWatcherLBD),
        cref(cr),
        blocker(p),
        blockerWatchRef(watchRef)
{
}

Watcher::Watcher(const CRef & cr, const Lit & p, const int & watchRef, const unsigned & l, const unsigned & activity)
      : activity(activity),
        _isProtected(false),
        _isImported(false),
        lbd(0),
        cref(cr),
        blocker(p),
        blockerWatchRef(watchRef)
{
   setLbd(l);
}

Watcher::Watcher(const Watcher & w)
      : activity(w.activity),
        _isProtected(w._isProtected),
        _isImported(w._isImported),
        lbd(w.lbd),
        cref(w.cref),
        blocker(w.blocker),
        blockerWatchRef(w.blockerWatchRef)
{
}

Watcher::Watcher(Watcher && w)
      : activity(w.activity),
        _isProtected(w._isProtected),
        _isImported(w._isImported),
        lbd(w.lbd),
        cref(w.cref),
        blocker(w.blocker),
        blockerWatchRef(w.blockerWatchRef)
{
}

TwoWatcherLists::TwoWatcherLists(ClauseBucketArray & cba, const unsigned & numLits)
      : cba(cba),
        watcher(2 * numLits)
{

}

bool TwoWatcherLists::isValidWatcher(const Watcher & w, const bool allowInvalidClause) const
{
   const Watcher & w2 = getOtherWatcher(w);
   const Watcher & w1 = getOtherWatcher(w2);
   bool res = w.getBlocker() == w1.getBlocker() && w.getCRef() == w1.getCRef() && w.isHeader() == w1.isHeader() && w.getLbd() == w1.getLbd() && w2.getCRef() == w1.getCRef();

   const BaseClause & c = cba.getClause(w.getCRef());
   assert(c.isPrivateClause() || c.shared().getNumRefs() > 0);
   if (!res)
      std::cout << "ohoh0";
   res &= w1.isHeader() != w2.isHeader();
   if (res)
   {
      res &= c.size() > 2;
      if (!res)
         std::cout << "ohoh1\n";
      res &= allowInvalidClause || (c[0] != lit_Undef);
      if (!res)
         std::cout << "oho1.5\n";
      res &= allowInvalidClause || c.contains(w.getBlocker());
      if (!res)
         std::cout << "ohoh2\n";
      res &= allowInvalidClause || c.contains(w2.getBlocker());
      if (!res)
         std::cout << "ohoh3\n";
      res &= allowInvalidClause || (c[0] != lit_Undef);
      if (!res)
         std::cout << "ohoh4\n";
   } else
      std::cout << "ohoh0.5\n";
   if (!res)
   {
      std::cout << "ohoh\n";
      std::cout << "unvalid watcher on " << w.getCRef() << " or " << w1.getCRef() << " or " << w2.getCRef() << "\n";
      std::cout.flush();
   }
   return res;
}

void TwoWatcherLists::assertCorrectWatchers(const CoreSolver & s) const
{
   for (int i = 0; i < size(); ++i)
   {
      const vec<Watcher> & ws = getWatcher(i);
      for (int j = 0; j < ws.size(); ++j)
      {
         assert(isValidWatcher(getWatcher(i, j)));
      }
   }
}

// Needed for WatcherListReference:

bool TwoWatcherLists::isMarkedAsRemoved(const Watcher & w) const
{
   assert(isValidWatcher(w));
   return (w.isHeader()) ? getOtherWatcher(w).isMarkedAsRemoved() : w.isMarkedAsRemoved();
}

void TwoWatcherLists::deleteSingleWatcher(const int listPos, const int watcherPos)
{
   ListType & ws = getWatcher(listPos);
   ws.unordered_remove(watcherPos);
   if (watcherPos < ws.size())
   {
      getOtherWatcher(ws[watcherPos]).setPos(watcherPos);  // it implicitly changed the watcher it is pointing to, check it
      //assert(isValidWatcher(ws[watcherPos]));
   }
}

void TwoWatcherLists::moveWatcher(const int listPos, const int wPos, const Lit & to)
{
   Watcher & w = getWatcher(listPos, wPos), &wo = getOtherWatcher(w);
   vec<Watcher> & wsNew = getWatcher(~to);
   assert(cba.getClause(w.getCRef()).contains(to));
   assert(isValidWatcher(w));
   int newPos = wsNew.size();
   wsNew.push(w);
   wo.setOther(to, newPos);  // TODO cirtical prefetch?
   assert(isValidWatcher(wo));
   assert(isValidWatcher(wsNew.last()));
}

void TwoWatcherLists::detach(CoreSolver & s, const VarSet & vs)
{
   detach(s, vs.getListPos(), vs.getWatcherPos());
}

bool TwoWatcherLists::containsCRef(const CRef cref) const
{
   bool res = false;
   for (int i = 0; i < watcher.size(); ++i)
   {
      for (int j = 0; j < watcher[i].size(); ++j)
         if (watcher[i][j].getCRef() == cref)
         {
            res = true;
            break;
         }
   }
   return res;
}

void TwoWatcherLists::detach(CoreSolver & s, const int listPos, const int wPos)
{
   --s.getStatistic().nTwoWatchedClauses;
   const Watcher & w = getWatcher(listPos, wPos);
   CRef cref = w.getCRef();
   assert(&w == &(getOtherWatcher(getOtherWatcher(w))));
   deleteSingleWatcher((~w.getBlocker()).x, w.getBlockerRef());
   deleteSingleWatcher(listPos, wPos);
   assert(!containsCRef(cref));
}
void TwoWatcherLists::changeCRef(CoreSolver & s, const VarSet & vs, const CRef & cref)
{
   changeCRef(s, vs.getListPos(), vs.getWatcherPos(), cref);
}
void TwoWatcherLists::changeCRef(CoreSolver & s, const int listPos, const int wPos, const CRef & cref)
{
   Watcher & w = getWatcher(listPos, wPos), &wo = getOtherWatcher(w);
   const CRef old = w.getCRef();
   assert(old != cref);
   assert(isValidWatcher(w, true));
   const BaseClause & newClause = cba.getClause(cref);
   assert(newClause.size() == cba.getClause(w.getCRef()).size());
   assert(newClause.contains(wo.getBlocker()) && newClause.contains(wo.getBlocker()));
   w.setCRef(cref);
   wo.setCRef(cref);
   assert(isValidWatcher(w));
   assert(!this->containsCRef(old));
}

VarSet TwoWatcherLists::replace(CoreSolver & s, const int listPos, const int wPos, const CRef & cref)
{
   Watcher & w = getWatcher(listPos, wPos), &wo = getOtherWatcher(w), &wooo = getOtherWatcher(getOtherWatcher(wo));
   assert(isValidWatcher(w));
   assert(isValidWatcher(wo));
   assert(&w == &getOtherWatcher(wo));
   assert(&wo == &wooo);
   const BaseClause & newClause = cba.getClause(cref);
   assert(isValidWatcher(w));
   if (newClause.contains(w.getBlocker()) && newClause.contains(wo.getBlocker()))
   {
      w.setCRef(cref);
      wo.setCRef(cref);
   } else
   {
      Watcher tmp = (w.isHeader()) ? w : wo;
      this->detach(s, listPos, wPos);
      auto vs = this->attach(cref, s, tmp.getLbd(), tmp.getActivity());
      assert(isValidWatcher(getWatcher(vs.getVarSet())));
   }
   return VarSet(WatcherType::TWO, listPos, wPos);
}

PropagateResult TwoWatcherLists::attachLowestLitNumbers(const CRef & cref, CoreSolver & s, const unsigned & lbd, const unsigned & activity)
{
   PropagateResult res;
   const BaseClause & c = cba.getClause(cref);
   assert(c.size() >= 2);
   Lit lHeader = c[1], lNon = c[0];
   for (int i = 1; i < c.size(); ++i)
   {
      assert(s.getLiteralSetting().value(c[0]) == l_Undef);
      if (c[i].x < lNon.x)
      {
         lHeader = lNon;
         lNon = c[i];
      } else if (c[i].x < lHeader.x)
         lHeader = c[i];
   }
   res.getVarSet() = plainAttach(s, cref, lHeader, lNon, lbd, activity);
   return res;

}

VarSet TwoWatcherLists::findWatcher(const CRef cref) const
{
   const BaseClause & c = cba.getClause(cref);
   assert(c[0] != lit_Undef);
   if (c.size() > 2)
      for (int i = 0; i < c.size(); ++i)
      {
         const vec<Watcher> & ws = getWatcher(~c[i]);
         for (int j = 0; j < ws.size(); ++j)
            if (ws[j].getCRef() == cref)
               return VarSet(ws[j], (~c[i]).x, j);
      }
   return VarSet();
}

PropagateResult TwoWatcherLists::attach(const CRef & cref, CoreSolver & s, const unsigned & lbd, const unsigned & activity)
{
   const LiteralSetting & litSet = s.getLiteralSetting();
   const BaseClause & c = cba.getClause(cref);
   // try to attach clauses, different scenarios can occur:
   // 1. Trivial: Clause can be attach due to unset literals or to true literals: return is PropagateResult() i.e. no conflict
   // 2. Clause evaluates to false: return PropagateResult to conflict with the latest set (highest level) literal
   assert(c.size() >= 2);
   // for the two literals: find lowest level true-lit, undef or highest level false-lit
   Lit w1 = c[0], w2 = c[1];
   PropagateResult res;

   if (litSet.value(w1) == l_False || litSet.value(w2) == l_False)  // ensure the clause is really two watched:
   {
      auto better = [&](const Lit & l1, const Lit & l2)
      {
         bool res = false;
         lbool val = litSet.value(l1);

         if(val == l_True)
         res = litSet.value(l2) == l_True && litSet.level(var(l1)) > litSet.level(var(l2));
         else if(val == l_False)
         res = litSet.value(l2) != l_False || litSet.level(var(l1)) < litSet.level(var(l2));
         else
         {
            assert(val == l_Undef);
            res = litSet.value(l2) == l_True;
         }
         return res;
      };

      for (int i = 1; i < c.size(); ++i)
      {
         if (better(w1, c[i]))
         {
            w2 = w1;
            w1 = c[i];
         } else if (better(w2, c[i]))
            w2 = c[i];

         if (litSet.value(w2) != l_False)
            break;
      }
   }
   assert(w1 != w2);
   res.getVarSet() = plainAttach(s, cref, w1, w2, lbd, activity);
   // check for case 2.:
   if (litSet.value(w1) == l_False)
   {
      assert(litSet.value(w2) == l_False);
      assert(litSet.level(var(w1)) >= litSet.level(var(w2)));
      for (int i = 0; i < c.size(); ++i)
         assert(litSet.level(var(w1)) >= litSet.level(var(c[i])));
      const Watcher & w = getWatcher(res.getVarSet());
      assert(w.getBlocker() == w2);
      assert(getOtherWatcher(w).getBlocker() == w1);
      res.setConflict(true);
   }
   assert(isValidWatcher(getWatcher(res.getVarSet())));
   return res;
}

TwoWatcherLists::WType & TwoWatcherLists::getWatcher(const VarSet & vs)
{
   assert(watcher.size() > vs.getListPos());
   const vec<Watcher> & ws = watcher[vs.getListPos()];
   assert(ws.size() > vs.getWatcherPos());
   //assert(cba.getClause(watcher[vs.getListPos()][vs.getWatcherPos()].getCRef()).contains(vs.getWatchedLit()));
   return getWatcher(vs.getListPos(), vs.getWatcherPos());
}

TwoWatcherLists::WType & TwoWatcherLists::getStateWatcher(const VarSet & vs)
{
   assert(watcher.size() > vs.getListPos());
   const vec<Watcher> & ws = watcher[vs.getListPos()];
   assert(ws.size() > vs.getWatcherPos());
   assert(cba.getClause(watcher[vs.getListPos()][vs.getWatcherPos()].getCRef()).contains(vs.getWatchedLit()));
   WType & w = getWatcher(vs.getListPos(), vs.getWatcherPos());
   return (w.isHeader()) ? getOtherWatcher(w) : w;
}

const TwoWatcherLists::WType & TwoWatcherLists::getWatcher(const VarSet & vs) const
{
   assert(watcher.size() > vs.getListPos());
   assert(watcher[vs.getListPos()].size() > vs.getWatcherPos());
   assert(cba.getClause(watcher[vs.getListPos()][vs.getWatcherPos()].getCRef()).contains(vs.getWatchedLit()));
   return getWatcher(vs.getListPos(), vs.getWatcherPos());
}

const CRef & TwoWatcherLists::getCRef(const VarSet & vs) const
{
   return getWatcher(vs).getCRef();
}

TwoWatcherLists::WType & TwoWatcherLists::getOtherWatcher(const Watcher & w)
{
   return getWatcher((~w.getBlocker()).x, w.getBlockerRef());
}
const TwoWatcherLists::WType & TwoWatcherLists::getOtherWatcher(const Watcher & w) const
{
   return getWatcher((~w.getBlocker()).x, w.getBlockerRef());
}
TwoWatcherLists::WType & TwoWatcherLists::getOtherWatcher(const VarSet & vs)
{
   assert(cba.getClause(getWatcher(vs).getCRef()).contains(vs.getWatchedLit()));
   return getOtherWatcher(getWatcher(vs));
}
const TwoWatcherLists::WType & TwoWatcherLists::getOtherWatcher(const VarSet & vs) const
{
   assert(cba.getClause(getWatcher(vs).getCRef()).contains(vs.getWatchedLit()));
   return getOtherWatcher(getWatcher(vs));
}

TwoWatcherLists::WType & TwoWatcherLists::getOtherWatcher(const int lPos, const int wPos)
{
   return getOtherWatcher(getWatcher(lPos, wPos));
}
const TwoWatcherLists::WType & TwoWatcherLists::getOtherWatcher(const int lPos, const int wPos) const
{
   return getOtherWatcher(getWatcher(lPos, wPos));
}

VarSet TwoWatcherLists::plainAttach(CoreSolver & s, const CRef & cref, const Lit & l1, const Lit & l2, const unsigned & lbd, const unsigned & activity)
{
   const BaseClause & c = cba.getClause(cref);
   assert(c.contains(l1));
   assert(c.contains(l2));
   assert(c.hasUniqueLits());
   assert(l1 != l2);
   ++s.getStatistic().nTwoWatchedClauses;

   auto & ws1 = getWatcher(~l1);
   auto & ws2 = getWatcher(~l2);
   unsigned pos1 = ws1.size();
   unsigned pos2 = ws2.size();
   ws1.push(Watcher(cref, l2, pos2, lbd, activity));  // header watcher
   ws2.push(Watcher(cref, l1, pos1));  // non header, it is determined by the missing passed lbd
   assert(isValidWatcher(ws1.last()));
   VarSet res(ws1[pos1], ~l1, pos1);
   const Watcher & w = (getWatcher(res).isHeader()) ? getWatcher(res) : getOtherWatcher(getWatcher(res));
   assert(w.getLbd() == lbd || w.getLbd() == MAX_LBD_VALUE);
   assert(getWatcher(res).getCRef() == cref && getOtherWatcher(getWatcher(res)).getCRef() == cref);
   return res;
}

VarSet TwoWatcherLists::attachOnFirst(const CRef & cref, CoreSolver & s, const unsigned & lbd)
{
   const BaseClause & c = cba.getClause(cref);
   return plainAttach(s, cref, c[0], c[1], lbd);
}

VarSet TwoWatcherLists::attachOneWatched(const OneWatcher & ow, CoreSolver & s, const Lit & p, const unsigned & lbd)
{
   const BaseClause & c = cba.getClause(ow.getCRef());
   const LiteralSetting & litSet = s.getLiteralSetting();
   assert(c.contains(p));
   int maxlevel = -1;
   int index1 = -1, index2 = -1;
   for (int k = 0; k < c.size(); k++)
   {
      assert(litSet.value(c[k]) == l_False);
      assert(litSet.level(var(c[k])) <= litSet.level(var(p)));
      if (c[k] == p)
      {
         index1 = k;
      } else if (litSet.level(var(c[k])) > maxlevel)
      {
         index2 = k;
         maxlevel = litSet.level(var(c[k]));
      }
   }
   assert(index1 != -1);
   return plainAttach(s, ow.getCRef(), p, c[index2], lbd);
}

OneWatcherLists::OneWatcherLists(ClauseBucketArray & cba, const unsigned & numLits)
      : cba(cba),
        watcher(2 * numLits)
{
}
void TwoWatcherLists::removeMarkedClauses(CoreSolver & s)
{
   int i, j, k;
   for (i = 0; i < watcher.size(); ++i)
   {
      ListType & ws = watcher[i];
      for (j = 0, k = 0; j < ws.size(); ++j)
      {
         const Watcher & w = (ws[j].isHeader()) ? getOtherWatcher(ws[j]) : ws[j];
         assert(isValidWatcher(ws[j]));
         assert(isValidWatcher(w));
         const BaseClause & c = cba.getClause(w.getCRef());
         if (w.isMarkedAsRemoved() || cba.shouldBeRemoved(w.getCRef()) || (c.isPrivateClause() && c.isReplaced()))
         {
            const Header & header = cba.getClause(w.getCRef()).getHeader();
            if (header.isPrivateClause())
            {
               assert(s.getStatistic().nPrivateCl > 0);
               --s.getStatistic().nPrivateCl;
            } else if (header.isSharedClause())
               --s.getStatistic().nSharedCl;
            cba.removeClause(s, w.getCRef());
            --s.getStatistic().nTwoWatchedClauses;
            this->deleteSingleWatcher((~ws[j].getBlocker()).x, ws[j].getBlockerRef());
         } else
         {
            ClauseUpdate cu = cba.getClauseUpdate(s, w.getCRef());
            if (cu.isReallocation())
               changeCRef(s, i, j, cu.cref);
            else if (cu.isReplacement())
            {
               PropagateResult pr;
               if (cu.c->size() == 2)
               {
                  pr = s.getThreadState().binWatched.attach(cu.cref, s);
               } else
                  pr = this->attach(cu.cref, s, (c.isPermanentClause()) ? c.getLbd() : ((w.isHeader()) ? w.getLbd() : getOtherWatcher(w).getLbd()));
               --s.getStatistic().nTwoWatchedClauses;
               this->deleteSingleWatcher((~ws[j].getBlocker()).x, ws[j].getBlockerRef());
               if (pr.isConflict() && s.getLiteralSetting().decisionLevel() == 0)
               {
                  s.setResult(l_False);
               }

               continue;
            }
            getOtherWatcher(ws[j]).setPos(k);
            Watcher & newW = ws[k++];
            newW = ws[j];
            assert(isValidWatcher(ws[k - 1]));
            if (newW.isHeader())
               newW.decreaseActivity();
            else
               ws[k - 1].setProtected(false);
         }
      }
      ws.resize_(k);
   }
   assertCorrectWatchers(s);
   assert(s.getThreadState().toVivifyRefs.size() == 0);
}

void TwoWatcherLists::removeMarkedClauses(CoreSolver & s, vec<CRef> & permRefs)
{
   assert(s.getThreadState().toVivifyRefs.size() == 0);
   int i, j, k;
   for (i = 0; i < watcher.size(); ++i)
   {
      ListType & ws = watcher[i];
      for (j = 0, k = 0; j < ws.size(); ++j)
      {
         const Watcher & w = (ws[j].isHeader()) ? getOtherWatcher(ws[j]) : ws[j];
         assert(isValidWatcher(ws[j]));
         assert(isValidWatcher(w));
         const BaseClause & c = cba.getClause(w.getCRef());
         if (w.isMarkedAsRemoved() || cba.shouldBeRemoved(w.getCRef()) || (c.isPrivateClause() && c.isReplaced()))
         {
            const Header & header = cba.getClause(w.getCRef()).getHeader();
            if (header.isPrivateClause())
            {
               assert(s.getStatistic().nPrivateCl > 0);
               --s.getStatistic().nPrivateCl;
            } else if (header.isSharedClause())
               --s.getStatistic().nSharedCl;
            cba.removeClause(s, w.getCRef());
            --s.getStatistic().nTwoWatchedClauses;
            this->deleteSingleWatcher((~ws[j].getBlocker()).x, ws[j].getBlockerRef());
         } else
         {
            ClauseUpdate cu = cba.getClauseUpdate(s, w.getCRef());
            if (cu.isReallocation())
               changeCRef(s, i, j, cu.cref);
            else if (cu.isReplacement())
            {
               if (cu.c->size() == 2)
               {
                  s.getThreadState().binWatched.attach(cu.cref, s);
               } else
                  this->attachOnFirst(cu.cref, s, (c.isPermanentClause()) ? c.getLbd() : ((w.isHeader()) ? w.getLbd() : getOtherWatcher(w).getLbd()));
               --s.getStatistic().nTwoWatchedClauses;
               this->deleteSingleWatcher((~ws[j].getBlocker()).x, ws[j].getBlockerRef());
               continue;
            }
            getOtherWatcher(ws[j]).setPos(k);
            ws[k++] = ws[j];
            assert(isValidWatcher(ws[k - 1]));
            if (!ws[k - 1].isHeader())
               ws[k - 1].setProtected(false);
            else
               getOtherWatcher(ws[k - 1]).setProtected(false);
            if (cba.getClause(ws[j].getCRef()).isPermanentClause())
               permRefs.push(ws[j].getCRef());
         }
      }
      ws.resize_(k);
   }
   assertCorrectWatchers(s);
}
vec<CRef> TwoWatcherLists::getAllCRefs() const
{
   int i, j;
   vec<CRef> res;
   for (i = 0; i < watcher.size(); ++i)
   {
      const ListType & ws = watcher[i];
      for (j = 0; j < ws.size(); ++j)
      {
         const Watcher & w = ws[j];
         if (w.isHeader() && !cba.shouldBeRemoved(w.getCRef()))
            res.push(w.getCRef());
      }
   }
   return res;
}

void OneWatcherLists::removeMarkedClauses(CoreSolver & s)
{
   int j, k;
   for (int i = 0; i < size(); ++i)
   {
      ListType & curList = watcher[i];
      for (j = 0, k = 0; j < curList.size(); ++j)
      {
         const WType & w = curList[j];
         if (isMarkedAsRemoved(w))
         {
            if (cba.getClause(w.getCRef()).isPrivateClause())
            {
               assert(s.getStatistic().nPrivateCl > 0);
               --s.getStatistic().nPrivateCl;
            } else if (cba.getClause(w.getCRef()).isSharedClause())
               --s.getStatistic().nSharedCl;
            cba.removeClause(s, w.getCRef());
         } else
         {
            ClauseUpdate cu = cba.getClauseUpdate(s, w.getCRef());
            if (cu.isReallocation())
               replace(s, i, j, cu.cref);
            else if (cu.isReplacement())
            {
               if (!attach(cu.cref, *cu.c, s))
                  s.setResult(l_False);
               continue;
            }
            curList[k++] = curList[j];
         }
      }
      s.getStatistic().nOneWatchedClauses -= j-k;
      curList.shrink(j-k);
   }
}
VarSet OneWatcherLists::replace(CoreSolver & s, const int listPos, const int wPos, const CRef & cref)
{
   return replace(s, VarSet(getWatcher(listPos, wPos), listPos, wPos), cref);
}

VarSet OneWatcherLists::replace(CoreSolver & s, const VarSet & vs, const CRef & cref)
{
   OneWatcher & w = getWatcher(vs);
   const BaseClause & newClause = cba.getClause(cref);
   if (newClause.contains(w.getBlocker()) && newClause.contains(w.getBlocker()))
   {
      w.setCRef(cref);
      Lit l;
      l.x = vs.getListPos();
      assert(newClause.contains(~l));
   } else
   {
      this->detach(s, vs);
      this->attach(cref, newClause, s);
   }
   return vs;
}

bool OneWatcherLists::attach(const CRef & cref, const BaseClause & c, CoreSolver & s)
{
   assert(c.size() > 2);
   const LiteralSetting & litSet = s.getLiteralSetting();
   // fast attach. So find first suitable lit, i.e. undef or l_True and attach it
   int i = 0;
   for (; i < c.size(); ++i)
      if (litSet.value(c[i]) != l_False)
      {
         getWatcher(~c[i]).push(OneWatcher(cref, c[((i + 1 < c.size()) ? i + 1 : 0)]));
         ++s.getStatistic().nOneWatchedClauses;
         break;
      }
   return i < c.size();
}

bool OneWatcherLists::isValidWatcher(const OneWatcher & w) const
{
   return (cba.getClause(w.getCRef()).contains(w.getBlocker()));
}
void OneWatcherLists::assertCorrectWatchers() const
{
   for (int i = 0; i < size(); ++i)
   {
      const vec<OneWatcher> & ws = getWatcher(i);
      for (int j = 0; j < ws.size(); ++j)
      {
         assert(isValidWatcher(getWatcher(i, j)));
         Lit l;
         l.x = i;
         assert(cba.getClause(getWatcher(i, j).getCRef()).contains(~l));
      }
   }
}

void OneWatcherLists::detach(CoreSolver& s, const VarSet & vs)
{
   --s.getStatistic().nOneWatchedClauses;
   watcher[vs.getListPos()].unordered_remove(vs.getWatcherPos());
}

BinaryWatcherLists::BinaryWatcherLists(ClauseBucketArray & cba, const unsigned & numLits)
      : cba(cba),
        watcher(2 * numLits)
{
}

bool BinaryWatcherLists::isConsistent() const
{
   bool res = true;
   std::unordered_map<CRef, unsigned> counter;
   for (int i = 0; i < watcher.size(); ++i)
   {
      const vec<BinaryWatcher> & ws = watcher[i];
      for (int j = 0; j < ws.size(); ++j)
         ++counter[ws[j].getCRef()];
   }

   for (const auto & p : counter)
   {
      if (p.second != 2)
      {
         res = false;
         break;
      }
   }
   return res;
}

PropagateResult BinaryWatcherLists::attach(const CRef & cref, CoreSolver & s)
{
   ++s.getStatistic().nTwoWatchedClauses;
   const BaseClause & c = cba.getClause(cref);
   assert(c.size() == 2);
   const LiteralSetting & litSet = s.getLiteralSetting();
   PropagateResult res;
   auto & ws1 = getWatcher(~c[0]), &ws2 = getWatcher(~c[1]);
   ws1.push(BinaryWatcher(cref, c[1]));
   ws2.push(BinaryWatcher(cref, c[0]));
   res.getVarSet().set(BinaryWatcher(), (~c[0]).x, ws1.size() - 1);
   assert(c.contains(ws1.last().getBlocker()));
   assert(c.contains(ws2.last().getBlocker()));
   if (litSet.value(c[0]) == l_False && litSet.value(c[1]) == l_False)  // conflict!
   {
      res.setConflict(true);
      unsigned i = (litSet.level(var(c[0])) > litSet.level(var(c[1]))) ? 0 : 1;
      res.getVarSet().set(BinaryWatcher(), (~c[i]).x, watcher[(~c[i]).x].size() - 1);
   }
   return res;
}
} /* namespace Glucose */
