#ifndef DCHECK_HPP
#define DCHECK_HPP
#pragma once


#include <string>
#include <sstream>
#include <stdexcept>

#ifndef DCHECK
#ifdef NDEBUG
#define DCHECK_(x,y,z)
#define DCHECK(x) 
#define DCHECK_EQ(x, y) 
#define DCHECK_NE(x, y) 
#define DCHECK_LE(x, y) 
#define DCHECK_LT(x, y) 
#define DCHECK_GE(x, y) 
#define DCHECK_GT(x, y) 
#else//NDEBUG
#define DCHECK_(x,y,z) \
  if (!(x)) throw std::runtime_error(std::string(" in file ") + __FILE__ + ':' + std::to_string(__LINE__) + (" the check failed: " #x) + ", we got " + std::to_string(y) + " vs " + std::to_string(z))
#define DCHECK(x) \
  if (!(x)) throw std::runtime_error(std::string(" in file ") + __FILE__ + ':' + std::to_string(__LINE__) + (" the check failed: " #x))
#define DCHECK_EQ(x, y) DCHECK_((x) == (y), x,y)
#define DCHECK_NE(x, y) DCHECK_((x) != (y), x,y)
#define DCHECK_LE(x, y) DCHECK_((x) <= (y), x,y)
#define DCHECK_LT(x, y) DCHECK_((x) < (y) ,x,y)
#define DCHECK_GE(x, y) DCHECK_((x) >= (y),x,y )
#define DCHECK_GT(x, y) DCHECK_((x) > (y) ,x,y)
#endif //NDEBUG
#endif //DCHECK


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

#endif /* DCHECK_HPP */
