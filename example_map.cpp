#include "separate/group_chaining.hpp"

#include <iostream>
#include <map>
using namespace std;

template<class T>
T random_int(const T& maxvalue) {
   return static_cast<T>(std::rand() * (1.0 / (RAND_MAX + 1.0 )) * maxvalue);
}

int main() {

	// separate_chaining::group::group_chaining_table<> table;
	// for(size_t i = 0; i < 100; ++i) {
	// 	table.find_or_insert(i,i*i);
	// 	for(size_t j = 0; j < i; ++j) {
	// 	    DCHECK_EQ(table[j], j*j);
	// 	}
	// }
	constexpr size_t groups = 255;

	separate_chaining::group::keyvalue_group<> kvgroup;
	kvgroup.initialize(groups, 64, 64);
	for(size_t i = 0; i < 1000; ++i) {
		kvgroup.push_back(rand() % groups, rand(), 64, rand(), 64);
	}
	kvgroup.clear();
	kvgroup.initialize(groups, 32, 32);
	for(size_t i = 0; i < 1000; ++i) {
		kvgroup.push_back(i % groups, i, 32, i, 32);
	}
	for(size_t i = 0; i < 1000; ++i) {
		DCHECK_EQ(kvgroup.read(i % groups, i/groups, 32, 32).first, i);
		DCHECK_EQ(kvgroup.read(i % groups, i/groups, 32, 32).second, i);
		DCHECK_EQ(kvgroup.find(i % groups, i, 32), i / groups);
	}
	cout << "Hello, World!";

	constexpr size_t key_width = 32;
	constexpr size_t value_width = 32;

	kvgroup.clear();
	std::map<size_t, size_t> maps[groups];
	kvgroup.initialize(groups, 32, 32);
	using key_type = uint64_t;
	using value_type = uint64_t;
   for(size_t reps = 0; reps < 100; ++reps) {
      for(size_t i = 0; i < 100; ++i) {
	 const key_type group_index = random_int<size_t>(groups);
	 const key_type key = random_int<key_type>(1ULL<<key_width);
	 const value_type value = random_int<value_type>(1ULL<<value_width);
	 maps[group_index][key] = value;
	 kvgroup.push_back(group_index, key, key_width, value, value_width);
      }
	  for(size_t group_index = 0; group_index < groups; ++group_index) {
		  for(auto el : maps[group_index]) {
			  const size_t position = kvgroup.find(group_index, el.first, key_width);
			  DCHECK_EQ(kvgroup.read_key(group_index, position, key_width), el.first);
			  DCHECK_EQ(kvgroup.read_value(group_index, position, value_width), el.second);
		  }

	  }
   }



	return 0;
}
