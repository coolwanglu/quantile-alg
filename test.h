/*
 * Testing framework for stream algorithms
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
#ifndef TEST_H__
#define TEST_H__

#include <random>
#include <vector>
#include <iostream>

template<class T>
void test()
{
    // prepare input
    const long length = 1000;
    std::vector<int> v;
    v.reserve(length);
    for(int i = 0; i < length; ++i)
        v.push_back(i);
    std::random_shuffle(v.begin(), v.end());

    // process the stream
    const double eps = 0.01;
    T alg(eps);
    for(auto i : v)
        alg.feed(i);
    alg.finalize();

    // queries
    for(double i = 0; i < 1.0; i += eps)
        std::cout << i << ' ' << alg.query_for_value(i) << std::endl;
}
#endif //TEST_H__
