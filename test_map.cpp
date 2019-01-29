#include <iostream>
#include <gtest/gtest.h>
#include <map>
#include "separate_chaining_map.hpp"
#include "keysplit_adapter.hpp"
#include <algorithm>
#include "bijective_hash.hpp"

using bijective_hash = poplar::bijective_hash::Xorshift;



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
   const size_t size = map.size();
   for(size_t i = 0; i < max_key; ++i) {
      map[i] = i;
   }
   ASSERT_EQ(map.size(), size);
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
   const size_t size = map.size();
   for(size_t i = 0; i < max_key; ++i) {
      map[max_key-i] = i;
   }
   ASSERT_EQ(map.size(), size);
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
	 map[key] = rev[key] = val;
	 ASSERT_EQ(map.size(), rev.size());
      }
      for(auto el : rev) {
	 ASSERT_EQ( map.find(el.first)->second, el.second);
      }
   }
}

template<class T>
void test_random_large(T& map) {
   using key_type = typename T::key_type;
   using value_type = typename T::value_type;
   const uint64_t max_key = map.max_key();
   constexpr uint64_t max_value = std::numeric_limits<value_type>::max();

   std::map<typename T::key_type, typename T::value_type> rev;
   for(size_t i = 0; i < 1000000; ++i) {
      const key_type key = random_int<key_type>(max_key);
      const value_type val = random_int<key_type>(max_value);
      map[key] = rev[key] = val;
      ASSERT_EQ(map.size(), rev.size());
   }
   for(auto el : rev) {
      ASSERT_EQ( map.find(el.first)->second, el.second);
   }
}



#define TEST_MAP(x,y) \
   TEST(x, id) { y; test_id(map); } \
   TEST(x, reverse) { y; test_rev(map); } \
   TEST(x, random) { y; test_random(map); } \
   TEST(x, random_large) { y; test_random_large(map); } \


#define COMMA ,

TEST_MAP(keysplit_adapter64, keysplit_adapter64<separate_chaining_map<varwidth_key_bucket COMMA uint16_t COMMA hash_mapping_adapter<uint64_t COMMA SplitMix>> COMMA separate_chaining_map<plain_key_bucket<uint64_t> COMMA uint16_t COMMA hash_mapping_adapter<uint64_t COMMA SplitMix>> > map)

TEST_MAP(keysplit_adapter, keysplit_adapter<separate_chaining_map<varwidth_key_bucket COMMA uint16_t COMMA hash_mapping_adapter<uint64_t COMMA SplitMix>>> map)

TEST_MAP(map_plain_16,  separate_chaining_map<plain_key_bucket<uint32_t> COMMA uint16_t COMMA hash_mapping_adapter<uint32_t COMMA SplitMix>> map)
TEST_MAP(map_plain_32,  separate_chaining_map<plain_key_bucket<uint32_t> COMMA uint32_t COMMA hash_mapping_adapter<uint32_t COMMA SplitMix>> map)
TEST_MAP(map_plain_Xor, separate_chaining_map<plain_key_bucket<uint32_t> COMMA uint32_t COMMA xorshift_hash> map(32))

TEST_MAP(map_var_16,  separate_chaining_map<varwidth_key_bucket COMMA uint16_t COMMA hash_mapping_adapter<uint64_t COMMA SplitMix>> map)
TEST_MAP(map_var_32,  separate_chaining_map<varwidth_key_bucket COMMA uint32_t COMMA hash_mapping_adapter<uint64_t COMMA SplitMix>> map)
TEST_MAP(map_var_Xor, separate_chaining_map<varwidth_key_bucket COMMA uint32_t COMMA xorshift_hash> map(32))



// TEST_MAP(separate_chaining_map_int, separate_chaining_map<uint64_t COMMA uint16_t> map(27))
// TEST_MAP(compact_separate_chaining_map, compact_separate_chaining_map<uint16_t COMMA bijective_hash> map(27))

TEST(separate_chaining_map, step) {
   constexpr size_t NUM_ELEMENTS = 1000000;
   constexpr size_t NUM_RANGE = 32;

   std::unordered_map<uint64_t,uint64_t> rev;
   separate_chaining_map<plain_key_bucket<uint32_t>, uint64_t, hash_mapping_adapter<uint32_t, SplitMix>> map(NUM_RANGE);
   for(size_t i = 0; i < NUM_ELEMENTS; ++i) {
      size_t key= random_int(1ULL<<NUM_RANGE);
      map[key] = rev[key] = i;
      ASSERT_EQ(map.size(), rev.size());
   }
   for(auto el : rev) {
      ASSERT_EQ( map.find(el.first)->second, el.second);
   }

}

int main(int argc, char **argv) {

   ::testing::InitGoogleTest(&argc, argv);
   return RUN_ALL_TESTS();
}
