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

#ifndef PARALLEL_UTILS_STATISTICPRINTER_H_
#define PARALLEL_UTILS_STATISTICPRINTER_H

#include <cinttypes>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <string>

#include "glucose/mtl/Vec.h"

namespace Sticky
{

template<typename T>
struct SimpleSample
{
      SimpleSample(const std::string & name)
            : val(0),
              name(name)
      {
      }

      SimpleSample & operator=(const T & in)
      {
         val = in;
         return *this;
      }

      std::string getVal() const
      {
         std::string res = getValInternal();
         if(name.size() > res.size())
              res = std::string(name.size() - res.size(),' ') + res;
         return res;
      }

      std::string getValUF() const
      {
         return getValInternal();
      }

      T value() const
      {
         return val;
      }

      std::string getName() const
      {
         std::string res = getValInternal();
         if(res.size() > name.size())
              res = std::string(res.size() - name.size(),' ') + name;
         else
            res = name;
         return res;
      }

      std::string getNameUF() const
      {
         return name;
      }

      int size() const
      {
         return std::max(getValInternal().size(),name.size());
      }


    private:
      T val;
      const std::string name;

      std::string getValInternal() const
      {
         std::stringstream ss("");
         ss << std::fixed << std::setprecision(1) << val;
         return ss.str();
      }
};

template<typename T>
struct MultiSample
{
      MultiSample(const std::string & name)
            : name(name)
      {
      }


      MultiSample(MultiSample&& in)
            : name(std::move(in.name))
      {
      }

      void clear()
      {
         elems.clear();
      }

      void add(const T & val)
      {
         elems.push(val);
      }

      int numElems() const
      {
         return elems.size();
      }

      std::string getVal() const
      {
         return getSum();
      }

      std::string getValUF() const
      {
         return makeString(sum());
      }

      std::string getVal(const int & idx) const
      {
         return format(getValInternal(idx));
      }

      std::string getMin() const
      {
         return format(makeString(min()));
      }
      std::string getMax() const
      {
         return format(makeString(max()));
      }
      std::string getSum() const
      {
         return format(makeString(sum()));
      }

      std::string getAverage() const
      {
         return format(makeString(average()));
      }

      std::string getName() const
      {
         return format(name);
      }

      std::string getNameUF() const
      {
         return name;
      }

      int size() const
      {
         assert(elems.size() > 0);
         unsigned res = name.size();
         for(int i=0;i<numElems(); ++i)
            if(res < getValInternal(i).size())
               res = getValInternal(i).size();
         return res;
      }

      T min() const
      {
         assert(elems.size() > 0);
         T minE = elems[0];
         for (int i = 1; i < elems.size(); ++i)
            minE = (minE > elems[i]) ? elems[i] : minE;
         return minE;
      }
      T max() const
      {
         assert(elems.size() > 0);
         T maxE = elems[0];
         for (int i = 1; i < elems.size(); ++i)
            maxE = (maxE < elems[i]) ? elems[i] : maxE;
         return maxE;
      }

      T sum() const
      {
         assert(elems.size() > 0);
         T sum = elems[0];
         for (int i = 1; i < elems.size(); ++i)
            sum += elems[i];
         return sum;
      }

      T average() const
      {
         return (double) sum() / elems.size();
      }

    private:

      Glucose::vec<T> elems;
      const std::string name;

      std::string format(const std::string & v) const
      {
         unsigned s = size();
         if(s > v.size())
            return std::string(s-v.size(),' ') + v;
         else
            return v;
      }

      std::string makeString(const T & v) const
      {
         std::stringstream ss("");
         ss << std::fixed << std::setprecision(1) << v;
         return ss.str();
      }

      std::string getValInternal(const int & idx) const
      {
         return makeString(elems[idx]);
      }
};

template<class ... Ts>
struct MultiSamplePrinter
{
    protected:

      void ITprintNames() const
      {
         std::cout << "|";
      }

      void ITprintValues(const int & idx) const
      {
         std::cout << "|";
      }
      void ITprintAverage() const
      {
         std::cout << "|";
      }
      void ITprintMin() const
      {
         std::cout << "|";
      }
      void ITprintMax() const
      {
         std::cout << "|";
      }
      void ITprintSum() const
      {
         std::cout << "|";
      }

      int getNum() const
      {
         return 1;
      }
};

template<class FirstSample, class ... Ts>
struct MultiSamplePrinter<FirstSample, Ts...> : MultiSamplePrinter<Ts...>
{

      MultiSamplePrinter(const FirstSample & f, Ts & ... ts)
            : MultiSamplePrinter<Ts...>::MultiSamplePrinter(ts...),
              sample(f)
      {
      }

      void printLine() const
      {
         std::cout << "c |---" << std::string(getNum()-1,'-') << "|" << std::endl;
      }

      void printNames() const
      {
         std::cout << "c |ids";
         ITprintNames();
         std::cout << std::endl;
      }

      void printValues(const int & idx) const
      {
         std::cout << "c |" << std::setw(3) << idx;
         ITprintValues(idx);
         std::cout << std::endl;
      }

      void printAverageValues() const
      {
         std::cout << "c |avg";
         ITprintAverage();
         std::cout << std::endl;
      }

      void printMinValues() const
      {
         std::cout << "c |min";
         ITprintMin();
         std::cout << std::endl;
      }

      void printMaxValues() const
      {
         std::cout << "c |max";
         ITprintMax();
         std::cout << std::endl;
      }

      void printSumValues() const
      {
         std::cout << "c |sum";
         ITprintSum();
         std::cout << std::endl;
      }

    protected:

      const FirstSample & sample;

      void ITprintNames() const
      {
         std::cout << "|" << sample.getName();
         MultiSamplePrinter<Ts...>::ITprintNames();
      }
      void ITprintAverage() const
      {
         std::cout << "|" << sample.getAverage();
         MultiSamplePrinter<Ts...>::ITprintAverage();
      }
      void ITprintMin() const
      {
         std::cout << "|" << sample.getMin();
         MultiSamplePrinter<Ts...>::ITprintMin();
      }
      void ITprintMax() const
      {
         std::cout << "|" << sample.getMax();
         MultiSamplePrinter<Ts...>::ITprintMax();
      }
      void ITprintSum() const
      {
         std::cout << "|" << sample.getSum();
         MultiSamplePrinter<Ts...>::ITprintSum();
      }

      void ITprintValues(const int & idx) const
      {
         std::cout << "|" << sample.getVal(idx);
         MultiSamplePrinter<Ts...>::ITprintValues(idx);
      }

      int getNum() const
      {
         return sample.size() + 1 + MultiSamplePrinter<Ts...>::getNum();
      }
};

template<class ... Ts>
MultiSamplePrinter<Ts...> makeMultiSamplePrinter(Ts & ... ts)
{
      return MultiSamplePrinter<Ts...>(ts...);
}


template<class ... Ts>
struct CSVPrinter
{
    protected:

      void ITprintNames() const
      {
         std::cout << std::endl;
         std::cout.flush();
      }

      void ITprintValues() const
      {
         std::cout << std::endl;
         std::cout.flush();
      }
};

template<class FirstSample, class ... Ts>
struct CSVPrinter<FirstSample, Ts...> : CSVPrinter<Ts...>
{

   CSVPrinter(const FirstSample & f, Ts & ... ts)
            : CSVPrinter<Ts...>::CSVPrinter(ts...),
              sample(f)
      {
      }

      void printHeader() const
      {
         std::cout << "c CSV_STICKY_OUT";
         ITprintNames();
      }
      void printValues() const
      {
         std::cout << "c CSV_STICKY_OUT";
         ITprintValues();
      }

    protected:

      const FirstSample & sample;

      void ITprintNames() const
      {
         std::cout << "," << sample.getNameUF();
         CSVPrinter<Ts...>::ITprintNames();
      }

      void ITprintValues() const
      {
         std::cout << "," << sample.getValUF();
         CSVPrinter<Ts...>::ITprintValues();
      }
};

template<class ... Ts>
CSVPrinter<Ts...> makeCSVPrinter(Ts & ... ts)
{
      return CSVPrinter<Ts...>(ts...);
}

template<class ... Ts>
struct SimpleSamplePrinter
{
    protected:

      void ITprintNames() const
      {
         std::cout << "|";
      }

      void ITprintValues() const
      {
         std::cout << "|";
      }

      int getNum() const
      {
         return 1;
      }
};

template<class FirstSample, class ... Ts>
struct SimpleSamplePrinter<FirstSample, Ts...> : SimpleSamplePrinter<Ts...>
{

      SimpleSamplePrinter(const FirstSample & f, Ts & ... ts)
            : SimpleSamplePrinter<Ts...>::SimpleSamplePrinter(ts...),
              sample(f)
      {
      }

      void printLine() const
      {
         std::cout << "c |" << std::string(getNum()-2,'-') << "|" << std::endl;
      }

      void printNames() const
      {
         std::cout << "c ";
         ITprintNames();
         std::cout << std::endl;
      }

      void printValues() const
      {
         std::cout << "c ";
         ITprintValues();
         std::cout << std::endl;
      }

    protected:

      const FirstSample & sample;

      void ITprintNames() const
      {
         std::cout << "|" << sample.getName();
         SimpleSamplePrinter<Ts...>::ITprintNames();
      }

      void ITprintValues() const
      {
         std::cout << "|" << sample.getVal();
         SimpleSamplePrinter<Ts...>::ITprintValues();
      }

      int getNum() const
      {
         return sample.size() + 1 + SimpleSamplePrinter<Ts...>::getNum();
      }
};

template<class ... Ts>
SimpleSamplePrinter<Ts...> makeSimpleSamplePrinter(Ts & ... ts)
{
      return SimpleSamplePrinter<Ts...>(ts...);
}

}

#endif /* PARALLEL_UTILS_STATISTICPRINTER_H_ */
