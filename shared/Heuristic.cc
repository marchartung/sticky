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

#include "Heuristic.h"
#include "shared/SharedTypes.h"

#include <limits>

using namespace Glucose;

//=================================================================================================
// Options:
extern DoubleOption opt_K;
extern DoubleOption opt_R;
extern IntOption opt_size_lbd_queue;
extern IntOption opt_size_trail_queue;

extern IntOption opt_first_reduce_db;
extern IntOption opt_inc_reduce_db;
extern IntOption opt_spec_inc_reduce_db;

extern IntOption opt_lb_size_minimzing_clause;
extern IntOption opt_lb_lbd_minimzing_clause;

extern DoubleOption opt_var_decay;
extern DoubleOption opt_clause_decay;
extern DoubleOption opt_max_var_decay;
extern DoubleOption opt_random_var_freq;
extern DoubleOption opt_random_seed;
extern IntOption opt_ccmin_mode;
extern IntOption opt_phase_saving;
extern BoolOption opt_rnd_init_act;
// CLAUSE SHARING

const char* _cs = "CLAUSE SHARING";
BoolOption opt_oneWatched(_cs, "useUnaryWatched", "Imports clauses from other solvers (except good and permanent clauses) as unary watched", true);
BoolOption opt_useEarlyImport(_cs, "useEarlyImp", "Enables/Disables import during propagation", true);
BoolOption opt_useLBDImprVivification(_cs, "useLBDVivi", "Clauses which lbd is improved are vivified", false);
BoolOption opt_useExportVivification(_cs, "useExportVivi", "Every exported clause is vivified", false);
BoolOption opt_useCompleteVivification(_cs, "useCompVivi", "Every permanent clause is vivified", false);
BoolOption opt_usePrivateVivi(_cs, "usePrivateVivi", "Vivify private clauses", false);
BoolOption opt_useBackTrackVivi(_cs, "useBacktrackVivi", "Vivify clauses using conflict analysis", true);
BoolOption opt_useDynamicVivi(_cs, "useDynamicVivi", "Vivify clauses bounded by dynamic impact-time meassure", false);

IntOption opt_numStartVivification(_cs, "useStartVivi", "During start up, each clause is n times vivified", 0, IntRange(0, std::numeric_limits<int32_t>::max()));
IntOption opt_importAfterNConflicts(_cs, "clImpAfterNCl", "Number of conflicts after which clauses are imported.", 32, IntRange(0, std::numeric_limits<int32_t>::max()));
IntOption opt_max_good_lbd(_cs, "maxPermLBD", "Good clauses are enforced to stay in all solvers. A clause is good, when their LBD is lower equal to", 2,
                           IntRange(0, std::numeric_limits<int32_t>::max()));
IntOption opt_max_good_sz(_cs, "maxPermSZ", "Good clauses are enforced to stay in all solvers. A clause is good, when their size is lower equal to", 30,
                          IntRange(0, std::numeric_limits<int32_t>::max()));
IntOption opt_max_shared_lbd(_cs, "maxSharedLBD", "Share clauses between solver. when their LBD is lower than", 6, IntRange(0, std::numeric_limits<int32_t>::max()));
IntOption opt_max_shared_sz(_cs, "maxSharedSZ", "Share clauses between solver. when their size is lower than", 30, IntRange(0, std::numeric_limits<int32_t>::max()));
IntOption opt_max_vivi_lbd(_cs, "maxVifiLBD", "Clause with a lbd equal lower than this, can be vivified", 4, IntRange(0, std::numeric_limits<int32_t>::max()));

IntOption opt_max_two_watched_lbd(_cs, "maxTwoWLBD", "Maximum LBD for imported clauses which are two watched instead of one watched", 3,
                                  IntRange(0, std::numeric_limits<int32_t>::max()));
IntOption opt_conflict_budget(_cs, "clBudget", "Maximum number of conflicts in a solver before abort (-1 = no limit)",-1,
                                  IntRange(-1, std::numeric_limits<int32_t>::max()));

IntOption opt_share_only_reused_clauses(_cs, "reusedOnly", "Shares clauses only when they are reused this many times", 2, IntRange(0, 100));
// GARBAGE HEURISTIC
const char* _sgc = "SHARED GARBAGE COLLECT";
DoubleOption opt_garbage_frac_shared(_sgc, "sgc-frac-sh", "The fraction of wasted memory allowed before the garbage collection is triggered", 0.40,
                                     DoubleRange(0, false, 1, false));

DoubleOption opt_frac_solver(_sgc, "sgc-frac-solver", "Portion of memory used for the solvers. Other part will be used for the clause database", 0.90,
                             DoubleRange(0, false, 1, false));

IntOption opt_sz_learnt_ring_buffer(_sgc, "learnt-buffer-sz", "Number of entries in the clause exchange ring buffer before entries are overwritten (number is per thread)", 100000,
                                    IntRange(0, std::numeric_limits<int32_t>::max()));
IntOption opt_sz_unary_ring_buffer(_sgc, "unary-buffer-sz", "Number of entries in the unary exchange ring buffer before entries are overwritten (number is per thread)", 10000,
                                   IntRange(0, std::numeric_limits<int32_t>::max()));

DoubleOption opt_shared_reduce_delay(_sgc, "shared-red-delay", "Shared database will reduce by this factor compared to normal solver instances", 1.0,
                                     DoubleRange(0.5, false, 10.0, false));
DoubleOption opt_dynamic_vivi_tol(_sgc, "dynViviTol", "Time spend at least for clause vivification", 0.005,
                                     DoubleRange(0.0, true, 1.0, true));

const char* _parallel = "PARALLEL";
BoolOption opt_print_aggregated(_parallel, "printAggr", "Prints only an aggregation of the solver specific data", true);
BoolOption opt_print_human(_parallel, "printHuman", "Prints human readable output instead of csv", false);
IntOption opt_maxmemory(_parallel, "maxmemory", "Maximum memory to use (in Mb)", 8000);
IntOption opt_maxtime(_parallel, "maxtime", "Maximum system time to solve (in seconds, -1 for no software limit)", -1);
IntOption opt_nbsolversmultithreads(_parallel, "nthreads", "Number of core threads", 2);
DoubleOption opt_statsInterval(_parallel, "statsinterval", "Seconds (real time) between two stats reports", 5.0, DoubleRange(1.0, true, 10000.0, false));
IntOption opt_enforce_restart_after_reduces(_parallel, "enforceRestarts", "Number of shared reduces without enforcing delayed solver to restart", 4, IntRange(1, 1000));
namespace Sticky
{
SolverHeuristic::SolverHeuristic()
      : K(opt_K),
        R(opt_R),
        sizeLBDQueue(opt_size_lbd_queue),
        sizeTrailQueue(opt_size_trail_queue),
        lbSizeMinimizingClause(opt_lb_size_minimzing_clause),
        lbLBDMinimizingClause(opt_lb_lbd_minimzing_clause),
        incReduceDB(opt_inc_reduce_db),
        specialIncReduceDB(opt_spec_inc_reduce_db),
        firstReduceDb(opt_first_reduce_db),
        cla_inc(1),
        var_inc(1),
        var_decay(opt_var_decay),
        max_var_decay(opt_max_var_decay),
        random_var_freq(opt_random_var_freq),
        random_seed(opt_random_seed),
        ccmin_mode(opt_ccmin_mode),
        phase_saving(opt_phase_saving),
        rnd_pol(false),
        rnd_init_act(opt_rnd_init_act),
        conflict_budget((opt_conflict_budget == -1) ? std::numeric_limits<decltype(conflict_budget)>::max() : opt_conflict_budget),
        propagation_budget(std::numeric_limits<decltype(propagation_budget)>::max()),
        chanseok(false),
        luby(false)
{
}

Sticky::DatabaseHeuristic::DatabaseHeuristic()
      : onlyPrintAggr(opt_print_aggregated),
        printHumanReadable(opt_print_human),
        sharedReduceDelay(opt_shared_reduce_delay),
        numBuckets(0),
        numThreads(opt_nbsolversmultithreads),
        maxAllocBytes(1024ULL * 1024ULL * opt_maxmemory),
        fracSolverMem(opt_frac_solver),
        outputInterval(opt_statsInterval),
        maxRuntime(opt_maxtime),
        garbageWasteFrac(opt_garbage_frac_shared),
        cla_decay(opt_clause_decay)
{
}

Sticky::SharingHeuristic::SharingHeuristic()
      : useOneWatched(opt_oneWatched),
        useEarlyImport(opt_useEarlyImport),
        useLBDImproveVivification(opt_useLBDImprVivification),

        useExportVivification(opt_useExportVivification),
        useCompleteVivification(opt_useCompleteVivification),
        usePrivateVivification(opt_usePrivateVivi),
        useBackTrackVivification(opt_useBackTrackVivi),
        useDynamicVivi(opt_useDynamicVivi),
        numStartUpVivification(opt_numStartVivification),
        importAfterConflicts(opt_importAfterNConflicts),
        maxVivificationLbd(opt_max_vivi_lbd),
        maxShareDirectLBD(opt_max_good_lbd),
        maxSharedLBD(opt_max_shared_lbd),
        maxTwoWatchedLBD(opt_max_two_watched_lbd),
        maxShareDirectSize(opt_max_good_sz),
        maxSharedSize(opt_max_shared_sz),
        numReusedBeforeSharing(opt_share_only_reused_clauses),
        numCRefsExchangePerThread(opt_sz_learnt_ring_buffer),
        numUnaryExchangePerThread(opt_sz_unary_ring_buffer),
        viviSpendTolerance(opt_dynamic_vivi_tol)
{
   if(opt_nbsolversmultithreads == 1)
   {
      maxSharedSize = 0;
      maxShareDirectSize = 0;
   }
   if(useDynamicVivi)
   {
      useExportVivification = true;
      useCompleteVivification = false;
      usePrivateVivification = false;
      numStartUpVivification = 0;
      maxVivificationLbd = maxShareDirectLBD;
   }
}
}
/* namespace Glucose */
