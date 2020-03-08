#include "base.hpp"
#include <separate/group_chaining.hpp>


TEST(map_group, out) { 
   using T = group::group_chaining_table<>;
   constexpr size_t key_width = 32;
   constexpr size_t value_width = 32;
   T map(key_width, value_width);
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

TEST_SMALL_MAP(map_group, group::group_chaining_table<> map(32,32))
TEST_SMALL_MAP(map_group_middle, group::group_chaining_table<> map(10,13))
TEST_MAP_FULL(map_group_low, group::group_chaining_table<> map(7,3))
