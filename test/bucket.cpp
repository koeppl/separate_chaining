#include "base.hpp"

#include <separate/bucket_table.hpp>

TEST_SMALL_MAP(map_bucket_plain_arb_16,  bucket_table<plain_bucket<uint32_t> COMMA plain_bucket<uint16_t> COMMA arbitrary_resize_bucket> map)
TEST_SMALL_MAP(map_bucket_var_arb_16,    bucket_table<varwidth_bucket<> COMMA plain_bucket<uint16_t> COMMA arbitrary_resize_bucket> map)
TEST_SMALL_MAP(map_bucket_plain_16,  bucket_table<plain_bucket<uint32_t> COMMA plain_bucket<uint16_t> COMMA incremental_resize> map)
TEST_SMALL_MAP(map_bucket_var_16,    bucket_table<varwidth_bucket<> COMMA plain_bucket<uint16_t> COMMA incremental_resize> map)
