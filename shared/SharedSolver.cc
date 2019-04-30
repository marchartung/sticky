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
#include "shared/SharedSolver.h"
#include "glucose/utils/System.h"
#include "glucose/mtl/XAlloc.h"
#include "parallel_utils/CPUBind.h"

#include <atomic>
#include <cstdlib>
#include <pthread.h>

namespace Sticky
{

struct SolverData
{
   unsigned id;
   ClauseDatabase * cDb;
   const SimpSolver * blueprint;
   CoreSolver ** solver;
   pthread_barrier_t * barrier;
   const vec<CRef> * initialClauses;

   SolverData(const unsigned & id, ClauseDatabase & cDb, const SimpSolver * blueprint, CoreSolver * & self, pthread_barrier_t & counter, const vec<CRef> & initialClauses)
         : id(id),
           cDb(&cDb),
           blueprint(blueprint),
           solver(&self),
           barrier(&counter),
           initialClauses(&initialClauses)
   {
   }
};

void * start_solver(void * data)
{
   SolverData * sd = reinterpret_cast<SolverData*>(data);
   CPUBind::bindThread(sd->id);
   Glucose::ByteCounter::init();
   CoreSolver * & solver = *(sd->solver);
   solver = reinterpret_cast<CoreSolver*>(Glucose::xrealloc(NULL, sizeof(CoreSolver)));
   solver = new (solver) CoreSolver(*(sd->cDb), *(sd->blueprint), sd->id, *(sd->initialClauses));
   SolverConfiguration::configure(**sd->solver, sd->id, sd->cDb->getBuckets().getHeuristic().numThreads);

   pthread_barrier_wait(sd->barrier);
   (*sd->solver)->solve();
   pthread_barrier_wait(sd->barrier);
   delete sd;
   Glucose::xfree(solver);
   solver = nullptr;
   return NULL;
}

SharedSolver::SharedSolver()
      : ClauseDatabase(),
        printTimer(buckets.getHeuristic().outputInterval),
        viviTimer(1),
        runTimer(buckets.getHeuristic().maxRuntime),
        initialSolver(new SimpSolver()),
        globalStat(*this)
{
}

SharedSolver::~SharedSolver()
{
   for (int i = 0; i < solvers.size(); ++i)
      delete solvers[i];
}

int SharedSolver::nVars() const
{
   return initialSolver->nVars();
}
int SharedSolver::nClauses() const
{
   return initialSolver->nClauses();
}

bool SharedSolver::addClause_(vec<Lit>& ps)
{
   return initialSolver->addClause(ps);
}

Var SharedSolver::newVar(bool sign, bool dvar)
{
   return initialSolver->newVar(sign, dvar);
}

void SharedSolver::run()
{
   bool completeSetter = true;
   printSolverStats();
   printTimer.reset();
   while (resourcesOk() && !jobFinished())
   {
      if (printTimer.isOver())
      {
         printSolverStats();
         printTimer.reset();
         if (completeSetter && globalStat.numConflicts.min() > 0)
         {
            completeSetter = false;
            ClauseDatabase::completeClauseNum = 2*globalStat.numPermanent.value();
         }
      }
      if (viviTimer.isOver())
      {
         globalStat.update(runTimer.getPassedTime());
         checkVivifyComplete(globalStat);
         viviTimer.reset();
      }
      usleep(15);
   }
   if (!resourcesOk())
   {
      setAbortSolving(true);
   }
}

int SharedSolver::getShowModel()
{
   return false;
}

const vec<lbool> & SharedSolver::getModel() const
{
   return initialSolver->model;
}

vec<CRef> SharedSolver::initializeDatabase()
{
   const SimpSolver & si = *initialSolver;
   const Glucose::ClauseAllocator & ca = si.ca;
   vec<CRef> res;
   res.capacity(si.clauses.size());
   for (int i = 0; i < si.clauses.size(); ++i)
   {
      const CRef & cref = si.clauses[i];
      assert(ca[cref].size() > 1);
      res.push(addInitialClause(ca[cref]));
   }
   ClauseDatabase::completeClauseNum = 2 * res.size();
   return res;
}

lbool SharedSolver::solve()
{
   vec<pthread_t> pids(solvers.size());
   pthread_barrier_t barrier;
   vec<CRef> crefs = initializeDatabase();
   pthread_barrier_init(&barrier, NULL, buckets.getHeuristic().numThreads + 1);
   std::cout << "c sticky runs with " << solvers.size() << " solvers." << std::endl;
   std::cout << "c Reduced problems consists of " << initialSolver->nVars() << " variables and " << initialSolver->nClauses() << " initial clauses" << std::endl;
   for (int i = 0; i < solvers.size(); ++i)
   {
      int ret = pthread_create(&(pids[i]), NULL, start_solver, new SolverData(i, *static_cast<ClauseDatabase *>(this), initialSolver, solvers[i], barrier, crefs));
      assert(ret == 0);
   }
   pthread_barrier_wait(&barrier);
   solvers[0]->getStatistic().nAllocPermanentCl += buckets.getStatistic().numInitCl;
   // free memory which is not needed anymore:
   crefs.clear(true);
   assert(initialSolver != nullptr);
   delete initialSolver;
   initialSolver = nullptr;

   for (int i = 0; i < solvers.size(); ++i)
      assert(solvers[i] != nullptr);
   run();
   printFinalStats();
   lbool res = prepareSolution();
   pthread_barrier_wait(&barrier);
   for (int i = 0; i < pids.size(); ++i)
      pthread_join(pids[i], NULL);
   pthread_barrier_destroy(&barrier);
   return res;
}

lbool SharedSolver::prepareSolution()
{
   lbool res = getResult();  // TODO if var setting is neccessary, here we should fix the model
//   if (res == l_True)
//   {
//      const CoreSolver & win = winner();
//      initialSolver->extendModel();
//
//      // Extend & copy model:
//      initialSolver->model.growTo(nVars());
//      vec<Lit> falseClause;
//      for (int i = 0; i < nVars(); i++)
//         initialSolver->model[i] = win.getModel()[i];
//      if (initialSolver->checkSolution(initialSolver->model, falseClause))
//      {
//      } else
//      {
//         std::cout << "SAT solution is false" << std::endl;
//         win.checkReason(falseClause);
//      }
//   } else if (res == l_False)
//   {
//      initialSolver->ok = false;
//   }

   return res;
}

bool SharedSolver::eliminate(bool turn_off_elim)
{
   assert(initialSolver != nullptr);
   return initialSolver->eliminate(turn_off_elim);
}

bool SharedSolver::simplify()
{
   assert(initialSolver != nullptr);
   return initialSolver->simplify();
}

bool SharedSolver::okay() const
{
   assert(initialSolver != nullptr);
   return initialSolver->okay();
}

void SharedSolver::printFinalStats()
{
   globalStat.update(runTimer.getPassedTime());
   globalStat.printFinal();
}

void SharedSolver::printSolverStats()
{
   globalStat.update(runTimer.getPassedTime());
   globalStat.printInterval();
}

bool SharedSolver::resourcesOk()
{
   return !runTimer.isOver();
}

} /* namespace Glucose */
