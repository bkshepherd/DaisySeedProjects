#pragma once

#include <cstdint>

static constexpr float fastInvSqrt(float number) noexcept {
    union {
        float f;
        uint32_t i;
    } conv = {.f = number};
    conv.i = 0x5f3759df - (conv.i >> 1);
    conv.f *= 1.5F - (number * 0.5F * conv.f * conv.f);
    return conv.f;
}

static constexpr float fastSqrt(float x) { return x * fastInvSqrt(x); }
