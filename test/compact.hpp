#include "base.hpp"

#include <separate/compact_chaining_map.hpp>

TEST_MAP(compact_map_32_8, compact_chaining_map<hash_mapping_adapter<uint32_t COMMA SplitMix> COMMA uint8_t  > map(32,64))
TEST_MAP(compact_map_8_8, compact_chaining_map<hash_mapping_adapter<uint8_t COMMA SplitMix>  COMMA uint8_t > map(8,64))
TEST_MAP(compact_map_64_8, compact_chaining_map<hash_mapping_adapter<uint64_t COMMA SplitMix> COMMA uint8_t  > map)
TEST_MAP(compact_map_Xor, compact_chaining_map<xorshift_hash<> > map)
TEST_MAP(compact_map_Xor_8_8, compact_chaining_map<xorshift_hash<> COMMA uint8_t > map(8,64))

TEST_MAP(compact_map_8, compact_chaining_map<hash_mapping_adapter<uint8_t COMMA SplitMix>   COMMA uint64_t > map(8,64))
TEST_MAP(compact_map_32, compact_chaining_map<hash_mapping_adapter<uint32_t COMMA SplitMix> COMMA uint64_t > map(32,64))
TEST_MAP(compact_map_64, compact_chaining_map<hash_mapping_adapter<uint64_t COMMA SplitMix> COMMA uint64_t > map)
