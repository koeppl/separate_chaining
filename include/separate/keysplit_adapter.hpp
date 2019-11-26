#pragma once

#include <cstdint>
#include <numeric>
#include <limits>
#include <algorithm>
#include "dcheck.hpp"

namespace separate_chaining {

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
   using navigator = typename map_type::navigator;
   using const_iterator = typename map_type::const_iterator;
   using size_type = typename map_type::size_type;

    //! returns the maximum value of a key that can be stored
   constexpr key_type max_key() const { return std::min<key_type>( (-1ULL)>>(64-max_bits), std::numeric_limits<key_type>::max()); }
   constexpr value_type max_value() const { return std::numeric_limits<value_type>::max(); }

   constexpr uint_fast8_t key_width() const { return max_bits; }

   void shrink_to_fit() {
      for(std::size_t i = 0; i < m_length; ++i) {
         m_maps[i]->shrink_to_fit();
      }
   }

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
    const const_iterator cend() const {
        return m_maps[0]->cend();
    }

    size_type erase(const key_type& key) {
       const uint_fast8_t key_width = bit_width(key);
       const uint_fast8_t slot = key_width == 0 ? 0 : (key_width-1)/m_interval;
       DCHECK_LT(slot, m_length);
       return m_maps[slot]->erase(key);
    }

    const_iterator find(const key_type& key) const {
       const uint_fast8_t key_width = bit_width(key);
       const uint_fast8_t slot = key_width == 0 ? 0 : (key_width-1)/m_interval;
       DCHECK_LT(slot, m_length);
       return m_maps[slot]->find(key);
    }

    navigator operator[](const key_type& key) {
       const uint_fast8_t key_width = bit_width(key);
       const uint_fast8_t slot = key_width == 0 ? 0 : (key_width-1)/m_interval;
       DCHECK_LT(slot, m_length);
       return (*m_maps[slot])[key];
    }

    size_type count(const key_type& key ) const {
       const uint_fast8_t key_width = bit_width(key);
       const uint_fast8_t slot = key_width == 0 ? 0 : (key_width-1)/m_interval;
       DCHECK_LT(slot, m_length);
       return m_maps[slot]->count(key);
    }
};


}//ns separate_chaining

