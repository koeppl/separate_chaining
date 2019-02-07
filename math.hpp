#pragma once

namespace separate_chaining {

//! returns the most significant bit counting at 0
constexpr int most_significant_bit(const uint64_t& x) {
    return x == 0 ? -1 : (sizeof(uint64_t)*8-1) - __builtin_clzll(x);
}


//! returns the number of bits needed to represent `x`
inline constexpr uint_fast8_t bit_width(size_t x) {
   return x == 0 ? 1U : 64 - __builtin_clzll(x);
}


//! computes ceil(dividend/divisor)
template<class T>
constexpr T ceil_div(const T& dividend, const T& divisor) { 
   return (dividend + divisor - 1) / divisor;
}


}//ns separate_chaining
