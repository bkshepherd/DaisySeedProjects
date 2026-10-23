#pragma once
#ifndef SPRING_REVERB_MODULE_H
#define SPRING_REVERB_MODULE_H

#include "../Util/dist_dsp.h"
#include "../Util/spring_dsp.h"
#include "base_effect_module.h"
#ifdef __cplusplus

/** @file spring_reverb_module.h */

namespace bkshepherd {

// Spring Reverb: a physical-modelling spring tank of 2 to 6 springs of different lengths, as the 2- and 3-spring tanks of real units (one spring on its own repeats
// its echoes very regularly: the more springs, the denser and less delay-like the tail). The input is summed to mono, high-passed (the driver of a tank has no
// bass), driven into a soft clip (Dwell, as on a Fender reverb: harder drive gives a louder, splashier tank) and low-passed at 4.2 kHz. Everything a spring carries
// is below its transition frequency of 4 kHz, so the springs run at a third of the sample rate (16 kHz): each is a loop of a dispersive allpass chain (the lows of an
// echo arrive first and the highs, towards 4 kHz, later: every echo is a short upward sweep, the "drip" or "boing"; see Util/spring_dsp.h) and a delay line (the
// transit of the wave to the end of the spring and back, which sets the spacing of the echoes), with damping in the loop and a gain that sets the decay time. The
// sum of the springs is interpolated back to the full rate, high-passed and low-passed (Tone).
//   Dwell   how hard the tank is driven: the level of the reverb and how much it splashes
//   Decay   the time it takes for the echoes to die away by 60 dB at 1 kHz (0.5 to 6 s)
//   Tone    the high cut of the return (1.5 to 12 kHz)
//   Mix     how much reverb is added to the dry signal, which is never changed (as the reverb control of an amp)
//   Drip    how dispersive the springs are: 0 is a plain echo train, 100% (the default) the dispersion of a real spring: every echo is an upward sweep,
//           the lows first and the highs, near the 4 kHz transition frequency, up to tens of ms later, which blurs the echoes
//   Tank    the length of the springs: Short (0.7 times the Medium round trips), Medium (34 ms for the first spring, 41 ms for the second, ...), Long (1.35 times)
//   Output  Mono: all the springs on both channels. Stereo: every other spring on each channel (the first on the left, the second on the right, ...). All the
//           springs run in both, so this costs nothing; the channels crossfade between the two in about 10 ms.
//   Springs how many springs run (2 to 6): each costs about the same, a third of what a spring costs at the full rate
class SpringReverbModule : public BaseEffectModule {
  public:
    enum Param { DWELL = 0, DECAY, TONE, MIX, DRIP, TANK, OUTPUT, SPRINGS, PARAM_COUNT };
    enum Tank { SHORT_TANK = 0, MEDIUM_TANK, LONG_TANK, TANK_COUNT };
    enum OutputMode { STEREO_OUT = 0, MONO_OUT, OUTPUT_COUNT };
    static constexpr int kMinSprings = 2;
    static constexpr int kMaxSprings = 6;

    SpringReverbModule();
    ~SpringReverbModule();

    float GetMaxTrailSeconds() const override { return 6.0f; }
    void Init(float sample_rate) override;
    void ParameterChanged(int parameter_id) override;
    void SetEnabled(bool isEnabled) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    void ProcessMonoBlock(const float *in, float *outL, float *outR, size_t n) override;
    void ProcessStereoBlock(const float *inL, const float *inR, float *outL, float *outR, size_t n) override;

  private:
    static constexpr int kDecimation = 3;      // the springs run at a third of the sample rate (16 kHz), with a stretch of 2: the same sweep in ms as a stretch of 6 at 48 kHz
    static constexpr int kMaxSections = 100;
    static constexpr size_t kMaxDelay = 1200;  // samples at the spring rate: 75 ms

    struct Spring {
        springdsp::DispersionChain<kMaxSections, 2> chain;
        springdsp::DelayLine<kMaxDelay> line;
        distdsp::OnePole damp;
        float delay = 500.0f, delayTarget = 500.0f; // samples at the spring rate: the delay line (the loop is this plus the chain)
        float gain = 0.0f;                          // the gain of the loop
        float inGain = 1.0f;                        // the gain of what is fed in: the energy of the tail does not depend on the decay
        float Process(float in);
        void Reset();
    };

    void Process(const float *inL, const float *inR, float *outL, float *outR, size_t n);
    void Recompute();
    void ClearState();

    float m_sampleRate, m_springRate;
    bool m_dirty;
    float m_dwellTarget, m_dwell;
    float m_wetTarget, m_wet;     // the gain of the wet signal (Mix times kWetGain); the dry signal is added as it is
    float m_monoTarget, m_mono;   // 0: stereo, 1: mono (a crossfade of the output)
    int m_springCount;            // how many springs run
    int m_phase;                  // 0, 1, 2: the springs run when it is 0
    float m_activity[kMaxSprings]; // 1 for a spring that runs, faded in over about 100 ms when it is switched on and out in about 10 ms when it is switched off; 0: not processed
    float m_weightA, m_weightB;   // the sums of the squares of the activities of the springs of the left and of the right channel, as of the latest run of the springs

    Spring m_spring[kMaxSprings];
    distdsp::Biquad m_hpIn, m_lpIn, m_lpIn2;
    distdsp::Biquad m_interp1[2], m_interp2[2], m_hpOut[2], m_lpOut[2];
    distdsp::SoftClipAdaa m_dwellClip;
};
} // namespace bkshepherd
#endif
#endif
