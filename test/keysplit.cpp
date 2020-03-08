#include "base.hpp"
#include <separate/keysplit_adapter.hpp>
#include <separate/separate_chaining_table.hpp>

// TEST_MAP(keysplit_adapter64, keysplit_adapter64<separate_chaining_map<varwidth_bucket<> COMMA plain_bucket<uint16_t> COMMA hash_mapping_adapter<uint64_t COMMA SplitMix>> COMMA separate_chaining_map<plain_bucket<uint64_t> COMMA plain_bucket<uint16_t> COMMA hash_mapping_adapter<uint64_t COMMA SplitMix>> > map)

TEST_MAP(keysplit_adapter, keysplit_adapter<separate_chaining_map<varwidth_bucket<> COMMA plain_bucket<uint16_t> COMMA hash_mapping_adapter<uint64_t COMMA SplitMix>>> map)


