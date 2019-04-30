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

#ifndef SHARED_CLAUSEWATCHER_H_
#define SHARED_CLAUSEWATCHER_H_

#include "shared/SharedTypes.h"
#include "shared/ClauseTypes.h"
#include "shared/PropagateResult.h"
#include "shared/LiteralSetting.h"
#include <atomic>

namespace Sticky
{

class ClauseBucketArray;
class CoreSolver;

struct BinaryWatcher
{

   inline static WatcherType getWatcherType()
   {
      return WatcherType::BINARY;
   }

   BinaryWatcher();

   BinaryWatcher(const CRef & cr, const Lit & p);

   inline const Lit & getBlocker() const
   {
      return blocker;
   }

   inline const CRef & getCRef() const
   {
      return cref;
   }

   inline void setCRef(const CRef & cr)
   {
      cref = cr;
   }

 private:
   Lit blocker;
   CRef cref;
};


struct Watcher
{
   static WatcherType getWatcherType()
   {
      return WatcherType::TWO;
   }

   Watcher(const CRef & cr, const Lit & p, const int & watchRef);

   Watcher(const CRef & cr, const Lit & p, const int & watchRef, const unsigned & ldb, const unsigned & activity);

   Watcher(const Watcher & w);

   Watcher(Watcher && w);

   inline bool operator==(const Watcher& w) const
   {
      return cref == w.cref;
   }
   inline bool operator!=(const Watcher& w) const
   {
      return !(*this == w);
   }

   inline void setOther(const Lit & p, const unsigned & pos)
   {
      blocker = p;
      blockerWatchRef = pos;
   }
   inline void setPos(const unsigned & pos)
   {
      blockerWatchRef = pos;
   }

   inline const CRef & getCRef() const
   {
      return cref;
   }
   inline void setCRef(const CRef & cr)
   {
      cref = cr;
   }

   inline void setLbd(const unsigned & l)
   {
      assert(isHeader());
      lbd = (l < MaxWatcherLBD) ? l : MaxWatcherLBD;
   }

   inline unsigned getLbd() const
   {
      return lbd;
   }

   inline void markAsRemoved()
   {
      assert(!isHeader());
//      if(cref == 592428)
//         std::cout << "aohoh\n";
      activity = ActivityStateDeleted;
   }

   inline bool isMarkedAsRemoved() const
   {
      assert(!isHeader());
      return activity == ActivityStateDeleted;
   }

   inline void setProtected(const bool isProt)
   {
      assert(!isHeader());
      _isProtected = isProt;
   }

   inline bool isProtected() const
   {
      assert(!isHeader());
      return _isProtected;
   }

   inline void increaseActivity()
   {
      assert(isHeader());
      if (activity < ActivityStateValid)
         ++activity;
   }

   inline void decreaseActivity()
   {
      assert(isHeader());
      activity /= 2;
   }

   inline Lit getBlocker() const
   {
      return blocker;
   }

   inline void setBlocker(const Lit & p, const int & watchRef)
   {
      blocker = p;
      blockerWatchRef = watchRef;
   }

   inline int getBlockerRef() const
   {
      return blockerWatchRef;
   }

   inline bool isHeader() const
   {
      return lbd != MaxWatcherLBD;
   }

   inline unsigned getActivity() const
   {
      assert(isHeader());
      return activity;
   }

   inline bool isImported() const
   {
      assert(isHeader());
      return _isImported;
   }

   inline void setImported(const bool b)
   {
      _isImported = b;
   }


   inline Watcher & operator=(const Watcher & in)
   {
      static_assert(sizeof(Watcher) == 2*sizeof(uint64_t), "Miss-aligned Watcher");
      *reinterpret_cast<uint64_t*>(&activity) = *reinterpret_cast<const uint64_t *>(&in.activity);
      *reinterpret_cast<uint64_t*>(&blocker) = *reinterpret_cast<const uint64_t *>(&in.blocker);

      assert(cref == in.cref);
      assert(blocker == in.blocker);
      assert(activity == in.activity);
      assert(_isProtected == in._isProtected);
      assert(_isImported == in._isImported);
      assert(lbd == in.lbd);
      assert(blockerWatchRef == in.blockerWatchRef);
      return *this;
   }

   inline Watcher & operator=(Watcher && in)
   {
      cref = in.cref;
      blocker = in.blocker;
      activity = in.activity;
      _isProtected = in._isProtected;
      _isImported = in._isImported;
      lbd = in.lbd;
      blockerWatchRef = in.blockerWatchRef;
      return *this;
   }

 private:
   static constexpr uint16_t ActivityStateDeleted = std::numeric_limits<uint16_t>::max();
   static constexpr uint16_t ActivityStateValid = std::numeric_limits<uint16_t>::max()-1;
   static constexpr uint16_t MaxWatcherLBD = std::numeric_limits<uint16_t>::max()/2;
   uint16_t activity;
   uint16_t _isProtected : 1;
   uint16_t _isImported : 1;
   uint16_t lbd : sizeof(uint16_t)*8-2;
   CRef cref;
   Lit blocker;
   unsigned blockerWatchRef;

};

struct OneWatcher
{

   inline static WatcherType getWatcherType()
   {
      return WatcherType::ONE;
   }

   OneWatcher(const CRef & cr, const Lit & p)
         : cref(cr),
           blocker(p)
   {
   }

   inline const Lit & getBlocker() const
   {
      return blocker;
   }

   inline const CRef & getCRef() const
   {
      return cref;
   }
   inline void setCRef(const CRef & cr)
   {
      cref = cr;
   }

   inline void setBlocker(const Lit & l)
   {
      blocker = l;
   }

   inline void markAsRemoved()
   {
      blocker = lit_Undef;
   }

   inline bool isMarkedAsRemoved() const
   {
      return blocker == lit_Undef;
   }

 private:
   CRef cref;
   Lit blocker;

};


class TwoWatcherLists
{
 public:
   typedef Watcher WType;
   typedef vec<WType> ListType;

   TwoWatcherLists(ClauseBucketArray & cba, const unsigned & numLits);
   int size() const;

   vec<CRef> getAllCRefs() const;
   // Needed for WatcherListReference:
   ListType & getWatcher(const Lit & p);
   const ListType & getWatcher(const Lit & p) const;
   ListType & getWatcher(const int & pos);
   const ListType & getWatcher(const int & pos) const;
   void markAsRemoved(const VarSet & vs);
   void removeMarkedClauses(CoreSolver & s);
   void removeMarkedClauses(CoreSolver & s, vec<CRef> & permRefs);
   void moveWatcher(const int listPos, const int wPos, const Lit & to);
   void detach(CoreSolver & s, const VarSet & vs);
   void detach(CoreSolver & s, const int listPos, const int wPos);
   VarSet replace(CoreSolver & s, const int listPos, const int wPos, const CRef & cref);
   void changeCRef(CoreSolver & s, const VarSet & vs, const CRef & cref);
   void changeCRef(CoreSolver & s, const int listPos, const int wPos, const CRef & cref);
   inline int getIndex(const Lit & l) const
   {
      return (l).x;
   }

   VarSet getVarSet(const WType & w) const;
   WType & getWatcher(const VarSet & vs);
   WType & getStateWatcher(const VarSet & vs);
   const WType & getWatcher(const VarSet & vs) const;
   const WType & getHeaderWatcher(const VarSet & vs) const;
   WType & getHeaderWatcher(const VarSet & vs);
   const WType & getWatcher(const int lPos, const int wPos) const;
   WType & getWatcher(const int lPos, const int wPos);
   VarSet findWatcher(const CRef cref) const;

   const CRef & getCRef(const VarSet & vs) const;

   PropagateResult attach(const CRef & cref, CoreSolver & s, const unsigned & lbd, const unsigned & activity = 0);
   PropagateResult attachLowestLitNumbers(const CRef & cref, CoreSolver & s, const unsigned & lbd, const unsigned & activity);
   VarSet plainAttach(CoreSolver & s, const CRef & cref, const Lit & w1, const Lit & w2, const unsigned & lbd, const unsigned & activity = 0);
   VarSet attachOnFirst(const CRef & cref, CoreSolver & s, const unsigned & lbd);
   VarSet attachOneWatched(const OneWatcher & ow, CoreSolver & s, const Lit & p, const unsigned & lbd);

   WType & getOtherWatcher(const Watcher & w);
   const WType & getOtherWatcher(const Watcher & w) const;
   WType & getOtherWatcher(const int lPos, const int wPos);
   const WType & getOtherWatcher(const int lPos, const int wPos) const;
   WType & getOtherWatcher(const VarSet & w);
   const WType & getOtherWatcher(const VarSet & w) const;


   bool isValidWatcher(const Watcher & w, const bool allowInvalidClause = false) const;
   void assertCorrectWatchers(const CoreSolver & s) const;

 private:
   ClauseBucketArray & cba;
   vec<ListType> watcher;

   bool isMarkedAsRemoved(const Watcher & w) const;

   void deleteSingleWatcher(const int listPos, const int watcherPos);
   bool containsCRef(const CRef cref) const;
};


inline VarSet TwoWatcherLists::getVarSet(const TwoWatcherLists::WType & w) const
{
   return VarSet(w,w.getBlocker(), w.getBlockerRef());
}

inline int TwoWatcherLists::size() const
{
   return watcher.size();
}

inline void TwoWatcherLists::markAsRemoved(const VarSet & vs)
{
   Watcher &w = getWatcher(vs);
   isValidWatcher(w);
   if (w.isHeader())
      getOtherWatcher(w).markAsRemoved();
   else
      w.markAsRemoved();
}

inline TwoWatcherLists::ListType & TwoWatcherLists::getWatcher(const Lit & p)
{
   return watcher[p.x];
}
inline const TwoWatcherLists::ListType & TwoWatcherLists::getWatcher(const Lit & p) const
{
   return watcher[p.x];
}
inline TwoWatcherLists::ListType & TwoWatcherLists::getWatcher(const int & pos)
{
   return watcher[pos];
}
inline const TwoWatcherLists::ListType & TwoWatcherLists::getWatcher(const int & pos) const
{
   return watcher[pos];
}

inline const TwoWatcherLists::WType & TwoWatcherLists::getWatcher(const int lPos, const int wPos) const
{
   assert(watcher.size() > lPos);
   const vec<Watcher> & ws = watcher[lPos];
   assert(ws.size() > wPos);
   return watcher[lPos][wPos];
}
inline TwoWatcherLists::WType & TwoWatcherLists::getWatcher(const int lPos, const int wPos)
{
   assert(watcher.size() > lPos);
   assert(watcher[lPos].size() > wPos);
   WType & res = watcher[lPos][wPos];
   return res;
}

class OneWatcherLists
{
 public:
   typedef OneWatcher WType;
   typedef vec<WType> ListType;

   OneWatcherLists(ClauseBucketArray & cba, const unsigned & numLits);
   int size() const;

   // Needed for WatcherListReference:
   ListType & getWatcher(const Lit & p);
   const ListType & getWatcher(const Lit & p) const;
   ListType & getWatcher(const int & pos);
   const ListType & getWatcher(const int & pos) const;

   void markAsRemoved(const VarSet & vs);
   void removeMarkedClauses(CoreSolver & s);
   void detach(CoreSolver& s, const VarSet & vs);

   VarSet replace(CoreSolver & s, const VarSet & vs, const CRef & cref);
   VarSet replace(CoreSolver & s, const int listPos, const int wPos, const CRef & cref);

   const WType & getWatcher(const VarSet & vs) const;
   WType & getWatcher(const VarSet & vs);
   const WType & getWatcher(const int lPos, const int wPos) const;
   WType & getWatcher(const int lPos, const int wPos);


   const CRef & getCRef(const VarSet & vs) const;

   bool attach(const CRef & cref, const BaseClause & c, CoreSolver & s);
   bool isMarkedAsRemoved(const OneWatcher & w) const;

   inline int getIndex(const Lit & l) const
   {
      return (l).x;
   }
   bool isValidWatcher(const OneWatcher & w) const;
   void assertCorrectWatchers() const;

 private:
   ClauseBucketArray & cba;
   vec<ListType> watcher;

};

inline bool OneWatcherLists::isMarkedAsRemoved(const OneWatcher & w) const
{
   return w.isMarkedAsRemoved();
}


inline const OneWatcherLists::WType & OneWatcherLists::getWatcher(const int lPos, const int wPos) const
{
   assert(watcher.size() > lPos);
   assert(watcher[lPos].size() > wPos);
   return watcher[lPos][wPos];
}
inline OneWatcherLists::WType & OneWatcherLists::getWatcher(const int lPos, const int wPos)
{
   assert(watcher.size() > lPos);
   assert(watcher[lPos].size() > wPos);
   return watcher[lPos][wPos];
}

inline OneWatcherLists::WType & OneWatcherLists::getWatcher(const VarSet & vs)
{
   return getWatcher(vs.getListPos(),vs.getWatcherPos());
}
inline const OneWatcherLists::WType & OneWatcherLists::getWatcher(const VarSet & vs) const
{
   return getWatcher(vs.getListPos(),vs.getWatcherPos());
}
inline OneWatcherLists::ListType & OneWatcherLists::getWatcher(const int & pos)
{
   return watcher[pos];
}
inline const OneWatcherLists::ListType & OneWatcherLists::getWatcher(const int & pos) const
{
   return watcher[pos];
}

inline const CRef & OneWatcherLists::getCRef(const VarSet & vs) const
{
   return getWatcher(vs).getCRef();
}
inline int OneWatcherLists::size() const
{
   return watcher.size();
}

// Needed for WatcherListReference:
inline OneWatcherLists::ListType & OneWatcherLists::getWatcher(const Lit & p)
{
   assert(watcher.size() > (p).x);
   return watcher[(p).x];
}
inline const OneWatcherLists::ListType & OneWatcherLists::getWatcher(const Lit & p) const
{
   assert(watcher.size() > (p).x);
   return watcher[(p).x];
}
inline void OneWatcherLists::markAsRemoved(const VarSet & vs)
{
   getWatcher(vs).markAsRemoved();
}

class BinaryWatcherLists
{
 public:
   typedef BinaryWatcher WType;
   typedef vec<WType> ListType;

   BinaryWatcherLists(ClauseBucketArray & cba, const unsigned & numLits);
   int size() const;
   // Needed for WatcherListReference:
   ListType & getWatcher(const Lit & p);
   const ListType & getWatcher(const Lit & p) const;
   ListType & getWatcher(const int & pos);
   const ListType & getWatcher(const int & pos) const;


   const WType & getWatcher(const VarSet & vs) const;
   WType & getWatcher(const VarSet & vs);
   const CRef & getCRef(const VarSet & vs) const;

   inline int getIndex(const Lit & l) const
   {
      return (l).x;
   }

   PropagateResult attach(const CRef & cref, CoreSolver & s);

 private:
   ClauseBucketArray & cba;
   vec<ListType> watcher;

   bool isConsistent() const;
};

inline int BinaryWatcherLists::size() const
{
   return watcher.size();
}

// Needed for WatcherListReference:
inline BinaryWatcherLists::ListType & BinaryWatcherLists::getWatcher(const Lit & p)
{
   return watcher[(p).x];
}
inline const BinaryWatcherLists::ListType & BinaryWatcherLists::getWatcher(const Lit & p) const
{
   return watcher[(p).x];
}
inline BinaryWatcherLists::ListType & BinaryWatcherLists::getWatcher(const int & pos)
{
   return watcher[pos];
}
inline const BinaryWatcherLists::ListType & BinaryWatcherLists::getWatcher(const int & pos) const
{
   return watcher[pos];
}

inline const BinaryWatcherLists::WType & BinaryWatcherLists::getWatcher(const VarSet & vs) const
{
   assert(vs.getWatcherType() == WatcherType::BINARY);
   assert(watcher.size() > vs.getListPos());
   assert(watcher[vs.getListPos()].size() > vs.getWatcherPos());
   return watcher[vs.getListPos()][vs.getWatcherPos()];
}
inline BinaryWatcherLists::WType & BinaryWatcherLists::getWatcher(const VarSet & vs)
{
   assert(vs.getWatcherType() == WatcherType::BINARY);
   assert(watcher.size() > vs.getListPos());
   assert(watcher[vs.getListPos()].size() > vs.getWatcherPos());
   return watcher[vs.getListPos()][vs.getWatcherPos()];
}
inline const CRef & BinaryWatcherLists::getCRef(const VarSet & vs) const
{
   return getWatcher(vs).getCRef();
}

} /* namespace Glucose */

#endif /* SHARED_CLAUSEWATCHER_H_ */
