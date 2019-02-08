#pragma once

#include "math.hpp"
#include "dcheck.hpp"
#include <tudocomp/util/sdsl_bits.hpp>

#include <immintrin.h>

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
   _mm_free(ptr);
   return newptr;
}


template<class key_t>
struct avx_functions {
   public:
   using key_type = key_t;
   static __m256i load(key_type i);
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



template<class key_t>
class avx2_key_bucket {
    public:
    using key_type = key_t;

    private:
    static constexpr size_t m_alignment = 32;
    key_type* m_keys = nullptr; //!bucket for keys
    ON_DEBUG(size_t m_length;)

    public:
    bool initialized() const { return m_keys != nullptr; } //!check whether we can add elements to the bucket
    void clear() {
        if(m_keys != nullptr) {
            _mm_free(m_keys);
        }
        m_keys = nullptr;
    }

    avx2_key_bucket() = default;

    void initiate(const size_t size) {
       DCHECK(m_keys == nullptr);
        m_keys = reinterpret_cast<key_type*>  (_mm_malloc(sizeof(key_type)*size, m_alignment));
        ON_DEBUG(m_length = size;)
    }



    void resize(const size_t oldsize, const size_t size, [[maybe_unused]] const size_t width) {

        m_keys = reinterpret_cast<key_type*>  (aligned_realloc(m_keys, sizeof(key_type)*oldsize,  sizeof(key_type)*size, m_alignment));
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

    size_t find(const uint64_t& key, const size_t length, [[maybe_unused]] const size_t width) const {
     constexpr size_t register_size = 32/sizeof(key_type); // number of `key_type` elements fitting in 256 bits = 32 bytes
     if(length >= register_size) {
        const __m256i pattern = avx_functions<key_type>::load(key);
        // avx_key_bucket
        //TODO: avx2 __m256i _mm256_broadcastw_epi16 (__m128i a) and __m256i _mm256_cmpeq_epi8 (__m256i a, __m256i b)
        for(size_t i = 0; i < length/register_size; ++i) {
           __m256i ma = _mm256_load_si256((__m256i*)(m_keys+i*register_size)); 
           const unsigned int mask = _mm256_movemask_epi8(avx_functions<key_type>::compare(ma, pattern));
           if(mask == 0) { continue; }
           if(~mask == 0) { return i*register_size; }
           const size_t least_significant = __builtin_ctz(mask);
           DCHECK_EQ(((least_significant)/sizeof(key_type))*sizeof(key_type), least_significant);
           const size_t pos = (least_significant)/sizeof(key_type);
           return i*register_size+pos;
        }
     }
    for(size_t i = (length/register_size)*register_size; i < length; ++i) {
       if(m_keys[i] == key) return i;
    }
    return -1ULL;
 }


    ~avx2_key_bucket() { clear(); }

    avx2_key_bucket(avx2_key_bucket&& other) 
        : m_keys(std::move(other.m_keys))
    {
        other.m_keys = nullptr;
        ON_DEBUG(m_length = other.m_length; other.m_length = 0;)
    }

    avx2_key_bucket& operator=(avx2_key_bucket&& other) {
        clear();
        m_keys = std::move(other.m_keys);
        other.m_keys = nullptr;
        ON_DEBUG(m_length = other.m_length; other.m_length = 0;)
        return *this;
    }

};






template<class key_t>
class plain_key_bucket {
    public:
    using key_type = key_t;

    protected:
    key_type* m_keys = nullptr; //!bucket for keys
    ON_DEBUG(size_t m_length;)

    public:

    bool initialized() const { return m_keys != nullptr; } //!check whether we can add elements to the bucket

    void clear() {
        if(m_keys != nullptr) {
            free(m_keys);
        }
        m_keys = nullptr;
    }

    plain_key_bucket() = default;

    key_type& operator[](const size_t index) {
        DCHECK_LT(index, m_length);
        return m_keys[index];
    }
    const key_type& operator[](const size_t index) const {
        DCHECK_LT(index, m_length);
        return m_keys[index];
    }

    void initiate(const size_t size) {
       DCHECK(m_keys == nullptr);
        m_keys = reinterpret_cast<key_type*>  (malloc(sizeof(key_type)*size));
        ON_DEBUG(m_length = size;)
    }

    void resize([[maybe_unused]] const size_t oldsize, const size_t size, [[maybe_unused]] const size_t width = 0) {
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
        ON_DEBUG(m_length = other.m_keys; other.m_keys = 0;)
    }
    plain_key_bucket(key_type*&& keys) 
        : m_keys(std::move(keys))
    {
        keys = nullptr;
    }

    plain_key_bucket& operator=(plain_key_bucket&& other) {
        clear();
        m_keys = std::move(other.m_keys);
        other.m_keys = nullptr;
        ON_DEBUG(m_length = other.m_length; other.m_length = 0;)
        return *this;
    }
};

template<class key_t>
class class_key_bucket : public plain_key_bucket<key_t> {
    public:
    using key_type = typename plain_key_bucket<key_t>::key_type;
    using super_class = plain_key_bucket<key_t>;

    public:

    void clear() {
        if(super_class::m_keys != nullptr) {
           delete [] super_class::m_keys;
        }
        super_class::m_keys = nullptr;
    }

    class_key_bucket() = default;

    void initiate(const size_t size) {
       DCHECK(super_class::m_keys == nullptr);
       super_class::m_keys = new key_type[size];
       ON_DEBUG(super_class::m_length = size);
    }

    void resize(const size_t oldsize, const size_t size, [[maybe_unused]] const size_t width = 0)  {
       key_type* keys = new key_type[size];
        for(size_t i = 0; i < oldsize; ++i) {
            keys[i] = std::move(super_class::m_keys[i]);
        }
        delete [] super_class::m_keys;
        super_class::m_keys = keys;
       ON_DEBUG(super_class::m_length = size);
    }

    ~class_key_bucket() { clear(); }

    class_key_bucket(class_key_bucket&& other) 
        : super_class(std::move(other.super_class::m_keys))
    {
        other.super_class::m_keys = nullptr;
        ON_DEBUG(super_class::m_length = other.m_length; other.m_length = 0;)
    }

    class_key_bucket& operator=(class_key_bucket&& other) {
        clear();
        super_class::m_keys = std::move(other.m_keys);
        other.m_keys = nullptr;
        ON_DEBUG(super_class::m_length = other.m_length; other.m_length = 0;)
        return *this;
    }

};

class varwidth_key_bucket {
    public:
    using key_type = uint64_t;

    private:
    key_type* m_keys = nullptr; //!bucket for keys
    ON_DEBUG(size_t m_length;)

    public:
    bool initialized() const { return m_keys != nullptr; } //!check whether we can add elements to the bucket
    void clear() {
        if(m_keys != nullptr) {
            free(m_keys);
        }
        m_keys = nullptr;
    }

    varwidth_key_bucket() = default;

    void initiate(const size_t size) {
       DCHECK(m_keys == nullptr);
        m_keys = reinterpret_cast<key_type*>  (malloc(sizeof(key_type)*size));
        ON_DEBUG(m_length = size;)
    }

    void resize(const size_t oldsize, const size_t size, const size_t width) {
       if(ceil_div<size_t>((oldsize)*width, 64) < ceil_div<size_t>((size)*width, 64)) {
          m_keys = reinterpret_cast<key_type*>  (realloc(m_keys, sizeof(key_type)*ceil_div<size_t>(size*width, 64) ));
       }
       ON_DEBUG(m_length = size;)
    }

    void write(const size_t i, const key_type& key, const uint_fast8_t width) {
        DCHECK_LT((static_cast<size_t>(i)*width)/64 + ((i)* width) % 64, 64*ceil_div<size_t>(m_length*width, 64) );
        DCHECK_LE(most_significant_bit(key), width);

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
        ON_DEBUG(m_length = other.m_length; other.m_length = 0;)
    }

    varwidth_key_bucket& operator=(varwidth_key_bucket&& other) {
        clear();
        m_keys = std::move(other.m_keys);
        other.m_keys = nullptr;
        ON_DEBUG(m_length = other.m_length; other.m_length = 0;)
        return *this;
    }
};

}//ns separate_chaining

