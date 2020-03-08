#pragma once

#include <iostream>
#include <gtest/gtest.h>
#include <map>
#include <algorithm>
#include <separate/bijective_hash.hpp>

#define COMMA ,

using namespace separate_chaining;
using bijective_hash = bijective_hash::Xorshift;

template<class T>
T random_int(const T& maxvalue) {
   return static_cast<T>(std::rand() * (1.0 / (RAND_MAX + 1.0 )) * maxvalue);
}



template<class T>
void test_map_outlier(T& map) {
   const uint_fast8_t max_bits = map.key_width();
   for(size_t i = 0; i < max_bits; ++i) {
      map[1ULL<<i] = i;
      ASSERT_EQ(map.size(), i+1);
   }
   for(size_t i = 0; i < max_bits; ++i) { //idempotent
      map[1ULL<<i] = i;
      ASSERT_EQ(map.size(), max_bits);
   }
   map[map.max_key()] = max_bits;
   for(size_t i = 0; i < max_bits; ++i) {
      ASSERT_EQ(map[1ULL<<i], i);
   }
   ASSERT_EQ(map[map.max_key()], max_bits);
   ASSERT_EQ(map.erase(map.max_key()), 1ULL);
   for(size_t i = 0; i < max_bits; ++i) {
      ASSERT_EQ(map.erase(1ULL<<i), 1ULL);
   }
   ASSERT_EQ(map.size(), 0ULL);
   for(size_t i = 0; i < max_bits; ++i) { //refill
      map[1ULL<<i] = i;
      ASSERT_EQ(map.size(), i+1);
   }
   for(size_t i = 0; i < max_bits; ++i) {
      ASSERT_EQ(map[1ULL<<i], i);
   }
}

template<class T>
void test_map_iterator(T& map) {
   const uint64_t max_key = std::min<uint64_t>(std::numeric_limits<uint8_t>::max(), map.max_key());
   const uint64_t max_value = map.max_value();
   DDCHECK_LE(max_key, std::numeric_limits<typename T::value_type>::max());

   for(size_t i = 0; i < max_key; ++i) {
      map[i] = (max_key-i) % max_value;
   }
   for(size_t i = 0; i < max_key; ++i) {
      ASSERT_EQ(map[i], (max_key-i) % max_value);
      ASSERT_EQ(map[i], map[i]);
   }
   map.shrink_to_fit();
   for(const auto& el : map) {
      ASSERT_EQ(el.second, (max_key - el.first) % max_value);
   }
   for(auto it = map.begin(); it != map.end(); ++it) {
      ASSERT_EQ(it->second, (max_key - it->first) % max_value);
   }
   for(auto it = map.rbegin_nav(); it != map.rend_nav(); --it) {
      ASSERT_EQ(it.value(), (max_key - it.key()) % max_value);
   }

   const size_t size = map.size();
   ASSERT_EQ(map.size(), size);
   for(size_t i = 0; i < max_key; ++i) {
      ASSERT_EQ(map.erase(i), 1ULL);
   }
   ASSERT_EQ(map.size(), 0ULL);
}

template<class T>
void test_map_id(T& map) {
   const uint64_t max_key = std::min<uint64_t>(std::numeric_limits<uint16_t>::max(), map.max_key());
   const uint64_t max_value = map.max_value();
   DDCHECK_LE(max_key, std::numeric_limits<typename T::value_type>::max());

   for(size_t i = 0; i < max_key; ++i) {
      map[i] = i % max_value;
   }
   for(size_t i = 0; i < max_key; ++i) {
      ASSERT_EQ(map[i], i % max_value);
      ASSERT_EQ(map[i], map[i]);
   }
   map.shrink_to_fit();
   const size_t size = map.size();
   for(size_t i = 0; i < max_key; ++i) {
      map[i] = i % max_value;
   }
   ASSERT_EQ(map.size(), size);
   for(size_t i = 0; i < max_key; ++i) {
      ASSERT_EQ(map.erase(i),1ULL);
   }
   ASSERT_EQ(map.size(), 0ULL);
}
template<class T>
void test_map_reverse(T& map) {
   const uint64_t max_key = std::min<uint64_t>(std::numeric_limits<uint16_t>::max(), map.max_key());
   const uint64_t max_value = map.max_value();
   DDCHECK_LE(max_key, std::numeric_limits<typename T::value_type>::max());

   for(size_t i = 0; i < max_key; ++i) {
      map[max_key-i] = i % max_value;
   }
   for(size_t i = 0; i < max_key; ++i) {
      ASSERT_EQ(map[max_key-i], i % max_value);
   }
   const size_t size = map.size();
   //idempotent
   for(size_t i = 0; i < max_key; ++i) {
      map[max_key-i] = i % max_value;
   }
   ASSERT_EQ(map.size(), size);
   //erase
   for(size_t i = 0; i < max_key; ++i) {
      ASSERT_EQ(map.erase(max_key-i), 1ULL);
   }
   ASSERT_EQ(map.size(), 0ULL);
}

template<class T>
void test_map_random_serialize(T& map) {
   using key_type = typename T::key_type;
   using value_type = typename T::value_type;
   const uint64_t max_key = map.max_key();
   const uint64_t max_value = map.max_value();
   for(size_t reps = 0; reps < 100; ++reps) {
      map.clear();
      for(size_t i = 0; i < 100; ++i) {
	 const key_type key = random_int<key_type>(max_key);
	 const value_type val = random_int<value_type>(max_value);
	 map[key] = val;
	 if(i % 13) { map.erase(random_int<key_type>(max_key)); }
      }
      std::stringstream ss(std::ios_base::in | std::ios_base::out | std::ios::binary);
      map.serialize(ss);
      T map2;
      ss.seekg(0);
      map2.deserialize(ss);
      DDCHECK_EQ(map.key_width(), map2.key_width());
      DDCHECK_EQ(map.size(), map2.size());
      DDCHECK_EQ(map.bucket_count(), map2.bucket_count());

      for(size_t i = 0; i < map.bucket_count(); ++i) {
	 DDCHECK_EQ(map.bucket_size(i), map2.bucket_size(i));
      }

      for(auto el : map2) {
	 auto it = map.find(el.first);
	 ASSERT_NE(it, map.end());
	 ASSERT_EQ( it->second, el.second);
      }
      for(auto el : map) {
	 auto it = map2.find(el.first);
	 ASSERT_NE(it, map.end());
	 ASSERT_EQ( it->second, el.second);
      }
   }
}

template<class T>
void test_map_random(T& map) {
   using key_type = typename T::key_type;
   using value_type = typename T::value_type;
   const uint64_t max_key = map.max_key();
   const uint64_t max_value = map.max_value();
   for(size_t reps = 0; reps < 100; ++reps) {

      map.clear();
      std::map<typename T::key_type, typename T::value_type> rev;
      for(size_t i = 0; i < 100; ++i) {
	 const key_type key = random_int<key_type>(max_key);
	 const value_type val = random_int<value_type>(max_value);
	 map[key] = rev[key] = val;
	 ASSERT_EQ(map.size(), rev.size());
	 if(! (i % 13)) map.shrink_to_fit();
      }
      for(size_t i = 0; i < 100; ++i) {
	 const key_type key = random_int<key_type>(max_key);
	 const typename T::size_type removed_elements = rev.erase(key);
	 ASSERT_EQ(map.erase(key), removed_elements);
	 ASSERT_EQ(map.size(), rev.size());
      }
      for(auto el : rev) {
	 auto it = map.find(el.first);
	 ASSERT_NE(it, map.end());
	 ASSERT_EQ( it->second, el.second);
      }
      for(size_t i = 0; i < 100; ++i) {
	 const key_type key = random_int<key_type>(max_key);
	 if(rev.find(key) == rev.end()) {
	    auto it = map.find(key);
	    ASSERT_EQ(it, map.end());
	 } else {
	    ASSERT_EQ(map.find(key)->first, rev.find(key)->first);
	    ASSERT_EQ(map.find(key)->second, rev.find(key)->second);
	 }
      }
   }
}

template<class T>
void test_map_random_large(T& map) {
   using key_type = typename T::key_type;
   using value_type = typename T::value_type;
   const uint64_t max_key = map.max_key();
   const uint64_t max_value = map.max_value();

   std::map<typename T::key_type, typename T::value_type> rev;
   for(size_t i = 0; i < 1000000; ++i) {
      const key_type key = random_int<key_type>(max_key);
      const value_type val = random_int<value_type>(max_value);
      map[key] = rev[key] = val;
      ASSERT_EQ(map.size(), rev.size());
   }
   for(auto el : rev) {
      ASSERT_EQ( map.find(el.first)->second, el.second);
   }
}




// template<class T>
// void test_map_id_value_resticted(T& map, const uint64_t max_value) {
//    const uint64_t max_key = std::min<uint64_t>(max_value, std::min<uint64_t>(std::numeric_limits<uint16_t>::max(), map.max_key()));
//
//    for(size_t i = 0; i < max_key; ++i) {
//       map[i] = i;
//    }
//    for(size_t i = 0; i < max_key; ++i) {
//       ASSERT_EQ(map[i], i);
//       ASSERT_EQ(map[i], map[i]);
//    }
//    map.shrink_to_fit();
//    const size_t size = map.size();
//    for(size_t i = 0; i < max_key; ++i) {
//       map[i] = i;
//    }
//    ASSERT_EQ(map.size(), size);
//    for(size_t i = 0; i < max_key; ++i) {
//       ASSERT_EQ(map.erase(i),1ULL);
//    }
//    ASSERT_EQ(map.size(), 0ULL);
// }
// template<uint8_t value_bitwidth>
// void test_fixwidth_bucket() {
//    using storage_type = uint8_t;
//    using key_type = uint16_t;
//    separate_chaining_map<plain_bucket<storage_type>, fixwidth_bucket<value_bitwidth, storage_type>, xorshift_hash<key_type, storage_type>, incremental_resize> small_map;
//    small_map.reserve(1ULL<<8);
//    test_map_id_value_resticted(small_map, 1ULL<<value_bitwidth);
// }
//
//
// TEST(bucket, fixwidth_bucket) {
//    test_fixwidth_bucket<3>();
//    test_fixwidth_bucket<4>();
//    test_fixwidth_bucket<7>();
//    test_fixwidth_bucket<8>();
//    test_fixwidth_bucket<14>();
//    test_fixwidth_bucket<16>();
// }
//


#define TEST_SMALL_MAP(x,y) \
   TEST(x, iterator) { y; test_map_iterator(map); } \
   TEST(x, outlier) { y; test_map_outlier(map); } \
   TEST(x, random) { y; test_map_random(map); }

#define TEST_MAP(x,y) \
   TEST(x, id) { y; test_map_id(map); } \
   TEST(x, reverse) { y; test_map_reverse(map); } \
   TEST(x, outlier) { y; test_map_outlier(map); } \
   TEST(x, random) { y; test_map_random(map); } \
   TEST(x, random_large) { y; test_map_random_large(map); }

#define TEST_MAP_FULL(x,y) \
   TEST(x, serialize) { y; test_map_random_serialize(map); } \
   TEST(x, id) { y; test_map_id(map); } \
   TEST(x, reverse) { y; test_map_reverse(map); } \
   TEST(x, outlier) { y; test_map_outlier(map); } \
   TEST(x, random) { y; test_map_random(map); } \
   TEST(x, random_large) { y; test_map_random_large(map); } 


int main(int argc, char **argv) {

   ::testing::InitGoogleTest(&argc, argv);
   return RUN_ALL_TESTS();
}
