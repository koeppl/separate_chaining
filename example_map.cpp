#include "separate/group_chaining.hpp"

#include <iostream>
using namespace std;


int main() {
	// separate_chaining::keyvalue_group<> group;
	// const size_t groups = 10;
    //
	// group.initiate(groups, 8, 8);
	// for(size_t i = 0; i < 100; ++i) {
	// 	group.push_back(i % groups, i, 8, i, 8);
	// }
	// for(size_t i = 0; i < 100; ++i) {
	// 	DCHECK_EQ(group.read(i % groups, i/groups, 8, 8).first, i);
	// 	DCHECK_EQ(group.read(i % groups, i/groups, 8, 8).second, i);
	// 	DCHECK_EQ(group.find(i % groups, i, 8), i / groups);
	// }
	// cout << "Hello, World!";

	separate_chaining::group_chaining_table<> table;
	for(size_t i = 0; i < 100; ++i) {
		table.find_or_insert(i,i);
	}


	return 0;
}
