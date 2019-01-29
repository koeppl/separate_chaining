#pragma once

#include "math.hpp"
#include "dcheck.hpp"
#include <tudocomp/util/sdsl_bits.hpp>

#include <immintrin.h>

   inline void* aligned_realloc(void* ptr, const size_t oldsize, const size_t size, const size_t alignment) {
        DCHECK_LT(oldsize, size);
        void* newptr = _mm_malloc(size, alignment);

        for(size_t i = 0; i < oldsize/sizeof(uint64_t); ++i) {
            reinterpret_cast<uint64_t*>(newptr)[i] = reinterpret_cast<uint64_t*>(ptr)[i];
        }
        for(size_t i = (oldsize/sizeof(uint64_t))*sizeof(uint64_t)+1; i < oldsize; ++i) {
            reinterpret_cast<uint8_t*>(newptr)[i] = reinterpret_cast<uint8_t*>(ptr)[i];
        }
        _mm_free(ptr);
        return newptr;
    }

class avx2_key_bucket {
    public:
    using key_type = uint64_t;

    private:
    static constexpr size_t m_alignment = 32;
    key_type* m_keys = nullptr; //!bucket for keys
    ON_DEBUG(size_t m_length;)

    public:
    void clear() {
        if(m_keys != nullptr) {
            _mm_free(m_keys);
        }
        m_keys = nullptr;
    }

    avx2_key_bucket() = default;

    void initiate() {
        m_keys = reinterpret_cast<key_type*>  (_mm_malloc(sizeof(key_type), m_alignment));
        ON_DEBUG(m_length = 1;)
    }



    void increment_size(const size_t size, [[maybe_unused]] const size_t width) {

        m_keys = reinterpret_cast<key_type*>  (aligned_realloc(m_keys, sizeof(key_type)*(size-1),  sizeof(key_type)*size, m_alignment));
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
        constexpr size_t register_size = 256/sizeof(key_type);
        const __m256i pattern = _mm256_set1_epi64x(key);
// avx_key_bucket
//TODO: avx2 __m256i _mm256_broadcastw_epi16 (__m128i a) and __m256i _mm256_cmpeq_epi8 (__m256i a, __m256i b)
       for(size_t i = 0; i < length/register_size; ++i) {
           __m256i ma = _mm256_load_si256((__m256i*)(m_keys+i*register_size)); 
           const unsigned int mask = _mm256_movemask_epi8(_mm256_cmpeq_epi64(ma, pattern));
           if(mask == 0) { continue; }
           if(~mask == 0) { return i*register_size; }
           const size_t least_significant = __builtin_ctz(~mask);
           DCHECK_EQ(((least_significant)/8)*8, least_significant);
           const size_t pos = (least_significant)/8;
           return i*register_size+pos;
       }
       return -1ULL;
    }

    ~avx2_key_bucket() { clear(); }

    avx2_key_bucket(avx2_key_bucket&& other) 
        : m_keys(std::move(other.m_keys))
    {
        other.m_keys = nullptr;
    }

    avx2_key_bucket& operator=(avx2_key_bucket&& other) {
        clear();
        m_keys = std::move(other.m_keys);
        other.m_keys = nullptr;
        return *this;
    }
};


template<class key_t, class allocator_t>
class class_key_bucket {
    public:
    using key_type = key_t;
    using allocator_type = allocator_t;

    private:
    allocator_type allocator;
    key_type* m_keys = nullptr; //!bucket for keys
    ON_DEBUG(size_t m_length;)

    public:

    void clear() {
        if(m_keys != nullptr) {
            allocator.deallocate(m_keys);
        }
        m_keys = nullptr;
    }

    class_key_bucket() = default;

    void initiate() {
        m_keys = allocator.template allocate<key_type>(1);
        ON_DEBUG(m_length = 1;)
    }

    void increment_size(const size_t size, [[maybe_unused]] const size_t width) {
        key_type* keys = allocator.template allocate<key_type>(size);
        for(size_t i = 0; i < size-1; ++i) {
            keys[i] = std::move(m_keys[i]);
        }
        allocator.deallocate(m_keys);
        m_keys = keys;
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

    ~class_key_bucket() { clear(); }

    class_key_bucket(class_key_bucket&& other) 
        : m_keys(std::move(other.m_keys))
    {
        other.m_keys = nullptr;
    }

    class_key_bucket& operator=(class_key_bucket&& other) {
        clear();
        m_keys = std::move(other.m_keys);
        other.m_keys = nullptr;
        return *this;
    }

};


template<class key_t>
class plain_key_bucket {
    public:
    using key_type = key_t;

    private:
    key_type* m_keys = nullptr; //!bucket for keys
    ON_DEBUG(size_t m_length;)

    public:
    void clear() {
        if(m_keys != nullptr) {
            free(m_keys);
        }
        m_keys = nullptr;
    }

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
    void clear() {
        if(m_keys != nullptr) {
            free(m_keys);
        }
        m_keys = nullptr;
    }

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
    }

    varwidth_key_bucket& operator=(varwidth_key_bucket&& other) {
        clear();
        m_keys = std::move(other.m_keys);
        other.m_keys = nullptr;
        return *this;
    }
};

