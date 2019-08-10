#pragma once
#include <tudocomp/ds/IntVector.hpp>

namespace separate_chaining {

  template<class key_t, class value_t>
    class dummy_overflow {
        using key_type = key_t;
        using value_type = value_t;

        private:
        static value_type m_dummy_value;
        static key_type m_dummy_key;

        public:
        dummy_overflow() { }

        static constexpr void resize_buckets(size_t) { }
        static constexpr bool need_consult() { return false; }

        static constexpr size_t size() { return 0; }
        static constexpr size_t capacity() { return 0; }

        static constexpr size_t next_position(const size_t position) { return position; }
        static constexpr size_t previous_position(const size_t position) { return position; }

        static constexpr void deserialize(std::istream&) {
        }
        static constexpr void serialize(std::ostream&) {
        }
        static constexpr bool valid_position(size_t) { return false; } //! is position a valid entry of the table?

        static constexpr size_t size_in_bytes(const size_t) {
            return 0;
        }
        static constexpr void clear() { }
        static constexpr size_t insert(const size_t bucket, const key_type&, const value_type&) { return 0; }
        static constexpr size_t find(const key_type&) {
            return (-1ULL);
        }
        constexpr value_type& operator[](const size_t ) {
            return m_dummy_value;
        }
        constexpr const value_type& operator[](const size_t ) const {
            return m_dummy_value;
        }
        // static key_type& key(const size_t ) {
        //     return m_dummy_key;
        // }
        const key_type& key(const size_t ) const {
            return m_dummy_key;
        }
        constexpr void erase(const size_t) const {}

    };

  template<class key_t, class value_t> typename dummy_overflow<key_t,value_t>::value_type dummy_overflow<key_t,value_t>::m_dummy_value = 0;
  template<class key_t, class value_t> typename dummy_overflow<key_t,value_t>::key_type dummy_overflow<key_t,value_t>::m_dummy_key = 0;

  template<class key_t, class value_t>
    class array_overflow {
        public:
        using key_type = key_t;
        using value_type = value_t;

        private:
        plain_bucket<key_type> m_keys;
        plain_bucket<value_type> m_values;
        static constexpr uint_fast8_t m_length = 16;
        uint_fast8_t m_elements = 0;

        public:
        size_t size() const { return m_elements; }
        size_t capacity() const { return m_length; }

        bool valid_position(const size_t position) const { return position < size(); } //! is position a valid entry of the table?
        
        array_overflow() {
            m_keys.initiate(m_length, 0);
            m_values.initiate(m_length, 0);
        }

        size_t next_position(const size_t position) const { 
          DCHECK_LT(position, m_length);
          return position+1;
        }
        size_t previous_position(const size_t position) const { 
          DCHECK_GT(position,0);
          return position-1;
        }

        void deserialize(std::istream& is) {
            is.read(reinterpret_cast<char*>(&m_elements), sizeof(decltype(m_elements)));
            m_keys.deserialize(is, m_elements, 0);
            m_values.deserialize(is, m_elements, 0);
        }
        void serialize(std::ostream& os) const {
            os.write(reinterpret_cast<const char*>(&m_elements), sizeof(decltype(m_elements)));
            m_keys.serialize(os, m_elements, 0);
            m_values.serialize(os, m_elements, 0);
        }

        static constexpr size_t size_in_bytes(const size_t size) {
            return decltype(m_keys)::size_in_bytes(size, 0) + decltype(m_values)::size_in_bytes(size, 0) + 1;
        }
        void clear() {
            m_keys.clear();
            m_values.clear();
        }
        void erase(const size_t position) {
            DCHECK_LT(position, m_elements);
            for(size_t i = m_elements; i < position; --i) {
              m_keys.write(i-1, m_keys.read(i,0),  0);
              m_values.write(i-1, m_values.read(i,0),  0);
            }
            --m_elements;
        }
        /*
         * @bucket: the bucket in which we wanted to insert the (key,value) pair
         *   this is necessary such that we do not need to consult 
         *
         * inserts a (key,value) pair
         * returns the ID or position of this pair
         */
        size_t insert(const size_t bucket, const key_type& key, const value_type& value) {
            DCHECK_LT(m_elements, m_length);
            m_bucketfull[bucket] = true;
            m_keys[m_elements] = key;
            m_values[m_elements] = value;
            return m_elements++;
        }
        tdc::BitVector m_bucketfull;
        void resize_buckets(size_t bucketcount) {
          m_bucketfull.resize(bucketcount);
        }

        
        bool need_consult(size_t bucket) const {
          DCHECK_LT(bucket, m_bucketfull.size());
          return m_bucketfull[bucket];
        }
        size_t find(const key_type& key) const { // returns position of key
            return m_keys.find(key, m_length, 0);
        }
        value_type& operator[](const size_t index) { // returns value
            DCHECK_LT(index, m_length);
            return m_values[index];
        }
        const value_type& operator[](const size_t index) const {
            DCHECK_LT(index, m_length);
            return m_values[index];
        }
        const key_type& key(const size_t index) const {
            DCHECK_LT(index, m_length);
            return m_keys[index];
        }

    };

}//ns

#include <unordered_map>

namespace separate_chaining {
  template<class key_t, class value_t>
    class map_overflow {
        public:
        using key_type = key_t;
        using value_type = value_t;

        private:
        std::unordered_map<key_type, value_type> m_map;
        size_t m_length = 0;

        public:
        size_t size() const { return m_map.size(); }
        size_t capacity() const { return m_length; }
        
        map_overflow() {
          m_map.max_load_factor(1);
        }

        bool valid_position(const size_t position) const { 
          return position < m_map.bucket_count() && m_map.begin(position) != m_map.end(position); 
        }
        size_t next_position(size_t position) const { 
          DCHECK_LT(position,m_map.bucket_count());
          do {
            ++position;
          } while(position < m_map.bucket_count() && m_map.begin(position) == m_map.end(position));
          return position;
        }
        size_t previous_position(size_t position) const { 
          DCHECK_GT(position,0);
          do {
            --position;
          } while(position > 0 && m_map.begin(position) == m_map.end(position));
          return position;
        }

        void deserialize(std::istream& is) {
          size_t elements;
            is.read(reinterpret_cast<char*>(&elements), sizeof(decltype(elements)));
            m_map.reserve(elements);
            for(size_t i = 0; i < elements; ++i) {
              key_type key;
              value_type value;
              is.read(reinterpret_cast<char*>(&key), sizeof(decltype(key)));
              is.read(reinterpret_cast<char*>(&value), sizeof(decltype(value)));
              m_map[key] = value;
            }

        }
        void serialize(std::ostream& os) const {
          const size_t elements = m_map.size();
            os.write(reinterpret_cast<const char*>(&elements), sizeof(decltype(elements)));
            for(auto& el : m_map) {
              os.write(reinterpret_cast<char*>(el.first), sizeof(decltype(el.first)));
              os.write(reinterpret_cast<char*>(el.first), sizeof(decltype(el.first)));
            }
        }

        static constexpr size_t size_in_bytes(const size_t size) {
          return -1; //TODO
        }
        void clear() {
          m_map.clear();
        }
        void erase(const size_t position) {
            DCHECK_LT(position, m_map.bucket_count());
            m_map.erase(m_map.cbegin(position)->first);
        }

        size_t insert(const size_t bucket, const key_type& key, const value_type& value) {
            DCHECK_LT(m_map.size(), m_length);
            m_bucketfull[bucket] = true;
            const size_t mybucket = m_map.bucket(key);
            if(m_map.begin(mybucket) == m_map.end(mybucket)) return static_cast<size_t>(-1ULL); // cannot insert as there is already an element present
            m_map[key] = value;
            return m_map.bucket(key);
        }
        tdc::BitVector m_bucketfull;
        void resize_buckets(size_t bucketcount) {
          m_length = bucketcount; // sets the max. number of elements to the number of buckets in the hash table
          m_map.reserve(bucketcount);
          m_bucketfull.resize(bucketcount);
        }
        
        bool need_consult(size_t bucket) const {
          DCHECK_LT(bucket, m_bucketfull.size());
          return m_bucketfull[bucket];
        }
        size_t find(const key_type& key) const { // returns position of key
            return m_map.bucket(key);
        }
        value_type& operator[](const size_t index) { // returns value
            DCHECK_LT(index, m_map.bucket_count());
            return m_map.begin(index)->second;
        }
        const value_type& operator[](const size_t index) const {
            DCHECK_LT(index, m_length);
            return m_map.begin(index)->second;
        }
        const key_type& key(const size_t index) const {
            DCHECK_LT(index, m_length);
            return m_map.begin(index)->first;
        }

    };

}//ns

