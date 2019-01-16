#include <iostream>
#include <gtest/gtest.h>
#include <map>
#include "separate_chaining_map.hpp"
#include "separate_chaining_map_int.hpp"
#include "keysplit_adapter.hpp"
#include <algorithm>

template<class T>
T random_int(const T& maxvalue) {
   return static_cast<T>(std::rand() * (1.0 / (RAND_MAX + 1.0 )) * maxvalue);
}

TEST(keysplit_adapter, id) {
   keysplit_adapter<separate_chaining_map<uint64_t,uint16_t>> map;
   for(size_t i = 0; i < std::numeric_limits<uint16_t>::max(); ++i) {
      map[i] = i;
   }
   for(size_t i = 0; i < std::numeric_limits<uint16_t>::max(); ++i) {
      ASSERT_EQ(map[i], i);
      ASSERT_EQ(map[i], map[i]);
   }
}

TEST(keysplit_adapter, random) {
   constexpr uint16_t maxval = std::numeric_limits<uint16_t>::max();
   for(size_t reps = 0; reps < 1000; ++reps) {
      keysplit_adapter<separate_chaining_map<uint64_t,uint16_t>> map;
      std::map<uint16_t,uint16_t> rev;
      for(size_t i = 0; i < 1000; ++i) {
	 const uint16_t key = random_int<uint16_t>(maxval);
	 const uint16_t val = random_int<uint16_t>(maxval);
	 rev[key] = map[key] = val;
      }
      for(auto el : rev) {
	 ASSERT_EQ( map.find(el.first)->second, el.second);
      }
   }
}



TEST(separate_chaining_int_map, id) {
   separate_chaining_map<uint64_t,uint16_t> map(27);
   for(size_t i = 0; i < std::numeric_limits<uint16_t>::max(); ++i) {
      map[i] = i;
   }
   for(size_t i = 0; i < std::numeric_limits<uint16_t>::max(); ++i) {
      ASSERT_EQ(map[i], i);
      ASSERT_EQ(map[i], map[i]);
   }
}

TEST(separate_chaining_int_map, random) {
   constexpr uint16_t maxval = std::numeric_limits<uint16_t>::max();
   for(size_t reps = 0; reps < 1000; ++reps) {
      separate_chaining_map<uint64_t, uint16_t> map(27);
      std::map<uint16_t,uint16_t> rev;
      for(size_t i = 0; i < 1000; ++i) {
	 const uint16_t key = random_int<uint16_t>(maxval);
	 const uint16_t val = random_int<uint16_t>(maxval);
	 rev[key] = map[key] = val;
      }
      for(auto el : rev) {
	 ASSERT_EQ( map.find(el.first)->second, el.second);
      }
   }
}


TEST(separate_chaining_map, id) {
   separate_chaining_map<uint16_t,uint16_t> map;
   for(size_t i = 0; i < std::numeric_limits<uint16_t>::max(); ++i) {
      map[i] = i;
   }
   for(size_t i = 0; i < std::numeric_limits<uint16_t>::max(); ++i) {
      ASSERT_EQ(map[i], i);
      ASSERT_EQ(map[i], map[i]);
   }
}

TEST(separate_chaining_map, random) {
   constexpr uint16_t maxval = std::numeric_limits<uint16_t>::max();
   for(size_t reps = 0; reps < 1000; ++reps) {
      separate_chaining_map<uint16_t,uint16_t> map;
      std::map<uint16_t,uint16_t> rev;
      for(size_t i = 0; i < 1000; ++i) {
	 const uint16_t key = random_int<uint16_t>(maxval);
	 const uint16_t val = random_int<uint16_t>(maxval);
	 rev[key] = map[key] = val;
      }
      for(auto el : rev) {
	 ASSERT_EQ( map.find(el.first)->second, el.second);
      }
   }
}


int main(int argc, char **argv) {
   ::testing::InitGoogleTest(&argc, argv);
   return RUN_ALL_TESTS();
}
