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

#ifndef SHARED_PROPAGATERESULT_H_
#define SHARED_PROPAGATERESULT_H_

#include <limits>
#include <cassert>

#include "shared/SharedTypes.h"

namespace Sticky
{
enum class PropagateResultType
{
   CONFLICT,
   PROPAGATION,
   NOTHING
};

enum class WatcherType
{
   UNIT = 0,
   ONE = 1,
   TWO = 2,
   BINARY = 3,
};

class PropagateResult;

#define WTYPE_BITS 2
#define NUM_WATCHER_POS_BITS sizeof(int) * 8 - WTYPE_BITS
#define MAX_WATCHER_POS_NUM (0xFFFFFFFF >> WTYPE_BITS)

class VarSet
{
      unsigned wType : WTYPE_BITS;
      unsigned watcherPos : NUM_WATCHER_POS_BITS;
      int listPos;
    public:

      static VarSet decision();

      VarSet();
      VarSet(VarSet && in);
      VarSet(const VarSet & in);

      VarSet(WatcherType wt, const int listPos, const int watcherPos);

      VarSet & operator=(const VarSet & vs);
      VarSet & operator=(VarSet && vs);

      bool operator!=(const VarSet & vs) const
      {
         return wType != vs.wType || watcherPos != vs.watcherPos || listPos != vs.listPos;
      }

      template<typename WType>
      VarSet(const WType & w, const Lit & lit, const unsigned & pos);

      template<typename WType>
      VarSet(const WType & w,const int & listPos, const unsigned & pos);

      template<typename WType>
      void set(const WType & w, const int & listPos, const unsigned & pos);
      void setUnit();
      void setPropagated(const WatcherType t);

      WatcherType getWatcherType() const;
      int getWatcherPos() const;
      int getListPos() const;
      Lit getWatchedLit() const;

      bool isPropagated() const;
      bool isValid() const;

      void set(const PropagateResult & result);
      void reset();

      static int npos();

      static VarSet create(const WatcherType wt, const int listPos, const int wPos)
      {
         VarSet res;
         res.wType = static_cast<unsigned>(wt);
         res.listPos = listPos;
         res.watcherPos = wPos;
         return res;
      }
};

inline int VarSet::npos()
{
   return std::numeric_limits<int>::max();
}

class PropagateResult
{
      PropagateResultType pType;
      VarSet p;

    public:
      PropagateResult();
      PropagateResult(const PropagateResult & in);
      PropagateResult(PropagateResult && in);

      PropagateResult & operator=(const PropagateResult & in);
      PropagateResult & operator=(PropagateResult && in);

      bool isConflict() const;
      bool isPropagation() const;
      const PropagateResultType & getResultType() const;
      const int & getWatcherPos() const;

      VarSet & getVarSet();
      const VarSet & getVarSet() const;

      template<typename WatcherIterator>
      void set(const WatcherIterator & wit, const bool isConflict);
      template<typename WType>
      void set(const WType & w, const int listPos, const int wpos, const bool isConflict);
      void setUnit();
      void setConflict(const bool & b);
};


inline VarSet & VarSet::operator=(const VarSet & vs)
{
   static_assert(sizeof(VarSet) == sizeof(uint64_t), "VarSet alignment invalid");
   *reinterpret_cast<uint64_t*>(this) = *reinterpret_cast<const uint64_t*>(&vs);
   assert(vs.wType == wType);
   assert(vs.listPos == listPos);
   assert(vs.watcherPos == watcherPos);
   //this->wType = vs.wType;
   //this->watcherPos = vs.watcherPos;
   //this->listPos = vs.listPos;
   return *this;
}
inline VarSet & VarSet::operator=(VarSet && vs)
{
   this->wType = vs.wType;
   this->watcherPos = vs.watcherPos;
   this->listPos = vs.listPos;
   return *this;
}

inline WatcherType VarSet::getWatcherType() const
{
   return static_cast<WatcherType>(wType);
}
inline int VarSet::getWatcherPos() const
{
   return watcherPos;
}
inline bool VarSet::isPropagated() const
{
   return getWatcherType() != WatcherType::UNIT;
}
inline void VarSet::set(const PropagateResult & result)
{
   *this = result.getVarSet();
}
inline void VarSet::setUnit()
{
   wType = (static_cast<unsigned>(WatcherType::UNIT));
}

inline void VarSet::setPropagated(const WatcherType t)
{
   wType = static_cast<unsigned>(t);
}

inline void VarSet::reset()
{
   setUnit();
}

inline void PropagateResult::setConflict(const bool & b)
{
      pType = (b) ? PropagateResultType::CONFLICT : PropagateResultType::NOTHING;
}

inline bool PropagateResult::isConflict() const
{
   return pType == PropagateResultType::CONFLICT;
}

inline bool PropagateResult::isPropagation() const
{
   return pType == PropagateResultType::PROPAGATION;
}
inline const PropagateResultType & PropagateResult::getResultType() const
{
   return pType;
}
inline Lit VarSet::getWatchedLit() const
{
   Lit tmp;
   tmp.x = listPos;
   return ~tmp;
}
inline int VarSet::getListPos() const
{
   return listPos;
}
inline const VarSet & PropagateResult::getVarSet() const
{
   return p;
}
inline  VarSet & PropagateResult::getVarSet()
{
   return p;
}


template<typename WType>
VarSet::VarSet(const WType & w, const Lit & lit, const unsigned & pos)
      : wType(static_cast<unsigned>(WType::getWatcherType())),
        watcherPos(pos),
        listPos((lit).x)
{
   assert(pos <= MAX_WATCHER_POS_NUM);
}

template<typename WType>
VarSet::VarSet(const WType & w, const int & listPos, const unsigned & pos)
      : wType(static_cast<unsigned>(WType::getWatcherType())),
        watcherPos(pos),
        listPos(listPos)
{
   assert(pos <= MAX_WATCHER_POS_NUM);
}
template<typename WType>
void VarSet::set(const WType & w, const int & lp, const unsigned & p)
{
   assert(p <= MAX_WATCHER_POS_NUM);
   wType = static_cast<unsigned>(WType::getWatcherType());
   watcherPos = p;
   listPos = lp;
}
template<typename WatcherIterator>
void PropagateResult::set(const WatcherIterator & wit, const bool isConflict)
{
   pType = ((isConflict) ? PropagateResultType::CONFLICT : PropagateResultType::NOTHING);
   p.set(*wit, wit.getListPos(), wit.getIndex());
}
template<typename WType>
void PropagateResult::set(const WType & w, const int listPos, const int wpos, const bool isConflict)
{
   pType = ((isConflict) ? PropagateResultType::CONFLICT : PropagateResultType::NOTHING);
   p.set(w, listPos, wpos);
}

struct PropState
{
      VarSet reason;
      int level;
      PropState();
      void set(const VarSet r, const int l);
      void setUnitClause();
};

}

#endif /* SHARED_PROPAGATERESULT_H_ */
