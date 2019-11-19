#include <tudocomp/util/sdsl_bits.hpp>
#include "dcheck.hpp"
#include "bucket.hpp"
#include "select.hpp"
#include "hash.hpp"
#include "overflow.hpp"

namespace separate_chaining {


/**
 * the internal representation of a key or value array in a `keyvalue_group`
 * It is unaware of its size, since a `keyvalue_group` can store for keys and values a separate `core_group` with the respective key and value bit widths.
**/
template<class internal_t>
class core_group {
    using storage_type = size_t; //TODO: separate storage_type and key_type
    ON_DEBUG(size_t m_length;)

    protected:
    storage_type* m_data = nullptr; //!bucket for keys

    public:
    bool initialized() const { return m_data != nullptr; } //!check whether we can add elements to the bucket

    void clear() {
        if(m_data != nullptr) {
            free(m_data);
        }
        m_data = nullptr;
        ON_DEBUG(m_length = 0;)
    }

    storage_type& operator[](const size_t index) {
        DDCHECK_LT(index, m_length);
        return m_data[index];
    }
    const storage_type& operator[](const size_t index) const {
        DDCHECK_LT(index, m_length);
        return m_data[index];
    }

    void initialize([[maybe_unused]] const uint_fast8_t width) {
       DDCHECK(m_data == nullptr);
       m_data = reinterpret_cast<storage_type*>(malloc(sizeof(storage_type)));
       ON_DEBUG(m_length = 0;)
    }

    void insert(const size_t index, const storage_type& key, [[maybe_unused]] const uint_fast8_t keywidth, size_t length) { //! length = old length
      DDCHECK(m_data != nullptr);
      DDCHECK_EQ(length, m_length);
      ON_DEBUG(m_length = length+1;)
      m_data = reinterpret_cast<storage_type*>  (realloc(m_data, sizeof(storage_type)*(length+1)));
      for(size_t i = length; i > index; --i) {
        m_data[i] = m_data[i-1];
      }
        m_data[index] = key;
    }
    storage_type read(size_t i, [[maybe_unused]]  size_t width) const {
        DDCHECK_LT(i, m_length);
        return m_data[i];
    }
    /*
     * search in the interval [`position_from`, `position_to`) for `key`
     */
    size_t find(const size_t position_from, const storage_type& key, const size_t position_to, [[maybe_unused]] const size_t width = 0) const {
      DDCHECK_LE(position_to, m_length);
      for(size_t i = position_from; i < position_to; ++i) {
        if(m_data[i] == key) return i;
      }
      return -1ULL;
    }

    core_group() = default;
    ~core_group() { clear(); }

    core_group(core_group&& other) 
        : m_data(std::move(other.m_data))
    {
        other.m_data = nullptr;
        ON_DEBUG(m_length = other.m_length; other.m_data = 0;)
    }
    core_group(storage_type*&& keys) 
        : m_data(std::move(keys))
    {
        keys = nullptr;
    }

    core_group& operator=(core_group&& other) {
        clear();
        m_data = std::move(other.m_data);
        other.m_data = nullptr;
        ON_DEBUG(m_length = other.m_length; other.m_length = 0;)
        return *this;
    }

};

/**!
 * `internal_t` is a tradeoff between the number of mallocs and unused space, as it defines the block size in which elements are stored, 
 * i.e., its memory consuption is quantisized by this type's byte size
**/
template<class internal_t = uint64_t>
class keyvalue_group {
    public:
    using internal_type = internal_t;
    using storage_type = uint64_t;
    using bucketsize_type = uint32_t;
    using core_group_type = core_group<internal_t>;
    static constexpr uint_fast8_t internal_bitwidth = sizeof(internal_type)*8;
    ON_DEBUG(size_t m_border_length;)
    bucketsize_type m_size; // number of elements in this group

    private:
    core_group_type m_keys; //! bucket for keys
    core_group_type m_values; //! bucket for values
    internal_type* m_border = nullptr; //! bit vector marking the borders, stores a 1 for each m_groupsize and a zero for each m_size
    ON_DEBUG(std::vector<size_t> m_border_array;)
    using group_index_type = uint16_t;
    group_index_type m_groupsize = 0; // m_groupsize + m_size  = bit vector length of m_border


    public:
    keyvalue_group() = default;

    bool empty() const { return m_size == 0; }

    // void deserialize(std::istream& is, const size_t size, const uint_fast8_t keywidth, const uint_fast8_t valuewidth) {
    //    ON_DEBUG(is.read(reinterpret_cast<char*>(&m_length), sizeof(decltype(m_length))));
    //    DDCHECK_LE(size, m_length);
    //    const size_t read_length = ceil_div<size_t>(size*width, storage_bitwidth);
    //    m_data = reinterpret_cast<internal_type*>  (malloc(sizeof(internal_type)*read_length));
    //    is.read(reinterpret_cast<char*>(m_data), sizeof(internal_type)*read_length);
    // }
    // void serialize(std::ostream& os, const size_t size, const uint_fast8_t width) const {
    //    ON_DEBUG(os.write(reinterpret_cast<const char*>(&m_length), sizeof(decltype(m_length))));
    //    DDCHECK_LE(size, m_length);
    //    const size_t write_length = ceil_div<size_t>(size*width, storage_bitwidth);
    //    os.write(reinterpret_cast<const char*>(m_data), sizeof(internal_type)*write_length);
    // }
    // static constexpr size_t size_in_bytes(const size_t size, const size_t width = 0) {
    //    ON_DEBUG(return size*sizeof(internal_type) + sizeof(m_length));
    //    const size_t length = ceil_div<size_t>(size*width, storage_bitwidth);
    //    return length*sizeof(internal_type);
    // }
    size_t size() const { return m_size; }
    size_t groupsize() const { return m_groupsize;} //! returns the number of buckets within this group
    size_t bucketsize(size_t i) const {  //! returns the number of elements in the i-th bucket
        if(empty()) { return 0; }
        const size_t prev_position = (i == 0) ? 0 : find_group_position(i-1) - (i-1);
        DCHECK_GE(find_group_position(i), i + prev_position);
        const size_t ret = find_group_position(i) - i - prev_position;
        DCHECK_EQ(ret, m_border_array[i] - (i == 0 ? 0 : m_border_array[i-1]));
        return ret;
    }


    bool initialized() const { return m_border != nullptr; } //!check whether we can add elements to the bucket
    void clear() {
        if(m_border != nullptr) { free(m_border); }
        m_border = nullptr;
        ON_DEBUG(m_border_array.clear();)
        m_keys.clear();
        m_values.clear();
    }


    void initialize(const size_t groupsize, const uint_fast8_t keywidth, const uint_fast8_t valuewidth) {
       DDCHECK(m_border == nullptr);
       m_keys.initialize(keywidth);
       m_values.initialize(valuewidth);
       m_size = 0;

       DCHECK_LT(groupsize, std::numeric_limits<decltype(m_groupsize)>::max());
       m_groupsize = groupsize;
       m_border = reinterpret_cast<internal_type*>  (malloc(sizeof(internal_type)* ceil_div<size_t>(groupsize+1, internal_bitwidth) ));
       memset(m_border, static_cast<char>(-1ULL), ceil_div<size_t>(groupsize+1, 8)); // groupsize+1 as we use the last '1' as a dummy border
       internal_type& last_border = m_border[ceil_div<size_t>(groupsize+1, internal_bitwidth)-1];
       last_border = static_cast<internal_type>(-1ULL)>>(internal_bitwidth - ((groupsize+1) % internal_bitwidth));

      ON_DEBUG(m_border_length = ceil_div<size_t>(groupsize+1, internal_bitwidth);)
      // ON_DEBUG(
              m_border_array.resize(groupsize, 0);
              {
              size_t sum = 0;
              for(size_t i = 0; i < m_border_length;++i) {
                sum += __builtin_popcountll(m_border[i]);
              }
              DCHECK_EQ(sum, m_groupsize+1);
              }//);
    }

    // void resize(const size_t oldsize, const size_t length, const size_t keywidth, const size_t valuewidth) {
    //   m_keys.resize(:
    //    if(ceil_div<size_t>((oldsize)*width, internal_bitwidth) < ceil_div<size_t>((length)*width, internal_bitwidth)) {
    //       m_data = reinterpret_cast<internal_type*>  (realloc(m_data, sizeof(internal_type) * ceil_div<size_t>(length*width, internal_bitwidth ) ));
    //    }
    //    ON_DEBUG(m_length = ceil_div<size_t>(length*width, internal_bitwidth);)
    // }
    //
    
    /**
     * Selects the `groupindex`-th one in the bit vector `m_border`.
     * This corresponds to the ending of the group `groupindex`
     * Subtracting `groupindex` from this return value gives the last entry position of the group `groupindex` in `m_keys` and `m_values`
     */ 
    size_t find_group_position(const group_index_type groupindex) const {
      group_index_type remaining_groupindex = groupindex;
      size_t sum = 0;
      const uint64_t* border_chunks = reinterpret_cast<uint64_t*>(m_border);
      for(size_t border_index = 0; ; ++border_index) {
        DDCHECK_LT(border_index, ceil_div<size_t>(m_border_length*sizeof(internal_type),sizeof(uint64_t)));
        const size_t popcount = __builtin_popcountll(border_chunks[border_index]);
        if(popcount <= remaining_groupindex) { remaining_groupindex -= popcount; sum += 64; continue; }
        DDCHECK_EQ(sum + bits::select64(border_chunks[border_index], remaining_groupindex+1), m_border_array[groupindex]+groupindex);
        return sum + bits::select64(border_chunks[border_index], remaining_groupindex+1);
      }
    }

    void push_back(const group_index_type groupindex, const storage_type key, const uint_fast8_t keywidth, const storage_type value, const uint_fast8_t valuewidth) {
      DCHECK_LT(m_size, std::numeric_limits<bucketsize_type>::max());

      const size_t group_ending = find_group_position(groupindex);
      m_keys.insert(group_ending-groupindex, key, keywidth, m_size);
      m_values.insert(group_ending-groupindex, value, valuewidth, m_size);
      ++m_size;
      const size_t new_border_size = ceil_div<size_t>(m_size + 1 + m_groupsize, internal_bitwidth);
      if(new_border_size > ceil_div<size_t>(m_size + m_groupsize, internal_bitwidth)) {
          m_border = reinterpret_cast<internal_type*>(realloc(m_border, sizeof(internal_type) * new_border_size));
          m_border[new_border_size-1] = 0;
          ON_DEBUG(++m_border_length;)
      }

      internal_type& current_chunk = m_border[group_ending/internal_bitwidth];

      bool highest_bit = current_chunk & 1ULL<<(internal_bitwidth-1);
      current_chunk = ((current_chunk) &  ((1ULL<< (group_ending % internal_bitwidth))-1) )
          | ((current_chunk<<1) &  ((-1ULL<< ((group_ending % internal_bitwidth)+1))) );

      for(size_t i = group_ending/internal_bitwidth+1; i < new_border_size; ++i) {
          bool new_highest_bit = m_border[i] & 1ULL<<(internal_bitwidth-1);
          m_border[i] = (m_border[i] << 1) | highest_bit;
          highest_bit = new_highest_bit;
      }
      ON_DEBUG({ // check whether all group '1's are still set
              size_t sum = 0;
              for(size_t i = 0; i < m_border_length;++i) {
                sum += __builtin_popcountll(m_border[i]);
              }
              DCHECK_EQ(sum, m_groupsize+1);
              });

      ON_DEBUG(// check whether the group sizes are correct
              for(size_t i = groupindex; i < m_border_array.size(); ++i) {
                ++m_border_array[i];
              }
              for(size_t i = 0; i < m_border_array.size(); ++i) {
                DCHECK_EQ(m_border_array[i], find_group_position(i)-i);
                }
              )


    }

    storage_type read_key(size_t groupindex, size_t position, size_t keywidth) const {
      DCHECK_LT(groupindex, m_groupsize);
      const size_t group_begin = groupindex == 0 ? 0 : find_group_position(groupindex-1)+1;
      return  m_keys.read(group_begin+position-groupindex, keywidth);
    }
    storage_type read_value(size_t groupindex, size_t position, size_t keywidth) const {
      DCHECK_LT(groupindex, m_groupsize);
      const size_t group_begin = groupindex == 0 ? 0 : find_group_position(groupindex-1)+1;
      return  m_values.read(group_begin+position-groupindex, keywidth);
    }

    std::pair<storage_type,storage_type> read(size_t groupindex, size_t position, size_t keywidth, size_t valuewidth) const {
      DCHECK_LT(groupindex, m_groupsize);
      const size_t group_begin = groupindex == 0 ? 0 : find_group_position(groupindex-1)+1;
      ON_DEBUG(
          const size_t next_group_begin = find_group_position(groupindex);
          DCHECK_LT(position, next_group_begin-group_begin);
          );
      return { m_keys.read(group_begin+position-groupindex, keywidth), m_values.read(group_begin+position-groupindex, valuewidth) };
    }

    /**
     * Gives the position of the entry with key `key` inside the group `groupindex`, or -1 if such an entry does not exist.
     */
    size_t find(const group_index_type groupindex, const storage_type& key, const uint_fast8_t keywidth) const {
      DCHECK_LT(groupindex, m_groupsize);
      const size_t group_begin = groupindex == 0 ? 0 : find_group_position(groupindex-1)+1;
      const size_t group_next_begin = find_group_position(groupindex);
      const size_t ret = m_keys.find(group_begin-groupindex, key, group_next_begin-groupindex, keywidth);
      return ret == (-1ULL) ? (-1ULL) : ret - (group_begin-groupindex); // do not subtract position from invalid position -1ULL
    }



    ~keyvalue_group() { clear(); }

    keyvalue_group(keyvalue_group&& other) 
        : m_keys(std::move(other.m_keys))
        , m_values(std::move(other.m_values))
        , m_size(std::move(other.m_size))
        , m_border(std::move(other.m_border))
    {
        other.m_border = nullptr;
        ON_DEBUG(m_border_length = std::move(other.m_border_length);)
    }

    keyvalue_group& operator=(keyvalue_group&& other) {
        clear();
        m_keys = std::move(other.m_keys);
        m_values = std::move(other.m_values);
        m_border = std::move(other.m_border);
        other.m_keys = nullptr;
        other.m_values = nullptr;
        other.m_border = nullptr;
        ON_DEBUG(m_border_length = other.m_border_length; other.m_border_length = 0;)
        return *this;
    }
};

// invariant: a keyvalue_group stores `key_width` many groups


/**
 * Iterator of a seperate hash table
 */
template<class hash_map>
struct group_chaining_navigator {
    public:
        using storage_type = typename hash_map::storage_type;
        using key_type = typename hash_map::key_type;
        using value_type = typename hash_map::value_type;
        // using value_ref_type = typename hash_map::value_ref_type;
        // using value_constref_type = typename hash_map::value_constref_type;
        using size_type = typename hash_map::size_type;
        using class_type = group_chaining_navigator<hash_map>;

        //private:
        hash_map& m_map;
        size_type m_bucket; //! in which bucket the iterator currently is
        size_type m_position; //! at which position in the bucket


        bool invalid() const {
            if(m_map.m_overflow.size() > 0 && m_bucket == m_map.bucket_count() && m_map.m_overflow.valid_position(m_position)) 
                { return false; }

            return m_bucket >= m_map.bucket_count() || m_position >= m_map.m_groups[m_map.bucketgroup(m_bucket)].bucketsize(m_map.rank_in_group(m_bucket));
        }

    public:
        hash_map& map() {
            return m_map;
        }
        const hash_map& map() const{
            return m_map;
        }
        const size_t& bucket() const {
            return m_bucket;
        }
        const size_t& position() const {
            return m_position;
        }
        group_chaining_navigator(hash_map& map, size_t bucket, size_t position) 
            : m_map(map), m_bucket(bucket), m_position(position) {
            }

        const key_type key()  const {
            DDCHECK(!invalid());
            const uint_fast8_t key_bitwidth = m_map.m_hash.remainder_width(m_map.m_buckets);
            DDCHECK_GT(key_bitwidth, 0);
            DDCHECK_LE(key_bitwidth, m_map.key_width());
            if(m_map.m_overflow.size() > 0 && m_bucket == m_map.bucket_count()) {
                return m_map.m_overflow.key(m_position);
            }

            const storage_type read_quotient = m_map.quotient_at(m_bucket, m_position, key_bitwidth);
            const key_type read_key = m_map.m_hash.inv_map(read_quotient, m_bucket, m_map.m_buckets);
            return read_key;
        }
        //typename std::add_const<value_type>::type& value() const {
        value_type value() const {
            DDCHECK(!invalid());
            if(m_map.m_overflow.size() > 0 && m_bucket == m_map.bucket_count()) {
                return m_map.m_overflow[m_position];
            }
            value_type i = m_map.value_at(m_bucket, m_position);
            return i;
        }
        // value_ref_type value_ref() {
        //     DDCHECK(!invalid());
        //     if(m_map.m_overflow.size() > 0 && m_bucket == m_map.bucket_count()) {
        //         return m_map.m_overflow[m_position];
        //     }
        //     return m_map.value_at(m_bucket, m_position);
        // }
        //
        class_type& operator++() { 
            if(m_map.m_overflow.size() > 0 && m_bucket == m_map.bucket_count()) {
                m_position = m_map.m_overflow.next_position(m_position);
                return *this;
            }

            DDCHECK_LT(m_bucket, m_map.bucket_count());
            if(m_position+1 >= m_map.bucket_size(m_bucket)) { 
                m_position = 0;
                do { //search next non-empty bucket
                    ++m_bucket;
                } while(m_bucket < m_map.bucket_count() && m_map.bucket_size(m_bucket) == 0); 
                return *this;
            } 
            ++m_position;
            return *this;
        }
        class_type& operator--() { 
            if(m_map.m_overflow.size() > 0 && m_bucket == m_map.bucket_count()) {
                if(m_position > 0) {
                    m_position = m_map.m_overflow.previous_position(m_position);
                    return *this;
                }
                --m_bucket;
                m_position = std::min<size_t>(m_position,  m_map.bucket_size(m_bucket))-1;
                return *this;
            }
            DDCHECK_LT(m_bucket, m_map.bucket_count());
            if(m_position > 0 && m_map.bucket_size(m_bucket) > 0) {
                m_position = std::min<size_t>(m_position,  m_map.bucket_size(m_bucket))-1; // makes an invalid pointer valid after erasing
                return *this;
            }
            do { //search previous non-empty bucket
                --m_bucket;
            } while(m_bucket < m_map.bucket_count() && m_map.bucket_size(m_bucket) == 0); 
            if(m_bucket < m_map.bucket_count()) {
                DDCHECK_NE(m_map.bucket_size(m_bucket), 0);
                m_position = m_map.bucket_size(m_bucket)-1;
            }
            return *this;
        }

        template<class U>
        bool operator!=(const group_chaining_navigator<U>& o) const {
            return !( (*this)  == o);
        }
        template<class U>
        bool operator==(const group_chaining_navigator<U>& o) const {
          if(!o.invalid() && !invalid()) return m_bucket == o.m_bucket && m_position == o.m_position; // compare positions
          return o.invalid() == invalid();
        }
};

/**
 * Iterator of a seperate hash table
 */
template<class hash_map>
struct group_chaining_iterator : public group_chaining_navigator<hash_map> {
    public:
        using key_type = typename hash_map::key_type;
        using value_type = typename hash_map::value_type;
        using pair_type = std::pair<key_type, value_type>;
        using size_type = typename hash_map::size_type;
        using class_type = group_chaining_iterator<hash_map>;
        using super_type = group_chaining_navigator<hash_map>;

        pair_type m_pair;

        void update() {
            m_pair = std::make_pair(super_type::key(), super_type::value());
        }

    public:
        group_chaining_iterator(hash_map& map, size_t bucket, size_t position) 
            : super_type(map, bucket, position) { 
                if(!super_type::invalid()) { update();}
            }

        class_type& operator++() { 
            super_type::operator++();
            if(!super_type::invalid()) { update(); }
            return *this;
        }

        const pair_type& operator*() const {
            DDCHECK(*this != super_type::m_map.cend());
            return m_pair;
        }

        const pair_type* operator->() const {
            DDCHECK(*this != super_type::m_map.cend());
            return &m_pair;
        }

        template<class U>
        bool operator!=(const group_chaining_iterator<U>& o) const {
            return !( (*this)  == o);
        }

        template<class U>
        bool operator==(const group_chaining_iterator<U>& o) const {
            return super_type::operator==(o);
        }
};

//! Storing an array of value buckets

/**
 * key_bucket_t: a bucket from `bucket.hpp`
 * hash_mapping_t: a hash mapping from `hash.hpp`
 * resize_strategy_t: either `arbitrary_resize` or `incremental_resize`
 */
template<class hash_mapping_t = xorshift_hash<>, class overflow_t = dummy_overflow<uint64_t,uint64_t>> //TODO: make overflow types bit-aware!
class group_chaining_table {
    public:
    using key_type = uint64_t;
    using value_type = uint64_t;
    using storage_type = uint64_t;
    using size_type = uint64_t; //! used for addressing the i-th bucket
	using overflow_type = overflow_t;
    using keyvalue_group_type = keyvalue_group<>;
    // using core_group_type = keyvalue_group_type::core_group_type;
    using bucketsize_type = keyvalue_group_type::bucketsize_type;

    using hash_mapping_type = hash_mapping_t;
    static_assert(std::is_same<typename hash_mapping_t::storage_type, storage_type>::value, "hash_mapping_t::storage_type must be uint64_t!");

    using class_type = group_chaining_table<hash_mapping_type, overflow_type>;
    using iterator = group_chaining_iterator<class_type>;
    using const_iterator = group_chaining_iterator<const class_type>;
    using navigator = group_chaining_navigator<class_type>;
    using const_navigator = group_chaining_navigator<const class_type>;

    // BEGIN member variables
    keyvalue_group_type* m_groups = nullptr;


    uint_fast8_t m_buckets = 0; //! log_2 of the number of buckets
    size_t m_elements = 0; //! number of stored elements
    uint_fast8_t m_key_width;
    uint_fast8_t m_value_width;
    hash_mapping_type m_hash; //! hash function
    mutable overflow_type m_overflow; //TODO: cht_overflow has non-const operations

    ON_DEBUG(key_type** m_plainkeys = nullptr;) //!bucket for keys in plain format for debugging purposes
    ON_DEBUG(value_type** m_plainvalues = nullptr;) //!bucket for values in plain format for debugging purposes
    ON_DEBUG(bucketsize_type* m_bucketsizes = nullptr); //! size of each bucket for debugging purposes
    // END member variables


    void swap(group_chaining_table& other) {
        // DDCHECK_EQ(m_width, other.m_width);
        ON_DEBUG(std::swap(m_plainkeys, other.m_plainkeys);)
        ON_DEBUG(std::swap(m_plainvalues, other.m_plainvalues);)
        ON_DEBUG(std::swap(m_bucketsizes, other.m_bucketsizes);)

        std::swap(m_key_width, other.m_key_width);
        std::swap(m_value_width, other.m_value_width);
        std::swap(m_groups, other.m_groups);
        std::swap(m_buckets, other.m_buckets);
        std::swap(m_hash, other.m_hash);
        std::swap(m_elements, other.m_elements);
        std::swap(m_overflow, other.m_overflow);
    }




    //!@see std::vector
    size_t capacity() const {
        const size_t cgroup_count = group_count();
        size_t size = 0;
        for(size_t group = 0; group < cgroup_count;  ++group) {
            size += m_groups[group].size();
        }
        return size;
    }


    private:
    void clear(const size_t group) { //! empties i-th group
        DCHECK(false);
        m_groups[group].clear();
#ifndef NDEBUG
        const size_t bucket_offset = group * max_groupsize();
        for(size_t bucket = 0; bucket < max_groupsize(); ++bucket) {
            if(m_bucketsizes[bucket_offset + bucket] > 0) {
                ON_DEBUG(free(m_plainkeys[bucket_offset + bucket]));
                ON_DEBUG(free(m_plainvalues[bucket_offset + bucket]));
            } else {
                DCHECK(m_plainkeys == nullptr);
                DCHECK(m_plainvalues == nullptr);
            }
        }
#endif
    }
    public:
    
    /**
     * Do NOT call this function unless you know what you are doing!
     * Assumes that all key and value buckets are no longer allocated and 
     * that it is left to do the final clean-up.
     */
    void clear_structure() { 
        delete [] m_groups;
        ON_DEBUG(free(m_bucketsizes));
        ON_DEBUG(free(m_plainkeys));
        ON_DEBUG(free(m_plainvalues));
        m_buckets = 0;
        m_elements = 0;
    }

    /**
     * Cleans up the hash table. Sets the hash table in its initial state.
     */
    void clear() { //! empties hash table
        const size_t cgroup_count = group_count();
        for(size_t group = 0; group < cgroup_count; ++group) {
            if(m_groups[group].empty()) continue;
            clear(group);
        }
        clear_structure();
    }

    public:

    size_t bucketgroup(const size_t bucket) const {
        return bucket / max_groupsize();
    }
    size_t rank_in_group(const size_t bucket) const {
        return bucket % max_groupsize();
    }
    size_t max_groupsize() const { 
        return m_key_width; 
    }

    //TODO: make private
    storage_type quotient_at(const size_t bucket, const size_t position, uint_fast8_t key_bitwidth) const {
        return m_groups[bucketgroup(bucket)].read_key(rank_in_group(bucket), position, key_bitwidth);
    }
    // const value_type value_at(const size_t bucket, const size_t position, uint_fast8_t value_bitwidth) const {
    //     return m_groups[bucketgroup(bucket)].read_value(rank_in_group(bucket), position, value_bitwidth);
    // }
    value_type value_at(const size_t bucket, const size_t position) const {
        return m_groups[bucketgroup(bucket)].read_value(rank_in_group(bucket), position, value_width());
    }

    //! returns the maximum value of a key that can be stored
    key_type max_key() const { return (-1ULL) >> (64-m_key_width); }

    //! returns the bit width of the keys
    uint_fast8_t key_width() const { return m_key_width; }

    //! returns the bit width of the values
    uint_fast8_t value_width() const { return m_value_width; }

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
#ifdef SEPARATE_MAX_BUCKET_SIZE
        return std::min<size_t>(SEPARATE_MAX_BUCKET_SIZE, std::numeric_limits<bucketsize_type>::max());
#else
        return std::numeric_limits<bucketsize_type>::max(); //, (separate_chaining::MAX_BUCKET_BYTESIZE*8)/m_width);
#endif
    }

    //! @see std::unordered_map
    size_type bucket_count() const {
        if(m_buckets == 0) return 0;
        return 1ULL<<m_buckets;
    }

    size_type group_count() const {
        return bucketgroup(bucket_count());
    }


    uint_fast8_t bucket_count_log2() const {
        return m_buckets;
    }

    //! @see std::unordered_map
    // size_t bucket_size(size_type n) const {
    //     return m_groups[bucken].bucketsize();
    // }

    size_t group_size(size_type n) const {
        return m_groups[n].size();
    }

    group_chaining_table(size_t keywidth = sizeof(key_type)*8, size_t valuewidth = sizeof(value_type)*8) 
        : m_key_width(keywidth)
        , m_value_width(valuewidth)
        , m_hash(m_key_width) 
        , m_overflow(m_key_width)
    {
        DDCHECK_GT(keywidth, 1);
        DDCHECK_LE(keywidth, 64);
        DDCHECK_GT(valuewidth, 1);
        DDCHECK_LE(valuewidth, 64);
    }

    // separate_chaining_table(separate_chaining_table&& other)
    //    : m_width(other.m_width)
    //    , m_keys(std::move(other.m_keys))
    //    , m_value_manager(std::move(other.m_value_manager))
    //    , m_bucketsizes(std::move(other.m_bucketsizes))
    //    , m_buckets(std::move(other.m_buckets))
    //    , m_elements(std::move(other.m_elements))
    //    , m_hash(std::move(other.m_hash))
    //    , m_resize_strategy(std::move(other.m_resize_strategy))
    //    , m_overflow(std::move(other.m_overflow))
    // {
    //
    //     ON_DEBUG(m_plainkeys = std::move(other.m_plainkeys); other.m_plainkeys = nullptr;)
    //     other.m_bucketsizes = nullptr; //! a hash map without buckets is already deleted
    // }
    //
    // separate_chaining_table& operator=(separate_chaining_table&& other) {
    //     // DDCHECK_EQ(m_width, other.m_width);
    //     clear();
    //     m_width       = std::move(other.m_width);
    //     m_keys        = std::move(other.m_keys);
    //     m_value_manager      = std::move(other.m_value_manager);
    //     m_buckets     = std::move(other.m_buckets);
    //     m_bucketsizes = std::move(other.m_bucketsizes);
    //     m_hash        = std::move(other.m_hash);
    //     m_elements    = std::move(other.m_elements);
    //     m_resize_strategy = std::move(other.m_resize_strategy);
    //     m_overflow       = std::move(other.m_overflow);
    //     ON_DEBUG(m_plainkeys = std::move(other.m_plainkeys); other.m_plainkeys = nullptr;)
    //     other.m_bucketsizes = nullptr; //! a hash map without buckets is already deleted
    //     return *this;
    // }
    



    //! Allocate `reserve` buckets. Do not confuse with reserving space for `reserve` elements.
    void reserve(size_t reserve) {
        uint_fast8_t reserve_bits = most_significant_bit(reserve);
        if(1ULL<<reserve_bits != reserve) ++reserve_bits;
        const size_t new_size = 1ULL<<reserve_bits;

        if(m_buckets == 0) {
#ifndef NDEBUG
            m_plainkeys   = reinterpret_cast<key_type**>  (malloc(new_size*sizeof(key_type*)));
            m_plainvalues   = reinterpret_cast<value_type**>  (malloc(new_size*sizeof(value_type*)));
            std::fill(m_plainkeys, m_plainkeys+new_size, nullptr);
            std::fill(m_plainvalues, m_plainvalues+new_size, nullptr);
            m_bucketsizes  = reinterpret_cast<bucketsize_type*>  (malloc(new_size*sizeof(bucketsize_type)));
            std::fill(m_bucketsizes, m_bucketsizes+new_size, 0);
#endif//NDEBUG

            m_groups = new keyvalue_group_type[bucketgroup(new_size)+1];
            m_buckets = reserve_bits;
        } else {
            group_chaining_table tmp_map(m_key_width, m_value_width);
            tmp_map.reserve(new_size);
#if STATS_ENABLED
            tdc::StatPhase statphase(std::string("resizing to ") + std::to_string(reserve_bits));
            print_stats(statphase);
#endif
            const size_t cbucket_count = bucket_count();
            const uint_fast8_t quotient_width = m_hash.remainder_width(m_buckets);
            DDCHECK_GT(quotient_width, 0);
            DDCHECK_LE(quotient_width, key_width());

            for(size_t bucket = 0; bucket < cbucket_count; ++bucket) {
                const keyvalue_group_type& group = m_groups[bucketgroup(bucket)];
                if(group.empty()) continue;
                for(size_t i = 0; i < group.groupsize(); ++i) {
                    const auto [read_quotient, read_value] = group.read(rank_in_group(bucket), i, quotient_width, m_value_width);
                    const key_type read_key = m_hash.inv_map(read_quotient, bucket, m_buckets);
                    DDCHECK_EQ(read_value, m_plainvalues[bucket][i]);
                    DDCHECK_EQ(read_key, m_plainkeys[bucket][i]);
                    tmp_map.find_or_insert(read_key, read_value);
                }
                clear(bucket);
            }


            {
                size_t i = m_overflow.first_position();
                while(m_overflow.valid_position(i)) {
                    tmp_map.find_or_insert(m_overflow.key(i), std::move(m_overflow[i]));
                    i = m_overflow.next_position(i);
                }
            }

            clear_structure();
            swap(tmp_map);
        }
    }
    // const navigator rbegin_nav() {
    //     const size_t cbucket_count = bucket_count();
    //     if(m_overflow.size() > 0) return { *this, cbucket_count, m_overflow.size() };
    //     if(cbucket_count == 0) return end_nav();
    //     for(size_t bucket = cbucket_count-1; bucket >= 0;  --bucket) {
    //         if(m_groupsm[bucket] > 0) {
    //             return { *this, bucket, static_cast<size_t>(m_bucketsizes[bucket]-1) };
    //         }
    //     }
    //     return end_nav();
    // }
    // const navigator rend_nav() { return end_nav(); }
    //
    const const_iterator cend() const {
        return { *this, -1ULL, -1ULL };
    }
    const iterator end() {
        return { *this, -1ULL, -1ULL };
    }
    const iterator begin() {
        const size_t cbucket_count = bucket_count();
        for(size_t bucket = 0; bucket < cbucket_count;  ++bucket) {
            const keyvalue_group_type& group = m_groups[bucketgroup(bucket)];
            if(group.bucketsize(bucket - bucketgroup(bucket)) > 0) {
                return { *this, bucket, 0 };
            }
        }
        if(m_overflow.size() > 0) return { *this, cbucket_count, 0 };
        return end();
    }
    // const const_iterator cbegin() const {
    //     const size_t cbucket_count = bucket_count();
    //     for(size_t bucket = 0; bucket < cbucket_count;  ++bucket) {
    //         if(m_bucketsizes[bucket] > 0) {
    //             return { *this, bucket, 0 };
    //         }
    //     }
    //     if(m_overflow.size() > 0) return { *this, cbucket_count, 0 };
    //     return cend();
    // }
    // const navigator begin_nav() {
    //     const size_t cbucket_count = bucket_count();
    //     for(size_t bucket = 0; bucket < cbucket_count;  ++bucket) {
    //         if(m_bucketsizes[bucket] > 0) {
    //             return { *this, bucket, 0 };
    //         }
    //     }
    //     if(m_overflow.size() > 0) return { *this, cbucket_count, 0 };
    //     return end_nav();
    // }
    // const navigator end_nav() {
    //     return { *this, -1ULL, -1ULL };
    // }
    // const const_navigator cbegin_nav() const {
    //     const size_t cbucket_count = bucket_count();
    //     for(size_t bucket = 0; bucket < cbucket_count;  ++bucket) {
    //         if(m_bucketsizes[bucket] > 0) {
    //             return { *this, bucket, 0 };
    //         }
    //     }
    //     if(m_overflow.size() > 0) return { *this, cbucket_count, 0 };
    //     return cend_nav();
    // }
    const const_navigator cend_nav() const {
        return { *this, -1ULL, -1ULL };
    }

    const_iterator find(const key_type& key) const {
        if(m_buckets == 0) return cend();
        if(m_overflow.size() > 0) {
            const size_t position = m_overflow.find(key);
            if(position != static_cast<size_t>(-1ULL)) {
                return const_iterator { *this, bucket_count(), position };
            }
        }
        const auto [quotient, bucket] = m_hash.map(key, m_buckets);
        DDCHECK_EQ(m_hash.inv_map(quotient, bucket, m_buckets), key);
        const size_t position = locate(bucket, quotient);
        if(position == static_cast<size_t>(-1ULL)) {
            return cend();
        }
        return const_iterator { *this, bucket, position };
    }

    private:
    size_t locate(const size_t& bucket, const storage_type& quotient) const {
        const uint_fast8_t key_bitwidth = m_hash.remainder_width(m_buckets);
        DDCHECK_GT(key_bitwidth, 0);
        DDCHECK_LE(key_bitwidth, key_width());
        DDCHECK_LE(most_significant_bit(quotient), key_bitwidth);

        const keyvalue_group_type& group = m_groups[bucketgroup(bucket)];


#ifndef NDEBUG
        const bucketsize_type& bucket_size = group.bucketsize(rank_in_group(bucket));
        key_type*& bucket_plainkeys = m_plainkeys[bucket];
        size_t plain_position = static_cast<size_t>(-1ULL);
        for(size_t i = 0; i < bucket_size; ++i) { 
            const key_type read_quotient = group.read_key(rank_in_group(bucket),i, key_bitwidth);
            const key_type read_key = m_hash.inv_map(read_quotient, bucket, m_buckets);
            DCHECK_EQ(read_key, bucket_plainkeys[i]);
            if(read_quotient == quotient) {
                plain_position = i;
                break;
            }
        }
#endif//NDEBUG
        
        const size_t position = group.empty() ? (-1ULL) : group.find(rank_in_group(bucket), quotient, key_bitwidth);

#ifndef NDEBUG
        DDCHECK_EQ(position, plain_position);
        if(position != static_cast<size_t>(-1ULL)) {
            DDCHECK_LT(position, bucket_size);
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
        if(m_overflow.size() > 0) {
            const size_t position = m_overflow.find(key);
            if(position != (-1ULL)) {
                return { bucket_count(), position };
            }
        }

        const auto [quotient, bucket] = m_hash.map(key, m_buckets);
        DDCHECK_EQ(m_hash.inv_map(quotient, bucket, m_buckets), key);

        return { bucket, locate(bucket, quotient) };
    }
    static constexpr size_t INITIAL_BUCKETS = 8;

    navigator find_or_insert(const key_type& key, const value_type& value) {
        DDCHECK_GT(key_width(), 1);

        if(m_buckets == 0) reserve(std::min<size_t>(key_width()-1, INITIAL_BUCKETS));
        const auto [quotient, bucket] = m_hash.map(key, m_buckets);
        DDCHECK_EQ(m_hash.inv_map(quotient, bucket, m_buckets), key);

        keyvalue_group_type& group = m_groups[bucketgroup(bucket)];
        const bucketsize_type bucket_size = group.bucketsize(rank_in_group(bucket));
        const size_t position = locate(bucket, quotient);

        // value_bucket_type& bucket_values = m_value_manager[bucket];
        if(position != static_cast<size_t>(-1ULL)) {
            DDCHECK_LT(position, bucket_size);
            return { *this, bucket, position };
        }
        if(m_overflow.need_consult(bucket)) {
            const size_t overflow_position = m_overflow.find(key);
            if(overflow_position != static_cast<size_t>(-1ULL)) {
                return { *this, bucket_count(), overflow_position };
            }
        }


        if(bucket_size == max_bucket_size()) {
            if(m_overflow.size() < m_overflow.capacity()) {
                const size_t overflow_position = m_overflow.insert(bucket, key, value);
                if(overflow_position != static_cast<size_t>(-1ULL)) { // could successfully insert element into overflow table
                    ++m_elements;
                    DDCHECK_EQ(m_overflow.find(key), overflow_position);
                    DDCHECK_EQ(m_overflow[m_overflow.find(key)], value);
                    return { *this, bucket_count(), overflow_position };
                }
            }
            // if(m_elements*separate_chaining::FAIL_PERCENTAGE < max_size()) {
            //     throw std::runtime_error("The chosen hash function is bad!");
            // }
            reserve(1ULL<<(m_buckets+1));
            return find_or_insert(key, value);
        }
        ++m_elements;

        // key_bucket_type& bucket_keys = m_keys[bucket];
        ON_DEBUG(key_type*& bucket_plainkeys = m_plainkeys[bucket];)
        const uint_fast8_t quotient_width = m_hash.remainder_width(m_buckets);
        DDCHECK_GT(quotient_width, 0);
        DDCHECK_LE(quotient_width, key_width());

        if(!group.initialized()) { group.initialize(max_groupsize(), quotient_width, value_width());}

#ifndef NDEBUG
        value_type*& bucket_plainvalues = m_plainvalues[bucket];
        DCHECK_EQ(m_bucketsizes[bucket], bucket_size);
        if(m_bucketsizes[bucket] == 0) {
            DCHECK(bucket_plainkeys == nullptr);
            DCHECK(bucket_plainvalues == nullptr);
            bucket_plainkeys   = reinterpret_cast<key_type*>  (malloc(sizeof(key_type)));
            bucket_plainvalues = reinterpret_cast<key_type*>  (malloc(sizeof(value_type)));
        } else {
            bucket_plainkeys   = reinterpret_cast<key_type*>  (realloc(bucket_plainkeys, sizeof(key_type)*(bucket_size+1)));
            bucket_plainvalues   = reinterpret_cast<key_type*>  (realloc(bucket_plainvalues, sizeof(value_type)*(bucket_size+1)));
        }
        ++m_bucketsizes[bucket];
#endif//NDEBUG
        DDCHECK_LE(key, max_key());
        ON_DEBUG(bucket_plainkeys[bucket_size] = key;)
        ON_DEBUG(bucket_plainvalues[bucket_size] = value;)
        
        DDCHECK_LT((static_cast<size_t>(bucket_size)*quotient_width)/64 + ((bucket_size)* quotient_width) % 64, 64*ceil_div<size_t>((bucket_size+1)*quotient_width, 64) );

        DDCHECK_LE(quotient_width, sizeof(key_type)*8);

        group.push_back(rank_in_group(bucket), quotient, quotient_width, value, value_width());
        DCHECK_EQ(m_bucketsizes[bucket], group.bucketsize(rank_in_group(bucket)));

        DDCHECK_EQ(m_hash.inv_map(group.read_key(rank_in_group(bucket), bucket_size, quotient_width), bucket, m_buckets), key);
        DDCHECK_EQ(bucket_plainvalues[bucket_size], group.read_value(rank_in_group(bucket), bucket_size, value_width()));

        return { *this, bucket, static_cast<size_t>(bucket_size) };
    }


    value_type operator[](const key_type& key) {
        return find_or_insert(key, value_type()).value();
    }

    ~group_chaining_table() { clear(); }





};





}//ns


