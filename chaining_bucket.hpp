#pragma once

#include <cstdint>
#include <functional>
#include <limits>
#include "dcheck.hpp"
#include "bucket.hpp"
#include "size.hpp"

#if STATS_ENABLED
#include <tudocomp_stat/StatPhase.hpp>
#endif

namespace separate_chaining {

    namespace chaining_bucket_types {
        using bucketsize_type = uint32_t; //! used for storing the sizes of the buckets
    }

/**
 * Defines how much a full bucket grows on insertion. 
 */
class arbitrary_resize_bucket {

    public:
    static constexpr size_t INITIAL_BUCKET_SIZE = 1; //! number of elements a bucket can store initially
    using bucketsize_type = chaining_bucket_types::bucketsize_type; //! used for storing the sizes of the buckets

    private:
    bucketsize_type m_length = 0; //! size of the bucket

    public:
    void assign(const size_t size) {
        m_length = size;
    }
    size_t size([[maybe_unused]]const size_t current_size) const {
        return m_length;
    }
    bool needs_resize(const bucketsize_type newsize) const {
        return m_length <= newsize;
    }

    size_t size_after_increment(const bucketsize_type newsize) {
        DCHECK_LE(static_cast<size_t>(arbitrary_resize::resize(newsize)),std::numeric_limits<bucketsize_type>::max());
        return m_length = arbitrary_resize::resize(newsize);
    }
};


template<class hash_map>
    class chaining_bucket_iterator {
    public:
        using key_type = typename hash_map::key_type;
        using value_type = typename hash_map::value_type;
        using pair_type = std::pair<key_type, value_type>;
        using bucketsize_type = typename hash_map::bucketsize_type;
        const hash_map& m_map;
        bucketsize_type m_position;

        pair_type m_pair;

        chaining_bucket_iterator(const hash_map& map, const bucketsize_type position) 
            : m_map(map), m_position(position) {
            }

        const pair_type* operator->()  {
            DCHECK_LT(m_position, m_map.size());
            const key_type read_key = m_map.m_keys.read(m_position, m_map.key_bit_width());
            m_pair = std::make_pair(read_key, m_map.m_values[m_position]);
            return &m_pair;
        }
        bool operator!=(const chaining_bucket_iterator& o) const {
            return !( (*this)  == o);
        }
        bool invalid() const {
            return m_position >= m_map.size();
        }
        bool operator==(const chaining_bucket_iterator& o) const {
          if(!o.invalid() && !invalid()) return o.m_position == m_position; // compare positions
          return o.invalid() == invalid();
        }
};


template<class key_bucket_t, class value_bucket_t, class resize_strategy_t>
class chaining_bucket {
    public:
    using key_bucket_type = key_bucket_t;
    using value_bucket_type = value_bucket_t;

    using resize_strategy_type = resize_strategy_t; //! how large a buckets becomes resized

    using key_type = typename key_bucket_type::key_type;
    using value_type = typename value_bucket_type::key_type;

    using bucketsize_type = chaining_bucket_types::bucketsize_type; //! used for storing the sizes of the buckets
    using size_type = uint64_t; //! used for addressing the i-th bucket
    using class_type = chaining_bucket<key_bucket_type, value_bucket_type, resize_strategy_type>;
    using iterator = chaining_bucket_iterator<class_type>;

    ON_DEBUG(key_type* m_plainkeys = nullptr;) //!bucket for keys in plain format for debugging purposes

    resize_strategy_type m_resize_strategy;

    key_bucket_type m_keys; //!bucket for keys
    value_bucket_type m_values;

    bucketsize_type m_elements = 0; //! number of stored elements
    const uint_fast8_t m_width;

    //!@see std::vector
    void shrink_to_fit() {
      if(!m_keys.initialized()) return;
      if(m_resize_strategy.size(m_elements) > m_elements) {
        m_keys.resize(m_elements, m_elements, m_width);
        m_values.resize(m_elements, m_elements, m_width);
        m_resize_strategy.assign(m_elements);
      }
    }

    //!@see std::vector
    size_t capacity() const {
      return m_resize_strategy.size(m_elements);
    }


    /**
     * Cleans up the hash table. Sets the hash table in its initial state.
     */
    void clear() { 
        m_keys.clear();
        m_values.clear();
        ON_DEBUG(if(m_plainkeys != nullptr) { free(m_plainkeys); m_plainkeys = nullptr; })
        m_elements = 0;
    }

    public:

    //! returns the maximum value of a key that can be stored
    key_type max_key() const { return (-1ULL) >> (64-m_width); }

    //! returns the bit width of the keys
    uint_fast8_t key_bit_width() const { return m_width; }

    //! @see std::unordered_map
    bool empty() const { return m_elements == 0; } 

    //! @see std::unordered_map
    bucketsize_type size() const {
        return m_elements;
    }

    bucketsize_type max_bucket_size() const { //! largest number of elements a bucket can contain before enlarging the hash table
        const size_t ret = m_resize_strategy.size(m_elements);
        DCHECK_LT(ret, std::numeric_limits<bucketsize_type>::max());
        return ret;
    }

    chaining_bucket(size_t width = sizeof(key_type)*8) 
        : m_width(width)
    {
        DCHECK_LE(width, sizeof(key_type)*8);
    }

    chaining_bucket(chaining_bucket&& other)
       : m_keys(std::move(other.m_keys))
       , m_values(std::move(other.m_values))
       , m_elements(std::move(other.m_elements))
       , m_resize_strategy(std::move(other.m_resize_strategy))
    {

        ON_DEBUG(m_plainkeys = std::move(other.m_plainkeys));
        other.m_elements = 0; 
    }

    chaining_bucket& operator=(chaining_bucket&& other) {
        clear();
        m_keys        = std::move(other.m_keys);
        m_values      = std::move(other.m_values);
        m_elements    = std::move(other.m_elements);
        m_resize_strategy = std::move(other.m_resize_strategy);
        ON_DEBUG(m_plainkeys = std::move(other.m_plainkeys));
        other.m_bucketsizes = nullptr; //! a hash map without buckets is already deleted
        return *this;
    }
    void swap(chaining_bucket& other) {
        DCHECK_EQ(m_width, other.m_width);
        ON_DEBUG(std::swap(m_plainkeys, other.m_plainkeys);)

        std::swap(m_keys, other.m_keys);
        std::swap(m_values, other.m_values);
        std::swap(m_elements, other.m_elements);
        std::swap(m_resize_strategy, other.m_resize_strategy);
    }

#if STATS_ENABLED
    void print_stats(tdc::StatPhase& statphase) {
            statphase.log("class", typeid(class_type).name());
            statphase.log("size", size());
            statphase.log("width", m_width);
            statphase.log("max_bucket_size", max_bucket_size());
            statphase.log("capacity", capacity());
    }
#endif

    const iterator end() const {
        return iterator { *this, static_cast<bucketsize_type>(-1ULL) };
    }

    iterator find(const key_type& key) const {
        if(!m_keys.initialized()) return end();
        const bucketsize_type position = locate(key);
        return iterator { *this, position };
    }

    bucketsize_type locate(const key_type& key) const {
#ifndef NDEBUG
        bucketsize_type plain_position = static_cast<bucketsize_type>(-1ULL);
        for(size_t i = 0; i < m_elements; ++i) { 
            const key_type read_key = m_keys.read(i, m_width);
            DCHECK_EQ(read_key, m_plainkeys[i]);
            if(m_plainkeys[i] == key) {
                plain_position = i;
                break;
            }
        }
#endif//NDEBUG

        const bucketsize_type position = static_cast<bucketsize_type>(m_keys.find(key, m_elements, m_width));

#ifndef NDEBUG
        DCHECK_EQ(position, plain_position);
        if(position != static_cast<bucketsize_type>(-1ULL)) {
            DCHECK_LT(position, m_elements);
            return position;
        }
#endif//NDEBUG
        return position;
    }

    value_type& operator[](const key_type& key) {
        DCHECK_LE(key, max_key());
        if(!m_keys.initialized()) {
            m_elements = 1;
            ON_DEBUG(m_plainkeys   = reinterpret_cast<key_type*>  (malloc(sizeof(key_type))));
            m_keys.initiate(resize_strategy_type::INITIAL_BUCKET_SIZE);
            m_values.initiate(resize_strategy_type::INITIAL_BUCKET_SIZE);
            m_resize_strategy.assign(resize_strategy_type::INITIAL_BUCKET_SIZE);
        } else { 
            const size_t position = locate(key);

            if(position != static_cast<bucketsize_type>(-1ULL)) {
                DCHECK_LT(position, m_elements);
                return m_values[position];
            }


            ++m_elements;


            ON_DEBUG(m_plainkeys   = reinterpret_cast<key_type*>  (realloc(m_plainkeys, sizeof(key_type)*m_elements));)

                if(m_resize_strategy.needs_resize(m_elements)) {
                    const size_t newsize = m_resize_strategy.size_after_increment(m_elements);
                    m_keys.resize(m_elements-1, newsize, m_width);
                    m_values.resize(m_elements-1, newsize);
                }
            DCHECK_LE(m_elements, m_resize_strategy.size(m_elements));
        }
        DCHECK_LE(m_elements, m_resize_strategy.size(m_elements));
      
        ON_DEBUG(m_plainkeys[m_elements-1] = key;)
        m_keys.write(m_elements-1, key, m_width);

        return m_values[m_elements-1];
    }

    ~chaining_bucket() { clear(); }

    /** @see std::set **/
    size_type count(const key_type& key ) const {
        return find(key) == end() ? 0 : 1;
    }

    //! @see std::set
    size_type erase(const key_type& key) {
        if(!m_keys.initialized()) { return 0; }

        const auto position = locate(key);

        if(position == static_cast<bucketsize_type>(-1ULL)) return 0;

        for(size_t i = position+1; i < m_elements; ++i) {
            m_keys.write(i-1, m_keys.read(i, m_width), m_width);
            ON_DEBUG(m_plainkeys[i-1] = m_plainkeys[i];)
            m_values[i-1] = m_values[i];
        }
        --m_elements;
        return 1;
    }

};


}//ns chaining_bucket
