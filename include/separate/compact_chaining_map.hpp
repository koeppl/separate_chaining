#pragma once

#include "separate_chaining_table.hpp"
#include <type_traits>


namespace separate_chaining {

    template<class storage_type>
    inline static void write_compact_int(storage_type* storage, const uint64_t bitposition, const uint8_t bitwidth, const uint64_t value) {
        constexpr size_t storage_bitwidth = sizeof(storage_type)*8;
        DCHECK_LT(storage_bitwidth, std::numeric_limits<uint8_t>::max());

        uint64_t* word = reinterpret_cast<uint64_t*>(storage + bitposition / storage_bitwidth);
        const uint8_t offset = bitposition % storage_bitwidth;
        tdc::tdc_sdsl::bits_impl<>::write_int(word, value, offset, bitwidth);
    }
    template<class storage_type>
    inline static uint64_t read_compact_int(const storage_type* storage, const uint64_t bitposition, const uint8_t bitwidth) {
        constexpr size_t storage_bitwidth = sizeof(storage_type)*8;
        const uint64_t* word = reinterpret_cast<const uint64_t*>(storage + bitposition / storage_bitwidth);
        const uint8_t offset = (bitposition) % storage_bitwidth;
        return tdc::tdc_sdsl::bits_impl<>::read_int(word, offset, bitwidth);
    }

    template<class map_t>
    class value_wrapper {
        using map_type = map_t;
        using size_type = typename map_type::size_type;
        using value_type = typename map_type::value_type;
        using bucketsize_type = typename map_type::bucketsize_type;
        using navigator = typename map_type::navigator;

        map_type& m_map;
        const size_type m_bucket;
        const bucketsize_type m_position;

        public:
        value_wrapper(navigator&& nav) 
            : m_map(nav.map())
            , m_bucket(nav.bucket()) 
            , m_position(nav.position())
        {}
        // value_wrapper(navigator&& nav) 
        //     : m_map(nav.map())
        //     , m_bucket(nav.bucket()) 
        //     , m_position(nav.position())
        // {}
        value_wrapper(map_type& map, const size_type bucket, const bucketsize_type position) 
            : m_map(map)
              , m_bucket(bucket)
              , m_position(position)
        {}
        value_wrapper& operator=(size_t value) {
            m_map.write_value(m_bucket, m_position, value);
            return *this;
        }
        operator size_t() const {
            return m_map.value_at(m_bucket, m_position);
        }
        template<class T>
        bool operator==(const T& other) const {
            return static_cast<size_t>(*this) == static_cast<size_t>(other);
        }
    };


/**
 * hash_mapping_t: a hash mapping from `hash.hpp`
 */
template<class hash_mapping_t, class storage_t = uint8_t>
class compact_chaining_map {
    public:
    using hash_mapping_type = hash_mapping_t;

    using storage_type = storage_t;
    constexpr static size_t storage_bitwidth = sizeof(storage_type)*8;
    using key_type = typename hash_mapping_t::key_type;
    using value_type = size_t;
    using value_ref_type = value_type;
    using value_constref_type = value_type;
    using overflow_type = dummy_overflow<key_type, value_type>;


    using bucketsize_type = separate_chaining::bucketsize_type; //! used for storing the sizes of the buckets
    using size_type = uint64_t; //! used for addressing the i-th bucket
    using class_type = compact_chaining_map<hash_mapping_type, storage_type>;
    using iterator = separate_chaining_iterator<class_type>;
    using const_iterator = separate_chaining_iterator<const class_type>;
    using navigator = separate_chaining_navigator<class_type>;
    using const_navigator = separate_chaining_navigator<const class_type>;
    using value_wrapper_type = value_wrapper<class_type>;

    static_assert(MAX_BUCKET_BYTESIZE/sizeof(key_type) <= std::numeric_limits<bucketsize_type>::max(), "enlarge separate_chaining::MAX_BUCKET_BYTESIZE for this key type!");

    ON_DEBUG(key_type** m_plainkeys = nullptr;) //!bucket for keys in plain format for debugging purposes
    ON_DEBUG(value_type** m_plainvalues = nullptr;) //!bucket for keys in plain format for debugging purposes

    ON_DEBUG(uint64_t** m_large_storage = nullptr;) //!bucket for keys
    storage_type** m_storage = nullptr; //!bucket for keys
    bucketsize_type* m_bucketsizes = nullptr; //! size of each bucket
    ON_DEBUG(size_t* m_storagesizes = nullptr;) //! bit size of each bucket

    uint_fast8_t m_buckets = 0; //! log_2 of the number of buckets
    size_t m_elements = 0; //! number of stored elements
    uint_fast8_t m_key_width;
    uint_fast8_t m_value_width;
    hash_mapping_type m_hash; //! hash function

    overflow_type m_overflow; // TODO: this is a dummy variable. Need to write the same logic as in separate_chaining_table.hpp for full functionality

    //! dummy: shrinks a bucket to its real size
    constexpr void shrink_to_fit(size_t) {}

    //!@see std::vector
    constexpr void shrink_to_fit() { }

    //!@see std::vector
    size_t capacity() const { return m_elements; }


    void clear(const size_t bucket) { //! empties i-th bucket
        ON_DEBUG(delete [] m_large_storage[bucket];)
        delete [] m_storage[bucket]; 
        ON_DEBUG(m_large_storage[bucket] = nullptr;)
        m_storage[bucket] = nullptr;
        ON_DEBUG(free(m_plainkeys[bucket]));
        ON_DEBUG(free(m_plainvalues[bucket]));
        ON_DEBUG(m_plainkeys[bucket] = nullptr);
        ON_DEBUG(m_plainvalues[bucket] = nullptr);
        m_bucketsizes[bucket] = 0;
        ON_DEBUG(m_storagesizes[bucket] = 0;)
    }
    
    /**
     * Do NOT call this function unless you know what you are doing!
     * Assumes that all key and value buckets are no longer allocated and 
     * that it is left to do the final clean-up.
     */
    void clear_structure() { 
        ON_DEBUG(delete [] m_large_storage;)
        delete [] m_storage;
        ON_DEBUG(free(m_plainkeys));
        ON_DEBUG(free(m_plainvalues));
        free(m_bucketsizes);
        ON_DEBUG(free(m_storagesizes);)
        m_bucketsizes = nullptr;
        m_buckets = 0;
        m_elements = 0;
    }

    /**
     * Cleans up the hash table. Sets the hash table in its initial state.
     */
    void clear() { //! empties hash table
        const size_t cbucket_count = bucket_count();
        if(m_bucketsizes != nullptr) {
            for(size_t bucket = 0; bucket < cbucket_count; ++bucket) {
                if(m_bucketsizes[bucket] == 0) continue;
                clear(bucket);
            }
            clear_structure();
        }
    }

    public:

    //! returns the maximum value of a key that can be stored
    key_type max_key() const { return (-1ULL) >> (64-m_key_width); }

    //! returns the maximum value that can be stored
    value_type max_value() const { return (-1ULL) >> (64-m_value_width); }

    //! returns the bit width of the keys
    uint_fast8_t key_bit_width() const { return m_key_width; }

    //! returns the bit width of the values
    uint_fast8_t value_bit_width() const { return m_value_width; }

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

    static constexpr size_t max_bucket_size() { //! largest number of elements a bucket can contain before enlarging the hash table
        return std::min<size_t>(std::numeric_limits<bucketsize_type>::max(), separate_chaining::MAX_BUCKET_BYTESIZE);
    }

    //! @see std::unordered_map
    size_type bucket_count() const {
        if(m_buckets == 0) return 0;
        return 1ULL<<m_buckets;
    }

    uint_fast8_t bucket_count_log2() const {
        return m_buckets;
    }

    //! @see std::unordered_map
    bucketsize_type bucket_size(size_type n) const {
        return m_bucketsizes[n];
    }

    compact_chaining_map(const size_t key_width = sizeof(key_type)*8, const size_t value_width = sizeof(value_type)*8) 
        : m_key_width(key_width)
        , m_value_width(value_width)
        , m_hash(m_key_width) 
    {
        DCHECK_GT(key_width, 1);
        DCHECK_LE(key_width, 64);
        DCHECK_LE(key_width, 8*sizeof(key_type));
        DCHECK_GT(value_width, 1);
        DCHECK_LE(value_width, 64);
        // DCHECK_LE(width, sizeof(key_type)*8);
    }

    compact_chaining_map(compact_chaining_map&& other)
       : m_key_width(other.m_key_width)
       , m_value_width(other.m_value_width)
       , m_storage(std::move(other.m_storage))
       , m_bucketsizes(std::move(other.m_bucketsizes))
       , m_buckets(std::move(other.m_buckets))
       , m_elements(std::move(other.m_elements))
       , m_hash(std::move(other.m_hash))
    {

       ON_DEBUG(m_large_storage = std::move(other.m_large_storage); other.m_large_storage = nullptr;)
        ON_DEBUG(m_storagesizes = std::move(other.m_storagesizes); other.m_storagesizes = nullptr;)
        ON_DEBUG(m_plainkeys = std::move(other.m_plainkeys); other.m_plainkeys = nullptr;)
        ON_DEBUG(m_plainvalues = std::move(other.m_plainvalues); other.m_plainvalues = nullptr;)
        other.m_bucketsizes = nullptr; //! a hash map without buckets is already deleted
    }

    compact_chaining_map& operator=(compact_chaining_map&& other) {
        // DCHECK_EQ(m_key_width, other.m_key_width);
        clear();
        m_key_width       = std::move(other.m_key_width);
        m_value_width       = std::move(other.m_value_width);
        m_buckets     = std::move(other.m_buckets);
        m_bucketsizes = std::move(other.m_bucketsizes);
        ON_DEBUG(m_storagesizes = std::move(other.m_storagesizes);)
        m_hash        = std::move(other.m_hash);
        m_elements    = std::move(other.m_elements);
        ON_DEBUG(m_large_storage     = std::move(other.m_large_storage);)
        m_storage     = std::move(other.m_storage);
        ON_DEBUG(m_plainkeys = std::move(other.m_plainkeys); other.m_plainkeys = nullptr;)
        ON_DEBUG(m_plainvalues = std::move(other.m_plainvalues); other.m_plainvalues = nullptr;)
        other.m_bucketsizes = nullptr; //! a hash map without buckets is already deleted
        ON_DEBUG(other.m_large_storage = nullptr;)
        other.m_storage = nullptr;
        return *this;
    }
    void swap(compact_chaining_map& other) {
        // DCHECK_EQ(m_key_width, other.m_key_width);
        ON_DEBUG(std::swap(m_plainkeys, other.m_plainkeys);)
        ON_DEBUG(std::swap(m_plainvalues, other.m_plainvalues);)
        ON_DEBUG(std::swap(m_storagesizes,other.m_storagesizes);)

        std::swap(m_key_width, other.m_key_width);
        std::swap(m_value_width, other.m_value_width);
        std::swap(m_buckets, other.m_buckets);
        std::swap(m_bucketsizes, other.m_bucketsizes);
        std::swap(m_hash, other.m_hash);
        std::swap(m_elements, other.m_elements);
        ON_DEBUG(std::swap(m_large_storage, other.m_large_storage);)
        std::swap(m_storage, other.m_storage);
    }

#if STATS_ENABLED
    void print_stats(tdc::StatPhase& statphase) {
            statphase.log("class", typeid(class_type).name());
            statphase.log("size", size());
            statphase.log("bucket_count", bucket_count());
            double deviation = 0;
            for(size_t i = 0; i < bucket_count(); ++i) {
                const double diff = (static_cast<double>(size()/bucket_count()) - bucket_size(i));
                deviation += diff*diff;
            }
            statphase.log("deviation", deviation);
            statphase.log("key_width", m_key_width);
            statphase.log("value ", m_value_width);
            statphase.log("value_width", m_value_width);
            statphase.log("max_bucket_size", max_bucket_size());
            statphase.log("capacity", capacity());
    }
#endif


    //! Allocate `reserve` buckets. Do not confuse with reserving space for `reserve` elements.
    void reserve(size_t reserve) {
        uint_fast8_t reserve_bits = most_significant_bit(reserve);
        if(1ULL<<reserve_bits != reserve) ++reserve_bits;
        const size_t new_size = 1ULL<<reserve_bits;

        if(m_buckets == 0) {
            ON_DEBUG(m_plainkeys   = reinterpret_cast<key_type**>  (malloc(new_size*sizeof(key_type*)));)
            ON_DEBUG(m_plainvalues   = reinterpret_cast<value_type**>  (malloc(new_size*sizeof(value_type*)));)
            ON_DEBUG( std::fill(m_plainkeys, m_plainkeys+new_size, nullptr);)
            ON_DEBUG( std::fill(m_plainvalues, m_plainvalues+new_size, nullptr);)

            ON_DEBUG(m_large_storage   = reinterpret_cast<uint64_t**>(malloc(new_size*sizeof(uint64_t*)));)
            m_storage   = reinterpret_cast<storage_type**>(malloc(new_size*sizeof(storage_type*)));
            m_bucketsizes  = reinterpret_cast<bucketsize_type*>  (malloc(new_size*sizeof(bucketsize_type)));
            ON_DEBUG(m_storagesizes = reinterpret_cast<size_t*> (malloc(new_size*sizeof(size_t))));
            ON_DEBUG(std::fill(m_storagesizes, m_storagesizes+new_size, 0);)
            std::fill(m_bucketsizes, m_bucketsizes+new_size, 0);
            ON_DEBUG(std::fill(m_large_storage, m_large_storage+new_size, nullptr);)
            std::fill(m_storage, m_storage+new_size, nullptr);
            m_buckets = reserve_bits;
        } else {
            compact_chaining_map tmp_map(m_key_width, m_value_width);
            tmp_map.reserve(new_size);
#if STATS_ENABLED
            tdc::StatPhase statphase(std::string("resizing to ") + std::to_string(reserve_bits));
            print_stats(statphase);
#endif
            const size_t cbucket_count = bucket_count();
            const uint_fast8_t key_bitwidth = m_hash.remainder_width(m_buckets);
            DCHECK_GT(key_bitwidth, 0);
            DCHECK_LE(key_bitwidth, key_bit_width());

            for(size_t bucket_it = 0; bucket_it < cbucket_count; ++bucket_it) {
                if(m_bucketsizes[bucket_it] == 0) continue;
                for(size_t i = 0; i < m_bucketsizes[bucket_it]; ++i) {
                    const key_type read_quotient = quotient_at(bucket_it, i, key_bitwidth);
                    const key_type read_key = m_hash.inv_map(read_quotient, bucket_it, m_buckets);
                    DCHECK_EQ(read_key, m_plainkeys[bucket_it][i]);

                    value_type read_value = value_at(bucket_it, i);
                    DCHECK_EQ(read_value, m_plainvalues[bucket_it][i]);

                    tmp_map.find_or_insert(read_key, std::move(read_value));
                }
                clear(bucket_it);
            }
            clear_structure();
            swap(tmp_map);
        }
    }
    const navigator rbegin_nav() {
        const size_t cbucket_count = bucket_count();
        if(cbucket_count == 0) return end_nav();
        for(size_t bucket = cbucket_count-1; bucket >= 0;  --bucket) {
            if(m_bucketsizes[bucket] > 0) {
                return { *this, bucket, static_cast<size_t>(m_bucketsizes[bucket]-1) };
            }
        }
        return end_nav();
    }
    const navigator rend_nav() { return end_nav(); }

    const const_iterator cend() const {
        return { *this, -1ULL, -1ULL };
    }
    const iterator end() {
        return { *this, -1ULL, -1ULL };
    }
    const iterator begin() {
        const size_t cbucket_count = bucket_count();
        for(size_t bucket = 0; bucket < cbucket_count;  ++bucket) {
            if(m_bucketsizes[bucket] > 0) {
                return { *this, bucket, 0 };
            }
        }
        return end();
    }
    const const_iterator cbegin() const {
        const size_t cbucket_count = bucket_count();
        for(size_t bucket = 0; bucket < cbucket_count;  ++bucket) {
            if(m_bucketsizes[bucket] > 0) {
                return { *this, bucket, 0 };
            }
        }
        return cend();
    }
    const navigator begin_nav() {
        const size_t cbucket_count = bucket_count();
        for(size_t bucket = 0; bucket < cbucket_count;  ++bucket) {
            if(m_bucketsizes[bucket] > 0) {
                return { *this, bucket, 0 };
            }
        }
        return end_nav();
    }
    const navigator end_nav() {
        return { *this, -1ULL, -1ULL };
    }
    const const_navigator cbegin_nav() const {
        const size_t cbucket_count = bucket_count();
        for(size_t bucket = 0; bucket < cbucket_count;  ++bucket) {
            if(m_bucketsizes[bucket] > 0) {
                return { *this, bucket, 0 };
            }
        }
        return cend_nav();
    }
    const const_navigator cend_nav() const {
        return { *this, -1ULL, -1ULL };
    }

    const_iterator find(const key_type& key) const {
        if(m_buckets == 0) return cend();
        const auto [quotient, bucket] = m_hash.map(key, m_buckets);
        DCHECK_EQ(m_hash.inv_map(quotient, bucket, m_buckets), key);
        const size_t position = locate(bucket, quotient);
        if(position == static_cast<size_t>(-1ULL)) {
            return cend();
        }
        return const_iterator { *this, bucket, position };
    }

    key_type quotient_at(const size_t bucket, const size_t position, const size_t quotient_width) const {
        DCHECK_LT((static_cast<size_t>(position)*quotient_width)/storage_bitwidth + ((position)* quotient_width) % storage_bitwidth, storage_bitwidth*ceil_div<size_t>(m_bucketsizes[bucket]*quotient_width, storage_bitwidth) );
        // const key_type ret2 = tdc::tdc_sdsl::bits_impl<>::read_int
        //     (reinterpret_cast<uint64_t*>(m_storage[bucket] + (static_cast<size_t>(position)*quotient_width)/storage_bitwidth)
        //     , ((position)* quotient_width) % storage_bitwidth
        //     , quotient_width);
        // DCHECK_EQ(ret, ret2);

        DCHECK_LE(position*quotient_width+quotient_width, m_storagesizes[bucket]);
        const key_type ret = read_compact_int(m_storage[bucket], position*quotient_width, quotient_width);
        DCHECK_EQ(ret, read_compact_int(m_storage[bucket], position*quotient_width, quotient_width));
        DCHECK_EQ(ret, m_large_storage[bucket][position]);
        return ret;
    }
    public:
    // const value_type& value_at(const size_t bucket, const size_t position) const {
    //     return const_cast<class_type&>(*this).value_at(bucket, position);
    // }

    value_type value_at(const size_t bucket, const size_t position) const {
        const uint_fast8_t quotient_bitwidth = m_hash.remainder_width(m_buckets);

        const size_t bit_position = m_bucketsizes[bucket]*quotient_bitwidth + position*value_bit_width(); 
        DCHECK_LE(bit_position+value_bit_width(), m_storagesizes[bucket]);
        value_type ret = read_compact_int(m_storage[bucket], bit_position, value_bit_width());

        DCHECK_EQ(ret, m_large_storage[bucket][m_bucketsizes[bucket]+position]);
        DCHECK_EQ(ret, m_plainvalues[bucket][position]);

        return ret;
    }

    void write_quotient(const size_t bucket, const size_t position, const uint_fast8_t quotient_width, const key_type quotient) const {
        ON_DEBUG(m_large_storage[bucket][position] = quotient;)
        DCHECK_EQ(m_large_storage[bucket][position], quotient);
        DCHECK_LT((static_cast<size_t>(position)*quotient_width)/storage_bitwidth + ((position)* quotient_width) % storage_bitwidth, storage_bitwidth*ceil_div<size_t>(m_bucketsizes[bucket]*quotient_width, storage_bitwidth) );
        DCHECK_LE(most_significant_bit(quotient), quotient_width);

        DCHECK_LE(position*quotient_width+quotient_width, m_storagesizes[bucket]);

        storage_type*& small_storage = m_storage[bucket];
        write_compact_int(small_storage, position*quotient_width, quotient_width, quotient);
        DCHECK_EQ(read_compact_int(small_storage, position*quotient_width, quotient_width), quotient);
    }


    void write_value(const size_t bucket, const size_t position, const value_type value) const {
        ON_DEBUG(m_large_storage[bucket][m_bucketsizes[bucket]+position] = value;)

        const uint_fast8_t quotient_bitwidth = m_hash.remainder_width(m_buckets);
        const size_t write_position = m_bucketsizes[bucket]*quotient_bitwidth + position*value_bit_width(); // position in bits
        DCHECK_LE(write_position+value_bit_width(), m_storagesizes[bucket]);

        write_compact_int(m_storage[bucket], write_position, value_bit_width(), value);
        DCHECK_EQ(m_large_storage[bucket][m_bucketsizes[bucket]+position], value);
        ON_DEBUG(m_plainvalues[bucket][position] = value);
    }

    private:

    void realloc_bucket(const size_t bucket, size_t oldsize, uint_fast8_t quotient_bitwidth) {
        const bucketsize_type& bucketsize = m_bucketsizes[bucket];
        ON_DEBUG(m_large_storage[bucket] = reinterpret_cast<uint64_t*>(realloc(m_large_storage[bucket], (sizeof(size_t)*2)*bucketsize)));

        if(ceil_div<size_t>(oldsize*(quotient_bitwidth+value_bit_width()), storage_bitwidth) != ceil_div<size_t>((bucketsize)*(quotient_bitwidth+value_bit_width()), storage_bitwidth)) {
            m_storage[bucket] = reinterpret_cast<storage_type*>  (realloc(m_storage[bucket], sizeof(storage_type) * ceil_div<size_t>(bucketsize*(quotient_bitwidth+value_bit_width()), storage_bitwidth ) ));
       }
        ON_DEBUG(m_storagesizes[bucket] = 8 * sizeof(storage_type) * ceil_div<size_t>(bucketsize*(quotient_bitwidth+value_bit_width()), storage_bitwidth); )

    }

    void enlarge_storage(const size_t bucket, uint_fast8_t quotient_bitwidth) {
        bucketsize_type& bucket_size = m_bucketsizes[bucket];
        const bucketsize_type old_bucket_size = bucket_size;
        DCHECK_LT(bucket_size, std::numeric_limits<bucketsize_type>::max());
        ++bucket_size;

        realloc_bucket(bucket, bucket_size-1, quotient_bitwidth);

        ON_DEBUG(uint64_t*& large_storage = m_large_storage[bucket];
        for(int i = old_bucket_size*2; i > old_bucket_size; --i) {
            large_storage[i] = large_storage[i-1];
        })
        
        storage_type*& small_storage = m_storage[bucket];
        for(size_t i = 1; i <= old_bucket_size; ++i) {
            const size_t from_bit_position = old_bucket_size*quotient_bitwidth + (old_bucket_size - i)*value_bit_width();
            const size_t to_bit_position = bucket_size*quotient_bitwidth + (old_bucket_size - i)*value_bit_width();
            const uint64_t read_value = read_compact_int(small_storage, from_bit_position, value_bit_width());

            DCHECK_LE(to_bit_position+value_bit_width(), m_storagesizes[bucket]);
            write_compact_int(small_storage, to_bit_position, value_bit_width(), read_value);
        }

        ON_DEBUG(
        for(size_t i = 0; i < old_bucket_size; ++i) {
            quotient_at(bucket, i, quotient_bitwidth);
            value_at(bucket,i);
        })

        ON_DEBUG(m_plainkeys[bucket] = reinterpret_cast<key_type*>  (realloc(m_plainkeys[bucket], sizeof(key_type)*bucket_size));)
        ON_DEBUG(m_plainvalues[bucket] = reinterpret_cast<value_type*>  (realloc(m_plainvalues[bucket], sizeof(value_type)*bucket_size));)

    }


    size_t locate(const size_t& bucket, const key_type& quotient) const {
        const uint_fast8_t key_bitwidth = m_hash.remainder_width(m_buckets);
        DCHECK_GT(key_bitwidth, 0);
        DCHECK_LE(key_bitwidth, key_bit_width());
        DCHECK_LE(most_significant_bit(quotient), key_bitwidth);

        bucketsize_type& bucket_size = m_bucketsizes[bucket];

        ON_DEBUG(key_type*& bucket_plainkeys = m_plainkeys[bucket];)
        size_t position = static_cast<size_t>(-1ULL);
        for(size_t i = 0; i < bucket_size; ++i) { 
            const key_type read_quotient = quotient_at(bucket, i, key_bitwidth);
            ON_DEBUG(const key_type read_key = m_hash.inv_map(read_quotient, bucket, m_buckets);)
            DCHECK_EQ(read_key , bucket_plainkeys[i]);
            if(read_quotient  == quotient) {
                position = i;
                break;
            }
        }

#ifndef NDEBUG
        if(position != static_cast<size_t>(-1ULL)) {
            DCHECK_LT(position, bucket_size);
            return position;
        }
#endif//NDEBUG
        return position;
    }

    public:
    /*
     * Returns the location of a key if it is stored in the table.
     * The location is a pair consisting of the bucket and the position within the bucket.
     * If the key is not in the table, the location is the bucket where the key should be hashed into, and the position is -1.
     */
    std::pair<size_t, size_t> locate(const key_type& key) const {
        if(m_buckets == 0) throw std::runtime_error("cannot query empty hash table");
        const auto [quotient, bucket] = m_hash.map(key, m_buckets);
        DCHECK_EQ(m_hash.inv_map(quotient, bucket, m_buckets), key);

        return { bucket, locate(bucket, quotient) };
    }

    navigator find_or_insert(const key_type& key, value_type&& value) {
        DCHECK_GT(key_bit_width(), 1);
        if(m_buckets == 0) reserve(std::min<size_t>(key_bit_width()-1, separate_chaining::INITIAL_BUCKETS));
        const auto [quotient, bucket] = m_hash.map(key, m_buckets);
        DCHECK_EQ(m_hash.inv_map(quotient, bucket, m_buckets), key);

        bucketsize_type& bucket_size = m_bucketsizes[bucket];
        const size_t position = locate(bucket, quotient);

        if(position != static_cast<size_t>(-1ULL)) {
            DCHECK_LT(position, bucket_size);
            return { *this, bucket ,position };
        }


        if(bucket_size == max_bucket_size()) {
            // if(m_elements*separate_chaining::FAIL_PERCENTAGE < max_size()) {
            //     throw std::runtime_error("The chosen hash function is bad!");
            // }
            reserve(1ULL<<(m_buckets+1));
            return find_or_insert(key, std::move(value));
        }
        ++m_elements;

        const uint_fast8_t key_bitwidth = m_hash.remainder_width(m_buckets);
        DCHECK_GT(key_bitwidth, 0);
        DCHECK_LE(key_bitwidth, key_bit_width());

        const size_t former_bucket_size = bucket_size;

        enlarge_storage(bucket, key_bitwidth);
        write_quotient(bucket, former_bucket_size, key_bitwidth, quotient);
        write_value(bucket, former_bucket_size, value);

#ifndef NDEBUG
        key_type*& bucket_plainkeys = m_plainkeys[bucket];
        value_type*& bucket_plainvalues = m_plainvalues[bucket];
        bucket_plainkeys[former_bucket_size] = key;
        bucket_plainvalues[former_bucket_size] = value;
        for(size_t i = 0; i < bucket_size; ++i) {
            const key_type read_quotient = quotient_at(bucket, i, key_bitwidth);
            const key_type read_key = m_hash.inv_map(read_quotient, bucket, m_buckets);
            DCHECK_EQ(read_key, bucket_plainkeys[i]);
            DCHECK_EQ(value_at(bucket, i), bucket_plainvalues[i]);
        }
#endif
        return { *this, bucket, static_cast<size_t>(bucket_size-1) };
    }


    value_wrapper_type operator[](const key_type& key) {
        return { find_or_insert(key, value_type()) };
    }

    ~compact_chaining_map() { clear(); }

    /** @see std::set **/
    size_type count(const key_type& key ) const {
        return find(key) == cend() ? 0 : 1;
    }

    size_type erase(const size_t bucket, const size_t position) {
        if(position == static_cast<size_t>(-1ULL)) return 0;


        bucketsize_type& bucket_size = m_bucketsizes[bucket];
        ON_DEBUG(key_type*& bucket_plainkeys = m_plainkeys[bucket];)
        ON_DEBUG(value_type*& bucket_plainvalues = m_plainvalues[bucket];)

        const uint_fast8_t quotient_bitwidth = m_hash.remainder_width(m_buckets);
        DCHECK_GT(quotient_bitwidth, 0);
        DCHECK_LE(quotient_bitwidth, key_bit_width());

#ifndef NDEBUG
        for(size_t i = 0; i < bucket_size; ++i) {
            const key_type read_quotient = quotient_at(bucket, i, quotient_bitwidth);
            const key_type read_key = m_hash.inv_map(read_quotient, bucket, m_buckets);
            DCHECK_EQ(read_key, bucket_plainkeys[i]);
            DCHECK_EQ(value_at(bucket, i), bucket_plainvalues[i]);
        }
#endif

        ON_DEBUG(
        for(size_t i = position+1; i < bucket_size; ++i) {
            bucket_plainkeys[i-1] = bucket_plainkeys[i];
            bucket_plainvalues[i-1] = bucket_plainvalues[i];
        })

        for(size_t i = position+1; i < bucket_size; ++i) {
            write_quotient(bucket, i-1, quotient_bitwidth, quotient_at(bucket, i, quotient_bitwidth));
        }
        // for(size_t i = 1; i < position; ++i) {
        //     write_value_(bucket, i-1, value_at_(bucket, i));
        // }
        // for(size_t i = position+1; i < bucket_size+1; ++i) {
        //     write_value_(bucket, i-2, value_at_(bucket, i));
        // }

        ON_DEBUG(
        uint64_t*& large_storage = m_large_storage[bucket];
        for(size_t i = bucket_size; i < bucket_size+position; ++i) {
            large_storage[i-1] = large_storage[i];
        }
        for(size_t i = bucket_size+position+1; i < 2*bucket_size; ++i) {
            large_storage[i-2] = large_storage[i];
        })

        storage_type*& small_storage = m_storage[bucket];
        for(size_t i = 0; i < position; ++i) {
            const size_t from_bit_position = (bucket_size)*quotient_bitwidth + (i)*value_bit_width();
            const size_t to_bit_position = (bucket_size-1)*quotient_bitwidth + (i)*value_bit_width();
            const uint64_t read_value = read_compact_int(small_storage, from_bit_position, value_bit_width());

            DCHECK_LE(to_bit_position+value_bit_width(), m_storagesizes[bucket]);
            write_compact_int(small_storage, to_bit_position, value_bit_width(), read_value);
        }
        for(size_t i = position+1; i < bucket_size; ++i) {
            const size_t from_bit_position = (bucket_size)*quotient_bitwidth + (i)*value_bit_width();
            const size_t to_bit_position = (bucket_size-1)*quotient_bitwidth + (i-1)*value_bit_width();
            const uint64_t read_value = read_compact_int(small_storage, from_bit_position, value_bit_width());

            DCHECK_LE(to_bit_position+value_bit_width(), m_storagesizes[bucket]);
            write_compact_int(small_storage, to_bit_position, value_bit_width(), read_value);
        }


        DCHECK_GT(bucket_size, 0);
        --bucket_size;
        realloc_bucket(bucket, bucket_size+1, quotient_bitwidth);
        --m_elements;


        
#ifndef NDEBUG
        for(size_t i = 0; i < bucket_size; ++i) {
            const key_type read_quotient = quotient_at(bucket, i, quotient_bitwidth);
            const key_type read_key = m_hash.inv_map(read_quotient, bucket, m_buckets);
            DCHECK_EQ(read_key, bucket_plainkeys[i]);
            DCHECK_EQ(value_at(bucket, i), bucket_plainvalues[i]);
        }
#endif


        if(bucket_size == 0) { //clear the bucket if it becomes empty
            clear(bucket);
        }
        return 1;
    }


    //! @see std::set
    size_type erase(const key_type& key) {
        if(m_buckets == 0) return 0;
        const auto [bucket, position] = locate(key);
        return erase(bucket, position);

    }

    size_type erase(const navigator& it) {
        return erase(it.bucket(), it.position());
    }
    size_type erase(const const_navigator& it) {
        return erase(it.bucket(), it.position());

    }

};


}//ns separate_chaining

