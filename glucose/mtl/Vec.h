/*******************************************************************************************[Vec.h]
 Copyright (c) 2003-2007, Niklas Een, Niklas Sorensson
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

#ifndef Glucose_Vec_h
#define Glucose_Vec_h

#include <assert.h>
#include <new>

#include "mtl/IntTypes.h"
#include "mtl/XAlloc.h"
#include<string.h>
#include <algorithm>
#include <iostream>

namespace Glucose
{

//=================================================================================================
// Automatically resizable arrays
//
// NOTE! Don't use this vector on datatypes that cannot be re-located in memory (with realloc)

template<class T>
class vec
{
   T* _data;
   int sz;
   int cap;

   // Don't allow copying (error prone):
   vec<T>& operator =(const vec<T>& other)
   {
      assert(0);
      return *this;
   }


   // Helpers for calculating next capacity:
   static inline int imax(int x, int y)
   {
      int mask = (y - x) >> (sizeof(int) * 8 - 1);
      return (x & mask) + (y & (~mask));
   }
   //static inline void nextCap(int& cap){ cap += ((cap >> 1) + 2) & ~1; }
   static inline void nextCap(int& cap)
   {
      cap += ((cap >> 1) + 2) & ~1;
   }

 public:
   // Constructors:
   vec()
         : _data(NULL),
           sz(0),
           cap(0)
   {
   }

   vec(vec<T> && in)
         : vec()
   {
      std::swap(_data, in._data);
      std::swap(sz, in.sz);
      std::swap(cap, in.cap);
   }

   explicit vec(int size)
         : _data(NULL),
           sz(0),
           cap(0)
   {
      growTo(size);
   }
   vec(int size, const T& pad)
         : _data(NULL),
           sz(0),
           cap(0)
   {
      growTo(size, pad);
   }
   ~vec()
   {
      clear(true);
   }

   vec<T> & operator=(vec<T>&& in)
   {
      std::swap(_data,in._data);
      std::swap(sz,in.sz);
      std::swap(cap,in.cap);
      return *this;
   }

   // Pointer to first element:
   operator T*(void)
   {
      return _data;
   }

   T* data()
   {
      return _data;
   }
   const T* data() const
   {
      return _data;
   }

   // Size operations:
   inline int size(void) const
   {
      return sz;
   }
   void shrink(int nelems)
   {
      assert(nelems <= sz);
      for (int i = 0; i < nelems; i++)
         --sz, _data[sz].~T();
   }
   void shrink_(int nelems)
   {
      assert(nelems <= sz);
      sz -= nelems;
   }
   int capacity(void) const
   {
      return cap;
   }

   void resize_(int nelems)
   {
      if (sz != nelems)
         sz = nelems;
   }

   void capacity(int min_cap);
   void growTo(int size);
   void lazy_growTo(int size);
   void growTo(int size, const T& pad);
   void clear(bool dealloc = false);
   void unordered_remove(const int & idx)
   {
      assert(sz > 0);
      _data[idx] = _data[--sz];
   }
   void erase(const int & startIdx, const int & endIdx)
   {
      assert(endIdx <= sz);
      if (startIdx == 0 && endIdx == sz)
         clear(false);
      else
      {
         int i = startIdx, j = endIdx;
         for (; j < sz; ++i, ++j)
         {
            _data[i] = _data[j];
            _data[j].~T();
         }
         shrink_(endIdx - startIdx);
      }
   }

   inline void erase(const vec<int> & idxs)
   {
      int i=idxs.size();
      while(i-- > 0)
      {
         assert(i == 0 || idxs[i] > idxs[i-1]); // idxs must be sorted increasing
         this->unordered_remove(idxs[i]);
      }
   }

   bool contains(const T & elem)
   {
      bool res = false;
      for(int i=0;i<sz;++i)
         if(elem == _data[i])
         {
            res = true;
            break;
         }
      return res;
   }

   // Stack interface:
   void push(void)
   {
      if (sz == cap)
         capacity(sz + 1);
      new (&_data[sz]) T();
      sz++;
   }
   void push(const vec<T> & in)
   {
      if (sz + in.size() > cap)
         capacity(sz + in.size());
      for (int i = sz; i < sz + in.size(); ++i)
         _data[i] = in[i - sz];
      sz += in.size();
   }
   void push(const T& elem)
   {
      if (sz == cap)
         capacity(sz + 1);
      push_(elem);
   }

   void push(T&& elem)
   {
      if (sz == cap)
         capacity(sz + 1);
      push_(std::move(elem));
   }

   void push_construct(const T& elem)
   {
      if (sz == cap)
         capacity(sz + 1);
      new (&_data[sz]) T(elem);
      sz++;
   }
   void push_(const T& elem)
   {
      assert(sz < cap);
      _data[sz++] = elem;
   }
   void push_(T&& elem)
   {
      assert(sz < cap);
      _data[sz++] = elem;
   }
   void pop(void)
   {
      assert(sz > 0);
      sz--, _data[sz].~T();
   }
   // NOTE: it seems possible that overflow can happen in the 'sz+1' expression of 'push()', but
   // in fact it can not since it requires that 'cap' is equal to INT_MAX. This in turn can not
   // happen given the way capacities are calculated (below). Essentially, all capacities are
   // even, but INT_MAX is odd.

   inline const T& last(void) const
   {
      return _data[sz - 1];
   }
   inline T& last(void)
   {
      return _data[sz - 1];
   }

   // Vector interface:
   inline const T& operator [](int index) const
   {
      return _data[index];
   }
   inline T& operator [](int index)
   {
      return _data[index];
   }

   // Duplicatation (preferred instead):
   void copyTo(vec<T>& copy) const
   {
      copy.clear();
      copy.lazy_growTo(sz);
      for (int i = 0; i < sz; i++)
         copy[i] = _data[i];
   }
   void moveTo(vec<T>& dest)
   {
      dest.clear(true);
      dest._data = _data;
      dest.sz = sz;
      dest.cap = cap;
      _data = NULL;
      sz = 0;
      cap = 0;
   }
   void memCopyTo(vec<T>& copy) const
   {
      copy.capacity(cap);
      copy.sz = sz;
      memcpy(copy._data, _data, sizeof(T) * cap);
   }

   void swap(vec<T> & swappy)
   {
      std::swap(_data, swappy._data);
      std::swap(sz, swappy.sz);
      std::swap(cap, swappy.cap);
   }

   bool hasUniqueElements() const
   {
      bool res = true;
      for(int i=0;i<size();++i)
         for(int j=i+1;j<size();++j)
            if(_data[i] == _data[j])
            {
              res = false;
              break;
            }
      return res;
   }

};

template<class T>
void vec<T>::capacity(int min_cap)
{
   if (cap >= min_cap)
      return;
   int add = imax((min_cap - cap + 1) & ~1, ((cap >> 1) + 2) & ~1);   // NOTE: grow by approximately 3/2
   cap += add;
   _data = reinterpret_cast<T*>(xrealloc(_data, cap * sizeof(T)));
}

template<class T>
void vec<T>::growTo(int size, const T& pad)
{
   if (sz >= size)
      return;
   capacity(size);
   for (int i = sz; i < size; i++)
      _data[i] = pad;
   sz = size;
}

template<class T>
void vec<T>::growTo(int size)
{
   if (sz >= size)
      return;
   capacity(size);
   for (int i = sz; i < size; i++)
      new (&_data[i]) T();
   sz = size;
}

template<class T>
void vec<T>::lazy_growTo(int size)
{
   if (sz >= size)
      return;
   capacity(size);
   sz = size;
}

template<class T>
void vec<T>::clear(bool dealloc)
{
   if (_data != NULL)
   {
      for (int i = 0; i < sz; i++)
         _data[i].~T();
      sz = 0;
      if (dealloc)
         xfree(_data), _data = NULL, cap = 0;
   }
}

//=================================================================================================
}

#endif
