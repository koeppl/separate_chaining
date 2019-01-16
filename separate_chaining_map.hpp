#pragma once

#include <cstdint>
#include <functional>

//! returns the most significant bit
constexpr int most_significant_bit(const uint64_t& x) {
    return x == 0 ? -1 : (sizeof(uint64_t)*8-1) - __builtin_clzll(x);
}


template<class key_type, class value_type, class hash_type = std::hash<key_type>>
class separate_chaining_map {
    public:
    static constexpr size_t MAX_BUCKETSIZE = 128/sizeof(key_type); //TODO: make constexpr
    using bucketsize_type = uint32_t; //! used for storing the sizes of the buckets
    using size_type = uint64_t; //! used for addressing the i-th bucket

    key_type** m_keys = nullptr; //!bucket for keys
    value_type** m_values = nullptr; //! bucket for values
    bucketsize_type* m_bucketsizes = nullptr; //! size of each bucket
    hash_type m_hash; //! hash function

    size_t m_buckets = 0; //! number of buckets
    size_t m_elements = 0; //! number of stored elements

    void clear() { //! empties hash table
        for(size_t bucket = 0; bucket < m_buckets; ++bucket) {
            if(m_bucketsizes[bucket] == 0) continue;
            free(m_keys[bucket]);
            free(m_values[bucket]);
        }
        if(m_buckets > 0) {
            free(m_keys);
            free(m_values);
            free(m_bucketsizes);
        }
        m_buckets = 0;
    }

    public:

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
        return m_buckets;
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
            m_keys   = reinterpret_cast<key_type**>  (malloc(new_size*sizeof(key_type*)));
            m_values = reinterpret_cast<value_type**>(malloc(new_size*sizeof(value_type*)));
            m_bucketsizes  = reinterpret_cast<bucketsize_type*>  (malloc(new_size*sizeof(bucketsize_type)));
            std::fill(m_bucketsizes, m_bucketsizes+new_size, 0);
			m_buckets = new_size;
        } else {
            separate_chaining_map<key_type, value_type, hash_type> tmp_map;
            tmp_map.reserve(new_size);
            for(size_t bucket = 0; bucket < m_buckets; ++bucket) {
                if(m_bucketsizes[bucket] == 0) continue;
                for(size_t i = 0; i < m_bucketsizes[bucket]; ++i) {
                    tmp_map[m_keys[bucket][i]] =  m_values[bucket][i];
                }
            }
            swap(tmp_map);
        }
    }

    struct iterator {
        public:
        const separate_chaining_map& m_map;
        size_t m_bucket;
        size_t m_position;

        std::pair<bool, value_type> m_pair;

        iterator(const separate_chaining_map& map, size_t bucket, size_t position) 
        : m_map(map), m_bucket(bucket), m_position(position) {
        }
        
        const std::pair<bool, value_type>* operator->()  {
            m_pair = m_bucket < m_map.bucket_count() ? std::make_pair(true, m_map.m_values[m_bucket][m_position]) : std::make_pair(false, value_type(0));
            return &m_pair;
        }
		bool operator!=(const iterator o) const {
			return !( (*this)  == o);
		}
		bool operator==(const iterator o) const {
			// if(o->m_bucket == -1ULL && this->m_bucket == -1ULL) return true; // both are end()
			// if(o->m_bucket == -1ULL || this->m_bucket == -1ULL) return false; // one is end()
			return m_bucket == o.m_bucket && m_position == o.m_position; // compare positions
		}
    };
    const iterator end() const {
        return iterator { *this, -1ULL, -1ULL };
    }

    iterator find(const key_type& key) const {
        if(m_buckets == 0) return end();
        const size_t bucket = m_hash(key) % m_buckets;
        //TODO should be const size_t bucket = m_hash(key) & ((1ull << m_buckets) - 1ull);
        const bucketsize_type& bucket_size = m_bucketsizes[bucket];
        const key_type*const bucket_keys = m_keys[bucket];

        for(size_t i = 0; i < bucket_size; ++i) { // needed?
            if(bucket_keys[i] == key) {
                return iterator { *this, bucket, i };
            }
        }
        return end();
    }

    value_type& operator[](const key_type& key) {
        if(m_buckets == 0) reserve(16);
        const size_t bucket = m_hash(key) % m_buckets;
        bucketsize_type& bucket_size = m_bucketsizes[bucket];
        key_type*& bucket_keys = m_keys[bucket];
        value_type*& bucket_values = m_values[bucket];

        for(size_t i = 0; i < bucket_size; ++i) { // needed?
            if(bucket_keys[i] == key) {
                return bucket_values[i];
            }
        }
        if(bucket_size == MAX_BUCKETSIZE) {
			if(m_elements*10 < max_bucket_count() * bucket_count()) {
				throw std::runtime_error("The chosen hash function is bad!");
			}
            reserve(m_buckets<<1);
            return operator[](key);
        }
        ++m_elements;


        if(bucket_size == 0) {
            bucket_size = 1;
            bucket_keys   = reinterpret_cast<key_type*>  (malloc(sizeof(key_type)));
            bucket_values = reinterpret_cast<value_type*>(malloc(sizeof(value_type)));
        } else {
            ++bucket_size;
            bucket_keys   = reinterpret_cast<key_type*>  (realloc(bucket_keys, sizeof(key_type)*bucket_size));
            bucket_values = reinterpret_cast<value_type*>(realloc(bucket_values, sizeof(value_type)*bucket_size));
        }
        bucket_keys[bucket_size-1] = key;
        return bucket_values[bucket_size-1];
    }

    ~separate_chaining_map() { clear(); }



};

// template< class key_type, class value_type, class hash_type>
// constexpr size_t separate_chaining_map<key_type,value_type,hash_type>::MAX_BUCKETSIZE;
