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

#ifndef SHARED_LITERALSETTING_H_
#define SHARED_LITERALSETTING_H_

#include "shared/SharedTypes.h"
#include "shared/Heuristic.h"
#include "shared/Statistic.h"
#include "shared/PropagateResult.h"
#include "parallel_utils/RandomGenerator.h"
#include "glucose/simp/SimpSolver.h"

namespace Sticky
{
struct AssignState
{
   WatcherType wType;
   bool polarity;
   lbool assign;
   unsigned watcherPos;
   int listPos;
   int level;

   AssignState()
      :wType(WatcherType::UNIT),
       polarity(false),
       assign(l_Undef),
       watcherPos(-1),
       listPos(-1),
       level(-1)
   {
   }

   inline VarSet getReason() const
   {
      return VarSet::create(wType,listPos,watcherPos);
   }

   inline void set(const VarSet & vs,const int level)
   {
      wType = vs.getWatcherType();
      watcherPos = vs.getWatcherPos();
      listPos = vs.getListPos();
      this->level = level;
   }

};
struct LiteralSetting
{
   int qhead;
   RandomGenerator rg;
   vec<Lit> newUnits;
   vec<Lit> trail;  // Assignment stack; stores all assigments made in the order they were made.
   vec<int> trail_lim;  // Separator indices for different decision levels in 'trail'.
//   vec<lbool> assigns;          // The current assignments.
//   vec<bool> polarity;         // The preferred polarity of each variable.
//   vec<PropState> vardata;          // Stores reason and level for each variable.
   vec<AssignState> state;
   vec<double> activity;  // A heuristic measurement of the activity of a variable.
   Heap<VarOrderLt> order_heap;  // A priority queue of variables ordered with respect to the variable activity.

   LiteralSetting() = delete;
   LiteralSetting(const Glucose::SimpSolver & s, double randomSeed);
   LiteralSetting(const LiteralSetting & litSetting);

   int nAssigns() const;       // The current number of assigned literals.
   int nVars() const;       // The current number of variables.
   int nFreeVars() const;

   Lit getDecision(const int level) const;

   lbool value(const Var x) const;       // The current value of a variable.
   lbool value(const Lit p) const;       // The current value of a literal.

   VarSet reason(const Var x) const;

   void setPolarity(const Var v, const bool b);  // Declare which polarity the decision heuristic should use for a variable. Requires mode 'polarity_user'.
   void setDecisionVar(const Var v, const bool b);  // Declare if a variable should be eligible for selection in the decision heuristic.

   char valuePhase(const Var v);

   // Operations on clauses:
   bool satisfied(const BaseClause& c) const;  // Returns TRUE if a clause is satisfied in the current state.

   // Main internal methods:
   //
   void insertVarOrder(const Var x);  // Insert a variable in the decision order priority queue.
   void newDecisionLevel();                     // Begins a new decision level.
   int decisionLevel() const;  // Gives the current decisionlevel.

   // Maintaining Variable/Clause activity:
   //
   void varDecayActivity(SolverHeuristic & h);  // Decay all variables with the specified factor. Implemented by increasing the 'bump' value instead.
   void varBumpActivity(const Var v, SolverHeuristic & h);  // Increase a variable with the current 'bump' value.

   uint32_t abstractLevel(const Var x) const;  // Used to represent an abstraction of sets of decision levels.

   int level(const Var x) const;
   int level(const Lit x) const;

   int maxLevel(const BaseClause & c) const;

   void rebuildOrderHeap();
   lbool getResult() const;

   bool hasOpenPropagation() const;
};


inline Lit LiteralSetting::getDecision(const int level) const
{
   assert(level <= trail_lim.size() && level > 0);
   assert(!reason(var(trail[trail_lim[level-1]])).isPropagated());
   return trail[trail_lim[level-1]];
}

inline bool LiteralSetting::hasOpenPropagation() const
{
   return trail.size() > qhead;
}

inline char LiteralSetting::valuePhase(const Var v)
{
   return state[v].polarity;
}

inline int LiteralSetting::nAssigns() const
{
   return trail.size();
}

inline int LiteralSetting::nVars() const
{
   return state.size();
}

inline void LiteralSetting::setPolarity(const Var v, bool b)
{
   state[v].polarity = b;
}
inline void LiteralSetting::setDecisionVar(const Var v, bool b)
{
   insertVarOrder(v);
}

inline int LiteralSetting::decisionLevel() const
{
   return trail_lim.size();
}

inline uint32_t LiteralSetting::abstractLevel(const Var x) const
{
   return 1 << (level(x) & 31);
}

inline lbool LiteralSetting::value(const Var x) const
{
   return state[x].assign;
}

inline lbool LiteralSetting::value(const Lit p) const
{
   return state[var(p)].assign ^ sign(p);
}

inline VarSet LiteralSetting::reason(const Var x) const
{
   const AssignState & s = state[x];
   return s.getReason();
}

inline int LiteralSetting::level(const Var x) const
{
   return state[x].level;
}
inline int LiteralSetting::level(const Lit x) const
{
   return state[var(x)].level;
}
inline int LiteralSetting::maxLevel(const BaseClause & c) const
{
   int res = level(var(c[0]));
   for (int i = 1; i < c.size(); ++i)
      if (res < level(var(c[i])))
         res = level(var(c[i]));
   return res;
}

inline void LiteralSetting::insertVarOrder(const Var x)
{
   if (!order_heap.inHeap(x))
      order_heap.insert(x);
}

inline void LiteralSetting::varDecayActivity(SolverHeuristic & h)
{
   h.var_inc *= (1 / h.var_decay);
}

inline void LiteralSetting::newDecisionLevel()
{
   trail_lim.push(trail.size());
}


}
#endif /* SHARED_LITERALSETTING_H_ */
