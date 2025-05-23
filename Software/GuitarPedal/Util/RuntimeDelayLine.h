// This is a modified version of delayline.h that makes it easier to use SDRAM
// for the buffer and allows for runtime allocation of the buffer size.

/*
Copyright (c) 2020 Electrosmith, Corp

Use of this source code is governed by an MIT-style
license that can be found in the LICENSE file or at
https://opensource.org/licenses/MIT.
*/

#pragma once
#ifndef DSY_RUNTIME_DELAY_H
#define DSY_RUNTIME_DELAY_H
#include <algorithm>
#include <cstdint>

namespace daisysp_modified {

template <typename T> class DelayLine {
  public:
    DelayLine() : buffer_(nullptr), size_(0), write_ptr_(0), delay_(1), frac_(0.f) {}

    void Init(T *buffer, size_t size) {
        buffer_ = buffer;
        size_ = size;
        Reset();
    }

    void Reset() {
        for (size_t i = 0; i < size_; ++i)
            buffer_[i] = T(0);
        write_ptr_ = 0;
        delay_ = 1;
        frac_ = 0.f;
    }

    void SetDelay(size_t delay) {
        delay_ = (delay < size_) ? delay : size_ - 1;
        frac_ = 0.f;
    }

    void SetDelay(float delay) {
        int32_t int_delay = static_cast<int32_t>(delay);
        frac_ = delay - static_cast<float>(int_delay);
        size_t safe_delay = static_cast<size_t>(std::max(int_delay, int32_t(0)));
        delay_ = (safe_delay < size_) ? safe_delay : size_ - 1;
    }

    void Write(const T sample) {
        buffer_[write_ptr_] = sample;
        write_ptr_ = (write_ptr_ - 1 + size_) % size_;
    }

    T Read() const {
        T a = buffer_[(write_ptr_ + delay_) % size_];
        T b = buffer_[(write_ptr_ + delay_ + 1) % size_];
        return a + (b - a) * frac_;
    }

  private:
    T *buffer_;
    size_t size_;
    size_t write_ptr_;
    size_t delay_;
    float frac_;
};

} // namespace daisysp_modified

#endif