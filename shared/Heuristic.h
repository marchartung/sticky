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

#ifndef SHARED_HEURISTIC_H_
#define SHARED_HEURISTIC_H_

#include <cstdint>

#include "shared/SharedTypes.h"
#include "shared/ClauseTypes.h"

namespace Sticky
{

struct SolverHeuristic
{
   // Constants For restarts
   double K;
   double R;
   double sizeLBDQueue;
   double sizeTrailQueue;

   // Constant for reducing clause
   int lbSizeMinimizingClause;
   unsigned int lbLBDMinimizingClause;
   unsigned int incReduceDB;
   unsigned int specialIncReduceDB;
   unsigned int firstReduceDb;

   // Constant for heuristic
   double cla_inc;          // Amount to bump next clause with.
   double var_inc;          // Amount to bump next variable with.
   double var_decay;
   double max_var_decay;
   double random_var_freq;
   double random_seed;
   int ccmin_mode;  // Controls conflict clause minimization (0=none, 1=basic, 2=deep).
   int phase_saving;  // Controls the level of phase saving (0=none, 1=limited, 2=full).
   bool rnd_pol;            // Use random polarities for branching heuristics.
   bool rnd_init_act;  // Initialize variable activities with a small random value.
   bool chanseok;
   bool luby;

   // Resource contraints:
   uint64_t conflict_budget;
   uint64_t propagation_budget;

   SolverHeuristic();
};

struct DatabaseHeuristic
{
   bool onlyPrintAggr;
   bool printHumanReadable;
   double sharedReduceDelay;
   unsigned numBuckets;
   unsigned numThreads;
   uint64_t maxAllocBytes;
   double fracSolverMem;
   double outputInterval;
   double maxRuntime;
   double garbageWasteFrac;
   double cla_decay;

   DatabaseHeuristic();
};

class SharingHeuristic
{
 public:

   inline const unsigned & sizeExchangeCRefBuffer() const
   {
      return numCRefsExchangePerThread;
   }
   inline const unsigned & sizeExchangeUnaryBuffer() const
   {
      return numUnaryExchangePerThread;
   }

   inline bool isPermanentClause(const unsigned & lbd, const int & size, const ActivityType & activity = std::numeric_limits<ActivityType>::max()) const
   {
      bool res = size == 2 || ( maxShareDirectLBD >= lbd && maxShareDirectSize >= size);
      return res;
   }

   inline bool isPermanentClause(const BaseClause & c) const
   {
      return isPermanentClause(c.getLbd(), c.size());
   }

   inline bool isSharedClause(const unsigned & lbd, const int & size, const ActivityType & activity = std::numeric_limits<ActivityType>::max()) const
   {
      bool res =  maxSharedLBD >= lbd && maxSharedSize >= size && activity >= numReusedBeforeSharing;
      return res;
   }

   inline bool isSharedClause(const BaseClause & c) const
   {
      return c.isPrivateClause() && isSharedClause(c.getLbd(), c.size(), 0);
   }

   inline bool isOneWatchedClause(const BaseClause & c) const
   {
      return !c.isPermanentClause() && useOneWatched && c.getLbd() > maxTwoWatchedLBD;
   }


   SharingHeuristic();

   bool useOneWatched;
   bool useEarlyImport;
   bool useLBDImproveVivification;
   bool useExportVivification;
   bool useCompleteVivification;
   bool usePrivateVivification;
   bool useBackTrackVivification;
   bool useDynamicVivi;
   unsigned numStartUpVivification;
   unsigned importAfterConflicts;
   unsigned maxVivificationLbd;
   unsigned maxShareDirectLBD;
   unsigned maxSharedLBD;
   unsigned maxTwoWatchedLBD;
   int maxShareDirectSize;
   int maxSharedSize;
   unsigned numReusedBeforeSharing;
   unsigned numCRefsExchangePerThread;
   unsigned numUnaryExchangePerThread;
   double viviSpendTolerance;
};

} /* namespace Glucose */

#endif /* SHARED_HEURISTIC_H_ */
