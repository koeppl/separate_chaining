#pragma once

/**
 * Iterator of a seperate hash table
 */
template<class hash_map>
struct separate_chaining_navigator {
    public:
        using storage_type = typename hash_map::storage_type;
        using key_type = typename hash_map::key_type;
        using value_type = typename hash_map::value_type;
        using size_type = typename hash_map::size_type;
        using class_type = separate_chaining_navigator<hash_map>;

        //private:
        hash_map& m_map;
        size_type m_bucket; //! in which bucket the iterator currently is
        size_type m_position; //! at which position in the bucket


        bool invalid() const {
            if(m_map.m_overflow.size() > 0 && m_bucket == m_map.bucket_count() && m_map.m_overflow.valid_position(m_position)) 
                { return false; }

            return m_bucket >= m_map.bucket_count() || m_position >= m_map.bucket_size(m_bucket);
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
        separate_chaining_navigator(hash_map& map, size_t bucket, size_t position) 
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
            return m_map.value_at(m_bucket, m_position);
        }
        // value_ref_type value_ref() {
        //     DDCHECK(!invalid());
        //     if(m_map.m_overflow.size() > 0 && m_bucket == m_map.bucket_count()) {
        //         return m_map.m_overflow[m_position];
        //     }
        //     return m_map.value_at(m_bucket, m_position);
        // }


        operator value_type() const { // cast to value
            return value();
        }
        class_type operator=(value_type val) {
            m_map.write_value(m_bucket, m_position, val);
            return *this;
        }


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
        bool operator!=(const separate_chaining_navigator<U>& o) const {
            return !( (*this)  == o);
        }
        template<class U>
        bool operator==(const separate_chaining_navigator<U>& o) const {
          if(!o.invalid() && !invalid()) return m_bucket == o.m_bucket && m_position == o.m_position; // compare positions
          return o.invalid() == invalid();
        }
};

/**
 * Iterator of a seperate hash table
 */
template<class hash_map>
struct separate_chaining_iterator : public separate_chaining_navigator<hash_map> {
    public:
        using key_type = typename hash_map::key_type;
        using value_type = typename hash_map::value_type;
        using pair_type = std::pair<key_type, value_type>;
        using size_type = typename hash_map::size_type;
        using class_type = separate_chaining_iterator<hash_map>;
        using super_type = separate_chaining_navigator<hash_map>;

        pair_type m_pair;

        void update() {
            m_pair = std::make_pair(super_type::key(), super_type::value());
        }

    public:
        separate_chaining_iterator(hash_map& map, size_t bucket, size_t position) 
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
        bool operator!=(const separate_chaining_iterator<U>& o) const {
            return !( (*this)  == o);
        }

        template<class U>
        bool operator==(const separate_chaining_iterator<U>& o) const {
            return super_type::operator==(o);
        }
};



