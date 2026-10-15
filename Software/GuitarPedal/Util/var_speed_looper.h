// A multimode audio looper with a variable playback speed (also backwards), from the FunBox project (https://github.com/GuitarML/Funbox, MIT licence, Copyright (c) 2024 Keith
// Bloemer; varSpeedLooper.h in the Pluto and Cassini effects), itself a modified version of DaisySP's Looper (MIT, Electrosmith). Used by the Pluto and Cassini modules.
//
// Modes:
//  - Normal:         the input is added to the loop for as long as it records
//  - Onetime dub:    recording starts at the first sample and adds to the loop, and stops after one full loop
//  - Replace:        the loop is replaced while it records
//  - Frippertronics: records endlessly with a fixed decay on every pass, like a tape delay
//
// Changes from the original: playback starts at the start of the loop when the first recording is stopped (the original read one sample just past its end, which relied on the
// buffer having been cleared), the buffer is not cleared in Init() (a new loop overwrites it, and only what has been recorded is ever read; clearing 11 MB of SDRAM would take
// a noticeable time every time an effect is selected), and playback interpolates between two samples, because the pedal runs at 48 kHz where the original ran at 96 kHz.
#pragma once
#ifndef VAR_SPEED_LOOPER_H
#define VAR_SPEED_LOOPER_H

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace varspeed {

class Looper {
  public:
    enum class Mode { NORMAL, ONETIME_DUB, REPLACE, FRIPPERTRONICS };

    void Init(float *mem, size_t size) {
        buffer_size_ = size;
        buff_ = mem;
        state_ = State::EMPTY;
        mode_ = Mode::NORMAL;
        reverse_ = false;
        rec_queue_ = false;
        win_idx_ = 0;
        pos_ = 0.0f;
        recsize_ = 0;
        increment_ = 1.0f;
    }

    /** Reads or writes the buffer according to the state and the mode; returns the loop's sample */
    float Process(const float input) {
        float sig = 0.f;
        bool hitloop = false;
        // The first loop is always recorded forwards at normal speed
        const float inc = (state_ == State::EMPTY || state_ == State::REC_FIRST) ? 1.f : GetIncrementSize();
        const float win = WindowVal(win_idx_ * kWindowFactor);
        switch (state_) {
        case State::EMPTY:
            break;
        case State::REC_FIRST:
            Write(pos_, input * win);
            if (win_idx_ < kWindowSamps - 1) win_idx_ += 1;
            recsize_ = static_cast<size_t>(pos_);
            pos_ += inc;
            if (pos_ > buffer_size_ - 1) {
                state_ = State::PLAYING;
                recsize_ = static_cast<size_t>(pos_ - 1);
                pos_ = 0;
            }
            break;
        case State::PLAYING:
            sig = Read(pos_);
            // 'Seamless looping': the first samples after the recording is done are recorded with the input faded out
            if (win_idx_ < kWindowSamps - 1) {
                Write(pos_, sig + input * (1.f - win));
                win_idx_ += 1;
            }
            pos_ += inc;
            hitloop = Wrap();
            if (hitloop && rec_queue_ && mode_ == Mode::ONETIME_DUB) {
                rec_queue_ = false;
                state_ = State::REC_DUB;
                win_idx_ = 0;
            }
            break;
        case State::REC_DUB:
            sig = Read(pos_);
            switch (mode_) {
            case Mode::REPLACE:
                Write(pos_, input * win);
                break;
            case Mode::FRIPPERTRONICS:
                Write(pos_, (input * win) + (sig * kFripDecayVal));
                break;
            case Mode::NORMAL:
            case Mode::ONETIME_DUB:
            default:
                Write(pos_, (input * win) + sig);
                break;
            }
            if (win_idx_ < kWindowSamps - 1) win_idx_ += 1;
            pos_ += inc;
            hitloop = Wrap();
            if (hitloop && mode_ == Mode::ONETIME_DUB) {
                state_ = State::PLAYING;
                win_idx_ = 0;
            }
            break;
        default:
            break;
        }
        return sig;
    }

    /** Erases the loop (the buffer itself is left as it is) */
    void Clear() { state_ = State::EMPTY; }

    /** Starts or stops recording, depending on the mode. The first trigger starts a new loop, the second sets its length and starts playback. */
    void TrigRecord() {
        switch (state_) {
        case State::EMPTY:
            pos_ = 0;
            recsize_ = 0;
            state_ = State::REC_FIRST;
            reverse_ = false;
            break;
        case State::REC_FIRST:
            state_ = recsize_ < 2 ? State::EMPTY : State::PLAYING; // nothing was recorded yet: no loop
            pos_ = 0; // the loop plays from its start (the original carried on from just past the end, one sample of whatever was in the buffer from before)
            break;
        case State::REC_DUB:
            state_ = State::PLAYING;
            break;
        case State::PLAYING:
            if (mode_ == Mode::ONETIME_DUB) rec_queue_ = true;
            else state_ = State::REC_DUB;
            break;
        default:
            state_ = State::EMPTY;
            break;
        }
        if (!rec_queue_) win_idx_ = 0;
    }

    bool Recording() const { return state_ == State::REC_DUB || state_ == State::REC_FIRST; }
    bool RecordingQueued() const { return rec_queue_; }
    bool IsEmpty() const { return state_ == State::EMPTY; }

    void SetMode(Mode mode) { mode_ = mode; }
    Mode GetMode() const { return mode_; }
    void SetReverse(bool state) { reverse_ = state; }
    bool GetReverse() const { return reverse_; }
    void SetIncrementSize(float increment) { increment_ = increment; }
    float GetIncrementSize() const { return reverse_ ? -increment_ : increment_; }

  private:
    static constexpr float kFripDecayVal = 0.7071067811865476f; // sin(pi / 4)
    static constexpr int kWindowSamps = 1200;
    static constexpr float kWindowFactor = (1.f / kWindowSamps);

    enum class State { EMPTY, REC_FIRST, PLAYING, REC_DUB };

    // Moves the position back into the loop; true when it passed an end
    bool Wrap() {
        if (pos_ > recsize_ - 1) {
            pos_ = 0;
            return true;
        }
        if (pos_ < 0) {
            pos_ = static_cast<float>(recsize_ - 1);
            return true;
        }
        return false;
    }

    // The sample at a fractional position, interpolated linearly with the next one (the first one after the last)
    float Read(float pos) const {
        const size_t i = static_cast<size_t>(pos);
        const float frac = pos - static_cast<float>(i);
        const size_t j = (i + 1 < recsize_) ? i + 1 : 0;
        const float a = buff_[i];
        return a + (buff_[j] - a) * frac;
    }
    void Write(float pos, float val) { buff_[static_cast<size_t>(pos)] = val; }
    static float WindowVal(float in) { return std::sin(1.5707963267948966f * in); } // linear to constant power

    Mode mode_ = Mode::NORMAL;
    State state_ = State::EMPTY;
    float *buff_ = nullptr;
    size_t buffer_size_ = 0;
    float pos_ = 0.0f;
    size_t win_idx_ = 0;
    bool reverse_ = false;
    size_t recsize_ = 0;
    bool rec_queue_ = false;
    float increment_ = 1.0f;
};

} // namespace varspeed

#endif
