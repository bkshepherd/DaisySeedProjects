#pragma once
#ifndef XORSHIFT32_H
#define XORSHIFT32_H

#include <stdint.h>

struct XorShift32 {
    uint32_t state = 0x12345678u;

    inline uint32_t nextU32() {
        uint32_t x = state;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        state = x;
        return x;
    }

    inline float randSigned(float lower, float upper) {
        const float t = (nextU32() >> 8) * (1.0f / 16777216.0f);
        return lower + (upper - lower) * t;
    }
};

#endif
