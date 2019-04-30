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
#include "shared/ClauseWatcher.h"
#include "shared/ClauseDatabase.h"
#include "shared/CoreSolver.h"
#include "shared/PropagateResult.h"
#include "shared/SharedTypes.h"
#include "shared/Heuristic.h"
#include "shared/Statistic.h"
#include "glucose/simp/SimpSolver.h"

#include <math.h>
#include <limits>
#include <algorithm>

using namespace Sticky;



double luby(double y, int x){

    int size, seq;
    for (size = 1, seq = 0; size < x+1; seq++, size = 2*size+1);
    while (size-1 != x){
        size = (size-1)>>1;
        seq--;
        x = x % size;
    }
    return pow(y, seq);
}

//=================================================================================================
// Constructor/Destructor:

CoreSolver::CoreSolver(ClauseDatabase & scDb, const SimpSolver & s, const unsigned & threadId, const vec<CRef> & initCRefs)
      : result(l_Undef),
        isMinimizer(false),
        lbdRoundCounter(0),
        reduceOnSize(false),
        lubyConflictlimit(luby(2, 0)*100),
        reduceOnSizeSize(12),  // Constant to use on size reductions
        permDiff(s.nVars(), 0),
        lastDecisionLevel(),
        seen(s.nVars(), 0),
        analyze_stack(),
        analyze_toclear(),
        trailQueue(),
        lbdQueue(),
        heuristic(),
        statistic(),
        cDb(scDb),
        lState(s, ((double) threadId + 1.0) * heuristic.random_seed),
        dbState(threadId, s.nVars(), cDb.getBuckets()),
        model(),
        conflict()
{
   assert(s.decisionLevel() == 0);

   for (Var i = 0; i < s.nVars(); ++i)
   {
      if (s.assigns[i] != l_Undef && s.vardata[i].level == 0)
      {
         uncheckedEnqueue(mkLit(i, s.assigns[i]  == l_False), VarSet(),true);
         assert(lState.value(mkLit(Var(i), s.assigns[i]  == l_False)) == l_True);
      }
      lState.state[i].polarity = lState.rg.randBool();
      lState.activity[i] = lState.rg.rand01();
      lState.order_heap.insert(i);
   }
   // check:
   for (int i = 0; i < s.vardata.size(); ++i)
      assert(s.vardata[i].level == 0);  // we cannot allow references to other clauses than the ones in the db
   lbdQueue.initSize(heuristic.sizeLBDQueue);
   trailQueue.initSize(heuristic.sizeTrailQueue);
   s.model.memCopyTo(model);
   s.conflict.memCopyTo(conflict);
   //s.decision.memCopyTo(decision);
   s.lbdQueue.copyTo(lbdQueue);
   s.trailQueue.copyTo(trailQueue);

   dbState.nConflictsBeforeReduce = heuristic.firstReduceDb;

   const auto & buckets = scDb.getBuckets();
   for (int i = 0; i < initCRefs.size(); ++i)
   {
      const CRef & cr = initCRefs[i];
      const BaseClause & c = buckets.getClause(cr);
      bool abort;
      if (c.size() > 2)
         abort = dbState.twoWatched.attach(cr, *this, 0).isConflict();
      else
      {
         assert(c.size() == 2);
         abort = dbState.binWatched.attach(cr, *this).isConflict();
      }
      if (abort)
      {
         result = l_False;
         setResult(l_False, "Initial clause attach solution found");
         break;
      }
   }
}

CoreSolver::~CoreSolver()
{
}

/************************************************************
 * Compute LBD functions
 *************************************************************/

unsigned int CoreSolver::computeLBD(const vec<Lit> & lits)
{
   int nblevels = 0;
   lbdRoundCounter++;

   for (int i = 0; i < lits.size(); i++)
   {
      int l = lState.level(var(lits[i]));
      if (permDiff[l] != lbdRoundCounter)
      {
         permDiff[l] = lbdRoundCounter;
         nblevels++;
      }
   }

   if (!reduceOnSize)
      return nblevels;
   if (lits.size() < reduceOnSizeSize)
      return lits.size();  // See the XMinisat paper
   return lits.size() + nblevels;
}

unsigned int CoreSolver::computeLBD(const BaseClause &c)
{
   int nblevels = 0;
   lbdRoundCounter++;

   for (int i = 0; i < c.size(); i++)
   {
      int l = lState.level(var(c[i]));
      if (permDiff[l] != lbdRoundCounter)
      {
         permDiff[l] = lbdRoundCounter;
         nblevels++;
      }
   }

   if (!reduceOnSize)
      return nblevels;
   if (c.size() < reduceOnSizeSize)
      return c.size();  // See the XMinisat paper
   return c.size() + nblevels;

}

void CoreSolver::setResult(const lbool res, const std::string & msg)
{
   this->result = res;
   cDb.foundSolution(*this, res, msg);
}

/******************************************************************
 * Minimisation with binary reolution
 ******************************************************************/
void CoreSolver::minimisationWithBinaryResolution(vec<Lit> &out_learnt)
{

// Find the LBD measure
   unsigned int lbd = computeLBD(out_learnt);
   Lit p = ~out_learnt[0];

   if (lbd <= heuristic.lbLBDMinimizingClause)
   {
      lbdRoundCounter++;

      for (int i = 1; i < out_learnt.size(); i++)
      {
         permDiff[var(out_learnt[i])] = lbdRoundCounter;
      }
      const vec<BinaryWatcher> & wbin = dbState.binWatched.getWatcher(p);
      int nb = 0;
      for (int i = 0; i < wbin.size(); ++i)
      {
         Lit imp = wbin[i].getBlocker();
         if (permDiff[var(imp)] == lbdRoundCounter && lState.value(imp) == l_True)
         {
            nb++;
            permDiff[var(imp)] = lbdRoundCounter - 1;
         }
      }
      int l = out_learnt.size() - 1;
      if (nb > 0)
      {
         for (int i = 1; i < out_learnt.size() - nb; i++)
         {
            if (permDiff[var(out_learnt[i])] != lbdRoundCounter)
            {
               std::swap(out_learnt[l], out_learnt[i]);
               l--;
               i--;
            }
         }
         out_learnt.shrink(nb);

      }
   }
}

// Revert to the state at given level (keeping all assignment at 'level' but not beyond).
//
void CoreSolver::cancelUntil(const int level)
{
   if (lState.decisionLevel() > level)
   {
      for (int c = lState.trail.size() - 1; c >= lState.trail_lim[level]; c--)
      {
         Var x = var(lState.trail[c]);
         lState.state[x].assign = l_Undef;
         if (heuristic.phase_saving > 1 || ((heuristic.phase_saving == 1) && c > lState.trail_lim.last()))
         {
            lState.state[x].polarity = sign(lState.trail[c]);
         }
         lState.insertVarOrder(x);
      }
      lState.qhead = lState.trail_lim[level];
      lState.trail.shrink(lState.trail.size() - lState.trail_lim[level]);
      lState.trail_lim.shrink(lState.trail_lim.size() - level);
      //std::cout << "backtrack level " << level << "\n";
   }
}

//=================================================================================================
// Major methods:

inline Lit CoreSolver::pickBranchLit()
{
   Var next = var_Undef;
//static int orderMax = 0;
//if (orderMax < order_heap.size())
//   orderMax = order_heap.size();

// Random decision:
   if (drand(heuristic.random_seed) < heuristic.random_var_freq && !lState.order_heap.empty())
   {
      next = lState.order_heap[irand(heuristic.random_seed, lState.order_heap.size())];
   }

// Activity based decision:
   while (next == var_Undef || lState.value(next) != l_Undef)
      if (lState.order_heap.empty())
      {
         next = var_Undef;
         break;
      } else
      {
         next = lState.order_heap.removeMin();
      }
   return next == var_Undef ? lit_Undef : mkLit(next, heuristic.rnd_pol ? drand(heuristic.random_seed) < 0.5 : lState.state[next].polarity);
}

const BaseClause & CoreSolver::getClause(const VarSet & vs) const
{
   return cDb(*this, vs);
}

const BaseClause & CoreSolver::getClause(const Var v) const
{
   assert(lState.reason(v).isPropagated());
   return cDb(*this, lState.reason(v));
}

/*_________________________________________________________________________________________________
 * |
 * |  analyzeFinal : (p : Lit)  ->  [void]
 * |
 * |  Description:
 * |    Specialized analysis procedure to express the final conflict in terms of assumptions.
 * |    Calculates the (possibly empty) set of assumptions that led to the assignment of 'p', and
 * |    stores the result in 'out_conflict'.
 * |________________________________________________________________________________________________@*/
void CoreSolver::findDecisionClauseForPropagation(Lit p, vec<Lit>& out_conflict)
{
   assert(out_conflict.size() == 0 && lState.decisionLevel() > 0);
   assert(lState.reason(var(p)).isPropagated());
   assert(cDb.getPropagatedLit(*this, lState.reason(var(p))) == p);
   out_conflict.push(p);
   seen[var(p)] = 1;
   for (int i = lState.trail.size() - 1; i >= lState.trail_lim[0]; i--)
   {
      Var x = var(lState.trail[i]);
      if (seen[x])
      {
         if (!lState.reason(x).isPropagated())
         {
            assert(lState.level(x) > 0);
            out_conflict.push(~lState.trail[i]);
         } else
         {
            const BaseClause& c = getClause(lState.reason(x));
            for (int j = 0; j < c.size(); j++)
               if (var(p) != var(c[j]) && lState.level(var(c[j])) > 0)
                  seen[var(c[j])] = 1;
         }
         seen[x] = 0;
      }
   }
   seen[var(p)] = 0;
   for (int i = 0; i < seen.size(); ++i)
      assert(seen[i] == 0);
}

bool CoreSolver::findDecisionClauseForConflict(const VarSet & confl, vec<Lit>& out_conflict, const CRef excludeRef)
{
   bool res = true;
   const BaseClause & conflC = getClause(confl);
   for (int i = 0; i < conflC.size(); ++i)
      seen[var(conflC[i])] = 1;

   for (int i = lState.trail.size() - 1; i >= lState.trail_lim[0]; i--)
   {
      Var x = var(lState.trail[i]);
      if (seen[x])
      {
         if (!lState.reason(x).isPropagated())
         {
            assert(lState.level(x) > 0);
            out_conflict.push(~lState.trail[i]);
         } else
         {
            if (cDb.getCRef(*this, lState.reason(x)) == excludeRef)
               res = false;  // no loop break here, seen must be reset
            const BaseClause& c = getClause(lState.reason(x));
            for (int j = 0; j < c.size(); j++)
            {
               assert(lState.value(c[j]) != l_Undef);
               if (lState.level(var(c[j])) > 0)
                  seen[var(c[j])] = 1;
            }
         }
         seen[x] = 0;
      }
   }
   for (int i = 0; i < conflC.size(); ++i)
      seen[var(conflC[i])] = 0;
   for (int i = 0; i < seen.size(); ++i)
      assert(seen[i] == 0);
   return res;
}

/*_________________________________________________________________________________________________
 |
 |  analyze : (confl : Clause*) (out_learnt : vec<Lit>&) (out_btlevel : int&)  ->  [void]
 |
 |  Description:
 |    Analyze conflict and produce a reason clause.
 |
 |    Pre-conditions:
 |      * 'out_learnt' is assumed to be cleared.
 |      * Current decision level must be greater than root level.
 |
 |    Post-conditions:
 |      * 'out_learnt[0]' is the asserting literal at level 'out_btlevel'.
 |      * If out_learnt.size() > 1 then 'out_learnt[1]' has the greatest decision level of the
 |        rest of literals. There may be others from the same level though.
 |
 |________________________________________________________________________________________________@*/
void CoreSolver::analyze(const VarSet & conflictProp, vec<Lit>& out_learnt, int& out_btlevel, unsigned int &lbd)
{
   int pathC = 0, index = lState.trail.size() - 1;
   Lit p = lit_Undef;
   VarSet prop = conflictProp;
   out_learnt.push();  // (leave room for the asserting literal)
// Generate conflict clause:
//
   do
   {
      assert(prop.getWatcherType() != WatcherType::UNIT);  // (otherwise should be UIP)
      cDb.notifyClauseUsedInConflict(*this, prop);
      const BaseClause & c = getClause(prop);
      for (int j = 0; j < c.size(); ++j)
      {
         const Lit & q = c[j];
         assert(lState.value(q) != l_Undef);
         if (q != p)
         {
            const Var varQ = var(q);
            if (!seen[varQ])
            {
               if (lState.level(varQ) == 0)
               {
               } else
               {  // Here, the old case
                  lState.varBumpActivity(varQ, heuristic);
                  seen[varQ] = 1;
                  if (lState.level(varQ) >= lState.decisionLevel())
                  {
                     pathC++;
                     // UPDATEVARACTIVITY trick (see competition'09 companion paper)
                     if (lState.reason(varQ).isPropagated() && !getClause(varQ).isPermanentClause())
                        lastDecisionLevel.push(q);
                  } else
                     out_learnt.push(q);
               }
            }
         }
      }

// Select next clause to look at:
      while (!seen[var(lState.trail[index--])])
         assert(index >= -1);
      p = lState.trail[index + 1];
      seen[var(p)] = 0;
      prop = lState.reason(var(p));
      pathC--;

   } while (pathC > 0);
   out_learnt[0] = ~p;
   for (int i = 1; i < out_learnt.size(); ++i)
   {
      assert(lState.level(out_learnt[i]) <= lState.level(p));
      assert(out_learnt[i] != ~p);
   }

// Simplify conflict clause:
//
   int i, j;
   out_learnt.copyTo(analyze_toclear);

   if (heuristic.ccmin_mode == 2)
   {
      uint32_t abstract_level = 0;
      for (i = 1; i < out_learnt.size(); i++)
         abstract_level |= lState.abstractLevel(var(out_learnt[i]));  // (maintain an abstraction of levels involved in conflict)

      for (i = j = 1; i < out_learnt.size(); i++)
         if (!lState.reason(var(out_learnt[i])).isPropagated() || !litRedundant(out_learnt[i], abstract_level))
            out_learnt[j++] = out_learnt[i];

   } else if (heuristic.ccmin_mode == 1)
   {
      for (i = j = 1; i < out_learnt.size(); i++)
      {
         Var x = var(out_learnt[i]);

         if (!lState.reason(x).isPropagated())
            out_learnt[j++] = out_learnt[i];
         else
         {
            const BaseClause& c = cDb(*this, lState.reason(var(out_learnt[i])));
            for (int k = ((c.size() == 2) ? 0 : 1); k < c.size(); k++)
               if (!seen[var(c[k])] && lState.level(var(c[k])) > 0)
               {
                  out_learnt[j++] = out_learnt[i];
                  break;
               }
         }
      }
   } else
      i = j = out_learnt.size();

   out_learnt.shrink(i - j);

   /* ***************************************
    Minimisation with binary clauses of the asserting clause
    First of all : we look for small clauses
    Then, we reduce clauses with small LBD.
    Otherwise, this can be useless
    */
   if (out_learnt.size() <= heuristic.lbSizeMinimizingClause)
      minimisationWithBinaryResolution(out_learnt);
// Find correct backtrack level:
//
   if (out_learnt.size() == 1)
      out_btlevel = 0;
   else
   {
      int max_i = 1;
// Find the first literal assigned at the next-highest level:
      for (int i = 2; i < out_learnt.size(); i++)
         if (lState.level(var(out_learnt[i])) > lState.level(var(out_learnt[max_i])))
            max_i = i;
// Swap-in this literal at index 1:
      Lit p = out_learnt[max_i];
      out_learnt[max_i] = out_learnt[1];
      out_learnt[1] = p;
      out_btlevel = lState.level(var(p));
   }

// Compute LBD
   lbd = computeLBD(out_learnt);

// UPDATEVARACTIVITY trick (see competition'09 companion paper)
   if (lastDecisionLevel.size() > 0)
   {
      for (int i = 0; i < lastDecisionLevel.size(); i++)
      {
         if (cDb.getLbd(*this, lState.reason(var(lastDecisionLevel[i]))) < lbd)
            lState.varBumpActivity(var(lastDecisionLevel[i]), heuristic);
      }
      lastDecisionLevel.clear();
   }
   for (int j = 0; j < analyze_toclear.size(); j++)
      seen[var(analyze_toclear[j])] = 0;  // ('seen[]' is now cleared)
}

// Check if 'p' can be removed. 'abstract_levels' is used to abort early if the algorithm is
// visiting literals at levels that cannot be removed later.

bool CoreSolver::litRedundant(Lit p, uint32_t abstract_levels)
{
   analyze_stack.clear();
   analyze_stack.push(p);
   int top = analyze_toclear.size();
   while (analyze_stack.size() > 0)
   {
      assert(lState.reason(var(analyze_stack.last())).isPropagated());
      const BaseClause& c = getClause(lState.reason(var(analyze_stack.last())));
      analyze_stack.pop();  //

      for (int i = 0; i < c.size(); i++)
      {
         const Lit & q = c[i];
         if (q != p && !seen[var(q)] && lState.level(var(q)) > 0)
         {
            if (lState.reason(var(q)).isPropagated() && (lState.abstractLevel(var(q)) & abstract_levels) != 0)
            {
               seen[var(q)] = 1;
               analyze_stack.push(q);
               analyze_toclear.push(q);
            } else
            {
               for (int j = top; j < analyze_toclear.size(); j++)
                  seen[var(analyze_toclear[j])] = 0;
               analyze_toclear.shrink(analyze_toclear.size() - top);
               return false;
            }
         }
      }
   }

   return true;
}

PropagateResult CoreSolver::propagateBinary(const Lit p)
{
   PropagateResult res;
   vec<BinaryWatcher> & wbin = dbState.binWatched.getWatcher(p);
   const int wIndex = dbState.binWatched.getIndex(p);

   lbool assignVal;
   for (int i = 0; i < wbin.size(); ++i)
   {
      const BinaryWatcher & w = wbin[i];
      const BaseClause & c = cDb[w];
      assert(c.size() == 2);
      assert(c.contains(~p));
      assert(c.contains(w.getBlocker()));

      assignVal = lState.value(w.getBlocker());
      if (assignVal == l_False)  // conflict found!
      {
         res.set(w, wIndex, i, true);
         lState.qhead = lState.trail.size();
         break;
      } else if (assignVal == l_Undef)
      {
         res.set(w, wIndex, i, false);
         uncheckedEnqueue(w.getBlocker(), res.getVarSet());
      }
   }
   return res;
}

PropagateResult CoreSolver::propagateTwoWatched(const Lit p)
{
   PropagateResult res;
   int k;
   vec<Watcher> & ws = dbState.twoWatched.getWatcher(p);
   int i = 0, j = 0, listIndex = dbState.twoWatched.getIndex(p);
   for (; i < ws.size(); ++i)
   {
      const Watcher & w = ws[i];
      const Lit blocker = w.getBlocker();
      if (lState.value(blocker) != l_True)
      {
         const BaseClause& c = cDb[w];
         assert(dbState.twoWatched.isValidWatcher(w));
         assert(c.contains(~p));
         assert(c.contains(blocker));
         k = 0;
         for (; k < c.size(); k++)
            if (c[k] != w.getBlocker() && lState.value(c[k]) != l_False)  // check if it can be a new watcher
            {
               assert(var(c[k]) != var(p));
               assert(var(blocker) != var(c[k]));
               assert(c.contains(c[k]));
               //dbState.twoWatched.moveWatcher(listIndex,i, c[k]);
               vec<Watcher> & wsNew = dbState.twoWatched.getWatcher(~c[k]);
               dbState.twoWatched.getOtherWatcher(ws[i]).setBlocker(c[k], wsNew.size());
               wsNew.push(w);
               assert(dbState.twoWatched.isValidWatcher(wsNew.last()));
               break;
            }

         if (k == c.size())
         {
            if (lState.value(blocker) == l_Undef)
            {
               res.set(w, listIndex, j, false);
               dbState.twoWatched.getOtherWatcher(ws[i]).setPos(j);
               ws[j++] = ws[i];
               uncheckedEnqueue(blocker, res.getVarSet());  // propagate other watched literal
            } else  // conflict found!
            {
               assert(lState.value(blocker) == l_False);
               res.set(w, listIndex, j, true);
               for (; i < ws.size(); ++i, ++j)
               {
                  dbState.twoWatched.getOtherWatcher(ws[i]).setPos(j);
                  ws[j] = ws[i];
               }
               lState.qhead = lState.trail.size();
            }
         }
      } else
      {
         dbState.twoWatched.getOtherWatcher(ws[i]).setPos(j);
         ws[j++] = ws[i];
      }
   }
   ws.resize_(j);
   for (i = 0; i < ws.size(); ++i)
   {
      assert(dbState.twoWatched.isValidWatcher(ws[i]));
   }
   //dbState.twoWatched.assertCorrectWatchers();
   return res;
}

PropagateResult CoreSolver::propagateOneWatched(const Lit p)
{
   PropagateResult res;
   vec<OneWatcher> & oBin = dbState.oneWatched.getWatcher(p);
   int i = 0;
   for (; i < oBin.size();)
   {
      const OneWatcher & w = oBin[i];
      if (lState.value(w.getBlocker()) != l_True)
      {
         const BaseClause& c = cDb[w];
         assert(c.contains(~p));
         assert(c[0] != lit_Undef);
         assert(c.size() > 2);
         int k = 0;
         for (; k < c.size(); k++)
            if (lState.value(c[k]) != l_False)  // check if it can be a new watcher
            {
               dbState.oneWatched.getWatcher(~c[k]).push(oBin[i]);
               oBin.unordered_remove(i);
               break;
            }

         if (k == c.size())  // conflict found!
         {
            res.set(w, dbState.oneWatched.getIndex(p), i, true);
            lState.qhead = lState.trail.size();
            break;
         }
      } else
         ++i;
   }
   return res;
}

/*_________________________________________________________________________________________________
 |
 |  propagate : [void]  ->  [Clause*]
 |
 |  Description:
 |    Propagates all enqueued facts. If a conflict arises, the conflicting clause is returned,
 |    otherwise CRef_Undef.
 |
 |    Post-conditions:
 |      * the propagation queue is empty, even if there was a conflict.
 |________________________________________________________________________________________________@*/
PropagateResult CoreSolver::propagate()
{
   PropagateResult confl;
   while (lState.qhead < lState.trail.size())
   {
      assert(!confl.isConflict());
      ++statistic.nPropagations;
      const Lit p = lState.trail[lState.qhead++];

      confl = propagateBinary(p);

      if (!confl.isConflict())
      {
         confl = propagateTwoWatched(p);
         if (!confl.isConflict())
            confl = propagateOneWatched(p);
      }
   }
   return confl;
}

void CoreSolver::resolveConflict(const PropagateResult & confl)
{
   int backtrack_level;
   vec<Lit> learnt_clause, selectors;
   unsigned int nblevels;
   assert(confl.getVarSet().isPropagated());
   const BaseClause & c = getClause(confl.getVarSet());
   assert(c.size() > 1);

// CONFLICT
   ++statistic.nConflicts;
   if (statistic.nConflicts % 5000 == 0 && heuristic.var_decay < heuristic.max_var_decay)
      heuristic.var_decay += 0.01;

   if (lState.decisionLevel() == 0)
   {
      setResult(l_False, "Unsatisfiable through level 0 conflict");
      return;
   }

   trailQueue.push(lState.trail.size());
// BLOCK RESTART (CP 2012 paper)
   if (statistic.nConflicts > LOWER_BOUND_FOR_BLOCKING_RESTART && lbdQueue.isvalid() && lState.trail.size() > heuristic.R * trailQueue.getavg())
   {
      lbdQueue.fastclear();
   }

   learnt_clause.clear();

   analyze(confl.getVarSet(), learnt_clause, backtrack_level, nblevels);
//std::cout << "conflict (" << statistic.nConflicts << ") on clause " << confl << " from " << decisionLevel() << " to " << backtrack_level << std::endl;
   lbdQueue.push(nblevels);
   statistic.sumLbd += nblevels;
   cancelUntil(backtrack_level);
   assert(backtrack_level > 0 || learnt_clause.size() == 1);
   uncheckedEnqueue(learnt_clause[0], cDb.addClause(*this, learnt_clause, nblevels));
   if (lState.reason(var(learnt_clause[0])).isPropagated())
      assert(getClause(lState.reason(var(learnt_clause[0]))).size() == learnt_clause.size());
   lState.varDecayActivity(heuristic);

   cDb.notifyNewConflict(*this);
}

bool CoreSolver::shouldRestart() const
{
   if(heuristic.luby)
      return statistic.nConflicts >= lubyConflictlimit;
   else
   return lbdQueue.isvalid() && ((lbdQueue.getavg() * heuristic.K) > (statistic.sumLbd / statistic.nConflicts));
}

void CoreSolver::restart()
{
   lbdQueue.fastclear();
   int bt = 0;
   cancelUntil(bt);
   statistic.nRestarts++;
   cDb.notifyRestart(*this);
   if(heuristic.luby)
      lubyConflictlimit += luby(2, statistic.nRestarts)*100;
}
void CoreSolver::addDecision()
{
   // New variable decision:
   Lit next = pickBranchLit();
   ++statistic.nDecisions;
   if (next == lit_Undef)
   {
      result = l_True;
      return;
   }

// Increase decision level and enqueue 'next'
   lState.newDecisionLevel();
   uncheckedEnqueue(next, VarSet::decision());

}

void CoreSolver::uncheckedEnqueue(const Lit p, const VarSet & from, const bool imported)
{
   assert(lState.value(p) == l_Undef && from.getWatcherType() != WatcherType::ONE);
   const int varP = var(p);
   lState.state[varP].assign = lbool(!sign(p));
   lState.state[varP].set((lState.decisionLevel() == 0) ? VarSet() : from, lState.decisionLevel());
   lState.trail.push_(p);
   if (!imported && lState.decisionLevel() == 0)
   {
      ++statistic.nUnit;
      //lState.newUnits.push(p);
      cDb.addUnit(*this, p);
   }
   assert(from.isValid() || (lState.decisionLevel() == 0 && lState.level(p) == 0));
   assert(!lState.reason(var(p)).isPropagated() || p != lState.getDecision(lState.decisionLevel()));
}

bool CoreSolver::enqueue(const Lit p, const VarSet & from)
{
   bool res = lState.value(p) == l_Undef;
   if (res)
      uncheckedEnqueue(p, from, true);
   return res;
}


/*_________________________________________________________________________________________________
 |
 |  search : (nof_conflicts : int) (params : const SearchParams&)  ->  [lbool]
 |
 |  Description:
 |    Search for a model the specified number of conflicts.
 |    NOTE! Use negative value for 'nof_conflicts' indicate infinity.
 |
 |  Output:
 |    'l_True' if a partial assigment that is consistent with respect to the clauseset is found. If
 |    all variables are decision variables, this means that the clause set is satisfiable. 'l_False'
 |    if the clause set is unsatisfiable. 'l_Undef' if the bound on number of conflicts is reached.
 |________________________________________________________________________________________________@*/
lbool CoreSolver::search()
{

   PropagateResult confl;
   while (!cDb.jobFinished() && result == l_Undef)
   {
      confl = propagate();

      if (confl.isConflict())
      {
         resolveConflict(confl);
         if (heuristic.conflict_budget < statistic.nConflicts)
            setResult(l_Undef, "Conflict budget reached");
      }

      if (shouldRestart())
         restart();
      else if (!lState.hasOpenPropagation())
         addDecision();
   }
   return result;
}

lbool CoreSolver::solve()  // Parameters are useless in core but useful for SimpSolver....
{
   lbool res = l_Undef;
   model.clear();
   conflict.clear();

   cDb.notifySolverStart(*this);

// Search:
   res = search();

   if (res != l_Undef)
   {

      if (res == l_True)
      {
         //// Extend & copy model:
         //model.growTo(lState.nVars());
         //for (int i = 0; i < lState.nVars(); i++)
         //   model[i] = lState.value(i);
      }
      setResult(res, "Solution through propagation");
   }
//cancelUntil(0);

   cDb.notifySolverEnd(*this);
   return result;

}

//========================
