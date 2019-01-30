#define STATS_ENABLED 1
#include <iostream>
#include <chrono>
#include <thread>
#include <cstdint>

#include <tudocomp_stat/StatPhase.hpp>
#include <tudocomp/ds/uint_t.hpp>

#include <unordered_map>
#include <map>

#include "separate_chaining_map.hpp"
#include "keysplit_adapter.hpp"
#include <tudocomp/util/compact_hash.hpp>
#include <tudocomp/util/compact_displacement_hash.hpp>
#include <tudocomp/util/compact_sparse_displacement_hash.hpp>
#include <tudocomp/util/compact_sparse_hash.hpp>

#include "/home/niki/code/prefixsearch/program/TPackedTrie/rigtorp/HashMap.h"

template<class T>
T random_int(const T& maxvalue) {
   return static_cast<T>(std::rand() * (1.0 / (RAND_MAX + 1.0 )) * maxvalue);
}

using value_type = uint64_t;
using key_type = uint32_t;

constexpr size_t NUM_ELEMENTS = 2000000;
constexpr size_t NUM_RANGE = 32; // most_significant_bit(NUM_ELEMENTS)
static_assert(sizeof(key_type)*8 >= NUM_RANGE, "Num range must fit into key_type");

template<class T>
void fill(T& map) {
    for(size_t i = 0; i < NUM_ELEMENTS; ++i) {
        map[random_int(1ULL<<NUM_RANGE)] = i;
    }
}

template<class T,class U>
void copy(const T& orig, U& copied) {
   for(auto el : orig) {
      copied[el.first] = el.second;
   }
}

// template<typename val_t> using elias_type = tdc::compact_sparse_hashmap::compact_elias_displacement_hashmap_t<val_t>;
// template<typename val_t> using elias_type = tdc::compact_sparse_hashmap::compact_elias_displacement_hashmap_t<val_t>;
//
// using namespace tdc::compact_sparse_hashmap;
//
// using compact_sparse_displacement_hashmap_t = generic_hashmap_t<poplar_xorshift_t, 
//       buckets_bv_t<val_t>, displacement_t<compact_displacement_table_t<4>> >;
//
//
// using compact_hashmap_t = generic_hashmap_t<hash_t, plain_sentinel_t<val_t>, cv_bvs_t >;
//
// tdc::compact_sparse_hashmap::generic_hashmap_t<poplar_xorshift_t, 

int main() {
  free(malloc(42));

  std::unordered_map<key_type,value_type> rev;
  fill(rev);




    tdc::StatPhase root("construction");
    root.log_stat("elements", rev.size());

    {
        tdc::StatPhase v("lower bound");
        key_type* keys = new key_type[rev.size()];
        value_type* values = new value_type[rev.size()];
        size_t i = 0;
        for(auto el : rev) {
           keys[i] = el.first;
           values[i] = el.second;
        }
        delete [] keys;
        delete [] values;
    }
    {
        tdc::StatPhase v("tudo elias");
        tdc::compact_sparse_hashmap::compact_sparse_elias_displacement_hashmap_t<value_type> map(0,NUM_RANGE);

        copy(rev,map);
    }
    {
        tdc::StatPhase v("tudo cleary");
        tdc::compact_sparse_hashmap::compact_sparse_hashmap_t<value_type> map(0,NUM_RANGE);

        copy(rev,map);
    }
    {
        tdc::StatPhase v("tudo layered");
        tdc::compact_sparse_hashmap::compact_sparse_displacement_hashmap_t<value_type> map(0,NUM_RANGE);
        copy(rev,map);
    }

    {
        tdc::StatPhase v("compact");
        separate_chaining_map<varwidth_key_bucket, plain_key_bucket<value_type>, xorshift_hash> map(NUM_RANGE);
        copy(rev,map);
        tdc::StatPhase finalize("finalize");
        map.print_stats(finalize);
    }
    {
        tdc::StatPhase v("plain 32");
        separate_chaining_map<plain_key_bucket<key_type>, plain_key_bucket<value_type>, hash_mapping_adapter<key_type, SplitMix>> map(NUM_RANGE);
        copy(rev,map);
        tdc::StatPhase finalize("finalize");
        map.print_stats(finalize);
    }
    {
        tdc::StatPhase v("split key");
        keysplit_adapter<separate_chaining_map<varwidth_key_bucket, plain_key_bucket<uint16_t>, hash_mapping_adapter<value_type, SplitMix>>> map;
        copy(rev,map);
    }


    {
        tdc::StatPhase v("vector reserved");
        std::vector<std::pair<key_type,value_type>> map;
        map.reserve(rev.size());
        for(auto el : rev) {
           map.push_back(el);
        }
    }
    {
        tdc::StatPhase v("vector");
        std::vector<std::pair<key_type,value_type>> map;
        for(auto el : rev) {
           map.push_back(el);
        }
    }
    {
        tdc::StatPhase v("rigtorp");
        rigtorp::HashMap<key_type, value_type, SplitMix> map;
        // std::unordered_map<key_type,value_type> map;
        copy(rev,map);
    }
    {
        tdc::StatPhase v("unordered_map");
        std::unordered_map<key_type,value_type> map;
        copy(rev,map);
    }
    {
        tdc::StatPhase v("map");
        std::map<key_type,value_type> map;
        copy(rev,map);
    }

    std::cout << root.to_json().dump(4) << "\n";
}
