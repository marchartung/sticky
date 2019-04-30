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

#ifndef SHARED_TYPES_H_h
#define SHARED_TYPES_H_h

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <pthread.h>
#include <atomic>
#include <limits>
#include <memory>
#include <iostream>

#include "glucose/core/SolverTypes.h"
#include "glucose/core/Solver.h"
#include "glucose/core/Dimacs.h"
#include "glucose/core/BoundedQueue.h"
#include "glucose/mtl/Vec.h"
#include "glucose/mtl/XAlloc.h"
#include "glucose/mtl/Heap.h"
#include "glucose/mtl/Sort.h"
#include "glucose/utils/Options.h"

namespace Sticky
{

typedef int32_t BaseAllocatorType;

// define what we need from Glucose

// Types
using Lit = Glucose::Lit;
using CRef = Glucose::CRef;
using lbool = Glucose::lbool;
using Var = Glucose::Var;

using BoolOption = Glucose::BoolOption;
using IntOption = Glucose::IntOption;
using DoubleOption = Glucose::DoubleOption;
using DoubleRange = Glucose::DoubleRange;
using IntRange = Glucose::IntRange;

template<class T>
using vec = Glucose::vec<T>;
template<class Comp>
using Heap = Glucose::Heap<Comp>;
template <class T>
using bqueue = Glucose::bqueue<T>;
using VarOrderLt = Glucose::VarOrderLt;
using OutOfMemoryException = Glucose::OutOfMemoryException;

class BufferOverflowException{};

// Functions and Constants:
using Glucose::lit_Undef;
using Glucose::mkLit;
using Glucose::mkLitFromInt;
using Glucose::CRef_Undef;
using Glucose::CRef_Del;

using Glucose::setUsageHelp;
using Glucose::parseOptions;
using Glucose::parse_DIMACS;

using Glucose::sort;
using Glucose::drand;
using Glucose::irand;

}

#endif
