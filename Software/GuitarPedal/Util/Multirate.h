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

#include <array>
#include <span>

#include <q/utility/ring_buffer.hpp>

constexpr size_t resample_factor = 6;
// constexpr size_t resample_factor = 1; // KAB Note: redefining as 1 to get around DaisySeedProjects effects being set up to process
// every sample, not every block
// Not sure what this will do TODO
//=============================================================================
class Decimator2 {
  public:
    float operator()(std::span<const float, resample_factor> s) {
        buffer1.push(s[0]);
        buffer1.push(s[1]);
        buffer1.push(s[2]);
        buffer2.push(filter1());

        buffer1.push(s[3]);
        buffer1.push(s[4]);
        buffer1.push(s[5]);
        buffer2.push(filter1());

        return filter2();
    }

  private:
    float filter1() {
        // 48000 Hz sample rate
        // 0-1800 Hz pass band (3 dB ripple)
        // 8000-24000 Hz stop band (-80 dB)
        return 0.000066177472224418f * (buffer1[offset1 + 0] + buffer1[offset1 + 20]) +
               0.0009613901552378511f * (buffer1[offset1 + 1] + buffer1[offset1 + 19]) +
               0.003835090815380887f * (buffer1[offset1 + 2] + buffer1[offset1 + 18]) +
               0.010496532623165526f * (buffer1[offset1 + 3] + buffer1[offset1 + 17]) +
               0.02272703591356282f * (buffer1[offset1 + 4] + buffer1[offset1 + 16]) +
               0.041464390530886956f * (buffer1[offset1 + 5] + buffer1[offset1 + 15]) +
               0.06591039391505207f * (buffer1[offset1 + 6] + buffer1[offset1 + 14]) +
               0.09309984953947406f * (buffer1[offset1 + 7] + buffer1[offset1 + 13]) +
               0.11829177835273737f * (buffer1[offset1 + 8] + buffer1[offset1 + 12]) +
               0.13620590247679107f * (buffer1[offset1 + 9] + buffer1[offset1 + 11]) + 0.14270010010002276f * buffer1[offset1 + 10];
    }

    float filter2() {
        // Half-band filter
        // 16000 Hz sample rate
        // 0-1800 Hz pass band
        return -0.00299995f * (buffer2[offset2 + 0] + buffer2[offset2 + 14]) +
               0.01858487f * (buffer2[offset2 + 2] + buffer2[offset2 + 12]) +
               -0.06984829f * (buffer2[offset2 + 4] + buffer2[offset2 + 10]) +
               0.30421664f * (buffer2[offset2 + 6] + buffer2[offset2 + 8]) + 0.5f * buffer2[offset2 + 7];
    }

    static constexpr std::size_t bsize1 = 32;
    static constexpr std::size_t fsize1 = 21;
    static constexpr std::size_t offset1 = bsize1 - fsize1;

    static constexpr std::size_t bsize2 = 16;
    static constexpr std::size_t fsize2 = 15;
    static constexpr std::size_t offset2 = bsize2 - fsize2;

    cycfi::q::ring_buffer<float> buffer1{bsize1};
    cycfi::q::ring_buffer<float> buffer2{bsize2};
};

//=============================================================================
class Interpolator {
  public:
    std::array<float, resample_factor> operator()(float s) {
        std::array<float, resample_factor> output;

        buffer1.push(s);

        buffer2.push(filter1a());
        output[0] = filter2a();
        output[1] = filter2b();
        output[2] = filter2c();

        buffer2.push(filter1b());
        output[3] = filter2a();
        output[4] = filter2b();
        output[5] = filter2c();

        return output;
    }

  private:
    // Filter 1
    // 16000 Hz sample rate
    // 0-3600 Hz pass band (3 dB ripple)
    // 4400-8000 Hz stop band (-80 dB)
    // Gain=2 in passband

    float filter1a() {
        return -0.0028536199247471473f * (buffer1[offset1 + 0] + buffer1[offset1 + 24]) +
               -0.040326725115203695f * (buffer1[offset1 + 1] + buffer1[offset1 + 23]) +
               -0.036134596458820015f * (buffer1[offset1 + 2] + buffer1[offset1 + 22]) +
               0.033522051189265496f * (buffer1[offset1 + 3] + buffer1[offset1 + 21]) +
               -0.031442224275585025f * (buffer1[offset1 + 4] + buffer1[offset1 + 20]) +
               0.03258337681750486f * (buffer1[offset1 + 5] + buffer1[offset1 + 19]) +
               -0.03538414864961937f * (buffer1[offset1 + 6] + buffer1[offset1 + 18]) +
               0.038811868988079715f * (buffer1[offset1 + 7] + buffer1[offset1 + 17]) +
               -0.042204493894155204f * (buffer1[offset1 + 8] + buffer1[offset1 + 16]) +
               0.045128824129776035f * (buffer1[offset1 + 9] + buffer1[offset1 + 15]) +
               -0.04736995557907843f * (buffer1[offset1 + 10] + buffer1[offset1 + 14]) +
               0.048831901671617876f * (buffer1[offset1 + 11] + buffer1[offset1 + 13]) + 0.9507771467941135f * buffer1[offset1 + 12];
    }

    float filter1b() {
        return -0.015961858776449508f * (buffer1[offset1 + 0] + buffer1[offset1 + 23]) +
               -0.056128740058266235f * (buffer1[offset1 + 1] + buffer1[offset1 + 22]) +
               0.011026026040094625f * (buffer1[offset1 + 2] + buffer1[offset1 + 21]) +
               0.003198795994721635f * (buffer1[offset1 + 3] + buffer1[offset1 + 20]) +
               -0.01108582057161854f * (buffer1[offset1 + 4] + buffer1[offset1 + 19]) +
               0.01951384497860086f * (buffer1[offset1 + 5] + buffer1[offset1 + 18]) +
               -0.030860282826182514f * (buffer1[offset1 + 6] + buffer1[offset1 + 17]) +
               0.04707993944078406f * (buffer1[offset1 + 7] + buffer1[offset1 + 16]) +
               -0.07155908583004919f * (buffer1[offset1 + 8] + buffer1[offset1 + 15]) +
               0.1129220770668398f * (buffer1[offset1 + 9] + buffer1[offset1 + 14]) +
               -0.2033122562119347f * (buffer1[offset1 + 10] + buffer1[offset1 + 13]) +
               0.6336728217960803f * (buffer1[offset1 + 11] + buffer1[offset1 + 12]);
    }

    // Filter 2
    // 48000 Hz sample rate
    // 0-3600 Hz pass band (3 dB ripple)
    // 8000-24000 Hz stop band (-80 dB)
    // Gain=3 in passband

    float filter2a() {
        return 0.00036440608905813593f * buffer2[offset2 + 0] + 0.0005821260464558225f * buffer2[offset2 + 1] +
               -0.043244023722481956f * buffer2[offset2 + 2] + -0.10310036386076359f * buffer2[offset2 + 3] +
               0.13604229993913602f * buffer2[offset2 + 4] + 0.5503466630244301f * buffer2[offset2 + 5] +
               0.4407091552750118f * buffer2[offset2 + 6] + 0.009420000864297772f * buffer2[offset2 + 7] +
               -0.09801301258361905f * buffer2[offset2 + 8] + -0.019627176246818184f * buffer2[offset2 + 9] +
               0.001762424830497545f * buffer2[offset2 + 10];
    }

    float filter2b() {
        return 0.001112114188613258f * (buffer2[offset2 + 0] + buffer2[offset2 + 10]) +
               -0.005449383064836152f * (buffer2[offset2 + 1] + buffer2[offset2 + 9]) +
               -0.07276547446584428f * (buffer2[offset2 + 2] + buffer2[offset2 + 8]) +
               -0.0709695783332148f * (buffer2[offset2 + 3] + buffer2[offset2 + 7]) +
               0.2904591843823435f * (buffer2[offset2 + 4] + buffer2[offset2 + 6]) + 0.590541634315722f * buffer2[offset2 + 5];
    }

    float filter2c() {
        return 0.001762424830497545f * buffer2[offset2 + 0] + -0.019627176246818184f * buffer2[offset2 + 1] +
               -0.09801301258361905f * buffer2[offset2 + 2] + 0.009420000864297772f * buffer2[offset2 + 3] +
               0.4407091552750118f * buffer2[offset2 + 4] + 0.5503466630244301f * buffer2[offset2 + 5] +
               0.13604229993913602f * buffer2[offset2 + 6] + -0.10310036386076359f * buffer2[offset2 + 7] +
               -0.043244023722481956f * buffer2[offset2 + 8] + 0.0005821260464558225f * buffer2[offset2 + 9] +
               0.00036440608905813593f * buffer2[offset2 + 10];
    }

    static constexpr std::size_t bsize1 = 32;
    static constexpr std::size_t fsize1 = 25;
    static constexpr std::size_t offset1 = bsize1 - fsize1;

    static constexpr std::size_t bsize2 = 16;
    static constexpr std::size_t fsize2 = 11;
    static constexpr std::size_t offset2 = bsize2 - fsize2;

    cycfi::q::ring_buffer<float> buffer1{bsize1};
    cycfi::q::ring_buffer<float> buffer2{bsize2};
};
