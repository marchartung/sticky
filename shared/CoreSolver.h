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

#ifndef CORE_Solver_h
#define CORE_Solver_h

#include "shared/DatabaseThreadState.h"
#include "shared/LiteralSetting.h"
#include "shared/SharedTypes.h"
#include "shared/ClauseTypes.h"

#include "shared/Heuristic.h"
#include "shared/Statistic.h"
#include "shared/ClauseWatcher.h"
#include "shared/PropagateResult.h"
#include "shared/SolverConfiguration.h"
#include "glucose/simp/SimpSolver.h"

#include <limits>

namespace Sticky
{
using Glucose::SimpSolver;
class ClauseDatabase;

//=================================================================================================
// Solver -- the main class:

class CoreSolver
{

   friend class SolverConfiguration;
   CoreSolver(const CoreSolver &s) = delete;
   CoreSolver(CoreSolver && s) = delete;

 public:

   // Constructor/Destructor:
   //
   CoreSolver(ClauseDatabase & cDb, const SimpSolver & solver, const unsigned & threadId, const vec<CRef> & initCRefs);
   ~CoreSolver();

   //void checkReason(const vec<Lit> & falseClause) const;

   lbool solve();                        // Search without assumptions.

   bool okay() const;           // FALSE means solver is in a conflicting state

   // Variable mode:
   //

   unsigned int computeLBD(const vec<Lit> & lits);
   unsigned int computeLBD(const BaseClause &c);

   lbool getResult() const;
   void setResult(const lbool res, const std::string & msg = "");
   const vec<lbool> & getModel() const;
   const vec<Lit> & getConflict() const;

   const unsigned & getThreadId() const;

   const DatabaseThreadState & getThreadState() const;
   DatabaseThreadState & getThreadState();

   const LiteralSetting & getLiteralSetting() const;
   LiteralSetting & getLiteralSetting();

   const SolverStatistic & getStatistic() const;
   SolverStatistic & getStatistic();

   const SolverHeuristic & getHeuristic() const;
   SolverHeuristic & getHeuristic();

   void cancelUntil(const int level);             // Backtrack until a certain level.
   Lit pickBranchLit();                   // Return the next decision variable.
   void findDecisionClauseForPropagation(Lit p, vec<Lit>& out_conflict);
   bool findDecisionClauseForConflict(const VarSet & confl, vec<Lit>& out_conflict, const CRef excludeRef = CRef_Undef);
   void analyze(const VarSet & confl, vec<Lit>& out_learnt, int& out_btlevel, unsigned int &nblevels);    // (bt = backtrack)
   bool litRedundant(const Lit p, const uint32_t abstract_levels);  // (helper method for 'analyze()')
   PropagateResult propagate();  // Perform unit propagation. Returns possibly conflicting clause.
   void resolveConflict(const PropagateResult & confl);

   bool enqueue(const Lit p, const VarSet & from);
   void uncheckedEnqueue(const Lit p, const VarSet & from, const bool imported = false);  // Enqueue a literal. Assumes value of literal is undefined.

 protected:

   // Solver state:
   lbool result;
   bool isMinimizer;
   // temporaries:
   unsigned int lbdRoundCounter;
   bool reduceOnSize;
   uint64_t lubyConflictlimit;
   int reduceOnSizeSize;                // See XMinisat paper
   vec<unsigned int> permDiff;  // permDiff[var] contains the current conflict number... Used to count the number of  LBD
   // UPDATEVARACTIVITY trick (see competition'09 companion paper)
   vec<Lit> lastDecisionLevel;

   vec<char> seen;
   vec<Lit> analyze_stack;
   vec<Lit> analyze_toclear;

   // Used for restart strategies
   bqueue<unsigned int> trailQueue;
   bqueue<unsigned int> lbdQueue;  // Bounded queues for restarts.

   SolverHeuristic heuristic;
   SolverStatistic statistic;

   // State variables
   ClauseDatabase & cDb;
   LiteralSetting lState;
   DatabaseThreadState dbState;

   // Extra results: (read-only member variable)
   //
   vec<lbool> model;  // If problem is satisfiable, this vector contains the model (if any).
   vec<Lit> conflict;  // If problem is unsatisfiable (possibly under assumptions),

   const BaseClause & getClause(const VarSet & vs) const;
   const BaseClause & getClause(const Var v) const;

   PropagateResult propagateBinary(const Lit l);
   PropagateResult propagateTwoWatched(const Lit l);
   PropagateResult propagateOneWatched(const Lit l);

   lbool search();

   bool shouldRestart() const;
   void restart();
   void addDecision();

   void minimisationWithBinaryResolution(vec<Lit> &out_learnt);

};

inline lbool CoreSolver::getResult() const
{
   return result;
}

inline const vec<lbool> & CoreSolver::getModel() const
{
   return model;
}
inline const vec<Lit> & CoreSolver::getConflict() const
{
   return conflict;
}

inline const unsigned & CoreSolver::getThreadId() const
{
   return dbState.threadId;
}

inline const DatabaseThreadState & CoreSolver::getThreadState() const
{
   return dbState;
}
inline DatabaseThreadState & CoreSolver::getThreadState()
{
   return dbState;
}

inline const LiteralSetting & CoreSolver::getLiteralSetting() const
{
   return lState;
}
inline LiteralSetting & CoreSolver::getLiteralSetting()
{
   return lState;
}

inline const SolverStatistic & CoreSolver::getStatistic() const
{
   return statistic;
}
inline SolverStatistic & CoreSolver::getStatistic()
{
   return statistic;
}

inline const SolverHeuristic & CoreSolver::getHeuristic() const
{
   return heuristic;
}
inline SolverHeuristic & CoreSolver::getHeuristic()
{
   return heuristic;
}

}

#endif
