/*
 * Random
 *
 * by Lu Wang <coolwanglu@gmaiol.com>
 */
#ifndef RANDOM_H__
#define RANDOM_H__

#include <deque>
#include <boost/random.hpp>
#include <boost/random/uniform_smallint.hpp>

template<class IntType>
class Random
{
public:
    
    constexpr static double DELTA = 0.1;

    Random(double eps)
        : last_n(0)
        , EPS(eps / std::sqrt(2.0))
        , buffer_size_limit (std::ceil(1.0 / EPS * std::sqrt(std::log(1.0 / DELTA)/std::log(2.0))))
        , layer_count_limit (std::ceil(std::log(1.0 / EPS * std::sqrt(std::log(1/DELTA)/std::log(2.0))*5.0/12.0) / std::log(2.0)))
        , buffer_count_limit (layer_count_limit + 1)
        , next_new_sample(0)
        , next_sampled(0)
        , prob_mask(0)
        , prob_mask_width(0)
        , prob_bits_length(0)
        , coin_bits_length(0)
    { 
        layers.resize(layer_count_limit);
        collapsable_layers.reserve(layer_count_limit);

        buffer_pool.resize(buffer_count_limit + 1);
        free_buffers.reserve(buffer_count_limit + 1);
        for(auto & buf : buffer_pool)
        {
            buf.reserve(buffer_size_limit);
            free_buffers.push_back(&buf);
        }

        bool r = prepare_cur_buffer();
        assert(r);
        (void)r; // mute gcc warning

        generator.seed(time(nullptr));
    }

    void feed(IntType v) {
        ++ last_n;
        if(!next_is_sampled()) return;

        cur_buffer->push_back(v);

        if(((long long)cur_buffer->size()) == buffer_size_limit)
        {
            std::sort(cur_buffer->begin(), cur_buffer->end());
            insert_cur_buffer_to_layers(layers.begin());
            if(!prepare_cur_buffer()) // cannot create a new buffer
            {
                collapse();
                // collapse will allocate a new buffer in the end
            }
        }
    }

    void collapse () {
        prepare_cur_buffer_for_collapse();

        bool top_empty = layers.back().empty();
                
        assert(!collapsable_layers.empty());
        auto cur_layer = collapsable_layers.back();
        // tricky stuff, see comments in insert_cur_buffer_to_layers
        collapsable_layers.pop_back();
        assert(cur_layer->size() > 1);
        buffer_t * buffer1 = cur_layer->front();
        cur_layer->pop_front();
        buffer_t * buffer2 = cur_layer->front();
        cur_layer->pop_front();

        //merge buffer1 + buffer2 -> cur_buffer
        {
            auto iter1 = buffer1->begin();
            auto iter1e = buffer1->end();
            auto iter2 = buffer2->begin();
            auto iter2e = buffer2->end();
            bool use = toss_coin();
            while(true)
            {
                if((*iter1) < (*iter2))
                {
                    if((use = !use)) cur_buffer->push_back(*iter1);
                    if(++iter1 == iter1e)
                    {
                        while(iter2 != iter2e)
                        {
                            if((use = !use)) cur_buffer->push_back(*iter2);
                            ++iter2;
                        }
                        break;
                    }
                }
                else
                {
                    if((use = !use)) cur_buffer->push_back(*iter2);
                    if(++iter2 == iter2e)
                    {
                        while(iter1 != iter1e)
                        {
                            if((use = !use)) cur_buffer->push_back(*iter1);
                            ++iter1;
                        }
                        break;
                    }
                }
            }
        }

        free_buffer(buffer1);
        free_buffer(buffer2);

        auto next_layer = cur_layer;
        ++next_layer;
        /*
         * if next_layer->size() == 2, insert_cur_buffer_to_layers() would insert it to the collapsable layers stack
         */
        insert_cur_buffer_to_layers(next_layer);
        if(!top_empty)
        {
            // when the top layer is not empty before collapse, we should halve the sample rate
            assert(cur_layer == layers.begin());
            assert(cur_layer->empty());
            layers.push_back(std::move(layers.front()));
            layers.pop_front();

            prob_mask = prob_mask * 2 + 1;
            ++prob_mask_width;
        }
        else if(cur_layer->size() > 1)
            collapsable_layers.push_back(cur_layer);


        {
            bool r = prepare_cur_buffer();
            assert(r);
            (void)r;
        }
    }

    struct QuantileEntry{
        IntType v;
        long long rank;
        QuantileEntry(IntType _v, long long _rank)
            :v(_v), rank(_rank)
        { }
    };

    void finalize () {
        heap.clear();
        heap.reserve(buffer_count_limit);

        summary_weight_sum = 0;
        long long s = 0;
        long long cur_weight = prob_mask + 1;
        if(!cur_buffer->empty())
        {
            std::sort(cur_buffer->begin(), cur_buffer->end());
            heap.push_back(HeapEntry(cur_buffer->begin(), cur_buffer->end(), cur_weight));
            summary_weight_sum += ((long long)cur_buffer->size() * cur_weight);
            s += cur_buffer->size();
        }

        for(auto & vl : layers)
        {
            for(auto & v : vl) 
            {
                if(!v->empty())
                {
                    heap.push_back(HeapEntry(v->begin(), v->end(), cur_weight));
                    summary_weight_sum += ((long long)v->size() * cur_weight);
                    s += v->size();
                }
            }
            cur_weight *= 2;
        }
        summary.clear();
        summary.reserve(s);

        auto comp = [](const HeapEntry & he1, const HeapEntry & he2) -> bool {
            return (*he2.iter) < (*he1.iter);
        };

        std::make_heap(heap.begin(), heap.end(), comp);

        long long cur_rank = 0;
        while(!heap.empty())
        {
            std::pop_heap(heap.begin(), heap.end(), comp);
            auto & he = heap.back();
            summary.push_back(QuantileEntry(*he.iter, cur_rank));
            cur_rank += he.weight;

            if(++he.iter == he.iterend)
            {
                heap.pop_back();
            }
            else
            {
                std::push_heap(heap.begin(), heap.end(), comp);
            }
        }
    }

    IntType query_for_value (double rank)
    {
        rank = std::ceil(rank*summary_weight_sum);
        auto iter = std::upper_bound(summary.begin(), summary.end(), 
                QuantileEntry(0, rank),
                [](const QuantileEntry & e1, QuantileEntry & e2)->bool {
                    return e1.rank < e2.rank;
                });
        assert(iter != summary.begin());
        -- iter;
        return iter->v;
    }

    long long last_n;

    double EPS;
    long long buffer_size_limit;
    long long layer_count_limit;
    long long buffer_count_limit;

    typedef std::vector<IntType> buffer_t;
    buffer_t * cur_buffer;
    std::vector<buffer_t> buffer_pool;
    std::vector<buffer_t*> free_buffers;

    bool prepare_cur_buffer() {
        if(free_buffers.size() < 2)
            return false;
        allocate_cur_buffer();
        return true;
    }

    void prepare_cur_buffer_for_collapse() {
        assert(free_buffers.size() == 1);
        allocate_cur_buffer();
    }

    void allocate_cur_buffer() {
        cur_buffer = free_buffers.back();
        free_buffers.pop_back();
        cur_buffer->clear();
    }

    void free_buffer(buffer_t * b) {
        free_buffers.push_back(b);
    }

    typedef std::deque<buffer_t*> layer_t;
    typedef std::deque<layer_t> layers_t;
    layers_t layers;
    typedef typename layers_t::iterator layer_iter_t;
    std::vector<layer_iter_t> collapsable_layers;

    void insert_cur_buffer_to_layers(layer_iter_t iter) {
        assert(iter != layers.end());
        iter->push_back(cur_buffer);
        /*
         * Tricky:
         * There are 2 possible situations when we insert cur_buffer to layers:
         * - a currently full buffer, which is inserted to the bottom layer
         * - a just merged buffer, which is inserted to the layer that is one level higher (than it was before merging)
         *
         * For the 1st case, if the size of the bottom layer become 2, we should mark it as collapsable and insert to the stack
         * For the 2nd case, since we always merge buffers from the lowest collapsable layer. And we always pop the old collapsable layer from the stack before inserting the merged cur_buffer (the old collapsable layer might be still collapsable after merging!), so this is safe
         */
        if(iter->size() == 2)
            collapsable_layers.push_back(iter);
    }

    struct HeapEntry{
        typename buffer_t::iterator iter, iterend;
        long long weight;
        HeapEntry(const typename buffer_t::iterator & _iter, const typename buffer_t::iterator & _iterend, long long _weight)
            :iter(_iter), iterend(_iterend), weight(_weight) { }
    };

    std::vector<HeapEntry> heap;
    std::vector<QuantileEntry> summary;
    long long summary_weight_sum;

    boost::mt11213b generator;
    typedef boost::mt11213b::result_type random_int_t;
    /* 
     * For every sample_step items, we only choose one out of them
     * next_new_sample: how much items left before we choose a new sample
     * next_sampled: how much items left before the next sampled item
     */
    long long next_new_sample;
    long long next_sampled;

    /*
     * we always use 2^-i (for some i) as the sample rate
     * prob_mask_width: i (as in above)
     */
    random_int_t prob_mask;
    int prob_mask_width;
    random_int_t prob_bits;
    int prob_bits_length;

    bool next_is_sampled(void) {
        if(prob_mask_width == 0) // no sample, choose all elements
            return true;
        if(next_new_sample == 0) // need to calculate the position of the next sample
        {
            if(prob_mask_width > prob_bits_length) // not enough bits
            {
                assert(prob_mask_width <= (long long)sizeof(random_int_t)*8);
                prob_bits = generator();
                prob_bits_length = sizeof(random_int_t)*8;
            }
            next_sampled = (prob_bits & prob_mask);
            prob_bits >>= prob_mask_width;
            prob_bits_length -= prob_mask_width;
            next_new_sample = prob_mask;
        }
        else
        {
            -- next_new_sample;
        }
        return ((next_sampled--) == 0);
    }
    random_int_t coin_bits;
    int coin_bits_length;
    bool toss_coin(void) 
    {
        if(coin_bits_length == 0)
        {
            coin_bits_length = 8 * sizeof(random_int_t);
            coin_bits = generator();
        }
        bool r = (coin_bits & 1);
        coin_bits >>= 1;
        -- coin_bits_length;
        return r;
    }
};
#endif //RANDOM_H__
