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
