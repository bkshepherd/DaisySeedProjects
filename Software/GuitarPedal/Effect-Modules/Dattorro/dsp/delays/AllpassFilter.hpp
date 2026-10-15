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
#include "InterpDelay.hpp"

class AllpassFilter {
  public:
    AllpassFilter() {
        // clear();
        gain = 0.;
    }

    AllpassFilter(int maxDelay, int initDelay = 0, float gain = 0.) {
        // clear();
        delay = InterpDelay(maxDelay, initDelay);
        this->gain = gain;
    }

    // inline void initializeAllPassFilter(const int &maxDelay, const float &initDelay = 0, const float &gain = 0.) {
    //     clear();
    //     // delay = InterpDelay(maxDelay, initDelay);
    //     delay.initializeDelay(maxDelay, initDelay);
    //     this->gain = gain;
    // }

#pragma GCC push_options
#pragma GCC optimize("Ofast")

    inline float process() {
        _inSum = input + delay.output * gain;
        output = delay.output + _inSum * gain * -1.;
        delay.input = _inSum;
        delay.process();
        return output;
    }

#pragma GCC pop_options

    void clear() {
        input = 0.;
        output = 0.;
        _inSum = 0.;
        _outSum = 0.;
        delay.clear();
    }

    inline void setGain(const float &newGain) { gain = newGain; }

    float input;
    float output;
    InterpDelay delay;

  private:
    float gain;
    float _inSum;
    float _outSum;
};
