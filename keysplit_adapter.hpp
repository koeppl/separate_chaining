#pragma once

#include <cstdint>
#include <numeric>
#include <limits>
#include <algorithm>
#include "dcheck.hpp"

/** can be used for keys in [0 .. max-1], where max = std::numeric_limits<uint64_t>::max()
 * max_bits: maximum number of bits a key can have
 * m_length: number of hash tables to use
 */
template<class map_type, size_t max_bits = 8*sizeof(typename map_type::key_type), size_t m_length = 8>
class keysplit_adapter {

   public:
   static std::size_t constexpr m_interval = ceil_div(max_bits, m_length); //! number of bit lengths assigned to one hash table
   static_assert(max_bits > 0 && m_length > 0, "m_length and max_bits must be positive");

   private:
   map_type* m_maps[m_length];

   public:
   using value_type = typename map_type::value_type;
   using key_type = typename map_type::key_type;
   using iterator = typename map_type::iterator;
   using size_type = typename map_type::size_type;

    //! returns the maximum value of a key that can be stored
   constexpr key_type max_key() const { return std::min<key_type>( (-1ULL)>>(64-max_bits), std::numeric_limits<key_type>::max()); }

   constexpr uint_fast8_t key_bit_width() const { return max_bits; }

   void clear() {
      for(std::size_t i = 0; i < m_length; ++i) {
         m_maps[i]->clear();
      }
   }

   keysplit_adapter() {
      for(std::size_t i = 0; i < m_length; ++i) {
         m_maps[i] = new map_type(std::min<size_t>((i+1)*m_interval,max_bits));
      }
   }
   ~keysplit_adapter() {
      for(std::size_t i = 0; i < m_length; ++i) {
         delete m_maps[i]; 
      }
   }



    //! @see std::unordered_map
    bool empty() const { 
      for(std::size_t i = 0; i < m_length; ++i) {
         if(!m_maps[i].empty()) return false;
      }
      return true;
    } 

    //! @see std::unordered_map
    std::size_t size() const {
       // return m_maps[0]->size();
       return std::accumulate(m_maps, m_maps+m_length, std::size_t(0), [] (std::size_t sum, const map_type* const& v) { return sum+v->size(); });
    }

    const iterator end() const {
        return m_maps[0]->end();
    }

    size_type erase(const key_type& key) {
       const uint_fast8_t key_bit_width = bit_width(key);
       const uint_fast8_t slot = key_bit_width == 0 ? 0 : (key_bit_width-1)/m_interval;
       DCHECK_LT(slot, m_length);
       return m_maps[slot]->erase(key);
    }

    iterator find(const key_type& key) const {
       const uint_fast8_t key_bit_width = bit_width(key);
       const uint_fast8_t slot = key_bit_width == 0 ? 0 : (key_bit_width-1)/m_interval;
       DCHECK_LT(slot, m_length);
       return m_maps[slot]->find(key);
    }

    value_type& operator[](const key_type& key) {
       const uint_fast8_t key_bit_width = bit_width(key);
       const uint_fast8_t slot = key_bit_width == 0 ? 0 : (key_bit_width-1)/m_interval;
       DCHECK_LT(slot, m_length);
       return (*m_maps[slot])[key];
    }

    size_type count(const key_type& key ) const {
       const uint_fast8_t key_bit_width = bit_width(key);
       const uint_fast8_t slot = key_bit_width == 0 ? 0 : (key_bit_width-1)/m_interval;
       DCHECK_LT(slot, m_length);
       return m_maps[slot]->count(key);
    }
};


/**
 * using compact hashing with key ranging to 64-bits do not work when using the xorshift hash function.
 * Instead, we use a different hash table `large_map_type` for storing 64-bit integers.
 */
template<class map_type, class large_map_type, size_t m_length = 8>
class keysplit_adapter64 {
   public:
   using value_type = typename large_map_type::value_type;
   using key_type = typename large_map_type::key_type;

   private:
   static constexpr size_t max_bits = sizeof(key_type) * 8;
   keysplit_adapter<map_type, max_bits-1> m_adapter;

   large_map_type m_large_map;
   static std::size_t constexpr m_interval = max_bits / m_length;
   static_assert(max_bits > m_length, "m_length is smaller than max_bits");

   public:

   class dummy_iterator {
        using pair_type = std::pair<key_type, value_type>;
        pair_type m_pair;
        bool m_end;
      public:
        dummy_iterator(pair_type&& other, bool end = false) : m_pair(std::move(other)), m_end(end) {}

        const pair_type* operator->() const {
            return &m_pair;
        }
        bool operator==(const dummy_iterator o) const {
           if(o.m_end == true && m_end == true) return true;
           if(o.m_end != m_end) return false;
           return m_pair == o.m_pair;
        }
   };


   using size_type = typename large_map_type::size_type;
   using iterator = dummy_iterator;

    //! returns the maximum value of a key that can be stored
   constexpr key_type max_key() const { return std::numeric_limits<key_type>::max(); }
   constexpr uint_fast8_t key_bit_width() const { return max_bits; }

   void clear() {
      m_adapter.clear();
      m_large_map.clear();
   }

   keysplit_adapter64() : m_large_map(max_bits) {
   }
   ~keysplit_adapter64() {
   }

    //! @see std::unordered_map
    bool empty() const { 
      return m_adapter.empty() && m_large_map.empty();
    } 

    //! @see std::unordered_map
    std::size_t size() const {
       return m_large_map.size() + m_adapter.size();
    }

    const iterator end() const {
        return iterator { std::make_pair<key_type, value_type>(0,0), true };
    }

    size_type count(const key_type& key ) const {
       if(bit_width(key) < max_bits-m_adapter.m_interval) {
          return m_adapter.count(key);
       } else {
          return m_large_map.count(key);
       }
    }

    size_type erase(const key_type& key) {
       if(bit_width(key) < max_bits-m_adapter.m_interval) {
          return m_adapter.erase(key);
       } else {
          return m_large_map.erase(key);
       }
    }

    iterator find(const key_type& key) const {
       if(bit_width(key) < max_bits-m_adapter.m_interval) {
          auto it = m_adapter.find(key);
          if(it == m_adapter.end()) return end();
          return dummy_iterator {std::make_pair(it->first, it->second) };
       } else {
          auto it = m_large_map.find(key);
          if(it == m_large_map.end()) return end();
          return dummy_iterator {std::make_pair(it->first, it->second)};
       }
    }

    value_type& operator[](const key_type& key) {
       return (bit_width(key) < max_bits-m_adapter.m_interval) ? m_adapter[key] : m_large_map[key];
    }
};

