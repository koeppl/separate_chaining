#include <tudocomp/util/sdsl_bits.hpp>
#include "dcheck.hpp"
#include "bucket.hpp"
#include "select.hpp"

namespace separate_chaining {


template<class internal_t>
class group_bucket {
    using storage_type = internal_t;
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

    void initiate([[maybe_unused]] const uint_fast8_t width) {
       DDCHECK(m_data == nullptr);
       m_data = reinterpret_cast<storage_type*>(malloc(sizeof(storage_type)));
       ON_DEBUG(m_length = 0;)
    }

    void insert(const size_t index, const storage_type& key, [[maybe_unused]] const uint_fast8_t keywidth, size_t length) { //! length = old length
      DDCHECK(m_data != nullptr);
      DCHECK_EQ(length, m_length);
      ON_DEBUG(m_length = length+1;)
      m_data = reinterpret_cast<storage_type*>  (realloc(m_data, sizeof(storage_type)*(length+1)));
      for(size_t i = index; i < length; ++i) {
        m_data[i+1] = m_data[i];
      }
        m_data[index] = key;
    }
    storage_type read(size_t i, [[maybe_unused]]  size_t width) const {
        DDCHECK_LT(i, m_length);
        return m_data[i];
    }
    size_t find(const size_t position_from, const storage_type& key, const size_t position_to, [[maybe_unused]] const size_t width = 0) const {
      DCHECK_LT(position_to, m_length);
      for(size_t i = position_from; i < position_to; ++i) {
        if(m_data[i] == key) return i;
      }
      return -1ULL;
    }

    group_bucket() = default;
    ~group_bucket() { clear(); }

    group_bucket(group_bucket&& other) 
        : m_data(std::move(other.m_data))
    {
        other.m_data = nullptr;
        ON_DEBUG(m_length = other.m_length; other.m_data = 0;)
    }
    group_bucket(storage_type*&& keys) 
        : m_data(std::move(keys))
    {
        keys = nullptr;
    }

    group_bucket& operator=(group_bucket&& other) {
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
template<class internal_t = uint8_t>
class bucket_group {
    public:
    using internal_type = internal_t;
    using storage_type = uint64_t;
    static constexpr uint_fast8_t internal_bitwidth = sizeof(internal_type)*8;
    ON_DEBUG(size_t m_border_length;)
    size_t m_size; // number of elements in this group

    private:
    group_bucket<internal_t> m_keys; //! bucket for keys
    group_bucket<internal_t> m_values; //! bucket for values
    internal_type* m_border = nullptr; //! bit vector marking the borders, stores a 1 for each m_groupsize and a zero for each m_size
    using group_type = uint16_t;
    group_type m_groupsize = 0; // m_groupsize + m_size  = bit vector length of m_border


    public:
    bucket_group() = default;

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

    bool initialized() const { return m_border != nullptr; } //!check whether we can add elements to the bucket
    void clear() {
        if(m_border != nullptr) { free(m_border); }
        m_border = nullptr;
        m_keys.clear();
        m_values.clear();
    }


    void initiate(const size_t groupsize, const uint_fast8_t keywidth, const uint_fast8_t valuewidth) {
       DDCHECK(m_border == nullptr);
       m_keys.initiate(keywidth);
       m_values.initiate(valuewidth);
       m_size = 0;

       DCHECK_LT(groupsize, std::numeric_limits<decltype(m_groupsize)>::max());
       m_groupsize = groupsize;
       m_border = reinterpret_cast<internal_type*>  (malloc(sizeof(internal_type)* ceil_div<size_t>(groupsize+1, internal_bitwidth) ));
       memset(m_border, static_cast<char>(-1ULL), ceil_div<size_t>(groupsize+1, 8)); // groupsize+1 as we use the last '1' as a dummy border
       internal_type& last_border = m_border[ceil_div<size_t>(groupsize+1, internal_bitwidth)-1];
       last_border = static_cast<internal_type>(-1ULL)>>(internal_bitwidth - ((groupsize+1) % internal_bitwidth));

      ON_DEBUG(m_border_length = ceil_div<size_t>(groupsize, internal_bitwidth);)
      ON_DEBUG({
              size_t sum = 0;
              for(size_t i = 0; i < m_border_length;++i) {
                sum += __builtin_popcountll(m_border[i]);
              }
              DCHECK_EQ(sum, m_groupsize+1);
              });
    }

    // void resize(const size_t oldsize, const size_t length, const size_t keywidth, const size_t valuewidth) {
    //   m_keys.resize(:
    //    if(ceil_div<size_t>((oldsize)*width, internal_bitwidth) < ceil_div<size_t>((length)*width, internal_bitwidth)) {
    //       m_data = reinterpret_cast<internal_type*>  (realloc(m_data, sizeof(internal_type) * ceil_div<size_t>(length*width, internal_bitwidth ) ));
    //    }
    //    ON_DEBUG(m_length = ceil_div<size_t>(length*width, internal_bitwidth);)
    // }
    //
    size_t find_group_position(group_type groupindex) {
      size_t sum = 0;
      const uint64_t* border_chunks = reinterpret_cast<uint64_t*>(m_border);
      for(size_t border_index = 0; ; ++border_index) {
        DDCHECK_LT(border_index, ceil_div<size_t>(m_border_length*sizeof(internal_type),sizeof(uint64_t)));
        const size_t popcount = __builtin_popcountll(border_chunks[border_index]);
        if(popcount <= groupindex) { groupindex -= popcount; sum += 64; continue; }
        return sum + bits::select64(border_chunks[border_index], groupindex+1);
      }
    }

    void push_back(const group_type groupindex, const storage_type key, const uint_fast8_t keywidth, const storage_type value, const uint_fast8_t valuewidth) {
      const size_t group_ending = find_group_position(groupindex+1)-1;
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
      ON_DEBUG({
              size_t sum = 0;
              for(size_t i = 0; i < m_border_length;++i) {
                sum += __builtin_popcountll(m_border[i]);
              }
              DCHECK_EQ(sum, m_groupsize+1);
              });

    }

    std::pair<storage_type,storage_type> read(const group_type groupindex, size_t position, size_t keywidth, size_t valuewidth) const {
      DCHECK_LT(groupindex, m_groupsize);
      const size_t group_begin = find_group_position(groupindex);
      ON_DEBUG(
          const size_t next_group_begin = find_group_position(groupindex+1);
          DCHECK_LT(position, next_group_begin-group_begin);
          );
      return { m_keys.read(group_begin+position-groupindex, keywidth), m_values.read(group_begin+position-groupindex, valuewidth) };
    }

    size_t find(const group_type groupindex, const storage_type& key, const uint_fast8_t keywidth) const {
      DCHECK_LT(groupindex, m_groupsize);
      const size_t group_begin = find_group_position(groupindex);
      const size_t group_next_begin = find_group_position(groupindex+1);
      return m_keys.find(group_begin-groupindex, key, group_next_begin-groupindex-1, keywidth);
    }


    ~bucket_group() { clear(); }

    bucket_group(bucket_group&& other) 
        : m_keys(std::move(other.m_keys))
        , m_values(std::move(other.m_values))
        , m_size(std::move(other.m_size))
        , m_border(std::move(other.m_border))
    {
        other.m_border = nullptr;
        ON_DEBUG(m_border_length = std::move(other.m_border_length);)
    }

    bucket_group& operator=(bucket_group&& other) {
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

}//ns


