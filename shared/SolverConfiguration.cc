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

#include "shared/CoreSolver.h"
#include "shared/SolverConfiguration.h"

namespace Sticky
{

void SolverConfiguration::configure(CoreSolver & s, unsigned solverId, unsigned numSolvers)
{
   SolverHeuristic & h = s.getHeuristic();
   unsigned setting = solverId % 8;
   double solverFactor = static_cast<double>(solverId + 1) / numSolvers;
   double noisevar_decay = 0.005 * (solverId % 8);
   int noiseReduceDB = 50 * solverId;

   switch (setting)
   {
      case (1):
         h.chanseok = true;
         h.var_decay = 0.94;
         h.max_var_decay = 0.96;
         break;
      case (2):
         h.luby = true;
         h.var_decay = 0.90;
         h.max_var_decay = 0.97;
         break;
      case (3):

         h.chanseok = true;
         h.var_decay = 0.85;
         h.max_var_decay = 0.93;
         break;
      case (4):
         h.luby = true;
         h.var_decay = 0.94;
         h.max_var_decay = 0.96;
         break;
      case (5):
         h.chanseok = true;
         h.var_decay = 0.93;
         h.max_var_decay = 0.96;
         break;
      case (6):

         h.chanseok = true;
         h.luby = true;
         h.var_decay = 0.85;
         h.max_var_decay = 0.94;
         break;

      default:
         ;
   }
   h.var_decay += noisevar_decay;
   h.firstReduceDb *= 0.25 + (solverFactor * 1.75);
   h.firstReduceDb += noiseReduceDB;
   h.incReduceDB *= 0.25 + (solverFactor * 1.75);

   //std::cout << "dc: " << h.var_decay << " fR: " << h.firstReduceDb << " iR: " << h.incReduceDB << " sLQ: " << h.sizeLBDQueue << " K: " << h.K << "\n";
   //std::cout.flush();
}
}
