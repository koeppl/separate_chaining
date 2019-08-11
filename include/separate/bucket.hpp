#pragma once

#include "math.hpp"
#include "dcheck.hpp"
#include <tudocomp/util/sdsl_bits.hpp>

#include <immintrin.h>

#include "broadwordsearch.hpp"

constexpr uint64_t BROADWORD_SEARCH_THRESHOLD = 0;

namespace separate_chaining {

inline void* aligned_realloc(void*const ptr, const size_t oldsize, const size_t size, const size_t alignment) {
   DCHECK_LE(oldsize, size);
   void* newptr = _mm_malloc(size, alignment);

   for(size_t i = 0; i < oldsize/sizeof(uint64_t); ++i) {
      reinterpret_cast<uint64_t*>(newptr)[i] = reinterpret_cast<const uint64_t*>(ptr)[i];
   }
   for(size_t i = (oldsize/sizeof(uint64_t))*sizeof(uint64_t); i < oldsize; ++i) {
      reinterpret_cast<uint8_t*>(newptr)[i] = reinterpret_cast<const uint8_t*>(ptr)[i];
   }
   DCHECK(ptr != nullptr);
   _mm_free(ptr);
   return newptr;
}

#ifdef __AVX2__

template<class storage_t>
struct avx_functions {
   public:
   using storage_type = storage_t;
   static __m256i load(storage_type i);
   static __m256i compare(__m256i a, __m256i b);
};

template<>
struct avx_functions<uint32_t> {
   static __m256i load(uint32_t i) { return _mm256_set1_epi32(i); }
   static __m256i compare(__m256i a, __m256i b) { return _mm256_cmpeq_epi32(a,b); }
};

template<>
struct avx_functions<uint64_t> {
   static __m256i load(uint64_t i) { return _mm256_set1_epi64x(i); }
   static __m256i compare(__m256i a, __m256i b) { return _mm256_cmpeq_epi64(a,b); }
};

template<>
struct avx_functions<uint16_t> {
   static __m256i load(uint16_t i) { return _mm256_set1_epi16(i); }
   static __m256i compare(__m256i a, __m256i b) { return _mm256_cmpeq_epi16(a,b); }
};

template<>
struct avx_functions<uint8_t> {
   static __m256i load(uint8_t i) { return _mm256_set1_epi8(i); }
   static __m256i compare(__m256i a, __m256i b) { return _mm256_cmpeq_epi8(a,b); }
};



template<class storage_t>
class avx2_bucket {
    public:
    using storage_type = storage_t;
    ON_DEBUG(size_t m_length;)

    private:
    static constexpr size_t m_alignment = 32;
    storage_type* m_data = nullptr; //!bucket for keys

    public:
    bool initialized() const { return m_data != nullptr; } //!check whether we can add elements to the bucket
    void clear() {
        if(m_data != nullptr) {
            _mm_free(m_data);
        }
        m_data = nullptr;
        ON_DEBUG(m_length = 0;)
    }

    avx2_bucket() = default;

    void initiate(const size_t length, [[maybe_unused]] const uint_fast8_t width) {
       DCHECK(m_data == nullptr);
        m_data = reinterpret_cast<storage_type*>  (_mm_malloc(sizeof(storage_type)*length, m_alignment));
        ON_DEBUG(m_length = length;)
#if defined(STATS_ENABLED) && !defined(MALLOC_DISABLED)
       throw std::runtime_error("Cannot use tudocomp stats in conjuction with avx2");
#endif
    }

    void deserialize(std::istream& is, const size_t size, [[maybe_unused]] const uint_fast8_t width) {
       ON_DEBUG(is.read(reinterpret_cast<char*>(&m_length), sizeof(decltype(m_length))));
       DCHECK_LE(size, m_length);
       initiate(size,width);
       is.read(reinterpret_cast<char*>(m_data), sizeof(storage_type)*size);
    }
    void serialize(std::ostream& os, const size_t size, [[maybe_unused]] const uint_fast8_t width) const {
       ON_DEBUG(os.write(reinterpret_cast<const char*>(&m_length), sizeof(decltype(m_length))));
       DCHECK_LE(size, m_length);
       os.write(reinterpret_cast<const char*>(m_data), sizeof(storage_type)*size);
    }
    static constexpr size_t size_in_bytes(const size_t size, [[maybe_unused]] const size_t width = 0) {
       ON_DEBUG(return size*sizeof(storage_type) + sizeof(m_length));
       return size*sizeof(storage_type);
    }



    void resize(const size_t oldsize, const size_t size, [[maybe_unused]] const size_t width) {

        m_data = reinterpret_cast<storage_type*>  (aligned_realloc(m_data, sizeof(storage_type)*oldsize,  sizeof(storage_type)*size, m_alignment));
        ON_DEBUG(m_length = size;)
    }

    void write(const size_t i, const storage_type& key, [[maybe_unused]] const uint_fast8_t width) {
        DCHECK_LT(i, m_length);
        m_data[i] = key;
    }
    storage_type read(size_t i, [[maybe_unused]]  size_t width) const {
        DCHECK_LT(i, m_length);
        return m_data[i];
    }

    size_t find(const uint64_t& key, const size_t length, [[maybe_unused]] const size_t width) const {
     constexpr size_t register_size = 32/sizeof(storage_type); // number of `storage_type` elements fitting in 256 bits = 32 bytes
     if(length >= register_size) {
        const __m256i pattern = avx_functions<storage_type>::load(key);
        // avx_bucket
        //TODO: avx2 __m256i _mm256_broadcastw_epi16 (__m128i a) and __m256i _mm256_cmpeq_epi8 (__m256i a, __m256i b)
        for(size_t i = 0; i < length/register_size; ++i) {
           __m256i ma = _mm256_load_si256((__m256i*)(m_data+i*register_size)); 
           const unsigned int mask = _mm256_movemask_epi8(avx_functions<storage_type>::compare(ma, pattern));
           if(mask == 0) { continue; }
           if(~mask == 0) { return i*register_size; }
           const size_t least_significant = __builtin_ctz(mask);
           DCHECK_EQ(((least_significant)/sizeof(storage_type))*sizeof(storage_type), least_significant);
           const size_t pos = (least_significant)/sizeof(storage_type);
           return i*register_size+pos;
        }
     }
    for(size_t i = (length/register_size)*register_size; i < length; ++i) {
       if(m_data[i] == key) return i;
    }
    return -1ULL;
 }


    ~avx2_bucket() { clear(); }

    avx2_bucket(avx2_bucket&& other) 
        : m_data(std::move(other.m_data))
    {
        other.m_data = nullptr;
        ON_DEBUG(m_length = other.m_length; other.m_length = 0;)
    }

    avx2_bucket& operator=(avx2_bucket&& other) {
        clear();
        m_data = std::move(other.m_data);
        other.m_data = nullptr;
        ON_DEBUG(m_length = other.m_length; other.m_length = 0;)
        return *this;
    }

};


#endif// __AVX2__







template<class storage_t>
class plain_bucket {
    public:
    using storage_type = storage_t;
    ON_DEBUG(size_t m_length;)

    protected:
    storage_type* m_data = nullptr; //!bucket for keys

    public:

    void deserialize(std::istream& is, const size_t size, [[maybe_unused]] const uint_fast8_t width) {
       ON_DEBUG(is.read(reinterpret_cast<char*>(&m_length), sizeof(decltype(m_length))));
       DCHECK_LE(size, m_length);
       initiate(size, width);
       is.read(reinterpret_cast<char*>(m_data), sizeof(storage_type)*size);
    }
    void serialize(std::ostream& os, const size_t size, [[maybe_unused]] const uint_fast8_t width) const {
       ON_DEBUG(os.write(reinterpret_cast<const char*>(&m_length), sizeof(decltype(m_length))));
       DCHECK_LE(size, m_length);
       os.write(reinterpret_cast<const char*>(m_data), sizeof(storage_type)*size);
    }
    static constexpr size_t size_in_bytes(const size_t size, [[maybe_unused]] const size_t width = 0) {
       ON_DEBUG(return size*sizeof(storage_type) + sizeof(m_length));
       return size*sizeof(storage_type);
    }


    bool initialized() const { return m_data != nullptr; } //!check whether we can add elements to the bucket

    void clear() {
        if(m_data != nullptr) {
            free(m_data);
        }
        m_data = nullptr;
        ON_DEBUG(m_length = 0;)
    }

    plain_bucket() = default;

    storage_type& operator[](const size_t index) {
        DCHECK_LT(index, m_length);
        return m_data[index];
    }
    const storage_type& operator[](const size_t index) const {
        DCHECK_LT(index, m_length);
        return m_data[index];
    }

    void initiate(const size_t length, [[maybe_unused]] const uint_fast8_t width) {
       DCHECK(m_data == nullptr);
       m_data = reinterpret_cast<storage_type*>  (malloc(sizeof(storage_type)*length));
       ON_DEBUG(m_length = length;)
    }



#if defined(__GNUC__) && !defined(__INTEL_COMPILER) && (((__GNUC__ * 100) + __GNUC_MINOR__) >= 800)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif
// this function creates warnings when storage_type is a class wrapped around a POD
    void resize([[maybe_unused]] const size_t oldsize, const size_t size, [[maybe_unused]] const size_t width = 0) {
        m_data = reinterpret_cast<storage_type*>  (realloc(m_data, sizeof(storage_type)*size));
        ON_DEBUG(m_length = size;)
    }


#if defined(__GNUC__) && !defined(__INTEL_COMPILER) && (((__GNUC__ * 100) + __GNUC_MINOR__) >= 800)
#pragma GCC diagnostic pop
#endif

    void write(const size_t i, const storage_type& key, [[maybe_unused]] const uint_fast8_t width = 0) {
        DCHECK_LT(i, m_length);
        m_data[i] = key;
    }
    storage_type read(size_t i, [[maybe_unused]]  size_t width) const {
        DCHECK_LT(i, m_length);
        return m_data[i];
    }
    size_t find(const storage_type& key, const size_t length, [[maybe_unused]] const size_t width = 0) const {
       for(size_t i = 0; i < length; ++i) {
          if(m_data[i] == key) return i;
       }
       return -1ULL;
    }

    ~plain_bucket() { clear(); }

    plain_bucket(plain_bucket&& other) 
        : m_data(std::move(other.m_data))
    {
        other.m_data = nullptr;
        ON_DEBUG(m_length = other.m_length; other.m_data = 0;)
    }
    plain_bucket(storage_type*&& keys) 
        : m_data(std::move(keys))
    {
        keys = nullptr;
    }

    plain_bucket& operator=(plain_bucket&& other) {
        clear();
        m_data = std::move(other.m_data);
        other.m_data = nullptr;
        ON_DEBUG(m_length = other.m_length; other.m_length = 0;)
        return *this;
    }
};

template<class storage_t>
class class_bucket : public plain_bucket<storage_t> {
    public:
    using storage_type = typename plain_bucket<storage_t>::storage_type;
    using super_class = plain_bucket<storage_t>;

    public:

    void clear() {
        if(super_class::m_data != nullptr) {
           delete [] super_class::m_data;
        }
        super_class::m_data = nullptr;
    }

    class_bucket() = default;

    void initiate(const size_t length, [[maybe_unused]] const uint_fast8_t width) {
       DCHECK(super_class::m_data == nullptr);
       super_class::m_data = new storage_type[length];
       ON_DEBUG(super_class::m_length = length);
    }

    void resize(const size_t oldsize, const size_t size, [[maybe_unused]] const size_t width = 0)  {
       storage_type* keys = new storage_type[size];
       for(size_t i = 0; i < oldsize; ++i) {
          keys[i] = std::move(super_class::m_data[i]);
       }
       DCHECK(super_class::m_data != nullptr);
       delete [] super_class::m_data;
       super_class::m_data = keys;
       ON_DEBUG(super_class::m_length = size);
    }

    ~class_bucket() { clear(); }

    class_bucket(class_bucket&& other) 
        : super_class(std::move(other.super_class::m_data))
    {
        other.super_class::m_data = nullptr;
        ON_DEBUG(super_class::m_length = other.m_length; other.m_length = 0;)
    }

    class_bucket& operator=(class_bucket&& other) {
        clear();
        super_class::m_data = std::move(other.m_data);
        other.m_data = nullptr;
        ON_DEBUG(super_class::m_length = other.m_length; other.m_length = 0;)
        return *this;
    }

};

/**!
 * `internal_t` is a tradeoff between the number of mallocs and unused space, as it defines the block size in which elements are stored, 
 * i.e., its memory consuption is quantisized by this type's byte size
**/
template<class internal_t = uint8_t>
class varwidth_bucket {
    public:
    using internal_type = internal_t;
    using storage_type = uint64_t;
    static constexpr uint_fast8_t storage_bitwidth = sizeof(internal_type)*8;
    ON_DEBUG(size_t m_length;)

    private:
    internal_type* m_data = nullptr; //!bucket for keys

    public:

    void deserialize(std::istream& is, const size_t size, const uint_fast8_t width) {
       ON_DEBUG(is.read(reinterpret_cast<char*>(&m_length), sizeof(decltype(m_length))));
       DCHECK_LE(size, m_length);
       const size_t read_length = ceil_div<size_t>(size*width, storage_bitwidth);
       m_data = reinterpret_cast<internal_type*>  (malloc(sizeof(internal_type)*read_length));
       is.read(reinterpret_cast<char*>(m_data), sizeof(internal_type)*read_length);
    }
    void serialize(std::ostream& os, const size_t size, const uint_fast8_t width) const {
       ON_DEBUG(os.write(reinterpret_cast<const char*>(&m_length), sizeof(decltype(m_length))));
       DCHECK_LE(size, m_length);
       const size_t write_length = ceil_div<size_t>(size*width, storage_bitwidth);
       os.write(reinterpret_cast<const char*>(m_data), sizeof(internal_type)*write_length);
    }
    static constexpr size_t size_in_bytes(const size_t size, const size_t width = 0) {
       ON_DEBUG(return size*sizeof(internal_type) + sizeof(m_length));
       const size_t length = ceil_div<size_t>(size*width, storage_bitwidth);
       return length*sizeof(internal_type);
    }

    bool initialized() const { return m_data != nullptr; } //!check whether we can add elements to the bucket
    void clear() {
        if(m_data != nullptr) {
            free(m_data);
        }
        m_data = nullptr;
        ON_DEBUG(m_length = 0;)
    }

    varwidth_bucket() = default;

    void initiate(const size_t length, const uint_fast8_t width) {
       DCHECK(m_data == nullptr);
        m_data = reinterpret_cast<internal_type*>  (malloc(sizeof(internal_type)* ceil_div<size_t>(length*width, storage_bitwidth) ));
        ON_DEBUG(m_length = ceil_div<size_t>(length*width, storage_bitwidth);)
    }

    void resize(const size_t oldsize, const size_t length, const size_t width) {
       if(ceil_div<size_t>((oldsize)*width, storage_bitwidth) < ceil_div<size_t>((length)*width, storage_bitwidth)) {
          m_data = reinterpret_cast<internal_type*>  (realloc(m_data, sizeof(internal_type) * ceil_div<size_t>(length*width, storage_bitwidth ) ));
       }
       ON_DEBUG(m_length = ceil_div<size_t>(length*width, storage_bitwidth);)
    }

    void write(const size_t i, const storage_type key, const uint_fast8_t width) {
        DCHECK_LT((static_cast<size_t>(i)*width)/storage_bitwidth + ((i)* width) % storage_bitwidth, storage_bitwidth*ceil_div<size_t>(m_length*width, storage_bitwidth) );
        DCHECK_LE(most_significant_bit(key), width);

        tdc::tdc_sdsl::bits_impl<>::write_int(reinterpret_cast<uint64_t*>(m_data + (static_cast<size_t>(i)*width)/storage_bitwidth), key, ((i)* width) % storage_bitwidth, width);
        DCHECK_EQ(tdc::tdc_sdsl::bits_impl<>::read_int(reinterpret_cast<uint64_t*>(m_data + (static_cast<size_t>(i)*width)/storage_bitwidth), ((i)* width) % storage_bitwidth, width), key);
    }
    storage_type read(size_t i, size_t width) const {
        DCHECK_LT((static_cast<size_t>(i)*width)/storage_bitwidth + ((i)* width) % storage_bitwidth, storage_bitwidth*ceil_div<size_t>(m_length*width, storage_bitwidth) );
        return tdc::tdc_sdsl::bits_impl<>::read_int(reinterpret_cast<uint64_t*>(m_data + (static_cast<size_t>(i)*width)/storage_bitwidth), ((i)* width) % storage_bitwidth, width);
    }

    size_t find(const storage_type& key, const size_t length, const uint_fast8_t width) const {
       DCHECK_LE(length*width, m_length*storage_bitwidth);
       if(length > BROADWORD_SEARCH_THRESHOLD && width < 64) {
          return broadwordsearch::broadsearch(reinterpret_cast<uint64_t*>(m_data), length, width, key);
       }


       uint8_t offset = 0;
       const uint64_t* it = reinterpret_cast<uint64_t*>(m_data);

       for(size_t i = 0; i < length; ++i) { // needed?
            const storage_type read_key = tdc::tdc_sdsl::bits_impl<>::read_int_and_move(it, offset, width);
            //DCHECK_EQ(read_key , bucket_plainkeys[i]);
            if(read_key == key) {
                return i;
            }
        }
       return -1ULL;
    }


    ~varwidth_bucket() { clear(); }

    varwidth_bucket(varwidth_bucket&& other) 
        : m_data(std::move(other.m_data))
    {
        other.m_data = nullptr;
        ON_DEBUG(m_length = other.m_length; other.m_length = 0;)
    }

    varwidth_bucket& operator=(varwidth_bucket&& other) {
        clear();
        m_data = std::move(other.m_data);
        other.m_data = nullptr;
        ON_DEBUG(m_length = other.m_length; other.m_length = 0;)
        return *this;
    }
};

// /**!
//  * The fixwidth_bucket is a varwidth_bucket with a fixed width-size.
//  * This bucket can be used to store values with fixed but arbitrary bit widths
// **/
// template<uint8_t bitwidth, class internal_t = uint8_t>
// class fixwidth_bucket {
//     public:
//
//     class value_wrapper {
//        fixwidth_bucket*const m_bucket;
//        uint64_t m_value;
//        const uint64_t m_position;
//
//        public:
//        value_wrapper() : m_bucket(nullptr), m_position(0) {}
//
//        value_wrapper(fixwidth_bucket*const bucket, uint64_t position, uint64_t value)
//           : m_bucket(bucket), m_position(position), m_value(value) {}
//
//        operator uint64_t() {
//           return m_value;
//        }
//        value_wrapper& operator=(const uint64_t val) {
//           DCHECK(m_bucket != nullptr);
//           m_bucket->write(m_position, val);
//        }
//        value_wrapper& operator=(const value_wrapper& val) {
//           return operator=(val.m_value);
//        }
//     };
//
//     using internal_type = internal_t;
//     using storage_type = value_wrapper;
//     static constexpr uint_fast8_t storage_bitwidth = sizeof(internal_type)*8;
//
//
//     private:
//     varwidth_bucket<internal_t> m_bucket;
//
//     public:
//
//     void deserialize(std::istream& is, const size_t size, [[maybe_unused]] const uint_fast8_t width) {
//        m_bucket.deserialize(is, size, bitwidth);
//     }
//     void serialize(std::ostream& os, const size_t size, [[maybe_unused]] const uint_fast8_t width) const {
//        m_bucket.serialize(os,size,bitwidth);
//     }
//     static constexpr size_t size_in_bytes(const size_t size, [[maybe_unused]] const size_t width = 0) {
//       return decltype(m_bucket)::size_in_bytes(size, bitwidth);
//     }
//
//     bool initialized() const { return m_bucket.initialized(); } //!check whether we can add elements to the bucket
//     void clear() { return m_bucket.clear(); }
//
//     fixwidth_bucket() = default;
//
//     void initiate(const size_t length, [[maybe_unused]] const uint_fast8_t width = 0) {
//        return m_bucket.initiate(length, bitwidth);
//     }
//
//     void resize(const size_t oldsize, const size_t length, [[maybe_unused]] const size_t width = 0) {
//        return m_bucket.resize(oldsize, length, bitwidth);
//     }
//
//     void write(const size_t i, const storage_type key, [[maybe_unused]] const uint_fast8_t width = 0) {
//        return m_bucket.write(i, key, bitwidth);
//     }
//
//     size_t find(const storage_type& key, const size_t length, [[maybe_unused]] const uint_fast8_t width = 0) const {
//        return m_bucket.find(key,length,bitwidth);
//     }
//
//
//     ~fixwidth_bucket() { clear(); }
//
//     fixwidth_bucket(fixwidth_bucket&& other) 
//         : m_bucket(std::move(other.m_bucket))
//     {
//     }
//
//     fixwidth_bucket& operator=(fixwidth_bucket&& other) {
//         clear();
//         m_bucket = std::move(other.m_bucket);
//         return *this;
//     }
//     storage_type operator[](const size_t index) {
//        return storage_type { this, index,  m_bucket.read(index, bitwidth) };
//     }
// };



}//ns separate_chaining

