// sliced_fourier.h
//
// A short-time Fourier transform like soundmath::Fourier (Util/STFT/fourier.h, from DaisySTFT by amcerbu), but the per-frame work is spread over
// later calls instead of running inside write().
//
// Fourier::write() runs the forward FFT, the user's spectral processing and the inverse FFT all at once whenever a frame completes. For a large
// transform (Venus uses N = 4096) that is several milliseconds in a single audio callback, far more than a 48-sample block (1 ms) allows. Here a
// completed frame is queued, and each call to Step() (the module calls it once per block) runs one stage of the oldest queued frame:
//     stage 0                 begin() hook, then the forward FFT
//     stages 1 .. chunkCount  the user's processing, one chunk of the spectrum at a time
//     stage chunkCount + 1    the inverse FFT
// The frame's output only starts being read `stride` samples after the frame completed (a fixed extra latency of one hop), which is enough time for
// all stages as long as the stages get run at least stepsPerFrame() times per `stride` samples (the module calls Step() on a sample-count schedule, so this holds
// for any block size). One extra slot is needed because a slot is now busy for N + stride + N samples.
// Only one frame is processed at a time, so the frequency-domain work buffers (`middle`, `out`) are shared by all slots: N samples each, not N per slot. That
// keeps them small enough to live in fast on-chip RAM, which matters because the FFT passes over them several times; only `in` (which holds each
// frame's input and then its output) is per slot.
//
// Apart from that delay of `stride` samples the output matches soundmath::Fourier with the same processing. One difference: Fourier lets a slot start
// writing again one sample early, so its frames drift by one sample per cycle; here a slot's cycle is exactly (2 * laps + 1) strides, so from the ninth
// frame on the frames sit one sample later than Fourier's (inaudible). tests/host/test_venus.cpp checks both the match and that spreading the stages
// over blocks changes nothing.
#pragma once
#ifndef VENUS_SLICED_FOURIER_H

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
// (the STFT headers use uint8_t, memset, std::function and trigonometry without including them)
#include "../../Util/STFT/shy_fft.h"
#include "../../Util/STFT/wave.h"

namespace soundmath {

// FFT: anything with Direct(T *in, T *out) and Inverse(T *in, T *out) in ShyFFT's layout (ShyFFT itself, or venus::CmsisRealFft)
template <typename T, size_t N, typename FFT = ShyFFT<T, N, RotationPhasor>> class SlicedFourier {
  public:
    /** Processes chunk `chunk` of `chunkCount` of one frame: reads the spectrum from `in`, writes the processed spectrum to `out`. Called for the chunks in
        order 0 .. chunkCount - 1 for every frame. */
    using ChunkProcessor = void (*)(void *context, const T *in, T *out, size_t chunk, size_t chunkCount);
    /** Called once at the start of every frame (before its forward FFT), e.g. to take a snapshot of the parameters used for the whole frame. */
    using FrameBegin = void (*)(void *context);

    // in needs to be an array of size (N * slotCount()), where slotCount() = 2 * laps + 1; middle and out are arrays of size N
    SlicedFourier(ChunkProcessor processChunk, FrameBegin beginFrame, void *context, size_t chunkCount, FFT *fft,
                  Wave<T> *window, size_t laps, T *in, T *middle, T *out)
        : processChunk_(processChunk), beginFrame_(beginFrame), context_(context), chunkCount_(chunkCount), in_(in), middle_(middle), out_(out),
          fft_(fft), window_(window), laps_(laps), stride_(N / laps), slots_(2 * laps + 1) {
        writepoint_ = new int[slots_];
        readpoint_ = new int[slots_];
        state_ = new uint8_t[slots_];
        startTick_ = new uint32_t[slots_];
        queue_ = new int[slots_];

        for (size_t i = 0; i < slots_; i++) {
            writepoint_[i] = -(int)(i * stride_); // staggered: slot i starts collecting input i * stride samples after slot 0
            readpoint_[i] = 0;
            state_[i] = kWriting;
            startTick_[i] = 0;
        }
    }

    ~SlicedFourier() {
        delete[] writepoint_;
        delete[] readpoint_;
        delete[] state_;
        delete[] startTick_;
        delete[] queue_;
    }

    size_t slotCount() const { return slots_; }
    /** Number of Step() calls needed per frame. The module must call Step() at least this often per `stride` samples. */
    size_t stepsPerFrame() const { return chunkCount_ + 2; }
    /** True while a frame is queued or being processed (the tests use this to run the stages immediately instead of spread out). */
    bool Busy() const { return current_ >= 0 || queueCount_ > 0; }
    /** Extra latency in samples compared to soundmath::Fourier. */
    size_t extraDelay() const { return stride_; }
    size_t stride() const { return stride_; }
    /** How many frames were not finished in time and had to be completed inside Process() (should stay 0). */
    uint32_t lateFinishes() const { return lateFinishes_; }

    /** Feeds one input sample and returns one output sample. */
    T Process(T x) {
        write(x);
        const T y = read();
        tick_++;
        return y;
    }

    /** Runs one stage of the oldest queued frame, if there is one. Call once per audio block (every ~48 samples). */
    void Step() {
        if (current_ < 0) {
            if (queueCount_ == 0) return;
            current_ = queue_[queueHead_];
            queueHead_ = (queueHead_ + 1) % slots_;
            queueCount_--;
            stage_ = 0;
        }
        runStage(current_, stage_);
        if (++stage_ == stepsPerFrame()) {
            state_[current_] = kWaiting;
            current_ = -1;
        }
    }

  private:
    enum : uint8_t { kWriting, kQueued, kWaiting, kReading };

    void runStage(int i, size_t stage) {
        if (stage == 0) {
            if (beginFrame_) beginFrame_(context_);
            fft_->Direct(in_ + i * N, middle_); // analysis
        } else if (stage <= chunkCount_) {
            processChunk_(context_, middle_, out_, stage - 1, chunkCount_);
        } else {
            fft_->Inverse(out_, in_ + i * N); // synthesis
        }
    }

    // writes a single sample (with windowing) into the in array of every slot that is collecting input
    void write(T x) {
        for (size_t i = 0; i < slots_; i++) {
            if (state_[i] != kWriting) continue;
            if (writepoint_[i] >= 0) {
                const T amp = (*window_)((T)writepoint_[i] / N);
                in_[writepoint_[i] + N * i] = amp * x;
            }
            writepoint_[i]++;

            if (writepoint_[i] == (int)N) { // the frame is complete: queue it, and schedule when its output starts being read
                state_[i] = kQueued;
                startTick_[i] = tick_ + (uint32_t)stride_;
                queue_[(queueHead_ + queueCount_) % slots_] = (int)i;
                queueCount_++;
            }
        }
    }

    // read a single reconstructed sample
    T read() {
        T accum = 0;
        for (size_t i = 0; i < slots_; i++) {
            if (state_[i] == kQueued || state_[i] == kWaiting) {
                if ((int32_t)(tick_ - startTick_[i]) < 0) continue; // not time to start reading this frame yet
                if (state_[i] == kQueued) finishNow(i);              // processing did not keep up: finish it now instead of reading garbage
                state_[i] = kReading;
                readpoint_[i] = 0;
            }
            if (state_[i] == kReading) {
                const T amp = (*window_)((T)readpoint_[i] / N);
                accum += amp * in_[readpoint_[i] + N * i];
                readpoint_[i]++;

                if (readpoint_[i] == (int)N) {
                    state_[i] = kWriting;
                    writepoint_[i] = 0;
                }
            }
        }
        accum /= N * laps_ / 2.0;
        return accum;
    }

    // Fallback if Step() was not called often enough: runs the remaining stages of slot i (and of any frames queued ahead of it) synchronously.
    void finishNow(size_t slot) {
        lateFinishes_++;
        while (state_[slot] == kQueued) Step();
    }

    ChunkProcessor processChunk_;
    FrameBegin beginFrame_;
    void *context_;
    size_t chunkCount_;
    T *in_, *middle_, *out_;
    FFT *fft_;
    Wave<T> *window_;
    size_t laps_, stride_, slots_;

    int *writepoint_;
    int *readpoint_;
    uint8_t *state_;
    uint32_t *startTick_;
    int *queue_;
    size_t queueHead_ = 0, queueCount_ = 0;
    int current_ = -1;
    size_t stage_ = 0;
    uint32_t tick_ = 0;
    uint32_t lateFinishes_ = 0;
};
} // namespace soundmath

#define VENUS_SLICED_FOURIER_H
#endif
