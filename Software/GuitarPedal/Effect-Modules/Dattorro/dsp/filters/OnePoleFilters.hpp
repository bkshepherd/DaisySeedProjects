// SPDX-License-Identifier: GPL-3.0-or-later
//
// Part of a Dattorro plate reverb port. Original algorithm and structure:
// ValleyRackFree / Plateau (PlateauNEVersio)
// Copyright (C) 2020, Valley Audio Soft, Dale Johnson
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//
// See Effect-Modules/Dattorro/README.md for the full provenance chain.

#pragma once
#include <cmath>
#include <cstdint>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define _1_FACT_2 0.5
#define _1_FACT_3 0.1666666667
#define _1_FACT_4 0.04166666667
#define _1_FACT_5 0.008333333333
#define _2M_PI 2.0 * M_PI

class OnePoleLPFilter {
  public:
    // OnePoleLPFilter(float cutoffFreq = 22049.0, float initSampleRate = 32000.0);
    // OnePoleLPFilter(float cutoffFreq = 14080.0, float initSampleRate = 32000.0);
    OnePoleLPFilter(float cutoffFreq = 22049.0, float initSampleRate = 32000.0) {
        setSampleRate(initSampleRate);
        setCutoffFreq(cutoffFreq);
    }

#pragma GCC push_options
#pragma GCC optimize("Ofast")

    inline float process() {
        _z = _a * input + _z * _b;
        output = _z;
        return output;
    }

#pragma GCC pop_options

    void clear() {
        input = 0.0;
        _z = 0.0;
        output = 0.0;
    }

    void setSampleRate(float sampleRate) {
        _sampleRate = sampleRate;
        _1_sampleRate_pi = (1.0 / sampleRate) * -_2M_PI;
        _maxCutoffFreq = sampleRate / 2.0 - 1.0;
        setCutoffFreq(_cutoffFreq);
    }

#pragma GCC push_options
#pragma GCC optimize("Ofast")

    // inline void setCutoffFreq(const float &cutoffFreq) {
    //     // if (cutoffFreq == _cutoffFreq) {
    //     //     return;
    //     // }

    //     _cutoffFreq = cutoffFreq;
    //     // _b = expf(-_2M_PI * _cutoffFreq * _1_sampleRate);
    //     // Using a linear equation instead of exponential because
    //     // it uses less operations. Not sure if it will make any
    //     // difference. This holds for the expected {-1 < x < 0}
    //     // _b = (0.361 * _cutoffFreq) + 1.;
    //     _b = _cutoffFreq + 1.;
    //     // _b = (0.361 * -_cutoffFreq * _1_sampleRate_pi) + 1;
    //     _a = 1.0 - _b;
    // }

    inline void setCutoffFreq(float cutoffFreq) {
        if (cutoffFreq == _cutoffFreq) {
            return;
        }

        _cutoffFreq = cutoffFreq;
        _b = expf(_cutoffFreq * _1_sampleRate_pi);
        _a = 1.0 - _b;
    }

#pragma GCC pop_options

    float input = 0.0;
    float output = 0.0;

  private:
    float _sampleRate = 32000.0;
    float _1_sampleRate_pi = (1.0 / _sampleRate) * -_2M_PI;
    float _cutoffFreq = 0.0;
    float _maxCutoffFreq = _sampleRate / 2.0;
    float _a = 0.0;
    float _b = 0.0;
    float _z = 0.0;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

class OnePoleHPFilter {
  public:
    // OnePoleHPFilter(float initCutoffFreq = 10.0, float initSampleRate = 32000.0);
    OnePoleHPFilter(float initCutoffFreq = 10.0, float initSampleRate = 32000.0) {
        setSampleRate(initSampleRate);
        setCutoffFreq(initCutoffFreq);
        clear();
    }

#pragma GCC push_options
#pragma GCC optimize("Ofast")

    inline float process() {
        _x0 = input;
        _y0 = _a0 * _x0 + _a1 * _x1 + _b1 * _y1;
        _y1 = _y0;
        _x1 = _x0;
        output = _y0;
        return _y0;
    }

#pragma GCC pop_options

    void clear() {
        input = 0.0;
        output = 0.0;
        _x0 = 0.0;
        _x1 = 0.0;
        _y0 = 0.0;
        _y1 = 0.0;
    }

#pragma GCC push_options
#pragma GCC optimize("Ofast")

    // inline void setCutoffFreq(const float &cutoffFreq) {
    //     // if (cutoffFreq == _cutoffFreq) {
    //     //     return;
    //     // }

    //     _cutoffFreq = cutoffFreq;
    //     // See above.
    //     _b1 = _cutoffFreq + 1.;
    //     // _b1 = (0.361 * _cutoffFreq) + 1.;
    //     // _b1 = (0.361 * _cutoffFreq * _1_sampleRate_pi) + 1;
    //     // _b1 = expf(-_2M_PI * _cutoffFreq * _1_sampleRate);
    //     // _a0 = (1.0 + _b1) / 2.0;
    //     _a0 = (1.0 + _b1) * 0.5;
    //     _a1 = -_a0;
    // }

    void setCutoffFreq(float cutoffFreq) {
        if (cutoffFreq == _cutoffFreq) {
            return;
        }

        _cutoffFreq = cutoffFreq;
        _b1 = expf(_cutoffFreq * _1_sampleRate_pi);
        _a0 = (1.0 + _b1) / 2.0;
        _a1 = -_a0;
    }

#pragma GCC pop_options

    void setSampleRate(float sampleRate) {
        _sampleRate = sampleRate;
        _1_sampleRate_pi = (1.0 / sampleRate) * -_2M_PI;
        _maxCutoffFreq = sampleRate / 2.0 - 1.0;
        setCutoffFreq(_cutoffFreq);
        clear();
    }

    float input = 0.0;
    float output = 0.0;

  private:
    float _sampleRate = 32000.0;
    float _1_sampleRate_pi = (1.0 / _sampleRate) * -_2M_PI;
    float _cutoffFreq = 0.0;
    float _maxCutoffFreq = _sampleRate / 2.0 - 1.0;
    float _y0 = 0.0;
    float _y1 = 0.0;
    float _x0 = 0.0;
    float _x1 = 0.0;
    float _a0 = 0.0;
    float _a1 = 0.0;
    float _b1 = 0.0;
};
