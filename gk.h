/*
 * Implementation of the GK algorithm
 * Copyright (c) 2013 Lu Wang <coolwanglu@gmail.com>
 */

/*
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#ifndef GK_H__
#define GK_H__

#include <map>
#include <vector>
#include <algorithm>
#include <limits>
#include <cstdint>
#include <unordered_map>
#include <queue>
#include <cassert>

template<class IntType>
class GK
{
public:
    long long last_n;
    double EPS;
    uint64_t max_v;

    class entry{
    public:
        entry () { }
        entry (unsigned int _g, unsigned int _d) : g(_g), delta(_d) { }
        unsigned int g,delta;
    };

    typedef std::multimap<IntType, entry> map_t;
    map_t entries_map;
    unsigned int max_gd;

    class SummaryEntry
    {
    public:
        IntType v;
        long long g, delta;
    };
    std::vector<SummaryEntry> summary;

    GK(double eps, uint64_t max_v = std::numeric_limits<IntType>::max()) 
        :last_n(0)
        ,EPS(eps) 
        ,max_v(max_v)
    {
        entry e(1,0);
        entries_map.insert(std::make_pair(max_v, e));
    }

    void finalize () {
        summary.clear();
        summary.reserve(entries_map.size());
        SummaryEntry se;
        se.g = 0;
        max_gd = 0;
        for(auto & p : entries_map)
        {
            const auto & e = p.second;
            unsigned int gd = e.g + e.delta;
            if(gd > max_gd) max_gd = gd;

            se.v = p.first;
            se.g += e.g;
            se.delta = e.delta;
            summary.push_back(se);
        }
        max_gd /= 2;
    }

    IntType query_for_value(double rank) const
    {
        SummaryEntry se;
        se.g = rank*last_n+max_gd;
        se.delta = 0;
        auto iter = upper_bound(summary.begin(), summary.end(),
                se,
                [](const SummaryEntry & e1, const SummaryEntry & e2)->bool{
                    return (e1.g + e1.delta) < (e2.g + e2.delta);
                });

        if(iter == summary.begin())
        {
            return 0;
        }

        return (--iter)->v;
    } 
    // record when an item might be removed
    class ThresholdEntry
    {
    public:
        unsigned int threshold; // the item could be removed only when the global threshold is no less than this value
        typename map_t::iterator map_iter;

        bool operator > (const ThresholdEntry & te) const {
            return threshold > te.threshold;
        }
    };

    std::priority_queue<ThresholdEntry, std::vector<ThresholdEntry>, std::greater<ThresholdEntry>> compress_heap;

    void feed(IntType v) {
        ++last_n;

        if((uint64_t)v == max_v) return;

        const auto iter = entries_map.upper_bound(v);
        //iter cannot be end()

        entry & ecur = iter->second;
        const unsigned int threshold = (unsigned int)std::floor(EPS * last_n * 2);
        unsigned int tmp = ecur.g + ecur.delta;
        if(tmp < threshold)
        {
            //no need to insert
            ++(ecur.g);

            // now the heap entries for recur and previous one (if there is) are affected
            // the threshold should have been increased by 1
            // we will fix this when trying to remove one
        }
        else
        {
            auto iter2 = entries_map.insert(iter, std::make_pair(v, entry(1, tmp-1)));

            // note that the heap entry for the previous one (if there is) is not affected!
            compress_heap.push({tmp + 1, iter2});

            // try to remove one
            while(true)
            {
                auto top_entry = compress_heap.top();
                if(top_entry.threshold > threshold)
                {
                    // all threshold in the heap are no less than the real threshold of the entry
                    // so we cannnot remove any tuple right now
                    break;
                }

                compress_heap.pop();
                // check if it is true
                auto map_iter2 = top_entry.map_iter; 
                auto map_iter1 = map_iter2++; 
                assert(map_iter2 != entries_map.end());

                auto & e1 = map_iter1->second;
                auto & e2 = map_iter2->second;

                auto real_threshold = e1.g + e2.g + e2.delta;

                if(real_threshold <= threshold) {
                    e2.g += e1.g;
                    entries_map.erase(map_iter1);
                    break;
                }
                else
                {
                    top_entry.threshold = real_threshold;
                    compress_heap.push(top_entry);
                }
            }
        }
    }
};

#endif // GK_H__
