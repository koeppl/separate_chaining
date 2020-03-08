#include "base.hpp"
#include <separate/separate_chaining_table.hpp>
#include <separate/bijective_hash.hpp>


TEST(map, quotienting) {
   using storage_type = uint8_t;
   using key_type = uint16_t;
   using value_type = key_type;
   separate_chaining_map<plain_bucket<storage_type>, plain_bucket<value_type>, xorshift_hash<key_type, storage_type>, incremental_resize> small_map;
   small_map.reserve(1ULL<<8);
   test_map_id(small_map);
}


TEST_MAP_FULL(map_var_var_low,  separate_chaining_map<varwidth_bucket<> COMMA varwidth_bucket<> COMMA hash_mapping_adapter<uint64_t COMMA SplitMix> COMMA arbitrary_resize> map(7,3))

TEST_MAP_FULL(map_var_Xor_64_OverMap, separate_chaining_map<varwidth_bucket<> COMMA plain_bucket<uint32_t> COMMA xorshift_hash<> COMMA incremental_resize COMMA map_overflow> map(64))
TEST_MAP_FULL(map_var_Xor_64_OverArray, separate_chaining_map<varwidth_bucket<> COMMA plain_bucket<uint32_t> COMMA xorshift_hash<> COMMA incremental_resize COMMA array_overflow> map(64))


TEST_MAP_FULL(map_var_arb_16,  separate_chaining_map<varwidth_bucket<> COMMA plain_bucket<uint16_t> COMMA hash_mapping_adapter<uint64_t COMMA SplitMix> COMMA arbitrary_resize> map)
TEST_MAP_FULL(map_plain_16,  separate_chaining_map<plain_bucket<uint32_t> COMMA plain_bucket<uint16_t> COMMA hash_mapping_adapter<uint32_t COMMA SplitMix> COMMA incremental_resize> map)
TEST_MAP_FULL(map_plain_32,  separate_chaining_map<plain_bucket<uint32_t> COMMA plain_bucket<uint32_t> COMMA hash_mapping_adapter<uint32_t COMMA SplitMix> COMMA incremental_resize> map)
TEST_MAP_FULL(map_plain_Xor, separate_chaining_map<plain_bucket<uint32_t> COMMA plain_bucket<uint32_t> COMMA xorshift_hash<uint32_t> COMMA incremental_resize> map(32))

TEST_MAP_FULL(map_plain_arb_16,  separate_chaining_map<plain_bucket<uint32_t> COMMA plain_bucket<uint16_t> COMMA hash_mapping_adapter<uint32_t COMMA SplitMix> COMMA arbitrary_resize> map)

TEST_MAP_FULL(map_var_Xor_64, separate_chaining_map<varwidth_bucket<> COMMA plain_bucket<uint32_t> COMMA xorshift_hash<> COMMA incremental_resize> map(64))

#ifdef __AVX2__
TEST_MAP_FULL(map_avx2_16_16,  separate_chaining_map<avx2_bucket<uint16_t> COMMA plain_bucket<uint16_t> COMMA hash_mapping_adapter<uint16_t COMMA SplitMix> COMMA incremental_resize > map)
TEST_MAP_FULL(map_avx2_8_16,  separate_chaining_map<avx2_bucket<uint8_t> COMMA plain_bucket<uint16_t> COMMA hash_mapping_adapter<uint8_t COMMA SplitMix> COMMA incremental_resize> map)

TEST_MAP_FULL(map_avx2_32_16,  separate_chaining_map<avx2_bucket<uint32_t> COMMA plain_bucket<uint16_t> COMMA hash_mapping_adapter<uint32_t COMMA SplitMix> COMMA incremental_resize > map)
TEST_MAP_FULL(map_avx2_32_32,  separate_chaining_map<avx2_bucket<uint32_t> COMMA plain_bucket<uint32_t> COMMA hash_mapping_adapter<uint32_t COMMA SplitMix> COMMA incremental_resize > map)
TEST_MAP_FULL(map_avx2_32_Xor, separate_chaining_map<avx2_bucket<uint32_t> COMMA plain_bucket<uint32_t> COMMA xorshift_hash<uint32_t> COMMA incremental_resize> map(32))

TEST_MAP_FULL(map_avx2_64_16,  separate_chaining_map<avx2_bucket<uint64_t> COMMA plain_bucket<uint16_t> COMMA hash_mapping_adapter<uint64_t COMMA SplitMix> COMMA incremental_resize > map)
TEST_MAP_FULL(map_avx2_64_32,  separate_chaining_map<avx2_bucket<uint64_t> COMMA plain_bucket<uint32_t> COMMA hash_mapping_adapter<uint64_t COMMA SplitMix> COMMA incremental_resize> map)
TEST_MAP_FULL(map_avx2_64_Xor, separate_chaining_map<avx2_bucket<uint64_t> COMMA plain_bucket<uint32_t> COMMA xorshift_hash<uint64_t> COMMA incremental_resize> map(32))
TEST_MAP_FULL(map_avx2_16_arb_16,  separate_chaining_map<avx2_bucket<uint16_t> COMMA plain_bucket<uint16_t> COMMA hash_mapping_adapter<uint16_t COMMA SplitMix> COMMA arbitrary_resize> map)
#endif//__AVX2__

TEST_MAP_FULL(map_plain_class32,  separate_chaining_map<class_bucket<uint32_t> COMMA class_bucket<uint32_t> COMMA hash_mapping_adapter<uint32_t COMMA SplitMix> COMMA incremental_resize> map)



TEST_MAP_FULL(map_var_16,  separate_chaining_map<varwidth_bucket<> COMMA plain_bucket<uint16_t> COMMA hash_mapping_adapter<uint64_t COMMA SplitMix> COMMA incremental_resize> map)
TEST_MAP_FULL(map_var_32,  separate_chaining_map<varwidth_bucket<> COMMA plain_bucket<uint32_t> COMMA hash_mapping_adapter<uint64_t COMMA SplitMix> COMMA incremental_resize> map)
TEST_MAP_FULL(map_var_Xor, separate_chaining_map<varwidth_bucket<> COMMA plain_bucket<uint32_t> COMMA xorshift_hash<uint64_t> COMMA incremental_resize> map(32))



TEST_SMALL_MAP(map_plain_small32,  separate_chaining_map<plain_bucket<uint32_t> COMMA plain_bucket<uint32_t> COMMA hash_mapping_adapter<uint32_t COMMA SplitMix> COMMA incremental_resize> map)



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



