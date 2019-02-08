#pragma once

#include <cstdint>
#include <functional>
#include <limits>
#include "dcheck.hpp"
#include "hash.hpp"
#include "bucket.hpp"
#include "size.hpp"
#if STATS_ENABLED
#include <tudocomp_stat/StatPhase.hpp>
#endif


namespace separate_chaining {

/**
 * Iterator of a seperate hash table
 */
template<class hash_map>
struct separate_chaining_iterator {
    public:
        using key_type = typename hash_map::key_type;
        using value_type = typename hash_map::value_type;
        using pair_type = std::pair<key_type, value_type>;
        using bucketsize_type = typename hash_map::bucketsize_type;
        using size_type = typename hash_map::size_type;
        const hash_map& m_map;
        size_type m_bucket; //! in which bucket the iterator currently is
        bucketsize_type m_position; //! at which position in the bucket

        pair_type m_pair;

        separate_chaining_iterator(const hash_map& map, size_t bucket, size_t position) 
            : m_map(map), m_bucket(bucket), m_position(position) {
            }

        const pair_type* operator->()  {
            if(m_bucket >= m_map.bucket_count()) {
                m_bucket = 0;
            }
            if(m_position >= m_map.bucket_size(m_bucket)) {
                m_position = 0;
            }

            const uint_fast8_t key_bitwidth = m_map.m_hash.remainder_width(m_map.m_buckets);
            const key_type read_quotient = m_map.m_keys[m_bucket].read(m_position, key_bitwidth);
            const key_type read_key = m_map.m_hash.inv_map(read_quotient, m_bucket, m_map.m_buckets);

            m_pair = std::make_pair(read_key, m_map.m_value_manager[m_bucket][m_position]);
            return &m_pair;
        }
        bool operator!=(const separate_chaining_iterator& o) const {
            return !( (*this)  == o);
        }
        bool operator==(const separate_chaining_iterator& o) const {
            // if(o->m_bucket == -1ULL && this->m_bucket == -1ULL) return true; // both are end()
            // if(o->m_bucket == -1ULL || this->m_bucket == -1ULL) return false; // one is end()
            return m_bucket == o.m_bucket && m_position == o.m_position; // compare positions
        }
};


//! dummy class for supporting hash sets without memory overhead
class null_value_bucket {
    bool m_true = true;
    public:
    using key_type = bool;
    bool& operator[](size_t)  {
        return m_true;
    }
    constexpr void clear() {}
    constexpr void initiate(size_t) {}
    constexpr void resize([[maybe_unused]] const size_t oldsize, [[maybe_unused]] const size_t size) {}
    null_value_bucket(null_value_bucket&&) {}
    null_value_bucket() = default;
};


//! dummy class for supporting hash sets without memory overhead
class value_dummy_manager {
    public:
    using value_bucket_type = null_value_bucket;
    using value_type = typename null_value_bucket::key_type;

    static null_value_bucket m_bucket;

    public:
    value_dummy_manager() = default;
    value_dummy_manager(value_dummy_manager&&) { }
    value_dummy_manager& operator=(value_dummy_manager&&) { return *this; }

    void clear(const size_t ) { //! empties i-th bucket
    }
    ~value_dummy_manager() {
    }
    void resize(const size_t) {
    }
    value_bucket_type& operator[](size_t) {
        return m_bucket;
    }
    const value_bucket_type& operator[](size_t) const {
        return m_bucket;
    }
    constexpr size_t value_width() const {
        return 0;
    }
};


//! Storing an array of value buckets
template<class value_bucket_t>
class value_array_manager {
    public:
    using value_bucket_type = value_bucket_t;
    using value_type = typename value_bucket_type::key_type;

    private:
    value_bucket_type* m_values = nullptr; //! bucket for values

    public:
    value_array_manager() = default;
    value_array_manager(value_array_manager&& o) : m_values(std::move(o.m_values)) { }
    value_array_manager& operator=(value_array_manager&& o) { 
        m_values = std::move(o.m_values); 
        o.m_values = nullptr; 
        return *this; 
    }

    void clear(const size_t bucket) { //! empties i-th bucket
        m_values[bucket].clear();
    }
    ~value_array_manager() {
        if(m_values != nullptr) {
            delete [] m_values;
        }
    }
    void resize(const size_t new_size) {
        m_values = new value_bucket_type[new_size];
    }
    value_bucket_type& operator[](size_t index) {
        return m_values[index];
    }
    const value_bucket_type& operator[](size_t index) const {
        return m_values[index];
    }
    constexpr size_t value_width() const {
            return sizeof(value_type);
    }


};



/**
 * key_bucket_t: a bucket from `bucket.hpp`
 * value_manager_t: Either `value_dummy_manager` or `value_array_manager<value_bucket_t>`, where `value_bucket_t` is either `class_bucket_t` or `plain_bucket_t`
 * hash_mapping_t: a hash mapping from `hash.hpp`
 * resize_strategy_t: either `arbitrary_resize` or `incremental_resize`
 */
template<class key_bucket_t, class value_manager_t, class hash_mapping_t, class resize_strategy_t>
class separate_chaining_table {
    public:
    using key_bucket_type = key_bucket_t;
    using value_manager_type = value_manager_t;
    using value_bucket_type = typename value_manager_t::value_bucket_type;
    using hash_mapping_type = hash_mapping_t;

    using resize_strategy_type = resize_strategy_t; //! how large a buckets becomes resized

    using key_type = typename key_bucket_type::key_type;
    using value_type = typename value_bucket_type::key_type;
    // static_assert(std::is_same<key_type, typename hash_mapping_t::key_type>::value, "key types of bucket and hash_mapping mismatch!") ;
    static_assert(std::numeric_limits<key_type>::max() <= std::numeric_limits<typename hash_mapping_t::key_type>::max(), "key types of bucket must have at most as many bits as key type of hash_mapping!") ;

    using bucketsize_type = separate_chaining::bucketsize_type; //! used for storing the sizes of the buckets
    using size_type = uint64_t; //! used for addressing the i-th bucket
    using class_type = separate_chaining_table<key_bucket_type, value_manager_type, hash_mapping_type, resize_strategy_type>;
    using iterator = separate_chaining_iterator<class_type>;


    resize_strategy_type m_resize_strategy;

    // static constexpr size_t MAX_BUCKETSIZE = separate_chaining::MAX_BUCKET_BYTESIZE/sizeof(key_type); //TODO: make constexpr

    ON_DEBUG(key_type** m_plainkeys = nullptr;) //!bucket for keys in plain format for debugging purposes

    key_bucket_type* m_keys = nullptr; //!bucket for keys
    value_manager_type m_value_manager;
    bucketsize_type* m_bucketsizes = nullptr; //! size of each bucket

    size_t m_buckets = 0; //! log_2 of the number of buckets
    size_t m_elements = 0; //! number of stored elements
    const uint_fast8_t m_width;
    hash_mapping_type m_hash; //! hash function

    //!@see std::vector
    void shrink_to_fit() {
        if(m_buckets == 0) return;
        const size_t cbucket_count = bucket_count();
        const uint_fast8_t key_bitwidth = m_hash.remainder_width(m_buckets);
        for(size_t bucket = 0; bucket < cbucket_count;  ++bucket) {
            const bucketsize_type& bucket_size = m_bucketsizes[bucket];
            if(m_resize_strategy.size(bucket_size, bucket) > bucket_size) { //TODO: use >=!
                m_keys[bucket].resize(bucket_size, bucket_size, key_bitwidth);
                m_value_manager[bucket].resize(bucket_size, bucket_size);
                m_resize_strategy.assign(bucket_size, bucket);
            }
        }
    }

    //!@see std::vector
    size_t capacity() const {
        const size_t cbucket_count = bucket_count();
        size_t size = 0;
        for(size_t bucket = 0; bucket < cbucket_count;  ++bucket) {
            size += m_resize_strategy.size(m_bucketsizes[bucket], bucket);
        }
        return size;
    }


    void clear(const size_t bucket) { //! empties i-th bucket
        m_value_manager.clear(bucket);
        m_keys[bucket].clear();
        ON_DEBUG(free(m_plainkeys[bucket]));
        m_bucketsizes[bucket] = 0;
    }
    
    /**
     * Do NOT call this function unless you know what you are doing!
     * Assumes that all key and value buckets are no longer allocated and 
     * that it is left to do the final clean-up.
     */
    void clear_structure() { 
        delete [] m_keys;
        ON_DEBUG(free(m_plainkeys));
        free(m_bucketsizes);
        m_bucketsizes = nullptr;
        m_buckets = 0;
        m_elements = 0;
    }

    /**
     * Cleans up the hash table. Sets the hash table in its initial state.
     */
    void clear() { //! empties hash table
        if(m_bucketsizes != nullptr) {
            for(size_t bucket = 0; bucket < (1ULL<<m_buckets); ++bucket) {
                if(m_bucketsizes[bucket] == 0) continue;
                clear(bucket);
            }
            clear_structure();
        }
    }

    public:

    //! returns the maximum value of a key that can be stored
    key_type max_key() const { return (-1ULL) >> (64-m_width); }

    //! returns the bit width of the keys
    uint_fast8_t key_bit_width() const { return m_width; }

    //! @see std::unordered_map
    bool empty() const { return m_elements == 0; } 

    //! @see std::unordered_map
    size_t size() const {
        return m_elements;
    }

    //! the maximum number of elements that can be stored with the current number of buckets.
    size_type max_size() const noexcept {
        return max_bucket_size() * bucket_count();
    }

    size_t max_bucket_size() const { //! largest number of elements a bucket can contain before enlarging the hash table
        const size_t ret = (separate_chaining::MAX_BUCKET_BYTESIZE*8)/m_width;
        DCHECK_LT(ret, std::numeric_limits<bucketsize_type>::max());
        return ret;
    }

    //! @see std::unordered_map
    size_type bucket_count() const {
        return 1ULL<<m_buckets;
    }

    //! @see std::unordered_map
    bucketsize_type bucket_size(size_type n) const {
        return m_bucketsizes[n];
    }

    separate_chaining_table(size_t width = sizeof(key_type)*8) 
        : m_width(width)
        , m_hash(m_width) 
    {
        DCHECK_LE(width, sizeof(key_type)*8);
    }

    separate_chaining_table(separate_chaining_table&& other)
       : m_keys(std::move(other.m_keys))
       , m_value_manager(std::move(other.m_value_manager))
       , m_bucketsizes(std::move(other.m_bucketsizes))
       , m_buckets(std::move(other.m_buckets))
       , m_elements(std::move(other.m_elements))
       , m_hash(std::move(other.m_hash))
       , m_resize_strategy(std::move(other.m_resize_strategy))
    {

        ON_DEBUG(m_plainkeys = std::move(other.m_plainkeys));
        other.m_bucketsizes = nullptr; //! a hash map without buckets is already deleted
    }

    separate_chaining_table& operator=(separate_chaining_table&& other) {
        clear();
        m_keys        = std::move(other.m_keys);
        m_value_manager      = std::move(other.m_value_manager);
        m_buckets     = std::move(other.m_buckets);
        m_bucketsizes = std::move(other.m_bucketsizes);
        m_hash        = std::move(other.m_hash);
        m_elements    = std::move(other.m_elements);
        m_resize_strategy = std::move(other.m_resize_strategy);
        ON_DEBUG(m_plainkeys = std::move(other.m_plainkeys));
        other.m_bucketsizes = nullptr; //! a hash map without buckets is already deleted
        return *this;
    }
    void swap(separate_chaining_table& other) {
        DCHECK_EQ(m_width, other.m_width);
        ON_DEBUG(std::swap(m_plainkeys, other.m_plainkeys);)

        std::swap(m_keys, other.m_keys);
        std::swap(m_value_manager, other.m_value_manager);
        std::swap(m_buckets, other.m_buckets);
        std::swap(m_bucketsizes, other.m_bucketsizes);
        std::swap(m_hash, other.m_hash);
        std::swap(m_elements, other.m_elements);
        std::swap(m_resize_strategy, other.m_resize_strategy);
    }

#if STATS_ENABLED
    void print_stats(tdc::StatPhase& statphase) {
            statphase.log("class", typeid(class_type).name());
            statphase.log("size", size());
            statphase.log("bytes", size_in_bytes());
            statphase.log("bucket_count", bucket_count());
            double deviation = 0;
            for(size_t i = 0; i < bucket_count(); ++i) {
                const double diff = (static_cast<double>(size()/bucket_count()) - bucket_size(i));
                deviation += diff*diff;
            }
            statphase.log("deviation", deviation);
            statphase.log("width", m_width);
            statphase.log("max_bucket_size", max_bucket_size());
            statphase.log("capacity", capacity());
    }
#endif


    //! Allocate `reserve` buckets. Do not confuse with reserving space for `reserve` elements.
    void reserve(size_t reserve) {
        size_t reserve_bits = most_significant_bit(reserve);
        if(1ULL<<reserve_bits != reserve) ++reserve_bits;
        const size_t new_size = 1ULL<<reserve_bits;

        if(m_buckets == 0) {
            ON_DEBUG(m_plainkeys   = reinterpret_cast<key_type**>  (malloc(new_size*sizeof(key_type*)));)

            m_resize_strategy.allocate(new_size);
            m_keys   = new key_bucket_type[new_size];
            m_value_manager.resize(new_size);
            m_bucketsizes  = reinterpret_cast<bucketsize_type*>  (malloc(new_size*sizeof(bucketsize_type)));
            std::fill(m_bucketsizes, m_bucketsizes+new_size, 0);
            m_buckets = reserve_bits;
        } else {
            separate_chaining_table tmp_map(m_width);
            tmp_map.reserve(new_size);
#if STATS_ENABLED
            tdc::StatPhase statphase(std::string("resizing to ") + std::to_string(reserve_bits));
            print_stats(statphase);
#endif
            const size_t cbucket_count = bucket_count();
            for(size_t bucket = 0; bucket < cbucket_count; ++bucket) {
                if(m_bucketsizes[bucket] == 0) continue;

                const uint_fast8_t key_bitwidth = m_hash.remainder_width(m_buckets);
                const key_bucket_type& bucket_keys_it = m_keys[bucket];
                for(size_t i = 0; i < m_bucketsizes[bucket]; ++i) {
                    const key_type read_quotient = bucket_keys_it.read(i, key_bitwidth);
                    const key_type read_key = m_hash.inv_map(read_quotient, bucket, m_buckets);
                    DCHECK_EQ(read_key, m_plainkeys[bucket][i]);
                    tmp_map[read_key] =  m_value_manager[bucket][i];
                }
                clear(bucket);
                //TODO: add free to save memory
            }
            clear_structure();
            swap(tmp_map);
        }
    }

    const iterator end() const {
        return iterator { *this, -1ULL, -1ULL };
    }

    iterator find(const key_type& key) const {
        if(m_buckets == 0) return end();
        const auto [quotient, bucket] = m_hash.map(key, m_buckets);
        DCHECK_EQ(m_hash.inv_map(quotient, bucket, m_buckets), key);
        const bucketsize_type position = locate(bucket, quotient);
        if(position == static_cast<bucketsize_type>(-1ULL)) {
            return end();
        }
        return iterator { *this, bucket, position };
    }

    bucketsize_type locate(const size_t& bucket, const key_type& quotient) const {
        const uint_fast8_t key_bitwidth = m_hash.remainder_width(m_buckets);
        DCHECK_LE(most_significant_bit(quotient), key_bitwidth);

        bucketsize_type& bucket_size = m_bucketsizes[bucket];
        key_bucket_type& bucket_keys = m_keys[bucket];

#ifndef NDEBUG
        key_type*& bucket_plainkeys = m_plainkeys[bucket];
        bucketsize_type plain_position = static_cast<bucketsize_type>(-1ULL);
        for(size_t i = 0; i < bucket_size; ++i) { 
            const key_type read_quotient = bucket_keys.read(i, key_bitwidth);
            ON_DEBUG(const key_type read_key = m_hash.inv_map(read_quotient, bucket, m_buckets);)
                DCHECK_EQ(read_key , bucket_plainkeys[i]);
            if(read_quotient  == quotient) {
                plain_position = i;
                break;
            }
        }
#endif//NDEBUG
        
        const bucketsize_type position = static_cast<bucketsize_type>(bucket_keys.find(quotient, bucket_size, key_bitwidth));

#ifndef NDEBUG
        DCHECK_EQ(position, plain_position);
        if(position != static_cast<bucketsize_type>(-1ULL)) {
            DCHECK_LT(position, bucket_size);
            return position;
        }
#endif//NDEBUG
        return position;
    }

    /*
     * Returns the location of a key if it is stored in the table.
     * The location is a pair consisting of the bucket and the position within the bucket.
     * If the key is not in the table, the location is the bucket where the key should be hashed into, and the position is -1.
     */
    std::pair<size_t, bucketsize_type> locate(const key_type& key) const {
        if(m_buckets == 0) throw std::runtime_error("cannot query empty hash table");
        const auto [quotient, bucket] = m_hash.map(key, m_buckets);
        DCHECK_EQ(m_hash.inv_map(quotient, bucket, m_buckets), key);

        return { bucket, locate(bucket, quotient) };
    }

    value_type& operator[](const key_type& key) {
        if(m_buckets == 0) reserve(separate_chaining::INITIAL_BUCKETS);
        const auto [quotient, bucket] = m_hash.map(key, m_buckets);
        DCHECK_EQ(m_hash.inv_map(quotient, bucket, m_buckets), key);

        bucketsize_type& bucket_size = m_bucketsizes[bucket];
        const size_t position = locate(bucket, quotient);

        value_bucket_type& bucket_values = m_value_manager[bucket];
        if(position != static_cast<bucketsize_type>(-1ULL)) {
            DCHECK_LT(position, bucket_size);
            return bucket_values[position];
        }


        if(bucket_size == max_bucket_size()) {
            // if(m_elements*separate_chaining::FAIL_PERCENTAGE < max_size()) {
            //     throw std::runtime_error("The chosen hash function is bad!");
            // }
            reserve(1ULL<<(m_buckets+1));
            return operator[](key);
        }
        ++m_elements;

        key_bucket_type& bucket_keys = m_keys[bucket];
        ON_DEBUG(key_type*& bucket_plainkeys = m_plainkeys[bucket];)
        const uint_fast8_t key_bitwidth = m_hash.remainder_width(m_buckets);

        if(bucket_size == 0) {
            bucket_size = 1;
            ON_DEBUG(bucket_plainkeys   = reinterpret_cast<key_type*>  (malloc(sizeof(key_type))));
            bucket_keys.initiate(resize_strategy_type::INITIAL_BUCKET_SIZE);
            bucket_values.initiate(resize_strategy_type::INITIAL_BUCKET_SIZE);
            m_resize_strategy.assign(resize_strategy_type::INITIAL_BUCKET_SIZE, bucket);
        } else {
            ++bucket_size;
            ON_DEBUG(bucket_plainkeys   = reinterpret_cast<key_type*>  (realloc(bucket_plainkeys, sizeof(key_type)*bucket_size));)

            if(m_resize_strategy.needs_resize(bucket_size, bucket)) {
                const size_t newsize = m_resize_strategy.size_after_increment(bucket_size, bucket);
                bucket_keys.resize(bucket_size-1, newsize, key_bitwidth);
                bucket_values.resize(bucket_size-1, newsize);
            }
        }
        DCHECK_LE(key, max_key());
        ON_DEBUG(bucket_plainkeys[bucket_size-1] = key;)
        
        DCHECK_LT((static_cast<size_t>(bucket_size-1)*key_bitwidth)/64 + ((bucket_size-1)* key_bitwidth) % 64, 64*ceil_div<size_t>(bucket_size*key_bitwidth, 64) );

        bucket_keys.write(bucket_size-1, quotient, key_bitwidth);
        DCHECK_EQ(m_hash.inv_map(bucket_keys.read(bucket_size-1, key_bitwidth), bucket, m_buckets), key);

        return bucket_values[bucket_size-1];
    }

    ~separate_chaining_table() { clear(); }

    /** @see std::set **/
    size_type count(const key_type& key ) const {
        return find(key) == end() ? 0 : 1;
    }

    //! @see std::set
    size_type erase(const key_type& key) {
        if(m_buckets == 0) return 0;

        const auto [bucket, position] = locate(key);

        bucketsize_type& bucket_size = m_bucketsizes[bucket];
        key_bucket_type& bucket_keys = m_keys[bucket];
        value_bucket_type& bucket_values = m_value_manager[bucket];
        ON_DEBUG(key_type*& bucket_plainkeys = m_plainkeys[bucket];)

        if(position == static_cast<bucketsize_type>(-1ULL)) return 0;

        const uint_fast8_t key_bitwidth = m_hash.remainder_width(m_buckets);

        for(size_t i = position+1; i < bucket_size; ++i) {
            bucket_keys.write(i-1, bucket_keys.read(i, key_bitwidth), key_bitwidth);
            ON_DEBUG(bucket_plainkeys[i-1] = bucket_plainkeys[i];)
            bucket_values[i-1] = bucket_values[i];
        }
        --bucket_size;
        --m_elements;
        return 1;
    }


    /**
     * number of bytes the hash table uses
     */
    size_type size_in_bytes() const {
        const uint_fast8_t key_bitwidth = m_hash.remainder_width(m_buckets);
        size_t bytes = sizeof(m_resize_strategy) * bucket_count() + sizeof(m_keys) + sizeof(m_value_manager) + sizeof(m_bucketsizes) + sizeof(m_buckets) + sizeof(m_elements) + sizeof(m_width) + sizeof(m_hash);
        for(size_t bucket = 0; bucket < bucket_count(); ++bucket) {
            bytes += ceil_div<size_t>(m_bucketsizes[bucket]*key_bitwidth, sizeof(key_type)*8)*sizeof(key_type);
            bytes += m_value_manager.value_width() *(m_bucketsizes[bucket]);
        }
        return bytes; 
    }

};



//! typedef for hash map
template<class key_bucket_t, class value_bucket_t, class hash_mapping_t, class resize_strategy_t = incremental_resize>
using separate_chaining_map = separate_chaining_table<key_bucket_t, value_array_manager<value_bucket_t>, hash_mapping_t, resize_strategy_t>;

//! typedef for hash set
template<class key_bucket_t, class hash_mapping_t, class resize_strategy_t = incremental_resize> 
using separate_chaining_set = separate_chaining_table<key_bucket_t, value_dummy_manager, hash_mapping_t, resize_strategy_t>;


typename value_dummy_manager::value_bucket_type value_dummy_manager::m_bucket;

}//ns separate_chaining

