#pragma once

#include "bijective_hash.hpp"

namespace separate_chaining {

class SplitMix { // from http://zimbry.blogspot.com/2011/09/better-bit-mixing-improving-on.html
   public:
   uint64_t operator()(uint64_t x) const {
      x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
      x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
      x = x ^ (x >> 31);
      return x;
   }
};

template<class key_t, class hash_function>
class hash_mapping_adapter {
    public:
    using key_type = key_t;
    using storage_type = key_t;

    private:
    uint8_t m_width;
    hash_function m_func;

    public:
    hash_mapping_adapter(uint_fast8_t key_width) : m_width(key_width) {}
    
    uint_fast8_t remainder_width([[maybe_unused]] const uint_fast8_t table_buckets) const {
       return m_width;
        // return sizeof(key_t)*8;
    }

    std::pair<storage_type, size_t> map(const key_type& key, [[maybe_unused]] const uint_fast8_t table_buckets) const {
        //return std::make_pair(key, m_func(key) & ((1ULL << table_buckets) - 1ULL));
        return std::make_pair(key, m_func(key) & (-1ULL >> (64-table_buckets) ));
    }
    key_type inv_map(const storage_type& remainder, [[maybe_unused]] const size_t& hash_value, [[maybe_unused]] const uint8_t table_buckets) const {
        return remainder;
    }
};

template<class key_t = uint64_t, class storage_t = key_t, class bijective_function = bijective_hash::Xorshift>
class bijective_hash_adapter {
    public:
    using key_type = key_t;
    using storage_type = storage_t;

    private:
    bijective_function func;

    public:
    bijective_hash_adapter(const uint_fast8_t width) : func(width) { }

    uint_fast8_t remainder_width(const uint_fast8_t table_buckets) const {
        DCHECK_LT(table_buckets, func.bits()); //! the hash table needs a remainder of at least one bit
        return func.bits() - table_buckets;
    }
    
    std::pair<storage_type, size_t> map(const key_type& key, const uint_fast8_t table_buckets) const {
        const size_t hash_value = func.hash(key);
        DCHECK_EQ(func.hash_inv(hash_value), key);
        DCHECK_LE(hash_value >> table_buckets, std::numeric_limits<storage_type>::max());
        //TODO: swap both entries, as H[i] can then be mapped directly to H[2i] and H[2i+1] on a resize
        const auto ret = std::make_pair(hash_value >> table_buckets, hash_value & ((1ULL << table_buckets) - 1ULL) );
        DCHECK_EQ(inv_map(ret.first, ret.second, table_buckets), key);
        return ret;
    }
    key_type inv_map(const storage_type remainder, const size_t hash_value, const uint8_t table_buckets) const {
        return func.hash_inv( (static_cast<uint64_t>(remainder) << table_buckets) + hash_value);
    }
};

template<class key_t = uint64_t, class storage_t = key_t> using xorshift_hash = bijective_hash_adapter<key_t, storage_t, bijective_hash::Xorshift>;

template<class key_t = uint64_t, class storage_t = key_t> using multiplicative_hash = bijective_hash_adapter<key_t, storage_t, bijective_hash::MultiplicativeHash>;

}//ns separate_chaining
