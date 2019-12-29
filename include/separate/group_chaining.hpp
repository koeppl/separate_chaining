#include <tudocomp/util/sdsl_bits.hpp>
#include "dcheck.hpp"
#include "bucket.hpp"
#include "select.hpp"
#include "hash.hpp"
#include "overflow.hpp"
#include "iterator.hpp"

namespace separate_chaining {


namespace group {


/**
 * the internal representation of a key or value array in a `keyvalue_group`
 * It is unaware of its size, since a `keyvalue_group` can store for keys and values a separate `core_group` with the respective key and value bit widths.
**/
template<class internal_t>
class core_group {
    using storage_type = size_t; //TODO: separate storage_type and key_type
    using internal_type = internal_t;
    static constexpr uint_fast8_t storage_bitwidth = sizeof(internal_type)*8;

    private:
    ON_DEBUG(size_t m_key_width;)
    ON_DEBUG(size_t m_length;)
    ON_DEBUG(storage_type* m_plain_data = nullptr;) //!bucket for keys, debug purpose only
    internal_type* m_data = nullptr; //!bucket for keys


    public:
    bool initialized() const { return m_data != nullptr; } //!check whether we can add elements to the bucket

    void clear() {
        if(m_data != nullptr) {
            free(m_data);
        }
        m_data = nullptr;
        ON_DEBUG(
        if(m_plain_data != nullptr) {
            free(m_plain_data);
        }
        m_plain_data = nullptr;
        )
        ON_DEBUG(m_length = 0;)
    }
    
    void initialize(const uint_fast8_t width) {
#ifndef NDEBUG
       DDCHECK(m_plain_data == nullptr);
       m_plain_data = reinterpret_cast<storage_type*>(malloc(sizeof(storage_type)));
       m_plain_data[0] = 0;
       m_key_width = width;
       m_length = 0;
       DDCHECK(m_data == nullptr);
#endif//NDEBUG

       m_data = reinterpret_cast<internal_type*>  (malloc(sizeof(internal_type)* ceil_div<size_t>(width, storage_bitwidth) ));
       memset(m_data, 0, sizeof(internal_type)* ceil_div<size_t>(width, storage_bitwidth));
    }

    void erase(const size_t index, const uint_fast8_t key_width, size_t length) { //! length = old length
      DDCHECK_EQ(m_key_width, key_width);
      DDCHECK(m_plain_data != nullptr);
      DDCHECK_EQ(length, m_length);


	  {//! moves elements in 64-bit blocks
		  const uint64_t* read_it = reinterpret_cast<uint64_t*>(m_data + (static_cast<size_t>(index+1)*key_width)/storage_bitwidth);
		  uint8_t read_offset = ((index+1)* key_width) % storage_bitwidth;
		  uint64_t* write_it = reinterpret_cast<uint64_t*>(m_data + (static_cast<size_t>(index)*key_width)/storage_bitwidth);
		  uint8_t write_offset = ((index)* key_width) % storage_bitwidth;

		  for(size_t i = 0; i < ( (length-index)*key_width) / 64; ++i) {
			  const size_t read_chunk = tdc::tdc_sdsl::bits_impl<>::read_int_and_move(read_it, read_offset, 64);
			  tdc::tdc_sdsl::bits_impl<>::write_int_and_move(write_it, read_chunk, write_offset, 64);
		  }

		  //! the final block could be smaller than 64-bits
		  const size_t remaining_bits = ((length-index)*key_width) - (( (length-index)*key_width) / 64)*64;
		  if(remaining_bits > 0) {
			  const size_t read_chunk = tdc::tdc_sdsl::bits_impl<>::read_int_and_move(read_it, read_offset, remaining_bits);
			  tdc::tdc_sdsl::bits_impl<>::write_int_and_move(write_it, read_chunk, write_offset, remaining_bits);
		  }
	  }


 
       // for(size_t i = index; i+1 < length; ++i) { //TODO: this can be speed up with word-packing!
       //     const size_t oldkey = read_(i+1, key_width);
       //     write_(i, oldkey, key_width);
       // }

#ifndef NDEBUG
      for(size_t i = index; i+1 < length; ++i) {
          m_plain_data[i] = m_plain_data[i+1];
      }
#endif //NDEBUG

      if(length-1 > 0) {
          ON_DEBUG(m_plain_data = reinterpret_cast<storage_type*>  (realloc(m_plain_data, sizeof(storage_type)*(length-1))));
          const size_t oldblocksize = ceil_div<size_t>(key_width*length, storage_bitwidth);
          const size_t newblocksize = ceil_div<size_t>(key_width*(length-1), storage_bitwidth);
          if(oldblocksize < newblocksize) {
              m_data = reinterpret_cast<internal_type*>  (realloc(m_data, sizeof(internal_type)* newblocksize));
          }
      }

      ON_DEBUG(m_length = length-1;)

#ifndef NDEBUG
       for(size_t i = 0; i < m_length; ++i) {
           DDCHECK_EQ(m_plain_data[i], read(i, key_width));
       }
#endif //NDEBUG
    }


    void insert(const size_t index, const storage_type& key, const uint_fast8_t key_width, size_t length) { //! length = old length
      DDCHECK_EQ(m_key_width, key_width);
      DDCHECK(m_plain_data != nullptr);
      DDCHECK_EQ(length, m_length);
#ifndef NDEBUG
      m_length = length+1;
      m_plain_data = reinterpret_cast<storage_type*>  (realloc(m_plain_data, sizeof(storage_type)*(length+1)));
      for(size_t i = length; i > index; --i) {
        m_plain_data[i] = m_plain_data[i-1];
      }
      m_plain_data[index] = key;
#endif //NDEBUG
       const size_t oldblocksize = ceil_div<size_t>(key_width*length, storage_bitwidth);
       const size_t newblocksize = ceil_div<size_t>(key_width*(length+1), storage_bitwidth);
       if(oldblocksize < newblocksize) {
           m_data = reinterpret_cast<internal_type*>  (realloc(m_data, sizeof(internal_type)* newblocksize));
           m_data[newblocksize-1] = 0;
       }

	   // //DEBUG!! 
	   // uint64_t* dummy_data = new uint64_t[newblocksize];
	   // memcpy(dummy_data, m_data, newblocksize*8);

#if(0) 
       size_t newkey = key;
       for(size_t i = index; i < length+1; ++i) { //! copy element by element to the right: slow!
           const size_t oldkey = read_(i, key_width);
           write_(i, newkey, key_width);
           newkey = oldkey;
       }
#endif//0

	   {//!insertion into m_data by word_packing
		   uint64_t read_pos = static_cast<size_t>(length)*key_width;
		   if(((length-index)*key_width)/64 > 0) { //! first copy in 64-bit blocks to the right
			   const uint64_t read_position = read_pos-64;
			   DDCHECK_LE(read_position+64, m_length*key_width);
			   const uint64_t* read_it = reinterpret_cast<uint64_t*>(m_data + read_position/storage_bitwidth);
			   uint8_t read_offset = read_position % storage_bitwidth;

			   const uint64_t write_position = read_pos+key_width-64;
			   DDCHECK_LE(write_position+64, m_length*key_width);
			   uint64_t* write_it = reinterpret_cast<uint64_t*>(m_data + write_position / storage_bitwidth);
			   uint8_t write_offset = write_position % storage_bitwidth;

			   //! start with the largest block, and move its elements one to the right
			   for(size_t i = 0; i < ( (length-index)*key_width) / 64; ++i) {
				   const size_t read_chunk = tdc::tdc_sdsl::bits_impl<>::read_int(read_it, read_offset, 64);
				   tdc::tdc_sdsl::bits_impl<>::write_int(write_it, read_chunk, write_offset, 64);
				   tdc::tdc_sdsl::bits_impl<>::move_left(read_it, read_offset, 64);
				   tdc::tdc_sdsl::bits_impl<>::move_left((const uint64_t*&)write_it, write_offset, 64);
				   DDCHECK_GE(read_pos, 64);
				   read_pos -= 64;
			   }
		   }

		   //! the place where entry 'index' is located
		   uint64_t*const read_it = reinterpret_cast<uint64_t*>(m_data + (static_cast<size_t>(index)*key_width)/storage_bitwidth);
		   const uint8_t read_offset = ((index)* key_width) % storage_bitwidth;

		   if(length > 0 && read_pos > index*key_width) { //! have we already copied the entry of 'index'?

			   uint64_t*const write_it = reinterpret_cast<uint64_t*>(m_data + (static_cast<size_t>(index+1)*key_width)/storage_bitwidth);
			   const uint8_t write_offset = ((index+1)* key_width) % storage_bitwidth;

			   //! the number of bits needed to be copied
			   const size_t remaining_bits = ((length-index)*key_width) - (((length-index)*key_width) / 64)*64;
			   DCHECK_GE(remaining_bits, 0);

			   const size_t read_chunk = tdc::tdc_sdsl::bits_impl<>::read_int(read_it, read_offset, remaining_bits);
			   tdc::tdc_sdsl::bits_impl<>::write_int(write_it, read_chunk, write_offset, remaining_bits);
		   }
		   tdc::tdc_sdsl::bits_impl<>::write_int(read_it, key, read_offset, key_width);
	   }
	   // for(size_t i = 0; i < newblocksize; ++i) {
		//    DDCHECK_EQ(dummy_data[i], m_data[i]);
	   // }
	   // write_(index, key, key_width);
	   // }

			   // delete [] dummy_data;


       // const uint8_t offset = (index * key_width) % storage_bitwidth;
       // uint64_t* it = reinterpret_cast<uint64_t*>(m_data +  (index* key_width)/storage_bitwidth);
       // DDCHECK_LT(offset+key_width, 64);
       //
       // uint64_t& current_block = *it;
       // const uint_fast8_t current_offset = std::min(offset+key_width, 64);
       // uint64_t overflow = current_block >> (64-key_width);
       // current_block = (current_block & (-1ULL>>(64-current_offset-key_width))) |
       //      ((current_block << key_width) & (-1ULL<<(current_offset)));
       //      current_block |= key << offset;
       //  if(current_offset == 64) {
       //      const uint_fast8_t remaining_key_length = offset+key_width-64; // how many bits of the key we have to copy
       //      const uint64_t new_overflow = it[block] >> (64-key_width);
       //
       //      const uint64_t upper_bits = it[1] >> (64-remaining_bits_length);
       //      it[1] 
       //      const size_t upper_bits = it[1] & (-1ULL)>>(upper_bits_length);
       //      overflow |= upper_bits << upper_bits_length;
       //  }
       //
       // for(size_t block = 2; block < ceil_div<size_t>(length-index, 64); ++block) { //! for each 64-bit block
       //     const uint64_t new_overflow = it[block] >> (64-key_width);
       //     it[block] <<= key_width;
       //     it[block] |= overflow;
       //     overflow = new_overflow;
       // }
#ifndef NDEBUG
       for(size_t i = 0; i < length; ++i) {
           DDCHECK_EQ(m_plain_data[i], read(i, key_width));
       }
#endif //NDEBUG

    }

    private:
    storage_type read_(size_t i, uint_fast8_t key_width) const { // direct access without debug check
      DDCHECK_EQ(m_key_width, key_width);
        DDCHECK_LT(i, m_length);
        DDCHECK_LT((static_cast<size_t>(i)*key_width)/storage_bitwidth + ((i)* key_width) % storage_bitwidth, storage_bitwidth*ceil_div<size_t>(m_length*key_width, storage_bitwidth) );
        return tdc::tdc_sdsl::bits_impl<>::read_int(reinterpret_cast<uint64_t*>(m_data + (static_cast<size_t>(i)*key_width)/storage_bitwidth), ((i)* key_width) % storage_bitwidth, key_width);
    }

    public:
    storage_type read(size_t i, uint_fast8_t key_width) const {
        DDCHECK_EQ(m_key_width, key_width);
        const size_t ret = read_(i, key_width);
        DDCHECK_EQ(m_plain_data[i], ret);
        return ret;

    }

    private:
    void write_(size_t index, size_t key, uint_fast8_t key_width) const {
        DDCHECK_EQ(m_key_width, key_width);
        DDCHECK_LE(most_significant_bit(key), key_width);
        DDCHECK_LT(index, m_length);
        DDCHECK_LT((static_cast<size_t>(index)*key_width)/storage_bitwidth + ((index)* key_width) % storage_bitwidth, storage_bitwidth*ceil_div<size_t>(m_length*key_width, storage_bitwidth) );

        tdc::tdc_sdsl::bits_impl<>::write_int(reinterpret_cast<uint64_t*>(m_data + (static_cast<size_t>(index)*key_width)/storage_bitwidth), key, ((index)* key_width) % storage_bitwidth, key_width);
    }
    public:

    void write(size_t index, size_t key, uint_fast8_t key_width) const {
        DDCHECK_EQ(m_key_width, key_width);
        DDCHECK_LE(most_significant_bit(key), key_width);
        DDCHECK_LT(index, m_length);
        DDCHECK_EQ(m_plain_data[index], read(index, key_width));
        ON_DEBUG(m_plain_data[index] = key);

        write_(index, key, key_width);

        DDCHECK_EQ(key, read(index, key_width));
#ifndef NDEBUG
        for(size_t i = 0; i < m_length; ++i) {
            read(i, key_width);
        }
#endif//NDEBUG
    }

    private:
#ifndef NDEBUG
    size_t find_debug(const size_t position_from, const storage_type& key, const size_t position_to, [[maybe_unused]] const uint_fast8_t key_width = 0) const {
       for(size_t i = position_from; i < position_to; ++i) {
           if(m_plain_data[i] == key) return i;
        }
       return -1ULL;
    }
#endif//NDEBUG

        public:

    /*
     * search in the interval [`position_from`, `position_to`) for `key`
     */
    size_t find(const size_t position_from, const storage_type& key, const size_t position_to, const uint_fast8_t key_width = 0) const {
        DDCHECK_EQ(m_key_width, key_width);

        DDCHECK_LT(position_from, position_to);
        DDCHECK_LE(position_to, m_length);
       uint8_t offset = (position_from * key_width) % storage_bitwidth;
       const uint64_t* it = reinterpret_cast<uint64_t*>(m_data +  (position_from * key_width)/storage_bitwidth);

       for(size_t i = 0; i < position_to-position_from; ++i) { // TODO
            const storage_type read_key = tdc::tdc_sdsl::bits_impl<>::read_int_and_move(it, offset, key_width);
            DDCHECK_EQ(read_key, m_plain_data[i+position_from]);
            if(read_key == key) {
                return position_from+i;
            }
       }
       DDCHECK_EQ(find_debug(position_from, key, position_to, key_width), -1ULL);
       return -1ULL;
    }

    core_group() = default;
    ~core_group() { clear(); }

    core_group(core_group&& other) 
        : m_data(std::move(other.m_data))
    {
        other.m_data = nullptr;
        ON_DEBUG(m_plain_data = std::move(other.m_plain_data));
        ON_DEBUG(other.m_plain_data = nullptr);
        ON_DEBUG(m_length = other.m_length; other.m_plain_data = 0;)
    }
    // core_group(storage_type*&& keys) 
    //     : m_plain_data(std::move(keys))
    // {
    //     keys = nullptr;
    // }

    core_group& operator=(core_group&& other) {
        clear();
        m_data = std::move(other.m_data);
        other.m_data = nullptr;
        ON_DEBUG(m_plain_data = std::move(other.m_plain_data));
        ON_DEBUG(other.m_plain_data = nullptr);
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
    using groupsize_type = uint32_t; //! type to address all elements in a group, limits also the bucket size
    using core_group_type = core_group<internal_t>;
    static constexpr uint_fast8_t internal_bitwidth = sizeof(internal_type)*8;
    ON_DEBUG(size_t m_border_length;)
    groupsize_type m_size = 0; // number of elements in this group

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

    // void deserialize(std::istream& is, const size_t size, const uint_fast8_t key_width, const uint_fast8_t valuewidth) {
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
        DDCHECK_GE(find_group_position(i), i + prev_position);
        const size_t ret = find_group_position(i) - i - prev_position;
        DDCHECK_EQ(ret, m_border_array[i] - (i == 0 ? 0 : m_border_array[i-1]));
        return ret;
    }


    bool initialized() const { return m_border != nullptr; } //!check whether we can add elements to the bucket
    void clear() {
        if(m_border != nullptr) { free(m_border); }
        m_border = nullptr;
        ON_DEBUG(m_border_array.clear();)
        m_keys.clear();
        m_values.clear();
        m_size = 0;
    }


    void initialize(const size_t groupsize, const uint_fast8_t key_width, const uint_fast8_t valuewidth) {
       DDCHECK(m_border == nullptr);
       m_keys.initialize(key_width);
       m_values.initialize(valuewidth);
       m_size = 0;

       DDCHECK_LT(groupsize, std::numeric_limits<decltype(m_groupsize)>::max());
       m_groupsize = groupsize;
       m_border = reinterpret_cast<internal_type*>  (malloc(sizeof(internal_type)* ceil_div<size_t>(groupsize+1, internal_bitwidth) ));
       memset(m_border, static_cast<char>(-1ULL), ceil_div<size_t>(groupsize+1, 8)); // groupsize+1 as we use the last '1' as a dummy border
       internal_type& last_border = m_border[ceil_div<size_t>(groupsize+1, internal_bitwidth)-1];
       last_border = static_cast<internal_type>(-1ULL)>>(internal_bitwidth - ((groupsize+1) % internal_bitwidth));

      ON_DEBUG(m_border_length = ceil_div<size_t>(groupsize+1, internal_bitwidth);)
      ON_DEBUG( 
              m_border_array.resize(groupsize, 0);
              {
              size_t sum = 0;
              for(size_t i = 0; i < m_border_length;++i) {
                sum += __builtin_popcountll(m_border[i]);
              }
              DDCHECK_EQ(sum, static_cast<size_t>(m_groupsize)+1);
              });
    }

    // void resize(const size_t oldsize, const size_t length, const size_t key_width, const size_t valuewidth) {
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

    void push_back(const group_index_type groupindex, const storage_type key, const uint_fast8_t key_width, const storage_type value, const uint_fast8_t valuewidth) {
      DDCHECK_LT(m_size, std::numeric_limits<groupsize_type>::max());

      const size_t group_ending = find_group_position(groupindex);
      m_keys.insert(group_ending-groupindex, key, key_width, m_size);
      m_values.insert(group_ending-groupindex, value, valuewidth, m_size);
      ++m_size;
      const size_t new_border_size = ceil_div<size_t>(m_size + 1 + m_groupsize, internal_bitwidth);
      if(new_border_size > ceil_div<size_t>(m_size + m_groupsize, internal_bitwidth)) {
          m_border = reinterpret_cast<internal_type*>(realloc(m_border, sizeof(internal_type) * new_border_size));
          m_border[new_border_size-1] = 0;
          ON_DEBUG(++m_border_length;)
      }
      DDCHECK_EQ(new_border_size, m_border_length);

      internal_type& current_chunk = m_border[group_ending/internal_bitwidth];

      bool highest_bit = current_chunk & 1ULL<<(internal_bitwidth-1); //! move the highest bit of the current chunk to the lowest bit of the next chunk
      if( (group_ending+1) % internal_bitwidth == 0) {
          current_chunk = current_chunk & ((-1ULL)>>(65-internal_bitwidth)); //! remove the most significant bit
      } else {
      current_chunk = ((current_chunk) &  ((1ULL<< (group_ending % internal_bitwidth))-1) )  //! keep the lower part of the chunk
          | ((current_chunk<<1) &  ((-1ULL<< ((group_ending % internal_bitwidth)+1))) ); //! move the upper part of the chunk by one position
      }

      for(size_t i = group_ending/internal_bitwidth+1; i < new_border_size; ++i) {
          bool new_highest_bit = m_border[i] & 1ULL<<(internal_bitwidth-1);
          m_border[i] = (m_border[i] << 1) | highest_bit;
          highest_bit = new_highest_bit;
      }

      ON_DEBUG(// check whether the group sizes are correct
      for(size_t i = groupindex; i < m_border_array.size(); ++i) {
          ++m_border_array[i];
      }
      for(size_t i = 0; i < m_border_array.size(); ++i) {
          DDCHECK_EQ(m_border_array[i], find_group_position(i)-i);
      }
      { // check whether all group '1's are still set
          size_t sum = 0;
          for(size_t i = 0; i < m_border_length;++i) {
              sum += __builtin_popcountll(m_border[i]);
          }
          DDCHECK_EQ(sum, static_cast<size_t>(m_groupsize)+1);
      }
      );//ON_DEBUG
    }

    void erase(size_t groupindex, size_t position, uint_fast8_t key_width, uint_fast8_t valuewidth) {
      const size_t prev_groupindex = groupindex == 0 ? 0 : groupindex-1; // number of ones we need to subtract to obtain the array_index from group_begin
      const size_t group_begin = groupindex == 0 ? 0 : find_group_position(prev_groupindex);
      const size_t array_index = group_begin - prev_groupindex + position;

      const size_t group_ending = find_group_position(groupindex);
      DDCHECK_LT(group_begin+position, group_ending);
      m_keys.erase(array_index, key_width, m_size);
      m_values.erase(array_index, valuewidth, m_size);

      // TODO: shrink size
      const size_t border_size = ceil_div<size_t>(m_size + 1 + m_groupsize, internal_bitwidth);
      DDCHECK_EQ(border_size, m_border_length);
    
      const size_t group_border_current = group_ending/internal_bitwidth;
      DDCHECK_LT(group_border_current, border_size);

      internal_type& current_chunk = m_border[group_border_current];
      if(group_ending % internal_bitwidth == 0) { // if the group ends with the first bit in the current block, we make this bit the highest bit of the previous block
          DDCHECK_EQ(m_border[group_border_current-1] & (1ULL<<(internal_bitwidth-1)), 0);
          DDCHECK_EQ(m_border[group_border_current] & 1ULL, 1);
          m_border[group_border_current-1] |= 1ULL<<(internal_bitwidth-1);
          current_chunk >>= 1;
      } else {

          // idea: move the least significant bit of the next block to the highest significant bit of the current block
          // if( (group_ending+1) % internal_bitwidth == 0) {
          //     current_chunk = current_chunk & ((-1ULL)>>(65-internal_bitwidth)); // remove the most significant bit
          // } else {

          DDCHECK_NE(current_chunk & (1ULL<< (group_ending % internal_bitwidth)), 0);
          DDCHECK_EQ(current_chunk & (1ULL<< ((group_ending-1) % internal_bitwidth)), 0);
          current_chunk = ((current_chunk) &  ((1ULL<< (group_ending % internal_bitwidth))-1) )
              | ((current_chunk &  (-1ULL<< (group_ending % internal_bitwidth)))>>1);
          // }
    }
      // copy the least significant bit of the next block 
      const size_t lowest_bit = group_border_current == (border_size-1) ? 0 : m_border[group_border_current+1] & 1ULL; 
      current_chunk |= (lowest_bit<<(internal_bitwidth-1));

      for(size_t i = group_ending/internal_bitwidth+1; i < border_size; ++i) {
          const size_t new_lowest_bit = i+1 == border_size ? 0 : (m_border[i+1] & 1ULL);
          m_border[i] = (m_border[i] >> 1) | (new_lowest_bit<<(internal_bitwidth-1));
      }

      --m_size;
      const size_t new_border_size = ceil_div<size_t>(m_size + 1 + m_groupsize, internal_bitwidth);
      if(new_border_size < border_size) {
          m_border = reinterpret_cast<internal_type*>(realloc(m_border, sizeof(internal_type) * new_border_size));
          ON_DEBUG(--m_border_length;)
      }
      DDCHECK_EQ(new_border_size, m_border_length);


      ON_DEBUG(// check whether the group sizes are correct

      for(size_t i = groupindex; i < m_border_array.size(); ++i) {
          --m_border_array[i];
      }
      for(size_t i = 0; i < m_border_array.size(); ++i) {
          DDCHECK_EQ(m_border_array[i], find_group_position(i)-i);
      }
      { // check whether all group '1's are still set
          size_t sum = 0;
          for(size_t i = 0; i < m_border_length;++i) {
              sum += __builtin_popcountll(m_border[i]);
          }
          DDCHECK_EQ(sum, static_cast<size_t>(m_groupsize)+1);
      }
      );//ON_DEBUG
    }

    storage_type read_key(size_t groupindex, size_t position, uint_fast8_t key_width) const {
      DDCHECK_LT(groupindex, m_groupsize);
      const size_t group_begin = groupindex == 0 ? 0 : find_group_position(groupindex-1)+1;
      return  m_keys.read(group_begin+position-groupindex, key_width);
    }
    storage_type read_value(size_t groupindex, size_t position, uint_fast8_t valuewidth) const {
      DDCHECK_LT(groupindex, m_groupsize);
      const size_t group_begin = groupindex == 0 ? 0 : find_group_position(groupindex-1)+1;
      return  m_values.read(group_begin+position-groupindex, valuewidth);
    }
    void write_value(size_t groupindex, size_t position, size_t value, uint_fast8_t valuewidth) const {
      DDCHECK_LT(groupindex, m_groupsize);
      const size_t group_begin = groupindex == 0 ? 0 : find_group_position(groupindex-1)+1;
      m_values.write(group_begin+position-groupindex, value, valuewidth);
    }

    std::pair<storage_type,storage_type> read(size_t groupindex, size_t position, uint_fast8_t key_width, uint_fast8_t valuewidth) const {
      DDCHECK_LT(groupindex, m_groupsize);
      const size_t group_begin = groupindex == 0 ? 0 : find_group_position(groupindex-1)+1;
      ON_DEBUG(
          const size_t next_group_begin = find_group_position(groupindex);
          DDCHECK_LT(position, next_group_begin-group_begin);
          );
      return { m_keys.read(group_begin+position-groupindex, key_width), m_values.read(group_begin+position-groupindex, valuewidth) };
    }

    /**
     * Gives the position of the entry with key `key` inside the group `groupindex`, or -1 if such an entry does not exist.
     */
    size_t find(const group_index_type groupindex, const storage_type& key, const uint_fast8_t key_width) const {
      DDCHECK_LT(groupindex, m_groupsize);
      const size_t group_begin = groupindex == 0 ? 0 : find_group_position(groupindex-1)+1;
      const size_t group_next_begin = find_group_position(groupindex);
      DDCHECK_LE(group_begin, group_next_begin);
      if(group_next_begin == group_begin) { return -1ULL; } //! group is empty
      const size_t ret = m_keys.find(group_begin-groupindex, key, group_next_begin-groupindex, key_width);
      return ret == (-1ULL) ? (-1ULL) : ret - (group_begin-groupindex); //! do not subtract position from invalid position -1ULL
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
    using groupsize_type = keyvalue_group_type::groupsize_type;

    using hash_mapping_type = hash_mapping_t;
    static_assert(std::is_same<typename hash_mapping_t::storage_type, storage_type>::value, "hash_mapping_t::storage_type must be uint64_t!");

    using class_type = group_chaining_table<hash_mapping_type, overflow_type>;
    using iterator = separate_chaining_iterator<class_type>;
    using const_iterator = separate_chaining_iterator<const class_type>;
    using navigator = separate_chaining_navigator<class_type>;
    using const_navigator = separate_chaining_navigator<const class_type>;

    // BEGIN member variables
    keyvalue_group_type* m_groups = nullptr; //! invariant: a keyvalue_group stores `key_width` many groups

    uint_fast8_t m_buckets = 0; //! log_2 of the number of buckets
    size_t m_elements = 0; //! number of stored elements
    uint_fast8_t m_key_width;
    uint_fast8_t m_value_width;
    hash_mapping_type m_hash; //! hash function
    mutable overflow_type m_overflow; //TODO: cht_overflow has non-const operations

    ON_DEBUG(key_type** m_plainkeys = nullptr;) //!bucket for keys in plain format for debugging purposes
    ON_DEBUG(value_type** m_plainvalues = nullptr;) //!bucket for values in plain format for debugging purposes
    ON_DEBUG(groupsize_type* m_bucketsizes = nullptr;) //! size of each bucket for debugging purposes
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
#ifndef NDEBUG
    void clear_bucket(const size_t bucket) { //! empties i-th bucket, only for debug purposes
        DDCHECK_LT(bucket, bucket_count());
        if(m_plainkeys[bucket] != nullptr) {
            free(m_plainkeys[bucket]);
            m_plainkeys[bucket] = nullptr;
        }
        if(m_plainvalues[bucket] != nullptr) {
            free(m_plainvalues[bucket]);
            m_plainvalues[bucket] = nullptr;
        }
        m_bucketsizes[bucket] = 0;
    }
#endif//NDEBUG

    void clear(const size_t group_i) { //! empties i-th group_i
#ifndef NDEBUG
        const size_t groupsize = m_groups[group_i].groupsize();
        const size_t bucket_offset = group_i * max_groupsize();
        for(size_t bucket_i = 0; bucket_i < groupsize; ++bucket_i) { //! local bucket ID
            const size_t bucket_number = bucket_offset + bucket_i; //! global bucket ID
            if(bucket_number >= bucket_count()) { break;} //! the number of buckets may not be divisible by the number of groups
            DDCHECK_EQ(bucketgroup(bucket_number), group_i);
            clear_bucket(bucket_number);
        }
#endif//NDEBUG
        m_groups[group_i].clear();
    }
    public:
    
    /**
     * Do NOT call this function unless you know what you are doing!
     * Assumes that all key and value buckets are no longer allocated and 
     * that it is left to do the final clean-up.
     */
    void clear_structure() { 
        if(m_groups != nullptr) {
            delete [] m_groups;
        }
        m_groups = nullptr;
#ifndef NDEBUG
        if(m_bucketsizes != nullptr) { free(m_bucketsizes); } m_bucketsizes = nullptr;
        if(m_plainkeys != nullptr) { free(m_plainkeys); } m_plainkeys = nullptr;
        if(m_plainvalues != nullptr) { free(m_plainvalues); } m_plainvalues = nullptr;
#endif
        m_buckets = 0;
        m_elements = 0;
        m_overflow.clear();
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

    //! the group to which the bucket with the global ID `bucket_number` belongs
    size_t bucketgroup(const size_t bucket_number) const {
        return bucket_number / max_groupsize();
    }

    //! the local ID in the respective group of the bucket with the global ID `bucket_number`
    size_t rank_in_group(const size_t bucket_number) const {
        return bucket_number % max_groupsize();
    }

    //! the number of buckets a group can contain
    size_t max_groupsize() const {  
        return std::max(2, most_significant_bit(max_bucket_size() * bucket_count())>>1 );  //TODO: tweaking parameter!
        // return 32;
        //return m_key_width; 
    }

    //TODO: make private
    storage_type quotient_at(const size_t bucket, const size_t position, uint_fast8_t key_bitwidth) const {
        DCHECK_LT(bucket, bucket_count());
        return m_groups[bucketgroup(bucket)].read_key(rank_in_group(bucket), position, key_bitwidth);
    }
    // const value_type value_at(const size_t bucket, const size_t position, uint_fast8_t value_bitwidth) const {
    //     return m_groups[bucketgroup(bucket)].read_value(rank_in_group(bucket), position, value_bitwidth);
    // }
    value_type value_at(const size_t bucket, const size_t position) const {
        if(bucket == bucket_count()) {
            return m_overflow[position];
        }
        DCHECK_LT(bucket, bucket_count());
        return m_groups[bucketgroup(bucket)].read_value(rank_in_group(bucket), position, value_width());
    }
    void write_value(const size_t bucket, const size_t position, const size_t value) const {
        if(bucket == bucket_count()) {
            m_overflow[position] = value;
        }
        else {
            DCHECK_LT(bucket, bucket_count());
            DCHECK_LE(value, (-1ULL)>>(64-value_width()));
            ON_DEBUG(m_plainvalues[bucket][position] = value);
            return m_groups[bucketgroup(bucket)].write_value(rank_in_group(bucket), position, value, value_width());
        }
    }

    //! returns the maximum value of a key that can be stored
    key_type max_key() const { return (-1ULL) >> (64-m_key_width); }
    //! returns the maximum value that can be stored
    value_type max_value() const { return (-1ULL) >> (64-m_value_width); }

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
        return std::min<size_t>(SEPARATE_MAX_BUCKET_SIZE, std::numeric_limits<groupsize_type>::max());
#else
        // return std::numeric_limits<uint8_t>::max(); //, (separate_chaining::MAX_BUCKET_BYTESIZE*8)/m_width);
        return 64; // TODO: tweaking parameter!
#endif
    }

    //! @see std::unordered_map
    size_type bucket_count() const {
        if(m_buckets == 0) return 0;
        return 1ULL<<m_buckets;
    }

    size_type group_count() const {
        return m_groups == nullptr ? 0 : bucketgroup(bucket_count())+1;
    }


    // uint_fast8_t bucket_count_log2() const {
    //     return m_buckets;
    // }

    //! @see std::unordered_map
    size_t bucket_size(size_type bucket) const {
        return m_groups[bucketgroup(bucket)].bucketsize(rank_in_group(bucket));
    }

    size_t group_size(size_type n) const {
        return m_groups[n].size();
    }

    group_chaining_table(size_t key_width = sizeof(key_type)*8, size_t value_width = sizeof(value_type)*8) 
        : m_key_width(key_width)
        , m_value_width(value_width)
        , m_hash(m_key_width) 
        , m_overflow(m_key_width, m_value_width)
    {
        DDCHECK_GE(m_value_width, 1);
        DDCHECK_LE(m_value_width, sizeof(value_type)*8);
        DDCHECK_GE(m_key_width, 1);
        DDCHECK_LE(m_key_width, sizeof(key_type)*8);
        DCHECK_LT(max_groupsize()*max_bucket_size(), std::numeric_limits<groupsize_type>::max());
    }


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
            m_bucketsizes  = reinterpret_cast<groupsize_type*>  (malloc(new_size*sizeof(groupsize_type)));
            std::fill(m_bucketsizes, m_bucketsizes+new_size, 0);
#endif//NDEBUG

            m_groups = new keyvalue_group_type[bucketgroup(new_size)+1];
            m_buckets = reserve_bits;
            m_overflow.resize_buckets(new_size, key_width(), value_width());
        } else {
            group_chaining_table tmp_map(m_key_width, m_value_width);
            tmp_map.reserve(new_size);
#if STATS_ENABLED
            tdc::StatPhase statphase(std::string("resizing to ") + std::to_string(reserve_bits));
            print_stats(statphase);
#endif
            const size_t cgroup_count = group_count();
            const uint_fast8_t quotient_width = m_hash.remainder_width(m_buckets);
            DDCHECK_GT(quotient_width, 0);
            DDCHECK_LE(quotient_width, key_width());


            {
                ON_DEBUG(size_t elements = 0;)
                for(size_t group_i = 0; group_i < cgroup_count; ++group_i) {
                    const keyvalue_group_type& group_it = m_groups[group_i];
                    if(group_it.empty()) continue;
                    const size_t groupsize = m_groups[group_i].groupsize();
                    for(size_t bucket_i = 0; bucket_i < groupsize; ++bucket_i) { //! the bucket index within the group
                        const size_t bucketsize =  group_it.bucketsize(bucket_i);
                        if(bucketsize == 0) { continue; }
                        const size_t bucket_number = bucket_i+group_i*max_groupsize(); //! the global bucket index
                        DDCHECK_EQ(rank_in_group(bucket_number), bucket_i);
                        DDCHECK_LT(bucket_number, bucket_count());
                        DDCHECK_EQ(bucketgroup(bucket_number), group_i);
                        DDCHECK_EQ(bucketsize, m_bucketsizes[bucket_number]);
                        for(size_t i = 0; i < bucketsize; ++i) {
                            const auto [read_quotient, read_value] = group_it.read(bucket_i, i, quotient_width, m_value_width);
                            const key_type read_key = m_hash.inv_map(read_quotient, bucket_number, m_buckets);
                            DDCHECK_EQ(read_value, m_plainvalues[bucket_number][i]);
                            DDCHECK_EQ(read_key, m_plainkeys[bucket_number][i]);
                            tmp_map.find_or_insert(read_key, read_value);
                            ON_DEBUG(++elements;)
                        }
                    }
                    clear(group_i);
                }
                ON_DEBUG(elements += m_overflow.size();)
                DDCHECK_EQ(elements, m_elements);
            }


            // for(size_t bucket = 0; bucket < cbucket_count; ++bucket) {
            //     const keyvalue_group_type& group = m_groups[bucketgroup(bucket)];
            //     if(group.empty()) continue;
            //     for(size_t i = 0; i < group.groupsize(); ++i) {
            //         const auto [read_quotient, read_value] = group.read(rank_in_group(bucket), i, quotient_width, m_value_width);
            //         const key_type read_key = m_hash.inv_map(read_quotient, bucket, m_buckets);
            //         DDCHECK_EQ(read_value, m_plainvalues[bucket][i]);
            //         DDCHECK_EQ(read_key, m_plainkeys[bucket][i]);
            //         tmp_map.find_or_insert(read_key, read_value);
            //     }
            //     // clear(bucket); //TODO: make call for clear a group
            // }


            {
                size_t i = m_overflow.first_position();
                while(m_overflow.valid_position(i)) {
                    tmp_map.find_or_insert(m_overflow.key(i), std::move(m_overflow[i]));
                    i = m_overflow.next_position(i);
                }
            }
            DDCHECK_EQ(m_elements, tmp_map.m_elements);
            clear_structure();
            swap(tmp_map);
        }
    }
    const navigator rbegin_nav() {
        const size_t cbucket_count = bucket_count();
        if(m_overflow.size() > 0) return { *this, cbucket_count, m_overflow.size() };
        if(cbucket_count == 0) return end_nav();
        for(size_t bucket = cbucket_count-1; bucket >= 0;  --bucket) {
            const keyvalue_group_type& group = m_groups[bucketgroup(bucket)];
            if(group.bucketsize(rank_in_group(bucket)) > 0) {
                return { *this, bucket, 0 };
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
            const keyvalue_group_type& group = m_groups[bucketgroup(bucket)];
            if(group.bucketsize(rank_in_group(bucket)) > 0) {
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
    const navigator end_nav() {
        return { *this, -1ULL, -1ULL };
    }
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
        const uint_fast8_t quotient_width = m_hash.remainder_width(m_buckets);
        DDCHECK_GT(quotient_width, 0);
        DDCHECK_LE(quotient_width, key_width());
        DDCHECK_LE(most_significant_bit(quotient), quotient_width);

        const keyvalue_group_type& group = m_groups[bucketgroup(bucket)];


#ifndef NDEBUG
        const groupsize_type& bucket_size = group.bucketsize(rank_in_group(bucket));
        key_type*& bucket_plainkeys = m_plainkeys[bucket];
        size_t plain_position = static_cast<size_t>(-1ULL);
        for(size_t i = 0; i < bucket_size; ++i) { 
            const key_type read_quotient = group.read_key(rank_in_group(bucket),i, quotient_width);
            const key_type read_key = m_hash.inv_map(read_quotient, bucket, m_buckets);
            DDCHECK_EQ(m_plainvalues[bucket][i], group.read_value(rank_in_group(bucket), i, value_width()));
            DDCHECK_EQ(read_key, bucket_plainkeys[i]);
            if(read_quotient == quotient) {
                plain_position = i;
                break;
            }
        }
#endif//NDEBUG
        
        const size_t position = group.empty() ? (-1ULL) : group.find(rank_in_group(bucket), quotient, quotient_width);

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
        if(m_overflow.size() > 0) { //TODO: query only if we have a full bucket!
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

    navigator find_or_insert(const key_type& key, value_type value) {
        DDCHECK_GT(key_width(), 1);

        if(m_buckets == 0) reserve(std::min<size_t>(key_width()-1, INITIAL_BUCKETS));
        const auto [quotient, bucket] = m_hash.map(key, m_buckets);
        DDCHECK_EQ(m_hash.inv_map(quotient, bucket, m_buckets), key);

        keyvalue_group_type& group = m_groups[bucketgroup(bucket)];
        const groupsize_type bucket_size = group.bucketsize(rank_in_group(bucket));
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
                const size_t overflow_position = m_overflow.insert(bucket, key, std::move(value));
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
        {
            size_t elements = 0;
            for(size_t group_i = 0; group_i < group_count(); ++group_i) {
                const keyvalue_group_type& group_it = m_groups[group_i];
                for(size_t bucket_i = 0; bucket_i < group_it.groupsize(); ++bucket_i) {
                    elements += group_it.bucketsize(bucket_i);
                }
            }
            elements += m_overflow.size();
            DDCHECK_EQ(elements+1, m_elements);
        }


        value_type*& bucket_plainvalues = m_plainvalues[bucket];
        DDCHECK_EQ(m_bucketsizes[bucket], bucket_size);
        if(m_bucketsizes[bucket] == 0) {
            DDCHECK(bucket_plainkeys == nullptr);
            DDCHECK(bucket_plainvalues == nullptr);
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
        DDCHECK_EQ(m_bucketsizes[bucket], group.bucketsize(rank_in_group(bucket)));

        DDCHECK_EQ(m_hash.inv_map(group.read_key(rank_in_group(bucket), bucket_size, quotient_width), bucket, m_buckets), key);
        DDCHECK_EQ(bucket_plainvalues[bucket_size], group.read_value(rank_in_group(bucket), bucket_size, value_width()));

        return { *this, bucket, static_cast<size_t>(bucket_size) };
    }

    constexpr void shrink_to_fit() { }


    navigator operator[](const key_type& key) {
        return find_or_insert(key, value_type());
    }

    ~group_chaining_table() { clear(); }

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

    size_type erase(const size_t bucket, const size_t position) {
        if(position == static_cast<size_t>(-1ULL)) return 0;
        if(m_overflow.size() > 0 && bucket == bucket_count()) {
            DDCHECK_LT(position, m_overflow.size());
            m_overflow.erase(position);
            --m_elements;
            return 1;
        }

        DDCHECK_LT(bucket, bucket_count());
        DDCHECK_LT(position, m_bucketsizes[bucket]);

        keyvalue_group_type& group = m_groups[bucketgroup(bucket)];
        
        const uint_fast8_t quotient_width = m_hash.remainder_width(m_buckets);
        DDCHECK_GT(quotient_width, 0);
        DDCHECK_LE(quotient_width, key_width());

#ifndef NDEBUG
        groupsize_type& bucket_size = m_bucketsizes[bucket];
        key_type*& bucket_plainkeys = m_plainkeys[bucket];
        value_type*& bucket_plainvalues = m_plainvalues[bucket];
        {

            const key_type read_quotient = group.read_key(rank_in_group(bucket),position, quotient_width);
            const key_type read_key = m_hash.inv_map(read_quotient, bucket, m_buckets);
            DDCHECK_EQ(read_key, bucket_plainkeys[position]);
            DDCHECK_EQ(bucket_plainvalues[position], group.read_value(rank_in_group(bucket), position, value_width()));
            for(size_t i = position+1; i < bucket_size; ++i) {
                bucket_plainkeys[i-1] = bucket_plainkeys[i];
                bucket_plainvalues[i-1] = bucket_plainvalues[i];
            }
            DDCHECK_GT(bucket_size, 0);
            --bucket_size;
        }
#endif//NDEBUG

        group.erase(rank_in_group(bucket), position, quotient_width, value_width());

#ifndef NDEBUG
        DDCHECK_EQ(bucket_size, group.bucketsize(rank_in_group(bucket)));
        for(size_t i = 0; i < bucket_size; ++i) { 
            const key_type read_quotient = group.read_key(rank_in_group(bucket),i, quotient_width);
            const key_type read_key = m_hash.inv_map(read_quotient, bucket, m_buckets);
            DDCHECK_EQ(read_key, bucket_plainkeys[i]);
            DDCHECK_EQ(bucket_plainvalues[i], group.read_value(rank_in_group(bucket), i, value_width()));
        }

#endif//NDEBUG

        --m_elements;
        if(group.empty()) { //clear the group if it becomes empty
            clear(bucketgroup(bucket));
        }
#ifndef NDEBUG
        if(m_bucketsizes[bucket] == 0) {
            clear_bucket(bucket);
        }
#endif
        return 1;
    }

#if STATS_ENABLED
    void print_stats(tdc::StatPhase& statphase) {
            statphase.log("class", typeid(class_type).name());
            statphase.log("size", size());
            // statphase.log("bytes", size_in_bytes());
            statphase.log("group_count", group_count());
            statphase.log("bucket_count", bucket_count());
            statphase.log("overflow_size", m_overflow.size());
            statphase.log("overflow_capacity", m_overflow.capacity());
            double deviation = 0;
            std::vector<size_t> bucket_sizes(bucket_count());
            for(size_t i = 0; i < bucket_count(); ++i) {
                bucket_sizes[i] = bucket_size(i);
                const double diff = (static_cast<double>(size()/bucket_count()) - bucket_size(i));
                deviation += diff*diff;
            }
            std::sort(bucket_sizes.begin(), bucket_sizes.end());
            statphase.log("deviation_bucket_size", deviation);
            statphase.log("key_width", m_key_width);
            statphase.log("value_width", m_value_width);
            statphase.log("median_bucket_size", 
                    (bucket_sizes.size() % 2 != 0)
                    ? static_cast<double>(bucket_sizes[bucket_sizes.size()/2])
                    : static_cast<double>(bucket_sizes[(bucket_sizes.size()-1)/2]+ bucket_sizes[(bucket_sizes.size()/2)])/2
                    );
            statphase.log("average_bucket_size", static_cast<double>(size()) / bucket_count());
            statphase.log("min_bucket_size", bucket_sizes[0]);
            statphase.log("max_bucket_size", max_bucket_size());
            statphase.log("max_group_size", max_groupsize());
            statphase.log("capacity", capacity());
    }
#endif

    //TODO: add swap, operator= and copy-constructor


};



}//ns group

}//ns separate_chaining


