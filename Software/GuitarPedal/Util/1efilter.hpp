#pragma once
#ifndef ONE_E_FILTER_H
#define ONE_E_FILTER_H
/*(utf8)
1€ Filter, template-compliant version

Copyright (c) 2014-2020 Jonathan Aceituno <join@oin.name>

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

25/04/14: fixed bug with last_time_ never updated on line 40
07/12/20: added MIT license
03/08/23: fixed implementation

For details, see https://gery.casiez.net/1euro/
*/

#include <cmath>

template <typename T = double> struct low_pass_filter {
    low_pass_filter() : hatxprev(0), xprev(0), hadprev(false) {}
    T operator()(T x, T alpha) {
        T hatx;
        if (hadprev) {
            hatx = alpha * x + (1 - alpha) * hatxprev;
        } else {
            hatx = x;
            hadprev = true;
        }
        hatxprev = hatx;
        xprev = x;
        return hatx;
    }
    T hatxprev;
    T xprev;
    bool hadprev;
};

template <typename T = double, typename timestamp_t = double> struct one_euro_filter {
    one_euro_filter(double _freq, T _mincutoff, T _beta, T _dcutoff)
        : freq(_freq), mincutoff(_mincutoff), beta(_beta), dcutoff(_dcutoff), last_time_(-1) {}

    T operator()(T x, timestamp_t t = -1) {
        T dx = 0;

        if (last_time_ != -1 && t != -1 && t != last_time_ && t > last_time_) {
            freq = 1.0 / (t - last_time_);
        }
        last_time_ = t;

        if (xfilt_.hadprev)
            dx = (x - xfilt_.hatxprev) * freq;

        T edx = dxfilt_(dx, alpha(dcutoff));
        T cutoff = mincutoff + beta * std::abs(static_cast<double>(edx));
        return xfilt_(x, alpha(cutoff));
    }

    double freq;
    T mincutoff, beta, dcutoff;

  private:
    T alpha(T cutoff) {
        T tau = 1.0 / (2 * M_PI * cutoff);
        T te = 1.0 / freq;
        return 1.0 / (1.0 + tau / te);
    }

    timestamp_t last_time_;
    low_pass_filter<T> xfilt_, dxfilt_;
};

#endif