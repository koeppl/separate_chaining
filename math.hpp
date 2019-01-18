#pragma once

//! returns the most significant bit
constexpr int most_significant_bit(const uint64_t& x) {
    return x == 0 ? -1 : (sizeof(uint64_t)*8-1) - __builtin_clzll(x);
}


//! computes ceil(dividend/divisor)
template<class T>
constexpr T ceil_div(const T& dividend, const T& divisor) { 
   return (dividend + divisor - 1) / divisor;
}


