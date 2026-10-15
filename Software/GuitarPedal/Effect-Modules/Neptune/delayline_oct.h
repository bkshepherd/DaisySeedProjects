// delayline_oct.h
//
// The delay line of the Neptune pedal of the FunBox project (https://github.com/GuitarML/Funbox, MIT, Copyright (c) 2024 Keith Bloemer), which is DaisySP's
// DelayLine (https://github.com/electro-smith/DaisySP, MIT, by shensley) with an octave mode: reading with the pointer moving twice as fast as the write pointer.
// Changes: the samples are in a buffer that is passed to Init() instead of a member (so the object, with its bookkeeping, can live in fast on-chip RAM while the
// samples are in SDRAM: see Saturn/README.md for why that matters), and the second tap, which the pedal never switches on, is left out.
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

template <typename T, size_t max_size> class DelayLineOct {
  public:
    /** Takes `samples` (max_size values) and clears them, sets the delay to 1 sample. */
    void Init(T *samples) {
        line_ = samples;
        Reset();
    }

    void Reset() {
        std::memset(line_, 0, max_size * sizeof(T));
        write_ptr_ = 0;
        delay_ = 1;
        frac_ = 0.0f;
        speed_ = 1;
    }

    /** Octave: the read pointer moves twice as fast as the write pointer. */
    void setOctave(bool isOctave) { speed_ = isOctave ? 2 : 1; }

    /** Sets the delay time in samples, with a fractional part for interpolating. */
    inline void SetDelay(float delay) {
        const int32_t int_delay = static_cast<int32_t>(delay);
        frac_ = delay - static_cast<float>(int_delay);
        delay_ = static_cast<size_t>(int_delay) < max_size ? int_delay : max_size - 1;
    }

    /** Writes a sample and advances the write pointer. */
    inline void Write(const T sample) {
        line_[write_ptr_] = sample;
        write_ptr_ = (write_ptr_ - 1 + max_size) % max_size;
    }

    /** The next sample, interpolated. */
    inline T Read() const {
        const T a = line_[(write_ptr_ * speed_ + delay_) % max_size];
        const T b = line_[(write_ptr_ * speed_ + delay_ + 1) % max_size];
        return a + (b - a) * frac_;
    }

  private:
    T *line_ = nullptr;
    float frac_ = 0.0f;
    size_t write_ptr_ = 0;
    size_t delay_ = 1;
    size_t speed_ = 1; // 1, or 2 in octave mode
};
