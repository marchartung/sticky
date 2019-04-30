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

#include "parallel_utils/CPUBind.h"
#include "shared/SharedTypes.h"
#include <thread>
#include <sstream>
#include <cassert>
#include <sched.h>

namespace Sticky
{

const char* _machine = "Machine";
IntOption opt_nhyperthreads(_machine, "nHyper", "Number of Hyperthreads enabled by the system", 2);
IntOption opt_nNumaNodes(_machine, "nNuma", "Number of numa nodes of the machine", 2);
BoolOption opt_use_pinning(_machine, "usePinning", "Enables or disables pinning of solver", true);

int getNumCores()
{
   return std::thread::hardware_concurrency();
}

void CPUBind::bindThread(const unsigned & tid)
{
#ifndef MACOS
   int mapping = getMapping(tid);
   bindThread(tid, mapping);
#endif
}

/* Core structure
 *
 * [num1]: 1:id1,core1h1 3:id2,core2h1 5:id5,core1h2 7:id6,core2h2
 * [num2]: 2:id3,core3h1 4:id4,core4h1 6:id7,core3h2 8:id8,core4h2
 * [num3]: 3:
 *
 * [vnuma1] :  0  1  2  3 [n1 h1]
 * [vnuma2] :  4  5  6  7 [n2 h1]
 * [vnuma3] :  8  9 10 11 [n1 h2]
 * [vnuma4] : 12 13 14 15 [n2 h2]
 */

int CPUBind::getMapping(const unsigned & tid)
{
   const int numVcores = getNumCores(), numHyperThreads = opt_nhyperthreads, numNumaNodes = opt_nNumaNodes;

   const int numCores = numVcores / numHyperThreads;
   const int numCorePerNuma = numCores / numNumaNodes;
   int simpleTid = tid % numVcores;
   int coreTid = simpleTid % numCores;

   int numaId = coreTid % numNumaNodes;
   int coreId = coreTid / numNumaNodes;
   int hyperId = simpleTid / numCores;
   return coreId + numaId * numCorePerNuma + hyperId * numCores;
}

std::string CPUBind::getString(const pthread_t & pid)
{
   std::stringstream ss("");
   cpu_set_t cpuset;
   pthread_t thread = pthread_self();
   int j, s = pthread_getaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
   if (s != 0)
      assert(false);
   for (j = 0; j < CPU_SETSIZE; j++)
      if (CPU_ISSET(j, &cpuset))
         ss << j << " ";
   return ss.str();
}

void CPUBind::print(const pthread_t * pids, const unsigned & n)
{
#ifndef MACOS
   for (unsigned i = 0; i < n; ++i)
      std::cout << "thread " << i << ": cpus " << getString(pids[i]) << std::endl;
   std::cout << "main thread: cpus " << getString(pthread_self()) << std::endl;
#endif
}

void CPUBind::bindThread(const unsigned & tid, const int& coreId)
{
   if (opt_use_pinning)
   {
#ifndef MACOS
      cpu_set_t cpuset;
      CPU_ZERO(&cpuset);       //clears the cpuset
      CPU_SET(coreId, &cpuset);  //set CPU 2 on cpuset
      sched_setaffinity(0, sizeof(cpuset), &cpuset);
#endif
   }
}

} /* namespace Concusat */
