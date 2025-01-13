#pragma once

#include <algorithm>
#include <cmath>

#include <gcem.hpp>
#include <q/detail/fast_math.hpp>

class LinearMapping
{
public:
    // min < max
    constexpr LinearMapping(float min, float max) :
        _min(min),
        _max(max)
    {
    }

    // 0 <= x <= 1
    float operator()(float x) const
    {
        return std::lerp(_min, _max, x);
    }

private:
    const float _min;
    const float _max;
};

class LogMapping
{
public:
    // min < center < max
    // center < (min + max) / 2
    // min + center > 0
    constexpr LogMapping(float min, float center, float max) :
        _offset(calculateOffset(min, center, max)),
        _log_min(gcem::log(min + _offset)),
        _log_range(gcem::log(max + _offset) - _log_min)
    {
    }

    // 0 < min < max
    constexpr LogMapping(float min, float max) :
        _offset(0),
        _log_min(gcem::log(min)),
        _log_range(gcem::log(max) - _log_min)
    {
    }

    // 0 <= x <= 1
    float operator()(float x) const
    {
        return fasterexp(_log_range*x + _log_min) - _offset;
    }

private:
    static constexpr float calculateOffset(float min, float center, float max)
    {
        return (center*center - min*max) / ((min+max) - 2*center);
    }

    const float _offset;
    const float _log_min;
    const float _log_range;
};
