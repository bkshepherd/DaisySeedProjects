#pragma once

#include <bit>
#include <cstdint>
#include <limits>

// https://en.wikipedia.org/wiki/Fast_inverse_square_root
static constexpr float fastInvSqrt(float number) noexcept
{
    //static_assert(std::numeric_limits<float>::is_iec559);
    //float const y = std::bit_cast<float>(
    //        0x5F1FFFF9 - (std::bit_cast<std::uint32_t>(x) >> 1));
    //return y * (0.703952253f * (2.38924456f - (x * y * y)));


    // Trouble using bit_cast, not a member of std error even when defining c++20 in makefile, using this found at:
    //  https://www.geeksforgeeks.org/fast-inverse-square-root/
    // function to find the inverse square root 

    const float threehalfs = 1.5F; 
 
    float x2 = number * 0.5F; 
    float y = number; 
  
    // evil floating point bit level hacking 
    long i = * ( long * ) &y; 
  
    // value is pre-assumed 
    i = 0x5f3759df - ( i >> 1 ); 
    y = * ( float * ) &i; 
  
    // 1st iteration 
    y = y * ( threehalfs - ( x2 * y * y ) ); 
  
    // 2nd iteration, this can be removed 
    // y = y * ( threehalfs - ( x2 * y * y ) ); 
 
    return y; 
 
    //return 1.0/sqrt(x); // work around for compile error using bit_cast
}




static constexpr float fastSqrt(float x)
{
    return fastInvSqrt(x) * x;
}
