#pragma once

#include "dcheck.hpp"

#include "separate_chaining_map.hpp"



template<class value_t, class hash_type>
class separate_chaining_map<uint64_t, value_t, hash_type> {
    public:
    using value_type = value_t;
    using key_type = uint64_t;
    using bucketsize_type = uint32_t; //! used for storing the sizes of the buckets
    using size_type = uint64_t; //! used for addressing the i-th bucket
    using iterator = separate_chaining_iterator<separate_chaining_map<key_type, value_type, hash_type>>;

    static constexpr size_t MAX_BUCKETSIZE = separate_chaining::MAX_BUCKET_BYTESIZE/sizeof(key_type); //TODO: make constexpr
    static_assert(MAX_BUCKETSIZE < std::numeric_limits<bucketsize_type>::max(), "enlarge separate_chaining::MAX_BUCKET_BYTESIZE for this key type!");

#ifndef NDEBUG
    key_type** m_plainkeys = nullptr; //!bucket for keys
#endif
    key_type** m_keys = nullptr; //!bucket for keys
    value_type** m_values = nullptr; //! bucket for values
    bucketsize_type* m_bucketsizes = nullptr; //! size of each bucket
    hash_type m_hash; //! hash function

    size_t m_buckets = 0; //! log_2 of the number of buckets
    size_t m_elements = 0; //! number of stored elements
    const uint_fast8_t m_width;

    void clear() { //! empties hash table
        if(m_buckets > 0) {
            for(size_t bucket = 0; bucket < (1ULL<<m_buckets); ++bucket) {
                if(m_bucketsizes[bucket] == 0) continue;
                free(m_keys[bucket]);
                free(m_values[bucket]);
            }
            free(m_keys);
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

    separate_chaining_map(size_t width) : m_width(width) {
    }

    separate_chaining_map(separate_chaining_map&& other)
       : m_keys(std::move(other.m_keys))
       , m_values(std::move(other.m_values))
       , m_bucketsizes(std::move(other.m_bucketsizes))
       , m_hash(std::move(other.m_hash))
       , m_buckets(std::move(other.m_buckets))
       , m_elements(std::move(other.m_elements))
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
        std::swap(m_keys, other.m_keys);
#ifndef NDEBUG
        std::swap(m_plainkeys, other.m_plainkeys);
#endif
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
#ifndef NDEBUG
            m_plainkeys   = reinterpret_cast<key_type**>  (malloc(new_size*sizeof(key_type*)));
#endif
            m_keys   = reinterpret_cast<key_type**>  (malloc(new_size*sizeof(key_type*)));
            m_values = reinterpret_cast<value_type**>(malloc(new_size*sizeof(value_type*)));
            m_bucketsizes  = reinterpret_cast<bucketsize_type*>  (malloc(new_size*sizeof(bucketsize_type)));
            std::fill(m_bucketsizes, m_bucketsizes+new_size, 0);
            m_buckets = reserve_bits;
        } else {
            separate_chaining_map<key_type, value_type, hash_type> tmp_map(m_width);
            tmp_map.reserve(new_size);
            for(size_t bucket = 0; bucket < 1ULL<<m_buckets; ++bucket) {
                if(m_bucketsizes[bucket] == 0) continue;

                uint8_t offset = 0;
                const key_type* bucket_keys_it = m_keys[bucket];
                for(size_t i = 0; i < m_bucketsizes[bucket]; ++i) {
                    const key_type read_key = tdc::tdc_sdsl::bits_impl<>::read_int_and_move(bucket_keys_it, offset, m_width);
                    DCHECK_EQ(read_key , m_plainkeys[bucket][i]);
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
        const size_t bucket = m_hash(key) & ((1ull << m_buckets) - 1ull);
        const bucketsize_type& bucket_size = m_bucketsizes[bucket];

        uint8_t offset = 0;
        const key_type* bucket_keys_it = m_keys[bucket];
        for(size_t i = 0; i < bucket_size; ++i) { // needed?
            const key_type read_key = tdc::tdc_sdsl::bits_impl<>::read_int_and_move(bucket_keys_it, offset, m_width);
            DCHECK_EQ(read_key , m_plainkeys[bucket][i]);
            if(read_key  == key) {
                return iterator { *this, bucket, i };
            }
        }
        return end();
    }

    value_type& operator[](const key_type& key) {
        if(m_buckets == 0) reserve(16);
        const size_t bucket = m_hash(key) & ((1ULL << m_buckets) - 1ULL);
        bucketsize_type& bucket_size = m_bucketsizes[bucket];
        key_type*& bucket_keys = m_keys[bucket];
#ifndef NDEBUG
        key_type*& bucket_plainkeys = m_plainkeys[bucket];
#endif
        value_type*& bucket_values = m_values[bucket];

        uint8_t offset = 0;
        const key_type* bucket_keys_it = m_keys[bucket];
        for(size_t i = 0; i < bucket_size; ++i) { // needed?
            const key_type read_key = tdc::tdc_sdsl::bits_impl<>::read_int_and_move(bucket_keys_it, offset, m_width);
            DCHECK_EQ(read_key , bucket_plainkeys[i]);
            if(read_key == key) {
                return bucket_values[i];
            }
        }
        if(bucket_size == MAX_BUCKETSIZE) {
			if(m_elements*10 < max_bucket_count() * bucket_count()) {
				throw std::runtime_error("The chosen hash function is bad!");
			}
            reserve(1ULL<<(m_buckets+1));
            return operator[](key);
        }
        ++m_elements;

        if(bucket_size == 0) {
            bucket_size = 1;
#ifndef NDEBUG
            bucket_plainkeys   = reinterpret_cast<key_type*>  (malloc(sizeof(key_type)));
#endif
            bucket_keys   = reinterpret_cast<key_type*>  (malloc(sizeof(key_type)));
            bucket_values = reinterpret_cast<value_type*>(malloc(sizeof(value_type)));
        } else {
            ++bucket_size;
#ifndef NDEBUG
            bucket_plainkeys   = reinterpret_cast<key_type*>  (realloc(bucket_plainkeys, sizeof(key_type)*bucket_size));
#endif
            bucket_values = reinterpret_cast<value_type*>(realloc(bucket_values, sizeof(value_type)*bucket_size));
            if(ceil_div<bucketsize_type>((bucket_size-1)*m_width, 64) < ceil_div<bucketsize_type>((bucket_size)*m_width, 64)) {
                bucket_keys   = reinterpret_cast<key_type*>  (realloc(bucket_keys, sizeof(key_type)*ceil_div<bucketsize_type>(bucket_size*m_width, 64) ));
            }
        }
        DCHECK_LE(key, max_key());
#ifndef NDEBUG
        bucket_plainkeys[bucket_size-1] = key;
#endif
        
        DCHECK_LT((static_cast<size_t>(bucket_size-1)*m_width)/64 + ((bucket_size-1)* m_width) % 64, 64*ceil_div<bucketsize_type>(bucket_size*m_width, 64) );

        tdc::tdc_sdsl::bits_impl<>::write_int(bucket_keys + (static_cast<size_t>(bucket_size-1)*m_width)/64, key, ((bucket_size-1)* m_width) % 64, m_width);
        DCHECK_EQ(tdc::tdc_sdsl::bits_impl<>::read_int(bucket_keys + (static_cast<size_t>(bucket_size-1)*m_width)/64, ((bucket_size-1)* m_width) % 64, m_width), key);

        return bucket_values[bucket_size-1];
    }

    ~separate_chaining_map() { clear(); }

};
