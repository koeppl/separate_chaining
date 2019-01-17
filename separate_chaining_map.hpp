#pragma once

#include <cstdint>
#include <functional>
#include <limits>
#include "dcheck.hpp"
#include <tudocomp/util/sdsl_bits.hpp>

//! returns the most significant bit
constexpr int most_significant_bit(const uint64_t& x) {
    return x == 0 ? -1 : (sizeof(uint64_t)*8-1) - __builtin_clzll(x);
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

namespace separate_chaining {
    static constexpr size_t MAX_BUCKET_BYTESIZE = 128;
    static constexpr size_t INITIAL_BUCKETS = 16;
}

#ifdef NDEBUG
#define ON_DEBUG(x)
#else
#define ON_DEBUG(x) x
#endif

template<class key_t>
class plain_key_bucket {
    using key_type = key_t;

    key_type* m_keys = nullptr; //!bucket for keys

    ON_DEBUG(size_t m_length;)

    void clear() {
        if(m_keys != nullptr) {
            free(m_keys);
        }
        m_keys = nullptr;
    }

    public:
    plain_key_bucket() = default;

    void initiate() {
        m_keys = reinterpret_cast<key_type*>  (malloc(sizeof(key_type)));
        ON_DEBUG(m_length = 1;)
    }

    void increment_size(const size_t size, [[maybe_unused]] const size_t width) {
        m_keys = reinterpret_cast<key_type*>  (realloc(m_keys, sizeof(key_type)*size));
        ON_DEBUG(m_length = size;)
    }

    void write(const size_t i, const key_type& key, [[maybe_unused]] const uint_fast8_t width) {
        DCHECK_LT(i, m_length);
        m_keys[i] = key;
    }
    key_type read(size_t i, [[maybe_unused]]  size_t width) const {
        DCHECK_LT(i, m_length);
        return m_keys[i];
    }
    size_t find(const key_type& key, const size_t length, [[maybe_unused]] const size_t width) const {
       for(size_t i = 0; i < length; ++i) {
          if(m_keys[i] == key) return i;
       }
       return -1ULL;
    }

    ~plain_key_bucket() { clear(); }

    plain_key_bucket(plain_key_bucket&& other) 
        : m_keys(std::move(other.m_keys))
    {
        other.m_keys = nullptr;
    }

    plain_key_bucket& operator=(plain_key_bucket&& other) {
        clear();
        m_keys = std::move(other.m_keys);
        other.m_keys = nullptr;
    }
};

//! computes ceil(dividend/divisor)
template<class T>
constexpr T ceil_div(const T& dividend, const T& divisor) { 
   return (dividend + divisor - 1) / divisor;
}


template<class key_t>
class varwidth_key_bucket {
    using key_type = uint64_t;

    key_type* m_keys = nullptr; //!bucket for keys

    ON_DEBUG(size_t m_length;)

    void clear() {
        if(m_keys != nullptr) {
            free(m_keys);
        }
        m_keys = nullptr;
    }

    public:
    varwidth_key_bucket() = default;

    void initiate() {
        m_keys = reinterpret_cast<key_type*>  (malloc(sizeof(key_type)));
        ON_DEBUG(m_length = 1;)
    }

    void increment_size(const size_t size, const size_t width) {
       if(ceil_div<size_t>((size-1)*width, 64) < ceil_div<size_t>((size)*width, 64)) {
          m_keys = reinterpret_cast<key_type*>  (realloc(m_keys, sizeof(key_type)*ceil_div<size_t>(size*width, 64) ));
       }
       ON_DEBUG(m_length = size;)
    }

    void write(const size_t i, const key_type& key, const uint_fast8_t width) {
        DCHECK_LT((static_cast<size_t>(i)*width)/64 + ((i)* width) % 64, 64*ceil_div<size_t>(m_length*width, 64) );

        tdc::tdc_sdsl::bits_impl<>::write_int(m_keys + (static_cast<size_t>(i)*width)/64, key, ((i)* width) % 64, width);
        DCHECK_EQ(tdc::tdc_sdsl::bits_impl<>::read_int(m_keys + (static_cast<size_t>(i)*width)/64, ((i)* width) % 64, width), key);
    }
    key_type read(size_t i, size_t width) const {
        return tdc::tdc_sdsl::bits_impl<>::read_int(m_keys + (static_cast<size_t>(i)*width)/64, ((i)* width) % 64, width);
    }

    size_t find(const key_type& key, const size_t length, const size_t width) const {
       DCHECK_LE(length, m_length);
       uint8_t offset = 0;
       const key_type* it = m_keys;

       for(size_t i = 0; i < length; ++i) { // needed?
            const key_type read_key = tdc::tdc_sdsl::bits_impl<>::read_int_and_move(it, offset, width);
            //DCHECK_EQ(read_key , bucket_plainkeys[i]);
            if(read_key == key) {
                return i;
            }
        }
       return -1ULL;
    }


    ~varwidth_key_bucket() { clear(); }

    varwidth_key_bucket(varwidth_key_bucket&& other) 
        : m_keys(std::move(other.m_keys))
    {
        other.m_keys = nullptr;
    }

    varwidth_key_bucket& operator=(varwidth_key_bucket&& other) {
        clear();
        m_keys = std::move(other.m_keys);
        other.m_keys = nullptr;
    }
};


template<class key_t, class value_t, class hash_t = std::hash<key_t>>
class separate_chaining_map {
    public:
    using key_type = key_t;
    using value_type = value_t;
    using hash_type = hash_t;
    using bucketsize_type = uint32_t; //! used for storing the sizes of the buckets
    using size_type = uint64_t; //! used for addressing the i-th bucket
    using iterator = separate_chaining_iterator<separate_chaining_map<key_type, value_type, hash_type>>;

    static constexpr size_t MAX_BUCKETSIZE = separate_chaining::MAX_BUCKET_BYTESIZE/sizeof(key_type); //TODO: make constexpr
    static_assert(MAX_BUCKETSIZE < std::numeric_limits<bucketsize_type>::max(), "enlarge separate_chaining::MAX_BUCKET_BYTESIZE for this key type!");

    varwidth_key_bucket<key_type>* m_keys = nullptr;
    value_type** m_values = nullptr; //! bucket for values
    bucketsize_type* m_bucketsizes = nullptr; //! size of each bucket
    hash_type m_hash; //! hash function

    size_t m_buckets = 0; //! log_2 of the number of buckets
    size_t m_elements = 0; //! number of stored elements

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
    constexpr uint_fast8_t width() const { return sizeof(uint64_t)*8; }

    //! returns the maximum value of a key that can be stored
    constexpr key_type max_key() const { return std::numeric_limits<key_type>::max(); }

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

    separate_chaining_map() {
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
            m_keys   = new varwidth_key_bucket<key_type>[new_size];
            m_values = reinterpret_cast<value_type**>(malloc(new_size*sizeof(value_type*)));
            m_bucketsizes  = reinterpret_cast<bucketsize_type*>  (malloc(new_size*sizeof(bucketsize_type)));
            std::fill(m_bucketsizes, m_bucketsizes+new_size, 0);
            m_buckets = reserve_bits;
        } else {
            separate_chaining_map<key_type, value_type, hash_type> tmp_map;
            tmp_map.reserve(new_size);
            for(size_t bucket = 0; bucket < 1ULL<<m_buckets; ++bucket) {
                if(m_bucketsizes[bucket] == 0) continue;
                for(size_t i = 0; i < m_bucketsizes[bucket]; ++i) {
                    tmp_map[m_keys[bucket].read(i, width())] =  m_values[bucket][i];
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
        const varwidth_key_bucket<key_type>& bucket_keys = m_keys[bucket];
        const size_t index = bucket_keys.find(key, bucket_size, width());
        return (index == -1ULL) ? end() : iterator { *this, bucket, index };
    }

    value_type& operator[](const key_type& key) {
        if(m_buckets == 0) reserve(separate_chaining::INITIAL_BUCKETS);
        const size_t bucket = m_hash(key) & ((1ULL << m_buckets) - 1ULL);
        bucketsize_type& bucket_size = m_bucketsizes[bucket];
        varwidth_key_bucket<key_type>& bucket_keys = m_keys[bucket];
        value_type*& bucket_values = m_values[bucket];

        {
            const size_t index = bucket_keys.find(key, bucket_size, width());
            if(index != -1ULL) { return bucket_values[index]; }
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
            bucket_keys.initiate();
            bucket_values = reinterpret_cast<value_type*>(malloc(sizeof(value_type)));
        } else {
            ++bucket_size;
            bucket_keys.increment_size(bucket_size, width());
            bucket_values = reinterpret_cast<value_type*>(realloc(bucket_values, sizeof(value_type)*bucket_size));
        }
        bucket_keys.write(bucket_size-1, key, width());
        return bucket_values[bucket_size-1];
    }

    ~separate_chaining_map() { clear(); }

};
