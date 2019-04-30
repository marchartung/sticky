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

#include "shared/Statistic.h"
#include "shared/Heuristic.h"
#include "shared/ClauseDatabase.h"
#include "shared/CoreSolver.h"
#include "glucose/utils/System.h"
#include "glucose/mtl/XAlloc.h"

#include <cinttypes>
#include <sstream>
#include <iostream>
#include <iomanip>

namespace Sticky
{

SolverStatistic::SolverStatistic()
      : medianLbd(std::min(SharingHeuristic().maxSharedLBD, SharingHeuristic().maxVivificationLbd)),
        sumLbd(0),
        nLastReduceConflicts(0),
        nRestarts(1),
        nReduces(0),
        nPropagations(0),
        nViviPropagations(0),
        nDecisions(0),
        nConflicts(0),
        nVivifications(0),
        sumVivificationLength(0),
        sumViviStartLength(0),
        failedVivifycations(0),
        nUnit(0),
        nExportedCl(0),
        nImportedCl(0),
        nPromotedCl(0),
        nPrivateCl(0),
        nSharedCl(0),
        nTwoWatchedClauses(0),
        nOneWatchedClauses(0),
        nAllocPrivateCl(0),
        nAllocSharedCl(0),
        nAllocPermanentCl(0)
{
}

double SolverStatistic::getViviImpact() const
{
   return (static_cast<double>(sumVivificationLength.load()) / static_cast<double>(sumViviStartLength))
         * (static_cast<double>(nVivifications.load()) / static_cast<double>(failedVivifycations.load() + nVivifications.load()));
}


double SolverStatistic::percentPropsSpendInVivi(const uint64_t & startProps) const
{
   uint64_t spend = nPropagations-startProps + nViviPropagations;
   return static_cast<double>(spend)/static_cast<double>(nPropagations);
}

GlobalStatistic::GlobalStatistic(const ClauseDatabase & db)
      : human(db.getBuckets().getHeuristic().printHumanReadable),
        threadDetails(!db.getBuckets().getHeuristic().onlyPrintAggr),
        db(db),
        sysTime("time"),
        conflictsPerSec("confl/s"),
        sumConflicts("conflicts"),
        sumPropagations("props"),
        sumViviPropagations("vProps"),
        numPermanent("permCl"),
        percentPrivate("%priv"),
        percentShared("%shar"),
        percentPermanent("%perm"),
        percentMemUsage("%mem"),
        mbMemUsage("MB"),
        percentBucketUsage("%bucket"),
        avgViviLength("avgViviLength"),

        numRestarts("restarts"),
        numReduces("reduces"),
        numPropagations("props"),
        numViviPropagations("vProps"),
        numConflicts("conflicts"),
        averageLbd("viviLbd"),
        numExported("refsExp"),
        numImported("refsImp"),
        numPromoted("promoted"),
        numPrivateClauses("privCl"),
        numSharedClauses("sharCl"),
        numTwoWatchedClauses("twoWCl"),
        numOneWatchedClauses("oneWCl"),
        numAllocatedPrivateClauses("allocPrivateCl"),
        numAllocatedSharedClauses("allocSharedCl"),
        numAllocatedPermanentClauses("allocPermCl"),
        nVivifications("nVivs"),
        avgVivificationLength("avgLen%"),
        failedVivifycations("fVivs")
{
   static bool printedHeader = false;
   if (!human && !printedHeader)
   {
      auto csvPrinter = makeCSVPrinter(sysTime, sumConflicts, sumPropagations, sumViviPropagations, numAllocatedPrivateClauses, numAllocatedSharedClauses,
                                       numAllocatedPermanentClauses, numTwoWatchedClauses, numOneWatchedClauses, numExported, nVivifications, failedVivifycations, avgViviLength,
                                       mbMemUsage);
      csvPrinter.printHeader();
      printedHeader = true;
   }
}

double getPercent(const uint64_t & a, const uint64_t & b)
{
   if ((a * 1000) / 1000 == 2)
   {
      uint64_t tmp = (a * 1000) / b;
      return static_cast<double>(tmp) / 10;  // 10, because for rounding multipied by 1000 and casted to int, than / 1000 and *100 for percent: 10a = a/1000 * 100
   } else
   {
      return static_cast<double>(a) * 100 / b;
   }
}

void GlobalStatistic::update(const double & sysT)
{
   numRestarts.clear();
   numReduces.clear();
   numPropagations.clear();
   numViviPropagations.clear();
   numConflicts.clear();
   averageLbd.clear();
   numExported.clear();
   numImported.clear();
   numPromoted.clear();
   numPrivateClauses.clear();
   numSharedClauses.clear();
   numTwoWatchedClauses.clear();
   numOneWatchedClauses.clear();
   numAllocatedPrivateClauses.clear();
   numAllocatedSharedClauses.clear();
   numAllocatedPermanentClauses.clear();
   nVivifications.clear();
   avgVivificationLength.clear();
   failedVivifycations.clear();

   for (unsigned i = 0; i < db.getNumSolverThreads(); ++i)
   {
      const SolverStatistic & stat = db.getSolver(i).getStatistic();
      numRestarts.add(stat.nRestarts);
      numReduces.add(stat.nReduces);
      numPropagations.add(stat.nPropagations);
      numViviPropagations.add(stat.nViviPropagations);
      numConflicts.add(stat.nConflicts);
      averageLbd.add(stat.medianLbd);
      numExported.add(stat.nExportedCl);
      numImported.add(stat.nImportedCl);
      numPromoted.add(stat.nPromotedCl);
      numPrivateClauses.add(stat.nPrivateCl);
      numSharedClauses.add(stat.nSharedCl);
      numTwoWatchedClauses.add(stat.nTwoWatchedClauses);
      numOneWatchedClauses.add(stat.nOneWatchedClauses);
      numAllocatedPrivateClauses.add(stat.nAllocPrivateCl);
      numAllocatedSharedClauses.add(stat.nAllocSharedCl);
      numAllocatedPermanentClauses.add(stat.nAllocPermanentCl);

      nVivifications.add(stat.nVivifications);
      avgVivificationLength.add(100.0 * static_cast<double>(stat.sumVivificationLength) / static_cast<double>(std::max(1lu, stat.sumViviStartLength.load())));
      failedVivifycations.add(stat.failedVivifycations);
   }
   uint64_t numAllocCl = numAllocatedPrivateClauses.sum() + numAllocatedSharedClauses.sum() + numAllocatedPermanentClauses.sum();

   sysTime = sysT;
   sumConflicts = numConflicts.sum();
   conflictsPerSec = numConflicts.sum() / sysT;
   sumPropagations = numPropagations.sum();
   sumViviPropagations = numViviPropagations.sum();

   numPermanent = numAllocatedPermanentClauses.sum();
   percentPrivate = getPercent(numAllocatedPrivateClauses.sum(), numAllocCl);
   percentShared = getPercent(numAllocatedSharedClauses.sum(), numAllocCl);
   percentPermanent = getPercent(numAllocatedPermanentClauses.sum(), numAllocCl);
   avgViviLength = avgVivificationLength.average();
   if (db.getNumSolverThreads() == db.getNumRunningThreads())  // hack
   {
      mbMemUsage = Glucose::ByteCounter::getNumMBytes() - db.getBuckets().getNumMBFreeBuckets();  //percentMemUsage.value() * (db.getBuckets().getHeuristic().maxAllocBytes)/(1024*2024*100);
      percentMemUsage = 100.0 * mbMemUsage.value() / (db.getBuckets().getHeuristic().maxAllocBytes / (1024 * 1024));
      percentBucketUsage = (db.getBuckets().getNumUsedMB() / db.getBuckets().getNumMBBuckets()) * 100.0;
   }
}

void GlobalStatistic::printInterval() const
{
   if (human)
   {
      auto mPrinter = makeMultiSamplePrinter(numRestarts, numReduces, numConflicts, numPropagations, numViviPropagations, numExported, numPromoted, numPrivateClauses,
                                             numSharedClauses, numTwoWatchedClauses, numOneWatchedClauses, nVivifications, failedVivifycations, avgVivificationLength, averageLbd);
      mPrinter.printLine();
      mPrinter.printNames();
      mPrinter.printLine();
      if (threadDetails)
      {
         for (unsigned i = 0; i < db.getNumSolverThreads(); ++i)
            mPrinter.printValues(i);
      } else
      {
         mPrinter.printAverageValues();
         mPrinter.printMinValues();
         mPrinter.printMaxValues();
      }
      mPrinter.printLine();

      auto sPrinter = makeSimpleSamplePrinter(sysTime, conflictsPerSec, sumConflicts, sumPropagations, sumViviPropagations, numPermanent, percentPrivate, percentShared,
                                              percentPermanent, percentMemUsage, mbMemUsage, percentBucketUsage);
      sPrinter.printLine();
      sPrinter.printNames();
      sPrinter.printLine();
      sPrinter.printValues();
      sPrinter.printLine();
   } else
   {
      auto csvPrinter = makeCSVPrinter(sysTime, sumConflicts, sumPropagations, sumViviPropagations, numAllocatedPrivateClauses, numAllocatedSharedClauses,
                                       numAllocatedPermanentClauses, numTwoWatchedClauses, numOneWatchedClauses, numExported, nVivifications, failedVivifycations, avgViviLength,
                                       mbMemUsage);
      csvPrinter.printValues();
   }
}

void GlobalStatistic::printFinal() const
{
   printInterval();
}

}
