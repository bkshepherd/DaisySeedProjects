/*
https://github.com/schult/terrarium-poly-octave

MIT License

Copyright (c) 2024 Steven Schulteis

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#pragma once

#include <algorithm>
#include <cmath>

#include <gcem.hpp>
#include <q/detail/fast_math.hpp>

class LinearMapping {
  public:
    // min < max
    constexpr LinearMapping(float min, float max) : _min(min), _max(max) {}

    // 0 <= x <= 1
    float operator()(float x) const { return std::lerp(_min, _max, x); }

  private:
    const float _min;
    const float _max;
};

class LogMapping {
  public:
    // min < center < max
    // center < (min + max) / 2
    // min + center > 0
    constexpr LogMapping(float min, float center, float max)
        : _offset(calculateOffset(min, center, max)), _log_min(gcem::log(min + _offset)),
          _log_range(gcem::log(max + _offset) - _log_min) {}

    // 0 < min < max
    constexpr LogMapping(float min, float max) : _offset(0), _log_min(gcem::log(min)), _log_range(gcem::log(max) - _log_min) {}

    // 0 <= x <= 1
    float operator()(float x) const { return fasterexp(_log_range * x + _log_min) - _offset; }

  private:
    static constexpr float calculateOffset(float min, float center, float max) {
        return (center * center - min * max) / ((min + max) - 2 * center);
    }

    const float _offset;
    const float _log_min;
    const float _log_range;
};
