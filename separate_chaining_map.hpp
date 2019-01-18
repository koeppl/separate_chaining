#pragma once

#include <cstdint>
#include <functional>
#include <limits>
#include "dcheck.hpp"
#include "hash.hpp"
#include "bucket.hpp"

namespace separate_chaining {
    static constexpr size_t MAX_BUCKET_BYTESIZE = 128;
    static constexpr size_t INITIAL_BUCKETS = 16;
    static constexpr size_t FAIL_PERCENTAGE = 10;
}



template<class hash_map>
struct separate_chaining_iterator {
    public:
        const hash_map& m_map;
        size_t m_bucket;
        size_t m_position;

        std::pair<bool, typename hash_map::value_type> m_pair;

        separate_chaining_iterator(const hash_map& map, size_t bucket, size_t position) 
            : m_map(map), m_bucket(bucket), m_position(position) {
            }

        const std::pair<bool, typename hash_map::value_type>* operator->()  {
            m_pair = m_bucket < m_map.bucket_count() ? std::make_pair(true, m_map.m_values[m_bucket][m_position]) : 
                std::make_pair(false, typename hash_map::value_type(0));
            return &m_pair;
        }
        bool operator!=(const separate_chaining_iterator o) const {
            return !( (*this)  == o);
        }
        bool operator==(const separate_chaining_iterator o) const {
            // if(o->m_bucket == -1ULL && this->m_bucket == -1ULL) return true; // both are end()
            // if(o->m_bucket == -1ULL || this->m_bucket == -1ULL) return false; // one is end()
            return m_bucket == o.m_bucket && m_position == o.m_position; // compare positions
        }
};




template<class bucket_t, class value_t, class hash_mapping_t>
class separate_chaining_map {
    public:
    using bucket_type = bucket_t;
    using value_type = value_t;
    using hash_mapping_type = hash_mapping_t;

    using key_type = typename bucket_type::key_type;
    // static_assert(std::is_same<key_type, typename hash_mapping_t::key_type>::value, "key types of bucket and hash_mapping mismatch!") ;
    static_assert(std::numeric_limits<key_type>::max() <= std::numeric_limits<typename hash_mapping_t::key_type>::max(), "key types of bucket must have at most as many bits as key type of hash_mapping!") ;

    using bucketsize_type = uint32_t; //! used for storing the sizes of the buckets
    using size_type = uint64_t; //! used for addressing the i-th bucket
    using class_type = separate_chaining_map<bucket_type, value_type, hash_mapping_type>;
    using iterator = separate_chaining_iterator<class_type>;

    static constexpr size_t MAX_BUCKETSIZE = separate_chaining::MAX_BUCKET_BYTESIZE/sizeof(key_type); //TODO: make constexpr
    static_assert(MAX_BUCKETSIZE < std::numeric_limits<bucketsize_type>::max(), "enlarge separate_chaining::MAX_BUCKET_BYTESIZE for this key type!");

    ON_DEBUG(key_type** m_plainkeys = nullptr;) //!bucket for keys in plain format for debugging purposes

    bucket_t* m_keys = nullptr; //!bucket for keys
    value_type** m_values = nullptr; //! bucket for values
    bucketsize_type* m_bucketsizes = nullptr; //! size of each bucket

    size_t m_buckets = 0; //! log_2 of the number of buckets
    size_t m_elements = 0; //! number of stored elements
    const uint_fast8_t m_width;
    hash_mapping_type m_hash; //! hash function

    void clear() { //! empties hash table
        if(m_buckets > 0) {
            for(size_t bucket = 0; bucket < (1ULL<<m_buckets); ++bucket) {
                if(m_bucketsizes[bucket] == 0) continue;
                free(m_values[bucket]);
            }
            delete [] m_keys;
            free(m_values);
            free(m_bucketsizes);
        }
        m_buckets = 0;
    }

    public:

    //! returns the maximum value of a key that can be stored
    key_type max_key() const { return (-1ULL) >> (64-m_width); }


    //! @see std::unordered_map
    bool empty() const { return m_elements == 0; } 

    //! @see std::unordered_map
    size_t size() const {
        return m_elements;
    }

    //! @see std::unordered_map
    bucketsize_type max_bucket_count() const {
        return MAX_BUCKETSIZE;
    }

    //! @see std::unordered_map
    size_type bucket_count() const {
        return 1ULL<<m_buckets;
    }

    //! @see std::unordered_map
    bucketsize_type bucket_size(size_type n) const {
        return m_bucketsizes[n];
    }

    separate_chaining_map(size_t width = sizeof(key_type)/8) : m_width(width), m_hash(m_width) {
        DCHECK_LE(width*8, sizeof(key_type));
    }

    separate_chaining_map(separate_chaining_map&& other)
       : m_keys(std::move(other.m_keys))
       , m_values(std::move(other.m_values))
       , m_bucketsizes(std::move(other.m_bucketsizes))
       , m_buckets(std::move(other.m_buckets))
       , m_elements(std::move(other.m_elements))
       , m_hash(std::move(other.m_hash))
    {
        other.m_buckets = 0; //! a hash map without buckets is already deleted
    }

    separate_chaining_map& operator=(separate_chaining_map&& other) {
        m_keys        = std::move(other.m_keys);
        m_values      = std::move(other.m_values);
        m_buckets     = std::move(other.m_buckets);
        m_bucketsizes = std::move(other.m_bucketsizes);
        m_hash        = std::move(other.m_hash);
        m_elements    = std::move(other.m_elements);
        other.m_buckets = 0; //! a hash map without buckets is already deleted
        return *this;
    }
    void swap(separate_chaining_map& other) {
        DCHECK_EQ(m_width, other.m_width);
        ON_DEBUG(std::swap(m_plainkeys, other.m_plainkeys);)

        std::swap(m_keys, other.m_keys);
        std::swap(m_values, other.m_values);
        std::swap(m_buckets, other.m_buckets);
        std::swap(m_bucketsizes, other.m_bucketsizes);
        std::swap(m_hash, other.m_hash);
        std::swap(m_elements, other.m_elements);
    }


    void reserve(size_t reserve) {
        size_t reserve_bits = most_significant_bit(reserve);
        if(1ULL<<reserve_bits != reserve) ++reserve_bits;
        const size_t new_size = 1ULL<<reserve_bits;

        if(m_buckets == 0) {
            ON_DEBUG(m_plainkeys   = reinterpret_cast<key_type**>  (malloc(new_size*sizeof(key_type*)));)

            m_keys   = new bucket_type[new_size];
            m_values = reinterpret_cast<value_type**>(malloc(new_size*sizeof(value_type*)));
            m_bucketsizes  = reinterpret_cast<bucketsize_type*>  (malloc(new_size*sizeof(bucketsize_type)));
            std::fill(m_bucketsizes, m_bucketsizes+new_size, 0);
            m_buckets = reserve_bits;
        } else {
            separate_chaining_map tmp_map(m_width);
            tmp_map.reserve(new_size);
            for(size_t bucket = 0; bucket < 1ULL<<m_buckets; ++bucket) {
                if(m_bucketsizes[bucket] == 0) continue;

                const uint_fast8_t key_bitwidth = m_hash.remainder_width(m_buckets);
                const bucket_type& bucket_keys_it = m_keys[bucket];
                for(size_t i = 0; i < m_bucketsizes[bucket]; ++i) {
                    const key_type read_quotient = bucket_keys_it.read(i, key_bitwidth);
                    const key_type read_key = m_hash.inv_map(read_quotient, bucket, m_buckets);
                    DCHECK_EQ(read_key, m_plainkeys[bucket][i]);
                    tmp_map[read_key] =  m_values[bucket][i];
                }
            }
            swap(tmp_map);
        }
    }

    const iterator end() const {
        return iterator { *this, -1ULL, -1ULL };
    }

    iterator find(const key_type& key) const {
        if(m_buckets == 0) return end();
        auto mapped = m_hash.map(key, m_buckets);
        const key_type& quotient = mapped.first; 
        const size_t& bucket = mapped.second;
        const bucketsize_type& bucket_size = m_bucketsizes[bucket];
        DCHECK_EQ(m_hash.inv_map(mapped.first, mapped.second, m_buckets), key);
        const uint_fast8_t key_bitwidth = m_hash.remainder_width(m_buckets);
        DCHECK_LE(most_significant_bit(quotient), key_bitwidth);

        const bucket_type& bucket_keys = m_keys[bucket];
        for(size_t i = 0; i < bucket_size; ++i) { // needed?
            const key_type read_quotient = bucket_keys.read(i, key_bitwidth);
            ON_DEBUG(const key_type read_key = m_hash.inv_map(read_quotient, bucket, m_buckets);)
            DCHECK_EQ(read_key , m_plainkeys[bucket][i]);
            if(read_quotient  == quotient) {
                return iterator { *this, bucket, i };
            }
        }
        return end();
    }

    value_type& operator[](const key_type& key) {
        if(m_buckets == 0) reserve(separate_chaining::INITIAL_BUCKETS);
        auto mapped = m_hash.map(key, m_buckets);
        const key_type& quotient = mapped.first; 
        const size_t& bucket = mapped.second;
        const uint_fast8_t key_bitwidth = m_hash.remainder_width(m_buckets);
        DCHECK_LE(most_significant_bit(quotient), key_bitwidth);

        DCHECK_EQ(m_hash.inv_map(mapped.first, mapped.second, m_buckets), key);

        bucketsize_type& bucket_size = m_bucketsizes[bucket];
        bucket_type& bucket_keys = m_keys[bucket];
        ON_DEBUG(key_type*& bucket_plainkeys = m_plainkeys[bucket];)

        value_type*& bucket_values = m_values[bucket];
        for(size_t i = 0; i < bucket_size; ++i) { 
            const key_type read_quotient = bucket_keys.read(i, key_bitwidth);
            ON_DEBUG(const key_type read_key = m_hash.inv_map(read_quotient, bucket, m_buckets);)
            DCHECK_EQ(read_key , m_plainkeys[bucket][i]);
            if(read_quotient  == quotient) {
                return bucket_values[i];
            }
        }
        if(bucket_size == MAX_BUCKETSIZE) {
			if(m_elements*separate_chaining::FAIL_PERCENTAGE < max_bucket_count() * bucket_count()) {
				throw std::runtime_error("The chosen hash function is bad!");
			}
            reserve(1ULL<<(m_buckets+1));
            return operator[](key);
        }
        ++m_elements;

        if(bucket_size == 0) {
            bucket_size = 1;
            ON_DEBUG(bucket_plainkeys   = reinterpret_cast<key_type*>  (malloc(sizeof(key_type))));
            bucket_keys.initiate();
            bucket_values = reinterpret_cast<value_type*>(malloc(sizeof(value_type)));
        } else {
            ++bucket_size;
            ON_DEBUG(bucket_plainkeys   = reinterpret_cast<key_type*>  (realloc(bucket_plainkeys, sizeof(key_type)*bucket_size));)
            bucket_keys.increment_size(bucket_size, key_bitwidth);
            bucket_values = reinterpret_cast<value_type*>(realloc(bucket_values, sizeof(value_type)*bucket_size));
        }
        DCHECK_LE(key, max_key());
        ON_DEBUG(bucket_plainkeys[bucket_size-1] = key;)
        
        DCHECK_LT((static_cast<size_t>(bucket_size-1)*key_bitwidth)/64 + ((bucket_size-1)* key_bitwidth) % 64, 64*ceil_div<bucketsize_type>(bucket_size*key_bitwidth, 64) );

        bucket_keys.write(bucket_size-1, quotient, key_bitwidth);
        DCHECK_EQ(m_hash.inv_map(bucket_keys.read(bucket_size-1, key_bitwidth), bucket, m_buckets), key);

        return bucket_values[bucket_size-1];
    }

    ~separate_chaining_map() { clear(); }

};
