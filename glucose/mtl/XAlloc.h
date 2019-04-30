/****************************************************************************************[XAlloc.h]
 Copyright (c) 2009-2010, Niklas Sorensson

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

#ifndef Glucose_XAlloc_h
#define Glucose_XAlloc_h

#include <errno.h>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <atomic>
#include <cassert>

namespace Glucose
{

//=================================================================================================
// Simple layer on top of malloc/realloc to catch out-of-memory situtaions and provide some typing:

class ByteCounter
{
 public:

   static void init();

   static void add(const int64_t & numBytes);
   static void sub(const int64_t & numBytes);

   static int64_t getNumMBytes();

 private:
   static std::atomic<unsigned> id;
   static thread_local int64_t allocatedByteCounter;
   static int64_t* byteAllocated[500];
};

inline void ByteCounter::init()
{
   unsigned threadId = id.fetch_add(1);
   byteAllocated[threadId] = &allocatedByteCounter;
}

inline int64_t ByteCounter::getNumMBytes()
{
   int64_t res = 0;
   for(unsigned i=0;i<id;++i)
      res += *(byteAllocated[i]);
   return static_cast<double>(res)/(1024.0*1024.0);
}
inline void ByteCounter::add(const int64_t & numBytes)
{
   allocatedByteCounter += numBytes;
}

inline void ByteCounter::sub(const int64_t & numBytes)
{
   allocatedByteCounter -= numBytes;
}

class OutOfMemoryException
{
};
static inline void* xrealloc(void *ptr, size_t size)
{
   void* mem = NULL;
   if (ptr != NULL)
   {
      ByteCounter::sub(reinterpret_cast<uint64_t*>(ptr)[-1]);
      mem = realloc(reinterpret_cast<void*>(&(reinterpret_cast<uint64_t*>(ptr)[-1])), size + 8);
   }
   else
   {
      mem = realloc(ptr, size + 8);
   }

   if(mem == NULL && errno == ENOMEM)
   {
      std::cout << "Error: Memory allocation failed";
      throw OutOfMemoryException();
   }
   ByteCounter::add(size);
   reinterpret_cast<uint64_t*>(mem)[0] = size;

   return reinterpret_cast<void*>(&(reinterpret_cast<uint64_t*>(mem)[1]));
}

static inline void xfree(void *ptr)
{
   ByteCounter::sub(reinterpret_cast<uint64_t*>(ptr)[-1]);
   free(&(reinterpret_cast<uint64_t*>(ptr)[-1]));
}

//=================================================================================================
}
#endif
