#include <iostream>
#include <gtest/gtest.h>
#include <map>
#include <algorithm>
#include <separate/separate_chaining_table.hpp>
#include <separate/keysplit_adapter.hpp>
#include <separate/bijective_hash.hpp>
#include <separate/bucket_table.hpp>

using namespace separate_chaining;
using bijective_hash = poplar::bijective_hash::Xorshift;


#define COMMA ,

template<class T>
T random_int(const T& maxvalue) {
   return static_cast<T>(std::rand() * (1.0 / (RAND_MAX + 1.0 )) * maxvalue);
}


template<class T>
void test_map_outlier(T& map) {
   const uint_fast8_t max_bits = map.key_bit_width();
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
   DCHECK_LE(max_key, std::numeric_limits<typename T::value_type>::max());

   for(size_t i = 0; i < max_key; ++i) {
      map[i] = max_key-i;
   }
   for(size_t i = 0; i < max_key; ++i) {
      ASSERT_EQ(map[i], max_key-i);
      ASSERT_EQ(map[i], map[i]);
   }
   map.shrink_to_fit();
   for(const auto& el : map) {
      ASSERT_EQ(el.first, max_key - el.second);
   }
   for(auto it = map.begin(); it != map.end(); ++it) {
      ASSERT_EQ(it->first, max_key - it->second);
   }
   for(auto it = map.rbegin_nav(); it != map.rend_nav(); --it) {
      ASSERT_EQ(it.key(), max_key - it.value());
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
   DCHECK_LE(max_key, std::numeric_limits<typename T::value_type>::max());

   for(size_t i = 0; i < max_key; ++i) {
      map[i] = i;
   }
   for(size_t i = 0; i < max_key; ++i) {
      ASSERT_EQ(map[i], i);
      ASSERT_EQ(map[i], map[i]);
   }
   map.shrink_to_fit();
   const size_t size = map.size();
   for(size_t i = 0; i < max_key; ++i) {
      map[i] = i;
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
   DCHECK_LE(max_key, std::numeric_limits<typename T::value_type>::max());

   for(size_t i = 0; i < max_key; ++i) {
      map[max_key-i] = i;
   }
   for(size_t i = 0; i < max_key; ++i) {
      ASSERT_EQ(map[max_key-i], i);
   }
   const size_t size = map.size();
   //idempotent
   for(size_t i = 0; i < max_key; ++i) {
      map[max_key-i] = i;
   }
   ASSERT_EQ(map.size(), size);
   //erase
   for(size_t i = 0; i < max_key; ++i) {
      ASSERT_EQ(map.erase(max_key-i), 1ULL);
   }
   ASSERT_EQ(map.size(), 0ULL);
}


template<class T>
void test_map_random(T& map) {
   using key_type = typename T::key_type;
   using value_type = typename T::value_type;
   const uint64_t max_key = map.max_key();
   constexpr uint64_t max_value = std::numeric_limits<value_type>::max();
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
   constexpr uint64_t max_value = std::numeric_limits<value_type>::max();

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


TEST(map, quotienting) {
   using storage_type = uint8_t;
   using key_type = uint16_t;
   using value_type = key_type;
   separate_chaining_map<plain_bucket<storage_type>, plain_bucket<value_type>, xorshift_hash<key_type, storage_type>, incremental_resize> small_map;
   small_map.reserve(1ULL<<8);
   test_map_id(small_map);

}

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



TEST_MAP(map_var_Xor_64, separate_chaining_map<varwidth_bucket COMMA plain_bucket<uint32_t> COMMA xorshift_hash<> COMMA incremental_resize> map(64))

TEST_MAP(map_avx2_16_16,  separate_chaining_map<avx2_bucket<uint16_t> COMMA plain_bucket<uint16_t> COMMA hash_mapping_adapter<uint16_t COMMA SplitMix> COMMA incremental_resize > map)
TEST_MAP(map_avx2_8_16,  separate_chaining_map<avx2_bucket<uint8_t> COMMA plain_bucket<uint16_t> COMMA hash_mapping_adapter<uint8_t COMMA SplitMix> COMMA incremental_resize> map)

TEST_MAP(map_avx2_32_16,  separate_chaining_map<avx2_bucket<uint32_t> COMMA plain_bucket<uint16_t> COMMA hash_mapping_adapter<uint32_t COMMA SplitMix> COMMA incremental_resize > map)
TEST_MAP(map_avx2_32_32,  separate_chaining_map<avx2_bucket<uint32_t> COMMA plain_bucket<uint32_t> COMMA hash_mapping_adapter<uint32_t COMMA SplitMix> COMMA incremental_resize > map)
TEST_MAP(map_avx2_32_Xor, separate_chaining_map<avx2_bucket<uint32_t> COMMA plain_bucket<uint32_t> COMMA xorshift_hash<uint32_t> COMMA incremental_resize> map(32))

TEST_MAP(map_avx2_64_16,  separate_chaining_map<avx2_bucket<uint64_t> COMMA plain_bucket<uint16_t> COMMA hash_mapping_adapter<uint64_t COMMA SplitMix> COMMA incremental_resize > map)
TEST_MAP(map_avx2_64_32,  separate_chaining_map<avx2_bucket<uint64_t> COMMA plain_bucket<uint32_t> COMMA hash_mapping_adapter<uint64_t COMMA SplitMix> COMMA incremental_resize> map)
TEST_MAP(map_avx2_64_Xor, separate_chaining_map<avx2_bucket<uint64_t> COMMA plain_bucket<uint32_t> COMMA xorshift_hash<uint64_t> COMMA incremental_resize> map(32))


TEST_MAP(map_plain_class32,  separate_chaining_map<class_bucket<uint32_t> COMMA class_bucket<uint32_t> COMMA hash_mapping_adapter<uint32_t COMMA SplitMix> COMMA incremental_resize> map)

TEST_MAP(map_plain_16,  separate_chaining_map<plain_bucket<uint32_t> COMMA plain_bucket<uint16_t> COMMA hash_mapping_adapter<uint32_t COMMA SplitMix> COMMA incremental_resize> map)
TEST_MAP(map_plain_32,  separate_chaining_map<plain_bucket<uint32_t> COMMA plain_bucket<uint32_t> COMMA hash_mapping_adapter<uint32_t COMMA SplitMix> COMMA incremental_resize> map)
TEST_MAP(map_plain_Xor, separate_chaining_map<plain_bucket<uint32_t> COMMA plain_bucket<uint32_t> COMMA xorshift_hash<uint32_t> COMMA incremental_resize> map(32))


TEST_MAP(map_var_16,  separate_chaining_map<varwidth_bucket COMMA plain_bucket<uint16_t> COMMA hash_mapping_adapter<uint64_t COMMA SplitMix> COMMA incremental_resize> map)
TEST_MAP(map_var_32,  separate_chaining_map<varwidth_bucket COMMA plain_bucket<uint32_t> COMMA hash_mapping_adapter<uint64_t COMMA SplitMix> COMMA incremental_resize> map)
TEST_MAP(map_var_Xor, separate_chaining_map<varwidth_bucket COMMA plain_bucket<uint32_t> COMMA xorshift_hash<uint64_t> COMMA incremental_resize> map(32))


TEST_MAP(map_avx2_16_arb_16,  separate_chaining_map<avx2_bucket<uint16_t> COMMA plain_bucket<uint16_t> COMMA hash_mapping_adapter<uint16_t COMMA SplitMix> COMMA arbitrary_resize> map)
TEST_MAP(map_plain_arb_16,  separate_chaining_map<plain_bucket<uint32_t> COMMA plain_bucket<uint16_t> COMMA hash_mapping_adapter<uint32_t COMMA SplitMix> COMMA arbitrary_resize> map)
TEST_MAP(map_var_arb_16,  separate_chaining_map<varwidth_bucket COMMA plain_bucket<uint16_t> COMMA hash_mapping_adapter<uint64_t COMMA SplitMix> COMMA arbitrary_resize> map)

TEST_SMALL_MAP(map_plain_small32,  separate_chaining_map<plain_bucket<uint32_t> COMMA plain_bucket<uint32_t> COMMA hash_mapping_adapter<uint32_t COMMA SplitMix> COMMA incremental_resize> map)
TEST_SMALL_MAP(map_bucket_plain_arb_16,  bucket_table<plain_bucket<uint32_t> COMMA plain_bucket<uint16_t> COMMA arbitrary_resize_bucket> map)
TEST_SMALL_MAP(map_bucket_var_arb_16,    bucket_table<varwidth_bucket COMMA plain_bucket<uint16_t> COMMA arbitrary_resize_bucket> map)
TEST_SMALL_MAP(map_bucket_plain_16,  bucket_table<plain_bucket<uint32_t> COMMA plain_bucket<uint16_t> COMMA incremental_resize> map)
TEST_SMALL_MAP(map_bucket_var_16,    bucket_table<varwidth_bucket COMMA plain_bucket<uint16_t> COMMA incremental_resize> map)


TEST_MAP(keysplit_adapter64, keysplit_adapter64<separate_chaining_map<varwidth_bucket COMMA plain_bucket<uint16_t> COMMA hash_mapping_adapter<uint64_t COMMA SplitMix>> COMMA separate_chaining_map<plain_bucket<uint64_t> COMMA plain_bucket<uint16_t> COMMA hash_mapping_adapter<uint64_t COMMA SplitMix>> > map)

TEST_MAP(keysplit_adapter, keysplit_adapter<separate_chaining_map<varwidth_bucket COMMA plain_bucket<uint16_t> COMMA hash_mapping_adapter<uint64_t COMMA SplitMix>>> map)



// TEST_MAP(separate_chaining_map_int, separate_chaining_map<uint64_t COMMA uint16_t> map(27))
// TEST_MAP(compact_separate_chaining_map, compact_separate_chaining_map<uint16_t COMMA bijective_hash> map(27))

TEST(separate_chaining_map, step) {
   constexpr size_t NUM_ELEMENTS = 1000000;
   constexpr size_t NUM_RANGE = 32;

   std::unordered_map<uint64_t,uint64_t> rev;
   separate_chaining_map<plain_bucket<uint32_t>, plain_bucket<uint64_t>, hash_mapping_adapter<uint32_t, SplitMix>> map(NUM_RANGE);
   for(size_t i = 0; i < NUM_ELEMENTS; ++i) {
      size_t key= random_int(1ULL<<NUM_RANGE);
      map[key] = rev[key] = i;
      ASSERT_EQ(map.size(), rev.size());
   }
   for(auto el : rev) {
      ASSERT_EQ( map.find(el.first)->second, el.second);
   }

}


template<class T>
void test_set_random(T& set) {
   using key_type = typename T::key_type;
   const uint64_t max_key = set.max_key();
   for(size_t reps = 0; reps < 1000; ++reps) {

      set.clear();
      std::set<typename T::key_type> rev;
      for(size_t i = 0; i < 1000; ++i) {
	 const key_type key = random_int<key_type>(max_key);
	 rev.insert(key); 
	 set[key];
	 ASSERT_EQ(set.size(), rev.size());
      }
      for(auto el : rev) {
	 ASSERT_NE( set.find(el), set.end());
      }
      for(size_t i = 0; i < 1000; ++i) {
	 const key_type key = random_int<key_type>(max_key);
	 if(rev.find(key) == rev.end()) {
	    ASSERT_EQ(set.find(key), set.end());
	 } else {
	    ASSERT_NE(set.find(key), set.end());
	 }
      }
   }
}
TEST(set_plain_32, random) { 
   separate_chaining_set<plain_bucket<uint32_t> COMMA hash_mapping_adapter<uint32_t COMMA SplitMix>> set;
   test_set_random(set);
} 



int main(int argc, char **argv) {

   ::testing::InitGoogleTest(&argc, argv);
   return RUN_ALL_TESTS();
}
