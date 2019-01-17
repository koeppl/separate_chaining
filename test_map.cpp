#include <iostream>
#include <gtest/gtest.h>
#include <map>
#include "separate_chaining_map.hpp"
#include "separate_chaining_map_int.hpp"
#include "compact_separate_chaining.hpp"
#include "keysplit_adapter.hpp"
#include <algorithm>
#include "bijective_hash.hpp"

using bijective_hash = poplar::bijective_hash::Xorshift;

class SplitMix { // from http://zimbry.blogspot.com/2011/09/better-bit-mixing-improving-on.html
   public:
   uint64_t operator()(uint64_t x) const {
      x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
      x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
      x = x ^ (x >> 31);
      return x;
   }
};


template<class T>
T random_int(const T& maxvalue) {
   return static_cast<T>(std::rand() * (1.0 / (RAND_MAX + 1.0 )) * maxvalue);
}


template<class T>
void test_id(T& map) {
   const uint64_t max_key = std::min<uint64_t>(std::numeric_limits<uint16_t>::max(), map.max_key());
   DCHECK_LE(max_key, std::numeric_limits<typename T::value_type>::max());

   for(size_t i = 0; i < max_key; ++i) {
      map[i] = i;
   }
   for(size_t i = 0; i < max_key; ++i) {
      ASSERT_EQ(map[i], i);
      ASSERT_EQ(map[i], map[i]);
   }
}
template<class T>
void test_rev(T& map) {
   const uint64_t max_key = std::min<uint64_t>(std::numeric_limits<uint16_t>::max(), map.max_key());
   DCHECK_LE(max_key, std::numeric_limits<typename T::value_type>::max());

   for(size_t i = 0; i < max_key; ++i) {
      map[max_key-i] = i;
   }
   for(size_t i = 0; i < max_key; ++i) {
      ASSERT_EQ(map[max_key-i], i);
   }
}


template<class T>
void test_random(T& map) {
   using key_type = typename T::key_type;
   using value_type = typename T::value_type;
   const uint64_t max_key = map.max_key();
   constexpr uint64_t max_value = std::numeric_limits<value_type>::max();
   for(size_t reps = 0; reps < 1000; ++reps) {

      map.clear();
      std::map<typename T::key_type, typename T::value_type> rev;
      for(size_t i = 0; i < 1000; ++i) {
	 const key_type key = random_int<key_type>(max_key);
	 const value_type val = random_int<key_type>(max_value);
	 rev[key] = map[key] = val;
      }
      for(auto el : rev) {
	 ASSERT_EQ( map.find(el.first)->second, el.second);
      }
   }
}

#define TEST_MAP(x,y) \
   TEST(x, id) { y; test_id(map); } \
   TEST(x, reverse) { y; test_rev(map); } \
   TEST(x, random) { y; test_random(map); }

#define COMMA ,
TEST_MAP(separate_chaining_map, separate_chaining_map<uint16_t COMMA uint16_t> map)
TEST_MAP(keysplit_adapter, keysplit_adapter<separate_chaining_map<uint64_t COMMA uint16_t COMMA SplitMix>> map)
TEST_MAP(separate_chaining_map_int, separate_chaining_map<uint64_t COMMA uint16_t> map(27))
TEST_MAP(compact_separate_chaining_map, compact_separate_chaining_map<uint16_t COMMA bijective_hash> map(27))


int main(int argc, char **argv) {

   {
   compact_separate_chaining_map<uint64_t,bijective_hash> map(27);
   for(size_t i = 0; i < std::numeric_limits<uint16_t>::max(); ++i) {
      map[i] = i;
   }
   for(size_t i = 0; i < std::numeric_limits<uint16_t>::max(); ++i) {
      DCHECK_EQ(map[i], i);
      DCHECK_EQ(map[i], map[i]);
   }
   }


   ::testing::InitGoogleTest(&argc, argv);
   return RUN_ALL_TESTS();
}
