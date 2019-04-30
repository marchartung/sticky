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

#include "utils/System.h"
#include "utils/ParseUtils.h"
#include "utils/Options.h"
#include "core/Dimacs.h"
#include "core/SolverTypes.h"

#include "SharedSolver.h"
#include <errno.h>

#include <signal.h>
#include <zlib.h>
#include <iostream>

using namespace Sticky;

static SharedSolver* pmsolver = nullptr;

static void SIGINT_exit(int signum)
{
   printf("\n");
   printf("*** INTERRUPTED ***\n");
   pmsolver->printFinalStats();
   printf("\n");
   printf("*** INTERRUPTED ***\n");

   _exit(signum);
}

//=================================================================================================
// Main:

int main(int argc, char** argv)
{
   Glucose::ByteCounter::init();
   try
   {
      setUsageHelp("c USAGE: %s [options] <input-file> <result-output-file>\n\n  where input may be either in plain or gzipped DIMACS.\n");

      parseOptions(argc, argv, true);

      SharedSolver msolver;
      pmsolver = &msolver;

      // Use signal handlers that forcibly quit until the solver will be able to respond to
      // interrupts:
      signal(SIGINT, SIGINT_exit);
      signal(SIGXCPU, SIGINT_exit);

      if (argc == 1)
         printf("c Reading from standard input... Use '--help' for help.\n");

      gzFile in = (argc == 1) ? gzdopen(0, "rb") : gzopen(argv[1], "rb");
      if (in == NULL)
         printf("c ERROR! Could not open file: %s\n", argc == 1 ? "<stdin>" : argv[1]), exit(1);
      parse_DIMACS(in, msolver);
      gzclose(in);

      int ret2 = msolver.simplify();
      if (ret2)
         msolver.eliminate();

      lbool ret;
      if (!ret2 || !msolver.okay())
      {
         printf("c Solved by unit propagation\n");
         ret = l_False;
      } else
      {
         ret = msolver.solve();
      }

      printf(ret == l_True ? "s SATISFIABLE\n" : ret == l_False ? "s UNSATISFIABLE\n" : "s INDETERMINATE\n");

      if (msolver.getShowModel() && ret == l_True)
      {
         const vec<lbool> & model = msolver.getModel();
         printf("v ");
         for (int i = 0; i < model.size(); i++)
            if (model[i] != l_Undef)
               printf("%s%s%d", (i == 0) ? "" : " ", (model[i] == l_True) ? "" : "-", i + 1);
         printf(" 0\n");
      }

#ifdef NDEBUG
      exit(ret == l_True ? 10 : ret == l_False ? 20 : 0);     // (faster than "return", which will invoke the destructor for 'Solver')
#else
      return (ret == l_True ? 10 : ret == l_False ? 20 : 0);
#endif
   } catch (OutOfMemoryException&)
   {
      printf("c Error: Out of memory. Please increase max memory\n");
      printf("c ===================================================================================================\n");
      printf("INDETERMINATE\n");
      exit(0);
   } catch (BufferOverflowException&)
   {
      printf("c Error: Buffer overflow. Please increase buffer size for clause exchange\n");
      printf("c ===================================================================================================\n");
      printf("INDETERMINATE\n");
      exit(0);
   }
}
