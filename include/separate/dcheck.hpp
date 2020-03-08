// #ifndef DCHECK_HPP
// #define DCHECK_HPP
#pragma once


#include <string>
#include <sstream>
#include <stdexcept>


#ifdef NDEBUG
#define ON_DEBUG(x)
#else
#define ON_DEBUG(x) x
#endif

#ifdef DDCHECK
#undef DDCHECK
#undef DDCHECK_EQ
#undef DDCHECK_NE
#undef DDCHECK_LE
#undef DDCHECK_LT
#undef DDCHECK_GE
#undef DDCHECK_GT
#endif

#ifndef DDCHECK
#ifdef NDEBUG
#define DDCHECK_(x,y,z)
#define DDCHECK(x) 
#define DDCHECK_EQ(x, y) 
#define DDCHECK_NE(x, y) 
#define DDCHECK_LE(x, y) 
#define DDCHECK_LT(x, y) 
#define DDCHECK_GE(x, y) 
#define DDCHECK_GT(x, y) 
#else//NDEBUG
#if defined(DCHECK)
#define DDCHECK(x) DCHECK(x)
#define DDCHECK_EQ(x, y) DCHECK_EQ(x, y) 
#define DDCHECK_NE(x, y) DCHECK_NE(x, y) 
#define DDCHECK_LE(x, y) DCHECK_LE(x, y) 
#define DDCHECK_LT(x, y) DCHECK_LT(x, y) 
#define DDCHECK_GE(x, y) DCHECK_GE(x, y) 
#define DDCHECK_GT(x, y) DCHECK_GT(x, y) 
#else//defined(DCHECK)
#define DDCHECK_(x,y,z) \
  if (!(x)) throw std::runtime_error(std::string(" in file ") + __FILE__ + ':' + std::to_string(__LINE__) + (" the check failed: " #x) + ", we got " + std::to_string(y) + " vs " + std::to_string(z))
#define DDCHECK(x) \
  if (!(x)) throw std::runtime_error(std::string(" in file ") + __FILE__ + ':' + std::to_string(__LINE__) + (" the check failed: " #x))
#define DDCHECK_EQ(x, y) DDCHECK_((x) == (y), x,y)
#define DDCHECK_NE(x, y) DDCHECK_((x) != (y), x,y)
#define DDCHECK_LE(x, y) DDCHECK_((x) <= (y), x,y)
#define DDCHECK_LT(x, y) DDCHECK_((x) < (y) ,x,y)
#define DDCHECK_GE(x, y) DDCHECK_((x) >= (y),x,y )
#define DDCHECK_GT(x, y) DDCHECK_((x) > (y) ,x,y)
#endif//defined(DCHECK)
#endif //NDEBUG
#endif //DDCHECK


// namespace std {
// std::string to_string(const std::string& str) { return str; }
// }
//
// template<class T>
// std::string vec_to_debug_string(const T& s) {
//     std::stringstream ss;
//     ss << "[";
//     if (s.size() > 0) {
//         for (size_t i = 0; i < s.size() - 1; i++) {
//             // crazy cast needed to bring negative char values
//             // into their representation as a unsigned one
//             //ss << uint((unsigned char) s[i]) << ", ";
//             ss << s[i] << ", ";
//         }
//         ss << s[s.size() - 1];
//     }
//     ss << "]";
//     return ss.str();
// }

// #endif /* DCHECK_HPP */
