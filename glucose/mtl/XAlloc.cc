/*
 * XAlloc.cc
 *
 *  Created on: 14.08.2018
 *      Author: hartung
 */

#include <glucose/mtl/XAlloc.h>

namespace Glucose
{

thread_local int64_t ByteCounter::allocatedByteCounter = 0;
std::atomic<unsigned> ByteCounter::id(0);
int64_t* ByteCounter::byteAllocated[500];

}

