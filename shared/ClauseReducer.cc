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

#include "shared/ClauseReducer.h"
#include "shared/ClauseDatabase.h"
#include "shared/ClauseWatcher.h"
#include "shared/SharedTypes.h"

namespace Sticky
{

ClauseReducer::ClauseReducer(ClauseDatabase & db, ClauseBucketArray & cba, CoreSolver & s)
      : db(db),
        cba(cba),
        s(s),
        dts(s.getThreadState()),
        ls(s.getLiteralSetting()),
        two(dts.twoWatched),
        one(dts.oneWatched),
        bin(dts.binWatched)
{
}

struct ActivityLitComp
{
   const vec<double> & activity;
   ActivityLitComp(const vec<double> & activity)
         : activity(activity)
   {
   }
   bool operator()(const Lit & a, const Lit & b) const
   {
      return activity[var(a)] > activity[var(b)];
   }
};

struct LevelLitComp
{
   const LiteralSetting & set;
   LevelLitComp(const LiteralSetting & set)
         : set(set)
   {
   }
   bool operator()(const Lit & a, const Lit & b) const
   {
      int lvla =
            (set.state[var(a)].level == -1) ?
                  std::numeric_limits<int>::max() : set.state[var(a)].level, lvlb =
            (set.state[var(b)].level == -1) ?
                  std::numeric_limits<int>::max() : set.state[var(b)].level;
      return lvla < lvlb;
   }
};

void ClauseReducer::cleanBinaryWatched()
{
   assert(ls.decisionLevel() == 0);
   for (int i = 0; i < bin.size(); ++i)
   {
      vec<BinaryWatcher> & ws = bin.getWatcher(i);
      Lit cur;
      cur.x = i;
      bool curTrue = ls.value(~cur) == l_True;
      int j = 0;
      for (int k = 0; k < ws.size(); ++k)
      {
         const BinaryWatcher & w = ws[k];
         BaseClause & c = cba.getClause(w.getCRef());
         assert(c.contains(~cur));
         if (curTrue || ls.value(w.getBlocker()) == l_True)
         {
            if (toInt(w.getBlocker()) < i)
            {
               cba.removeClause(s, w.getCRef());
               --s.getStatistic().nTwoWatchedClauses;
            }
         } else
            ws[j++] = ws[k];
      }
      ws.shrink(ws.size() - j);
   }
}

bool ClauseReducer::getVivifyConflictClause(const PropagateResult & pr,
                                            const CRef vivRef,
                                            vec<Lit> & decisionClause)
{
   assert(pr.isConflict());
   bool res = s.findDecisionClauseForConflict(pr.getVarSet(), decisionClause);
   assert(decisionClause.size() > 0);
   return res;
}

bool ClauseReducer::checkVivification()
{
   deleteIdx.clear();
   propClause.clear();
   bool res = true;
   assert(ls.decisionLevel() == 0);
   if (ls.hasOpenPropagation())
   {
      if (s.propagate().isConflict())
      {
         s.setResult(l_False, "Found solution through level 0 propagation");
         res = false;
      }
   }
   assert(ls.trail.size() == ls.qhead);

   return res;
}

VififyResult ClauseReducer::prepareViviClause(const BaseClause & c, vec<Lit> & outClause)
{
   for (int i = 0; i < c.size(); ++i)
   {
      if (ls.value(c[i]) == l_Undef)
         outClause.push(c[i]);
      else if (ls.value(c[i]) == l_True)
      {
         return VififyResult::TRUE;  // clause already satisfied
      }
   }
   if (outClause.size() == 0)
   {
      s.setResult(l_False, "Found solution through unsatisfiable clause");
   }

   // here literals may be sorted
   return VififyResult::NOT_VIVIFIED;
}

void ClauseReducer::doVivificationPropagation(const CRef vivRef, vec<Lit> & outClause)
{
   propClause.clear();
   deleteIdx.clear();
   bool propagatedTrueOrConflict = false;
   for (int i = 0; i < outClause.size(); ++i)
   {
      if (ls.value(outClause[i]) == l_Undef)
      {
         ls.newDecisionLevel();
         s.uncheckedEnqueue(~outClause[i], VarSet::decision());
         PropagateResult pr = s.propagate();
         if (pr.isConflict())
         {
            if (db.getSharingHeuristic().useBackTrackVivification)
            {
               if (getVivifyConflictClause(pr, vivRef, propClause))
               {
                  propagatedTrueOrConflict = true;
                  break;
               } else  // cref was part of the conflict so just remove the trailing false lits:
               {
                  for (i = i + 1; i < outClause.size(); ++i)
                     if (ls.value(outClause[i]) == l_False)
                        deleteIdx.push(i);
               }
            } else
               // just remove the pending literals:
               for (i = i + 1; i < outClause.size(); ++i)
                  deleteIdx.push(i);

         }
      } else if (ls.value(outClause[i]) == l_True && i + 1 < outClause.size())
      {
         getVivifyTruePropagationClause(outClause, i, propClause);
         propagatedTrueOrConflict = true;
         break;
      } else if (ls.value(outClause[i]) == l_False)
         deleteIdx.push(i);
   }
   s.cancelUntil(0);
   if (propagatedTrueOrConflict && outClause.size() - deleteIdx.size() >= propClause.size())
      outClause.swap(propClause);
   else
      outClause.erase(deleteIdx);
   if (outClause.size() == 0)
   {
      s.setResult(l_False, "Found zero vivification clause");
      const BaseClause & c = cba.getClause(vivRef);
      for (int i = 0; i < c.size(); ++i)
         outClause.push(c[i]);
   }
   assert(outClause.hasUniqueElements());

}

unsigned ClauseReducer::vivifyClause(const CRef cref)
{
   const BaseClause & c = cba.getClause(cref);
   PropagateResult propResult;
   vec<Lit> outClause;
   unsigned vivLbd = c.getLbd();
   unsigned res = 0;
   switch (vivifyClause(cref, outClause, vivLbd))
   {
      case VififyResult::VIVIFIED:
         assert(outClause.size() < c.size());
         if (db.replaceClause(s, cref, outClause, (vivLbd < c.getLbd()) ? vivLbd : c.getLbd()))
         {
            assert(
                  cba.getClause(cref).isPrivateClause()
                        || !s.getThreadState().twoWatched.findWatcher(cref).isValid());
            if (outClause.size() == 1)
            {
               s.uncheckedEnqueue(outClause[0], VarSet());
            }
            res = 1;
         }
         break;
      case VififyResult::TRUE:
         cba.markClauseAsDeleted(cref);
         break;
      case VififyResult::NOT_VIVIFIED:
         ++s.getStatistic().failedVivifycations;
         break;
      default:
         ;
   }
   return res;
}

VififyResult ClauseReducer::vivifyClause(const CRef cref, vec<Lit> & outClause, unsigned & vivLbd)
{
   assert(outClause.size() == 0);
   VififyResult res = VififyResult::NOT_VIVIFIED;
   const BaseClause & c = cba.getClause(cref);
   if (checkVivification())
   {
      res = prepareViviClause(c, outClause);
      int startSize = outClause.size();
      s.getStatistic().sumViviStartLength += outClause.size();
      if (res == VififyResult::NOT_VIVIFIED && outClause.size() > 0)
      {
         doVivificationPropagation(cref, outClause);
         if (outClause.size() < c.size())
         {
            if (outClause.size() < startSize)
            {
               ++s.getStatistic().nVivifications;
               s.getStatistic().sumVivificationLength += startSize - outClause.size();
            } else
               ++s.getStatistic().failedVivifycations;
            vivLbd =
                  (vivLbd > static_cast<unsigned>(outClause.size() - 1)) ?
                        outClause.size() - 1 : vivLbd;
            res = VififyResult::VIVIFIED;
         }
      }
   }
   return res;
}

void ClauseReducer::getVivifyTruePropagationClause(const vec<Lit> & clause,
                                                   int trueIdx,
                                                   vec<Lit> & outClause)
{
   assert(ls.value(clause[trueIdx]) == l_True);
   // find true lit with lowest level
   Lit p = clause[trueIdx];
   for (trueIdx = trueIdx + 1; trueIdx < outClause.size(); ++trueIdx)
      if (ls.value(clause[trueIdx]) == l_True && ls.level(clause[trueIdx]) < ls.level(p))
         p = clause[trueIdx];
   s.cancelUntil(ls.level(p));
   s.findDecisionClauseForPropagation(p, outClause);
   assert(outClause.size() > 0);
}

unsigned ClauseReducer::vivifyClauses(const RefVec<CRef> & refs)
{
   uint64_t startProps = s.getStatistic().nPropagations;
   unsigned res = 0;
   CRef cref;
   for (int i = 0; i < refs.size(); ++i)
   {
      BaseClause & c = cba.getClause(refs[i]);
      c.setVivified();
      assert(c[0] != lit_Undef);
      if (!cba.shouldBeRemoved(refs[i]) && !cba.shouldBeReplaced(refs[i]) && !c.isReplaced()
            && !c.isPrivDel())
      {
         cref = refs[i];
         if (!cba.shouldBeReplaced(cref))
         {
            //db.importCRefs(s);
            db.importUnits(s);
            res += vivifyClause(cref);
         }
      }
      if (db.jobFinished()
            || (db.getSharingHeuristic().useDynamicVivi && !c.isPermanentClause()
                  && s.getStatistic().getViviImpact() + db.getSharingHeuristic().viviSpendTolerance
                        < s.getStatistic().percentPropsSpendInVivi(startProps)))
         break;
   }
   s.getStatistic().nViviPropagations += s.getStatistic().nPropagations - startProps;
   return res;
}

void ClauseReducer::reduce()
{
   assert(ls.decisionLevel() == 0);
   cleanBinaryWatched();
   int kept = reduceTwoWatched();
   reduceOneWatched(kept);
   cba.garbageCollection(s);
   //two.assertCorrectWatchers(s);
}

unsigned ClauseReducer::getRemoveNumTwoWatchedClauses() const
{
   unsigned minDelete = (s.getStatistic().nConflicts - s.getStatistic().nLastReduceConflicts) / 2;
   assert(
         s.getStatistic().nPrivateCl + s.getStatistic().nSharedCl
               > s.getStatistic().nOneWatchedClauses);
   unsigned maxDelete = (s.getStatistic().nPrivateCl + s.getStatistic().nSharedCl
         - s.getStatistic().nOneWatchedClauses) / 2;
   if (minDelete > maxDelete)
      return minDelete;
   else
      return maxDelete;

}
unsigned ClauseReducer::getRemoveNumOneWatchedClauses(const unsigned & keptTwo) const
{
   if (s.getStatistic().nOneWatchedClauses > 2 * (keptTwo + 100))
      return s.getStatistic().nOneWatchedClauses - 2 * keptTwo;
   else
      return s.getStatistic().nOneWatchedClauses / 2;
}

struct TwoRef
{
   int listPos;
   int wPos;
};

void ClauseReducer::collectPrivateGoodClauses(vec<CRef> & crefs)
{
   vec<TwoRef> refs;
   int numRelevant = 0;
   for (int i = 0; i < two.size(); ++i)
   {
      vec<Watcher> & ws = two.getWatcher(i);
      for (int j = 0; j < ws.size(); ++j)
      {
         const BaseClause & c = cba.getClause(ws[j].getCRef());
         if (c.getLbd() > 0 && ws[j].isHeader() && !ws[j].isImported())
            ++numRelevant;
         if (ws[j].isHeader() && !ws[j].isImported() && !c.isReplaced() && !c.isVivified()
               && c.getLbd() > 0 && !cba.shouldBeRemoved(ws[j].getCRef())
               && (db.getSharingHeuristic().useDynamicVivi
                     || db.getLbd(s, ws[j]) <= db.getSharingHeuristic().maxVivificationLbd))
         {
            refs.push( { i, j });
         }
      }
   }
   int limit = (refs.size() / 2 > numRelevant) ? refs.size() / 2 - numRelevant : 0;
   if (limit > 0)
   {
      sort(refs,
           [this](const TwoRef & v1, const TwoRef &v2)
           {
              const Watcher & w1 = two.getWatcher(v1.listPos,v1.wPos), & w2 = two.getWatcher(v2.listPos,v2.wPos);
              unsigned tmp1 = w1.getLbd(), tmp2 = w2.getLbd();
              if(tmp1 == tmp2)
              {
                 tmp1 = w1.getActivity(); tmp2 = w2.getActivity();
                 if(tmp1 == tmp2)
                 return cba.getClause(w1.getCRef()).size() > cba.getClause(w2.getCRef()).size();  // size compare
                 else
                 return tmp1 < tmp2;// activity compare
              }
              else
              return tmp1 > tmp2;  // lbd compare
           });
      if (db.getSharingHeuristic().useDynamicVivi)
      {
         sort(refs,
              [this](const TwoRef & v1, const TwoRef &v2)
              {
                 const Watcher & w1 = two.getWatcher(v1.listPos,v1.wPos), & w2 = two.getWatcher(v2.listPos,v2.wPos);
                 unsigned tmp1 = w1.getActivity(), tmp2 = w2.getActivity();

                 if(tmp1 == tmp2)
                 return cba.getClause(w1.getCRef()).size() > cba.getClause(w2.getCRef()).size();  // size compare
                 else
                 return tmp1 < tmp2;// activity compare
              });
      }
   }

   for (int i = refs.size() - 1; i > limit; --i)
   {
      Watcher & w = two.getWatcher(refs[i].listPos, refs[i].wPos);
      BaseClause & c = cba.getClause(w.getCRef());
      if (!c.isVivified())  // data race, has no impact 1
      {
         crefs.push(w.getCRef());
      }
   }
}

int ClauseReducer::reduceTwoWatched()
{
   vec<TwoRef> refs;
// first collect references to clauses, and update clauses by reallocation, new versions or deletions
   if (s.getHeuristic().chanseok)
   {
      for (int i = 0; i < two.size(); ++i)
      {
         vec<Watcher> &ws = two.getWatcher(i);
         for (int j = 0; j < ws.size(); ++j)
         {
            Watcher & w = ws[j];
            if (w.isHeader())
            {
               BaseClause & c = db.getClause(w);
               if (!cba.shouldBeRemoved(w.getCRef()) && !cba.shouldBeReplaced(w.getCRef())
                     && !c.isPermanentClause())
               {
                  if (w.getLbd() > 2 && (w.getActivity() > 0 || w.getLbd() > 4))
                     refs.push( { i, j });
               }
            }
         }
      }

      // sort 'bad' clause references ones to the beginning:
      sort(refs,
           [this](const TwoRef & v1, const TwoRef &v2)
           {
              const Watcher & w1 = two.getWatcher(v1.listPos,v1.wPos), & w2 = two.getWatcher(v2.listPos,v2.wPos);
              return w1.getActivity() < w2.getActivity();
           });

   } else
   {
      for (int i = 0; i < two.size(); ++i)
      {
         vec<Watcher> &ws = two.getWatcher(i);
         for (int j = 0; j < ws.size(); ++j)
         {
            Watcher & w = ws[j];
            if (w.isHeader())
            {
               BaseClause & c = db.getClause(w);
               if (!cba.shouldBeRemoved(w.getCRef()) && !cba.shouldBeReplaced(w.getCRef())
                     && !c.isPermanentClause())
               {
                  if (w.getLbd() > 2)
                     refs.push( { i, j });
               }
            }
         }
      }

      // sort 'bad' clause references ones to the beginning:
      sort(refs,
           [this](const TwoRef & v1, const TwoRef &v2)
           {
              const Watcher & w1 = two.getWatcher(v1.listPos,v1.wPos), & w2 = two.getWatcher(v2.listPos,v2.wPos);
              unsigned tmp1 = w1.getLbd(), tmp2 = w2.getLbd();
              if(tmp1 == tmp2)
              {
                 tmp1 = w1.getActivity(); tmp2 = w2.getActivity();
                 if(tmp1 == tmp2)
                 return cba.getClause(w1.getCRef()).size() > cba.getClause(w2.getCRef()).size();  // size compare
                 else
                 return tmp1 < tmp2;// activity compare
              }
              else
              return tmp1 > tmp2;  // lbd compare
           });
   }
   int mid = refs.size() / 2;
   const Watcher & w = two.getWatcher(refs[mid].listPos, refs[mid].wPos);
   s.getStatistic().medianLbd = std::min(w.getLbd(), db.getSharingHeuristic().maxVivificationLbd);
   int limit = std::min(getRemoveNumTwoWatchedClauses(), static_cast<unsigned>(refs.size()));
// mark as removed so they can be deleted without invalidating references
   int i;
   for (i = 0; i < limit; ++i)
   {
      assert(two.getWatcher(refs[i].listPos, refs[i].wPos).isHeader());
      if (!db[two.getWatcher(refs[i].listPos, refs[i].wPos)].isPermanentClause())
         two.getOtherWatcher(refs[i].listPos, refs[i].wPos).markAsRemoved();
      else
         break;
   }
   limit = refs.size() - limit;
   refs.clear(true);
   if (db.shouldCollectCRefs() && db.getCompleteViviRefs().tryWriteLock())
   {
      two.removeMarkedClauses(s, db.getCompleteViviRefs().getVec());
      db.getCompleteViviRefs().releaseWriteLock();
   } else
      two.removeMarkedClauses(s);
   return limit;
}

void ClauseReducer::reduceOneWatched(const unsigned & keptTwo)
{
   struct OneRef
   {
      int listPos;
      int wPos;
      CRef cref;
   };
   vec<OneRef> refs;
// first collect references to clauses, and update clauses by reallocation, new versions or moves to two watched
   for (int i = 0; i < one.size(); ++i)
   {
      vec<OneWatcher> &ws = one.getWatcher(i);
      for (int j = 0; j < ws.size(); ++j)
      {
         OneWatcher & w = ws[j];
         if (cba.shouldBeRemoved(w.getCRef()))
            w.markAsRemoved();
         else if (!cba.shouldBeReplaced(w.getCRef()))
         {
            assert(!cba.getClause(w.getCRef()).isPermanentClause());
            refs.push( { i, j, w.getCRef() });
         }
      }

   }
// sort 'bad' clause references ones to the beginning:
   sort(refs, [this](const OneRef & v1, const OneRef &v2)
   {
      const BaseClause & c1 = cba.getClause(v1.cref), & c2 = cba.getClause(v2.cref);
      unsigned tmp1 = c1.size(), tmp2 = c2.size();
      if(tmp1 == tmp2)
      {
         tmp1 = c1.getLbd(); tmp2 = c2.getLbd();
         return tmp1 > tmp2;  // lbd compare
     }
     else
     return tmp1 > tmp2;  // size compare
  });
   const int limit = std::min(getRemoveNumOneWatchedClauses(keptTwo),
                              static_cast<unsigned>(refs.size()));
// mark as removed so they can be deleted without invalidating references
   int i;
   for (i = 0; i < limit; ++i)
      one.getWatcher(refs[i].listPos, refs[i].wPos).markAsRemoved();

   if (refs.size() > 0)
      one.removeMarkedClauses(s);
}

} /* namespace Concusat */
