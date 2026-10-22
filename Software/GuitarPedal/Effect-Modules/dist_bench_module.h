#pragma once
#ifndef DIST_BENCH_MODULE_H
#define DIST_BENCH_MODULE_H

#include "bass_dist_module.h"
#include "chainsaw_module.h"
#include "distortion_module.h"
#include "exciter_module.h"
#include "mono_effect_module.h"
#include "muff_fuzz_module.h"
#include "octave_fuzz_module.h"
#include "overdrive_module.h"
#include "preamp_module.h"
#include "screamer_module.h"
#include "tape_sat_module.h"
#include "wavefolder_module.h"
#ifdef __cplusplus

/** @file dist_bench_module.h */

namespace bkshepherd {

// Dist Bench: a test bench for the distortion effects. Up to four stages in series, each one Off or one of the nine distortion family effects, the
// Overdrive or the Distortion (without oversampling). Every stage has the same generic controls (Type, Drive, Tone, Char, Level), mapped onto the
// chosen effect's own parameters; at 50% a generic control is that effect's default, so a stage left at 50% sounds as the effect does on its own. Parameters a stage does not map stay at
// the effect's default. Each active stage is trimmed by -6 dB (the effects are level-matched to +6 dB) and the bench adds +6 dB at the end, so one
// stage at its defaults is as loud as the effect on its own. Changing a stage's Type crossfades over 0.1 s.
//
// Every stage holds an instance of every type (created and initialised once, so nothing is allocated or settled in the audio thread); only the
// selected one is processed.
class DistBenchModule : public MonoEffectModule {
  public:
    static constexpr int kStages = 4;
    enum StageParam { TYPE = 0, DRIVE, TONE, CHAR, LEVEL, STAGE_PARAMS };
    enum Param { OUT_LEVEL = kStages * STAGE_PARAMS, MIX, PARAM_COUNT };
    /** The parameter id of a stage's generic control (stage 0..3). */
    static constexpr int StageParamId(int stage, StageParam p) { return stage * STAGE_PARAMS + p; }

    enum Type { OFF = 0, PREAMP, SCREAMER, MUFF_FUZZ, WAVEFOLDER, EXCITER, BASS_DIST, OCTAVE_FUZZ, TAPE_SAT, OVERDRIVE, DISTORTION, CHAINSAW, TYPE_COUNT };

    DistBenchModule();
    ~DistBenchModule();

    void Init(float sample_rate) override;
    void ParameterChanged(int parameter_id) override;

  protected:
    void Process(const float *in, float *out, size_t n) override;

  private:
    struct Stage {
        PreampModule preamp;
        ScreamerModule screamer;
        MuffFuzzModule muffFuzz;
        WavefolderModule wavefolder;
        ExciterModule exciter;
        BassDistModule bassDist;
        OctaveFuzzModule octaveFuzz;
        TapeSatModule tapeSat;
        OverdriveModule overdrive;
        DistortionModule distortion;
        ChainsawModule chainsaw;

        int type = OFF;     // the selected type
        int prevType = OFF; // the type being faded out
        float fade = 1.0f;  // 0 = prevType, 1 = type
        float gainTarget = 0.5f, gain = 0.5f;
        bool dirty = true;

        BaseEffectModule *Effect(int t);
    };

    void ApplyStage(int s);                                          // the generic controls of stage s onto its effects
    void RunEffect(Stage &st, int t, const float *in, float *out, size_t n); // out = the stage's effect t on in (t = OFF: a copy)

    static constexpr size_t kChunk = 64;
    Stage m_stage[kStages];
    bool m_dirty;
    float m_outTarget, m_out;
    float m_mixTarget, m_mix;
    float m_fadeStep;
    float m_bufA[kChunk], m_bufB[kChunk], m_bufR[kChunk];
};
} // namespace bkshepherd
#endif
#endif
