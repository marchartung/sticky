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
#ifndef SHARED_STATISTIC_H_
#define SHARED_STATISTIC_H_

#include <limits>
#include <string>
#include <iostream>
#include <atomic>

#include "glucose/mtl/Vec.h"
#include "parallel_utils/StatisticPrinter.h"
namespace Sticky
{
class CoreSolver;
class ClauseDatabase;

using std::string;
using std::to_string;
using std::atomic;

struct SolverStatistic
{

   uint32_t medianLbd;
   uint64_t sumLbd;
   uint64_t nLastReduceConflicts;
   std::atomic<uint64_t> nRestarts;
   std::atomic<uint64_t> nReduces;
   std::atomic<uint64_t> nPropagations;
   std::atomic<uint64_t> nViviPropagations;
   std::atomic<uint64_t> nDecisions;
   std::atomic<uint64_t> nConflicts;
   std::atomic<uint64_t> nVivifications;
   std::atomic<uint64_t> sumVivificationLength;
   std::atomic<uint64_t> sumViviStartLength;
   std::atomic<uint64_t> failedVivifycations;
   std::atomic<uint64_t> nUnit;
   std::atomic<uint64_t> nExportedCl;
   std::atomic<uint64_t> nImportedCl;
   std::atomic<uint64_t> nPromotedCl;
   std::atomic<uint64_t> nPrivateCl;
   std::atomic<uint64_t> nSharedCl;
   std::atomic<uint64_t> nTwoWatchedClauses;
   std::atomic<uint64_t> nOneWatchedClauses;
   std::atomic<int64_t> nAllocPrivateCl;
   std::atomic<int64_t> nAllocSharedCl;
   std::atomic<int64_t> nAllocPermanentCl;

   SolverStatistic();

   double getViviImpact() const;

   double percentPropsSpendInVivi(const uint64_t & startProps = 0) const;
};

struct DatabaseStatistic
{
   uint64_t numInitCl;
   uint64_t padder2[7];
   std::atomic<uint64_t> numFreeBuckets;

   DatabaseStatistic()
         : numInitCl(0),
           numFreeBuckets(0)
   {
   }
};

struct GlobalStatistic
{
   bool human;
   bool threadDetails;
   const ClauseDatabase & db;

   SimpleSample<double> sysTime;
   SimpleSample<double> conflictsPerSec;
   SimpleSample<uint64_t> sumConflicts;
   SimpleSample<uint64_t> sumPropagations;
   SimpleSample<uint64_t> sumViviPropagations;
   SimpleSample<int64_t> numPermanent;
   SimpleSample<double> percentPrivate;
   SimpleSample<double> percentShared;
   SimpleSample<double> percentPermanent;
   SimpleSample<double> percentMemUsage;
   SimpleSample<double> mbMemUsage;
   SimpleSample<double> percentBucketUsage;
   SimpleSample<double> avgViviLength;

   MultiSample<uint64_t> numRestarts;
   MultiSample<uint64_t> numReduces;
   MultiSample<uint64_t> numPropagations;
   MultiSample<uint64_t> numViviPropagations;
   MultiSample<uint64_t> numConflicts;
   MultiSample<unsigned> averageLbd;
   MultiSample<uint64_t> numExported;
   MultiSample<uint64_t> numImported;
   MultiSample<uint64_t> numPromoted;
   MultiSample<uint64_t> numPrivateClauses;
   MultiSample<uint64_t> numSharedClauses;
   MultiSample<uint64_t> numTwoWatchedClauses;
   MultiSample<uint64_t> numOneWatchedClauses;
   MultiSample<int64_t> numAllocatedPrivateClauses;
   MultiSample<int64_t> numAllocatedSharedClauses;
   MultiSample<int64_t> numAllocatedPermanentClauses;
   MultiSample<uint64_t> nVivifications;
   MultiSample<double> avgVivificationLength;
   MultiSample<uint64_t> failedVivifycations;

   GlobalStatistic(const ClauseDatabase & db);

   void update(const double & sysTime);

   void printInterval() const;

   void printFinal() const;

};

} /* namespace Concsat */

#endif /* SHARED_STATISTIC_H_ */
