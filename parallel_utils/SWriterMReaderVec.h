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
#ifndef PARALLEL_UTILS_SWRITERMREADERVEC_H_
#define PARALLEL_UTILS_SWRITERMREADERVEC_H_

namespace Sticky
{
#include "glucose/mtl/Vec.h"

template<typename T>
class RefVec
{
 public:
   RefVec()
         : sz(0),
           elems(nullptr)
   {
   }

   RefVec(RefVec<T> && in)
         : sz(in.sz),
           elems(in.elems)
   {
   }

   RefVec(const RefVec<T> & in)
         : sz(in.sz),
           elems(in.elems)
   {
   }

   RefVec(const Glucose::vec<T> & in)
         : sz(in.size()),
           elems(in.data())
   {
   }

   RefVec(const Glucose::vec<T> & elems, const int startIdx, const int endIdx)
         : sz(endIdx - startIdx - ((elems.size() < endIdx) ? endIdx - elems.size() : 0)),
           elems(elems.data() + startIdx)
   {
      assert(startIdx >= 0 && startIdx < elems.size());
      assert(startIdx <= endIdx);
   }

   RefVec<T> & operator=(const RefVec<T> & in)
   {
      sz = in.sz;
      elems = in.elems;
      return *this;
   }

   const T& operator[](const int & idx) const
   {
      return elems[idx];
   }

   int size() const
   {
      return sz;
   }

 private:
   int sz;
   const T * elems;
};

template<typename T>
class SWriterMReaderVec
{
 public:

   SWriterMReaderVec()
         : numDistributed(0),
           numToProcess(0),
           numProcessed(0),
           elems()
   {
   }

   ~SWriterMReaderVec()
   {
   }

   SWriterMReaderVec<T> & operator=(Glucose::vec<T> && in)
   {
      assert(numDistributed == -1);
      elems = std::move(in);
      return *this;
   }

   void clear()
   {
      bool check = false;
      while (!check)
         check = tryWriteLock();
      releaseWriteLock();
   }

   bool tryWriteLock()
   {
      bool res = false;
      if (numProcessed == numToProcess)
      {
         int expected = numDistributed;
         if (expected != -1 && numDistributed.compare_exchange_strong(expected, -1))
         {
            res = true;
            numToProcess = 0;
            elems.clear();
         }
      }
      return res;
   }

   void writeLock()
   {
      bool res = false;
      while (!res)
         res = tryWriteLock();
   }

   void push(const T & elem)
   {
      assert(numDistributed == -1);
      elems.push(elem);
   }

   void push(const Glucose::vec<T> & in)
   {
      for (int i = 0; i < in.size(); ++i)
         elems.push(in[i]);
   }

   void releaseWriteLock()
   {
      assert(numDistributed == -1);
      setBack();
   }

   void setBack()
   {
      numProcessed = 0;
      numToProcess = elems.size();
      numDistributed = 0;
   }

   RefVec<T> getElements()
   {
      if (numDistributed >= 0 && numProcessed < numToProcess)
      {
         int startIdx = numDistributed.fetch_add(chunkSize);
         if (startIdx < numToProcess)
         {
            assert(numToProcess == elems.size());
            return RefVec<T>(elems, startIdx, startIdx + chunkSize);
         } else
            return RefVec<T>();
      } else
         return RefVec<T>();
   }

   bool notifyProccessed(const RefVec<T> & in)
   {
      bool res = false;
      if (in.size() > 0)
      {
         int fetched = numProcessed.fetch_add(in.size());
         assert(numProcessed <= numToProcess);
         res = fetched + in.size() == numToProcess;
      }
      return res;
   }

   int size() const
   {
      return numToProcess.load();
   }

   vec<T> & getVec()
   {
      return elems;
   }

 private:
   static const int chunkSize = 50;
   std::atomic<int> numDistributed;
   std::atomic<int> numToProcess;
   std::atomic<int> numProcessed;
   Glucose::vec<T> elems;

};
}

#endif /* PARALLEL_UTILS_SWRITERMREADERVEC_H_ */
