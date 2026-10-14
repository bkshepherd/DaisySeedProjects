// delayline_reverse.h
//
// Reverse delay line from the Neptune pedal of the FunBox project (https://github.com/GuitarML/Funbox, MIT, Copyright (c) 2024 Keith Bloemer), which has it from the
// Veno-Echo pedal (https://github.com/AdamFulford/Veno-Echo, MIT):
//
// Copyright 2021 Adam Fulford
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the
// Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
// Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
// PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
// ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
// How it works: the line is written forwards while two read pointers run backwards through it; when the pointer that is playing gets near the write position the other
// one is jumped to just behind it and the two are cross-faded, so the recorded audio comes out in reverse in segments as long as the delay time.
// Changes from the original: the samples are in a buffer that is passed to Init() (so the object can live in fast on-chip RAM while the samples are in SDRAM), the
// unused ReadFwd() and SetSpeed() are left out, the delay time is kept at 2400 samples or more (at the pedal it never went below that once the glide to the target had started), and the jump to
// "just behind the write position" wraps around (at the pedal, with the write pointer at 0, it read outside the buffer).
#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

template <typename T, size_t max_size> class DelayLineReverse {
  public:
    /** Takes `samples` (max_size values) and clears them. */
    void Init(T *samples) {
        line_ = samples;
        Reset();
    }

    void Reset() {
        delay1_ = 2400; // the shortest reverse delay time
        fadetime = 2300; // in samples
        std::memset(line_, 0, max_size * sizeof(T));
        write_ptr_ = 0;
        read_ptr1_ = 0;
        read_ptr2_ = 0;
        headDiff_ = 0;
        playinghead_ = false;
        fadepos_ = 0.0f;
        fading_ = false;
    }

    /** Sets the delay time in samples (at least 2400 and less than max_size). */
    inline void SetDelay1(float delay) {
        const int32_t int_delay = static_cast<int32_t>(delay);
        delay1_ = static_cast<size_t>(int_delay) < max_size ? int_delay : max_size - 1;
        if (delay1_ < 2400) delay1_ = 2400;
    }

    /** Writes a sample and advances the pointers. */
    inline void Write(const T sample) {
        line_[write_ptr_] = sample;
        write_ptr_ = (write_ptr_ + 1) % max_size;         // forwards
        headDiff_ = (headDiff_ + 1 + delay1_) % delay1_;
        read_ptr1_ = (read_ptr1_ - 1 + max_size) % max_size; // the read pointers run backwards
        read_ptr2_ = (read_ptr2_ - 1 + max_size) % max_size;

        if (headDiff_ > (delay1_ - fadetime - 1)) { // start of the cross-fade region
            if (!fading_) {
                fading_ = true;
                if (!playinghead_) {
                    read_ptr2_ = (write_ptr_ + max_size - 1) % max_size; // jump pointer 2 to just behind the write position
                } else {
                    read_ptr1_ = (write_ptr_ + max_size - 1) % max_size;
                }
            }
        }

        if (fading_) {
            if (!playinghead_) { // head 1 is playing
                fadepos_ = fadepos_ + (1.0f / fadetime);
                if (fadepos_ > 1.0f) {
                    fadepos_ = 1.0f;
                    fading_ = false;
                    playinghead_ = true;
                }
            } else { // head 2 is playing
                fadepos_ = fadepos_ - (1.0f / fadetime);
                if (fadepos_ < 0.0f) {
                    fadepos_ = 0.0f;
                    fading_ = false;
                    playinghead_ = false;
                }
            }
        }
    }

    /** The next sample of the reversed audio: the two read pointers, cross-faded. */
    inline T ReadRev() const {
        const float read1 = line_[read_ptr1_];
        const float read2 = line_[read_ptr2_];
        const float scalar_1 = sinf(fadepos_ * ((float)M_PI * 0.5f));
        const float scalar_2 = sinf((1.0f - fadepos_) * ((float)M_PI * 0.5f));
        return (read2 * scalar_1) + (read1 * scalar_2);
    }

  private:
    T *line_ = nullptr;
    size_t write_ptr_ = 0;
    size_t read_ptr1_ = 0;
    size_t read_ptr2_ = 0;
    size_t delay1_ = 2400;
    size_t headDiff_ = 0;
    size_t fadetime = 2300;
    bool playinghead_ = false;
    float fadepos_ = 0.0f;
    bool fading_ = false;
};
