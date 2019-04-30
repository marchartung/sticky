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
#ifndef UTILS_LOCKRINGBUFFER_H_
#define UTILS_LOCKRINGBUFFER_H_

#include "shared/SharedTypes.h"
#include "glucose/mtl/XAlloc.h"

#include <algorithm>
#include <memory>
#include <pthread.h>

namespace Sticky
{

template<typename T>
class LockRingBuffer
{
 public:
   typedef int size_type;

   LockRingBuffer(const size_type & maxSize)
         : pos(0),
           mem(maxSize)
   {
      pthread_mutex_init(&mtx, 0);
   }

   ~LockRingBuffer()
   {
      pos = 0;
      pthread_mutex_destroy(&mtx);
   }

   void push(const T & val)
   {
      pthread_mutex_lock(&mtx);
      mem[pos] = val;
      next();
      pthread_mutex_unlock(&mtx);
   }

   size_type get(const size_type & inPos, vec<T> & out)
   {
      pthread_mutex_lock(&mtx);
      size_type readPos = inPos, ringBufferEnd = pos;
      if (readPos != ringBufferEnd)
      {
         if (readPos > ringBufferEnd)
         {
            for (; readPos < mem.size(); ++readPos)
               out.push(mem[readPos]);
            readPos = 0;
         }
         for (; readPos < ringBufferEnd; ++readPos)
            out.push(mem[readPos]);
      }
      pthread_mutex_unlock(&mtx);
      assert(readPos == ringBufferEnd);
      return (ringBufferEnd == mem.size()) ? 0 : ringBufferEnd;
   }

   size_type get(const size_type & fromPos, const size_type & toPos, vec<T> & out)
   {
      pthread_mutex_lock(&mtx);
      size_type readPos = fromPos, ringBufferEnd = toPos, realPos = pos;
      //TODO implement a fix, which allows overflow
      if (readPos != ringBufferEnd)
      {
         if (readPos > ringBufferEnd)
         {
            for (; readPos < mem.size(); ++readPos)
               out.push(mem[readPos]);
            readPos = 0;
         }
         for (; readPos < ringBufferEnd; ++readPos)
            out.push(mem[readPos]);
      }
      pthread_mutex_unlock(&mtx);
      return (readPos == mem.size()) ? 0 : readPos;
   }

   size_type getNumNew(const size_type & inPos) const
   {
      size_type tmpPos = pos;
      return (inPos < tmpPos) ? tmpPos-inPos : mem.size()-inPos+tmpPos;
   }

 protected:
   std::atomic<size_type> pos;
   pthread_mutex_t mtx;
   vec<T> mem;

   // only call when mtx is locked by the calling thread
   void next()
   {
      if (pos + 1 == mem.size())
         pos = 0;
      else
         ++pos;
   }

};

} /* namespace Glucose */

#endif /* UTILS_LOCKRINGBUFFER_H_ */
