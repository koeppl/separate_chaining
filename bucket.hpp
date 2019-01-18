#pragma once

#include "math.hpp"
#include "dcheck.hpp"
#include <tudocomp/util/sdsl_bits.hpp>

template<class key_t>
class plain_key_bucket {
    public:
    using key_type = key_t;

    private:
    key_type* m_keys = nullptr; //!bucket for keys
    ON_DEBUG(size_t m_length;)

    void clear() {
        if(m_keys != nullptr) {
            free(m_keys);
        }
        m_keys = nullptr;
    }

    public:
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

    void clear() {
        if(m_keys != nullptr) {
            free(m_keys);
        }
        m_keys = nullptr;
    }

    public:
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

