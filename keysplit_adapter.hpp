#pragma once

#include <cstdint>
#include <numeric>
#include <limits>
#include <algorithm>

/** can be used for keys in [0 .. max-1], where max = std::numeric_limits<uint64_t>::max()
 */
template<class map_type>
class keysplit_adapter {
   static std::size_t constexpr m_length = 16;
   map_type* m_maps[m_length];
   static std::size_t constexpr m_interval = sizeof(uint64_t) * 8 / m_length;

   public:
   using value_type = typename map_type::value_type;
   using key_type = typename map_type::key_type;
   using iterator = typename map_type::iterator;

    //! returns the maximum value of a key that can be stored
   constexpr key_type max_key() const { return std::numeric_limits<key_type>::max(); }

   void clear() {
      for(std::size_t i = 0; i < m_length; ++i) {
         m_maps[i]->clear();
      }
   }

   keysplit_adapter() {
      for(std::size_t i = 0; i < m_length; ++i) {
         m_maps[i] = new map_type(std::min<size_t>((i+1)*m_interval,64));
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
        return iterator { *this, -1ULL, -1ULL };
    }

    iterator find(const key_type& key) const {
       return m_maps[most_significant_bit(key)/m_interval]->find(key);
    }

    value_type& operator[](const key_type& key) {
       return (*m_maps[most_significant_bit(key+1)/m_interval])[key];
    }
};

