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

#ifndef DSJ_LFO_HPP
#define DSJ_LFO_HPP
#include <cmath>
#include <cstdint>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class TriSawLFO {
  public:
    TriSawLFO(float sampleRate = 32000.0, float frequency = 1.0) {
        phase = 0.0;
        _output = 0.0;
        _sampleRate = sampleRate;
        _1_sampleRate = 1 / sampleRate;
        _step = 0.0;
        _rising = true;
        setFrequency(frequency);
        setRevPoint(0.5);
    }

    inline float process() {
        if (_step > 1.0) {
            _step -= 1.0;
            _rising = true;
        }

        if (_step >= _revPoint) {
            _rising = false;
        }

        if (_rising) {
            _output = _step * _riseRate;
        } else {
            _output = _step * _fallRate - _fallRate;
        }

        _step += _stepSize;
        _output *= 2.0;
        _output -= 1.0;
        return _output;
    }

    inline void setFrequency(const float &frequency) {
        if (frequency == _frequency) {
            return;
        }
        _frequency = frequency;
        calcStepSize();
    }

    inline void setRevPoint(const float &revPoint) {
        _revPoint = revPoint;
        if (_revPoint < 0.0001) {
            _revPoint = 0.0001;
        }
        if (_revPoint > 0.999) {
            _revPoint = 0.999;
        }

        _riseRate = 1.0 / _revPoint;
        _fallRate = -1.0 / (1.0 - _revPoint);
    }

    void setSamplerate(float sampleRate) {
        _sampleRate = sampleRate;
        _1_sampleRate = 1 / sampleRate;
        calcStepSize();
    }

    inline float getOutput() const { return _output; }

    float phase;

  private:
    float _output;
    float _sampleRate;
    float _1_sampleRate;
    float _frequency = 0.0;
    float _revPoint;
    float _riseRate;
    float _fallRate;
    float _step;
    float _stepSize;
    bool _rising;

    inline void calcStepSize() {
        // _stepSize = _frequency / _sampleRate;
        _stepSize = _frequency * _1_sampleRate;
    }
};

#endif
