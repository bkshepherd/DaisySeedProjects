#pragma once
#ifndef SATURN_MODULE_H
#define SATURN_MODULE_H

#include "../Util/STFT/cmsis_fft.h"
#include "../Util/STFT/sliced_fourier.h"
#include "base_effect_module.h"
#include "daisysp.h"
#include <stdint.h>
#ifdef __cplusplus

/** @file saturn_module.h */

namespace bkshepherd {

namespace saturn {
constexpr size_t kOrder = 10;
constexpr size_t kN = size_t(1) << kOrder; // size of the transform
constexpr size_t kLaps = 4;                // overlapping windows
constexpr size_t kStride = kN / kLaps;     // samples between frames (256)
constexpr size_t kBins = kN / 2;           // bins in a frame
constexpr size_t kChunks = 16;             // chunks per frame (kBins / kChunks bins each): small steps, so that no single audio block gets a big share of a frame's work
constexpr int kLinesPerSet = 175;          // bins that get a delay line (real and imaginary part each), per channel (up to 8.2 kHz, as in the original); mono mode uses two sets for twice as many bins
constexpr int kReferenceBins = 175;        // the original's number of bins per channel: the delay and feedback slopes across the bins are kept per Hz, as if there were this many
} // namespace saturn

// Saturn: a spectral delay. A short-time Fourier transform splits the signal into frequency bins and every bin has its own delay line (with feedback), so
// the delay time can grow with frequency, follow a sine, or be random. Plus a pre-delay (with optional feedback), a low/high-pass that works on the bins, slow "drift" of
// the delay time and feedback, a mono mode that doubles the number of bins, a stereo mode with two transforms, and a random stereo spread. Adapted from the Saturn pedal
// of the FunBox project by GuitarML (MIT licence, see Saturn/README.md).
class SaturnModule : public BaseEffectModule {
  public:
    enum Param {
        PREDELAY = 0,
        MIX,
        FILTER,
        TIME,
        FEEDBACK,
        DRIFT_RATE,
        TIME_MODE,
        FDBK_MODE,
        DRIFT,
        MONO,
        STEREO_IN,
        STEREO_SPREAD,
        PRE_FDBK,
        PARAM_COUNT
    };

    SaturnModule();
    ~SaturnModule();

    void Init(float sample_rate) override;
    void ParameterChanged(int parameter_id) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    void ProcessMonoBlock(const float *in, float *outL, float *outR, size_t n) override;
    void ProcessStereoBlock(const float *inL, const float *inR, float *outL, float *outR, size_t n) override;
    bool AlternateFootswitchForTempo() const override { return false; }
    void GetDebugText(char *buffer, size_t size) const override;

    // Per-frame hooks and the per-bin processing, called by the sliced transform (public only so that the static callbacks can reach them)
    static void BeginFrame0(void *self) { static_cast<SaturnModule *>(self)->m_frame[0] = static_cast<SaturnModule *>(self)->m_pendingFrame; }
    static void BeginFrame1(void *self) { static_cast<SaturnModule *>(self)->m_frame[1] = static_cast<SaturnModule *>(self)->m_pendingFrame; }
    static void ProcessChunk0(void *self, const float *in, float *out, size_t chunk, size_t chunkCount) {
        const size_t per = saturn::kBins / chunkCount;
        static_cast<SaturnModule *>(self)->ProcessBins(0, in, out, chunk * per, (chunk + 1) * per);
    }
    static void ProcessChunk1(void *self, const float *in, float *out, size_t chunk, size_t chunkCount) {
        const size_t per = saturn::kBins / chunkCount;
        static_cast<SaturnModule *>(self)->ProcessBins(1, in, out, chunk * per, (chunk + 1) * per);
    }

  private:
    // What the per-bin processing needs, latched at the start of every frame
    struct FrameParams {
        int filterMode = 2;      // 0 = low-pass (bins below filterBin), 1 = high-pass (bins above), 2 = off
        unsigned filterBin = 0;
        int activeBins = saturn::kLinesPerSet; // bins that get delayed: kLinesPerSet, or twice that in mono mode
        bool mono = false;
        bool spread = false;
    };

    __attribute__((noclone)) void ProcessSamples(const float *inL, const float *inR, float *outL, float *outR, size_t n, bool stereoInput);
    void UpdateControls();
    void ProcessBins(int channel, const float *in, float *out, size_t first, size_t last);
    float NextRandom(); // 0 .. 1

    soundmath::CmsisRealFft<saturn::kN> *m_fft;
    soundmath::SlicedFourier<float, saturn::kN, soundmath::CmsisRealFft<saturn::kN>> *m_stft[2];
    FrameParams m_frame[2];
    FrameParams m_pendingFrame;

    daisysp::Oscillator m_drift;

    float m_sampleRate;
    float m_framesPerSecond; // transform frames per second (sample rate / stride): the delay lines are advanced once per frame
    int m_stepAccum;         // schedule of the pipeline stages
    int m_driftAccum;        // samples since the drift oscillator was last advanced
    int m_controlAccum;      // samples since the controls were last turned into delay settings
    float m_driftMultiplier; // 0 .. 1
    float m_prevTime, m_prevFdbk;
    int m_prevTimeMode, m_prevFdbkMode;
    bool m_prevMono;
    bool m_forceUpdate;
    int m_clearRemaining; // delay lines still to be cleared after a switch between mono and stereo
    float m_preTarget;
    float m_preFeedback;
    float m_stereoField[saturn::kLinesPerSet];
    uint32_t m_rng;

    // Timing of the parts of the audio path, for the Show Debug screen: the slowest call in the last two seconds, in percent of the time the block lasts
    float m_tStepsMax, m_tOneStepMax, m_tLoopMax, m_tCtlMax; // being collected
    float m_tSteps, m_tOneStep, m_tLoop, m_tCtl;             // shown
    int m_tWindowSamples;
};
} // namespace bkshepherd
#endif
#endif
