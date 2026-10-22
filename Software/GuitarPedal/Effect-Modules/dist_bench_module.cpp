#include "dist_bench_module.h"
#include <algorithm>
#include <array>
#include <cmath>

using namespace bkshepherd;
using namespace distdsp;

namespace {
const char *s_typeNames[DistBenchModule::TYPE_COUNT] = {"Off",       "Preamp",    "Screamer",  "Muff Fuzz", "Wavefolder",
                                                        "Exciter",   "Bass Dist", "Octa Fuzz", "Tape Sat",  "Overdrive", "Distortion", "Chainsaw"};
const char *s_stageNames[DistBenchModule::kStages][DistBenchModule::STAGE_PARAMS] = {{"1 Type", "1 Drive", "1 Tone", "1 Char", "1 Level"},
                                                                                     {"2 Type", "2 Drive", "2 Tone", "2 Char", "2 Level"},
                                                                                     {"3 Type", "3 Drive", "3 Tone", "3 Char", "3 Level"},
                                                                                     {"4 Type", "4 Drive", "4 Tone", "4 Char", "4 Level"}};

constexpr float kStageTrim = 0.5f; // -6 dB for every active stage: the effects are level-matched to +6 dB
constexpr float kMakeup = 2.0f;    // +6 dB at the end, so one stage at its defaults is as loud as the effect on its own
constexpr float kFadeSeconds = 0.1f;
constexpr float kSmoothing = 0.002f;

float LevelDb(float v) { return ParamDisplay::Lin(v, -24.0f, 6.0f); }

// A generic control (0..1, 0.5 = the effect's default) to the effect's own 0..1 value: 0..0.5 covers 0..default, 0.5..1 covers default..1.
float Relative(float g, float def) { return g <= 0.5f ? 2.0f * g * def : def + (g - 0.5f) * 2.0f * (1.0f - def); }

void SetRelative(BaseEffectModule &m, int id, float g) { m.SetParameterAsFloat(id, Relative(g, m.GetParameterDefaultValueAsFloat(id))); }

// The same for a binned parameter (a mode): 0.5 = the default mode, below and above step through the modes before and after it.
void SetBinned(BaseEffectModule &m, int id, float g) {
    const int n = m.GetParameterBinCount(id);
    const int def = static_cast<int>(m.GetParameterDefaultValueRaw(id));
    int idx = g <= 0.5f ? static_cast<int>(std::lround(2.0f * g * def)) : def + static_cast<int>(std::lround((g - 0.5f) * 2.0f * (n - 1 - def)));
    m.SetParameterRaw(id, static_cast<uint32_t>(std::clamp(idx, 0, n - 1)));
}

ParameterMetaData FloatParam(const char *name, float def, int knob, const char *unit, int decimals, bool sign, float (*fn)(float)) {
    ParameterMetaData p{};
    p.name = name;
    p.valueType = ParameterValueType::Float;
    p.defaultValue.float_value = def;
    p.knobMapping = knob;
    p.midiCCMapping = -1;
    p.unit = unit;
    p.decimals = decimals;
    p.showSign = sign;
    p.displayFn = fn;
    return p;
}
} // namespace

static const auto s_metaData = [] {
    std::array<ParameterMetaData, DistBenchModule::PARAM_COUNT> params{};
    for (int s = 0; s < DistBenchModule::kStages; s++) {
        ParameterMetaData type{};
        type.name = s_stageNames[s][DistBenchModule::TYPE];
        type.valueType = ParameterValueType::Binned;
        type.valueBinCount = DistBenchModule::TYPE_COUNT;
        type.valueBinNames = s_typeNames;
        type.defaultValue.uint_value = s == 0 ? DistBenchModule::SCREAMER : DistBenchModule::OFF; // raw values are 0-based, as the Type enum
        type.knobMapping = -1;
        type.midiCCMapping = -1;
        params[DistBenchModule::StageParamId(s, DistBenchModule::TYPE)] = type;

        const auto pct = [](float v) { return ParamDisplay::Percent(v); };
        params[DistBenchModule::StageParamId(s, DistBenchModule::DRIVE)] = FloatParam(s_stageNames[s][DistBenchModule::DRIVE], 0.5f, s, " %", 0, false, pct);
        params[DistBenchModule::StageParamId(s, DistBenchModule::TONE)] = FloatParam(s_stageNames[s][DistBenchModule::TONE], 0.5f, -1, " %", 0, false, pct);
        params[DistBenchModule::StageParamId(s, DistBenchModule::CHAR)] = FloatParam(s_stageNames[s][DistBenchModule::CHAR], 0.5f, -1, " %", 0, false, pct);
        params[DistBenchModule::StageParamId(s, DistBenchModule::LEVEL)] =
            FloatParam(s_stageNames[s][DistBenchModule::LEVEL], 0.8f, -1, " dB", 1, true, [](float v) { return LevelDb(v); });
    }
    params[DistBenchModule::OUT_LEVEL] = FloatParam("Out Level", 0.8f, 4, " dB", 1, true, [](float v) { return LevelDb(v); });
    params[DistBenchModule::MIX] = FloatParam("Mix", 1.0f, 5, " %", 0, false, [](float v) { return ParamDisplay::Percent(v); });
    return params;
}();

BaseEffectModule *DistBenchModule::Stage::Effect(int t) {
    switch (t) {
    case PREAMP:      return &preamp;
    case SCREAMER:    return &screamer;
    case MUFF_FUZZ:   return &muffFuzz;
    case WAVEFOLDER:  return &wavefolder;
    case EXCITER:     return &exciter;
    case BASS_DIST:   return &bassDist;
    case OCTAVE_FUZZ: return &octaveFuzz;
    case TAPE_SAT:    return &tapeSat;
    case OVERDRIVE:   return &overdrive;
    case DISTORTION:  return &distortion;
    case CHAINSAW:    return &chainsaw;
    default:          return nullptr;
    }
}

DistBenchModule::DistBenchModule() : MonoEffectModule(), m_dirty(true), m_outTarget(1.0f), m_out(1.0f), m_mixTarget(1.0f), m_mix(1.0f), m_fadeStep(1.0f) {
    m_name = "Dist Bench";
    m_paramMetaData = s_metaData.data();
    this->InitParams(static_cast<int>(s_metaData.size()));
}

DistBenchModule::~DistBenchModule() {}

void DistBenchModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);
    m_fadeStep = 1.0f / (kFadeSeconds * sample_rate);
    for (int s = 0; s < kStages; s++) {
        Stage &st = m_stage[s];
        for (int t = PREAMP; t < TYPE_COUNT; t++) st.Effect(t)->Init(sample_rate); // each one settles its own filters
        st.distortion.SetParameterAsBool(DistortionModule::OVERSAMP, false);     // 16x oversampling in four stages would cost too much
        ApplyStage(s);
        st.dirty = false;
        st.prevType = st.type; // start at the selected types, without a fade
        st.fade = 1.0f;
        st.gain = st.gainTarget;
    }
    m_outTarget = dbToGain(LevelDb(GetParameterAsFloat(OUT_LEVEL)));
    m_mixTarget = GetParameterAsFloat(MIX);
    m_out = m_outTarget;
    m_mix = m_mixTarget;
    m_dirty = false;
}

void DistBenchModule::ParameterChanged(int parameter_id) {
    if (parameter_id >= 0 && parameter_id < kStages * STAGE_PARAMS) m_stage[parameter_id / STAGE_PARAMS].dirty = true;
    m_dirty = true;
}

void DistBenchModule::ApplyStage(int s) {
    Stage &st = m_stage[s];
    const int type = std::clamp(GetParameterAsBinnedValue(StageParamId(s, TYPE)) - 1, 0, TYPE_COUNT - 1);
    const float drive = GetParameterAsFloat(StageParamId(s, DRIVE));
    const float tone = GetParameterAsFloat(StageParamId(s, TONE));
    const float chr = GetParameterAsFloat(StageParamId(s, CHAR));
    st.gainTarget = dbToGain(LevelDb(GetParameterAsFloat(StageParamId(s, LEVEL)))) * kStageTrim;

    // Every type of the stage gets the controls, so a type that is switched to later already has them.
    SetRelative(st.preamp, PreampModule::GAIN, drive);
    SetRelative(st.preamp, PreampModule::TREBLE, tone);
    SetBinned(st.preamp, PreampModule::VOICING, chr);

    SetRelative(st.screamer, ScreamerModule::DRIVE, drive);
    SetRelative(st.screamer, ScreamerModule::TONE, tone);
    SetBinned(st.screamer, ScreamerModule::MODE, chr);

    SetRelative(st.muffFuzz, MuffFuzzModule::SUSTAIN, drive);
    SetRelative(st.muffFuzz, MuffFuzzModule::TONE, tone);
    SetBinned(st.muffFuzz, MuffFuzzModule::MODE, chr);

    SetRelative(st.wavefolder, WavefolderModule::FOLD, drive);
    SetRelative(st.wavefolder, WavefolderModule::TONE, tone);
    SetRelative(st.wavefolder, WavefolderModule::SYMMETRY, chr);

    // Exciter: Char balances the 2nd harmonic (below 50%) against the 3rd (above); 50% is the default mix of the two.
    SetRelative(st.exciter, ExciterModule::DRIVE, drive);
    SetRelative(st.exciter, ExciterModule::FOCUS, tone);
    st.exciter.SetParameterAsFloat(ExciterModule::H2, std::min(1.0f, st.exciter.GetParameterDefaultValueAsFloat(ExciterModule::H2) * 2.0f * (1.0f - chr)));
    st.exciter.SetParameterAsFloat(ExciterModule::H3, std::min(1.0f, st.exciter.GetParameterDefaultValueAsFloat(ExciterModule::H3) * 2.0f * chr));

    SetRelative(st.bassDist, BassDistModule::DRIVE, drive);
    SetRelative(st.bassDist, BassDistModule::TONE, tone);
    SetBinned(st.bassDist, BassDistModule::CLIP, chr);

    SetRelative(st.octaveFuzz, OctaveFuzzModule::FUZZ, drive);
    SetRelative(st.octaveFuzz, OctaveFuzzModule::TONE, tone);
    SetRelative(st.octaveFuzz, OctaveFuzzModule::OCTAVE, chr);

    SetRelative(st.tapeSat, TapeSatModule::DRIVE, drive);
    SetRelative(st.tapeSat, TapeSatModule::TONE, tone);
    SetRelative(st.tapeSat, TapeSatModule::WARMTH, chr);

    SetRelative(st.overdrive, OverdriveModule::DRIVE, drive); // the Overdrive has no tone or second control

    SetRelative(st.distortion, DistortionModule::GAIN, drive);
    SetRelative(st.distortion, DistortionModule::TONE, tone);
    SetBinned(st.distortion, DistortionModule::DIST_TYPE, chr);

    SetRelative(st.chainsaw, ChainsawModule::DIST, drive); // Tone is the Chainsaw's High (the upper-mid peaks), Char its Low
    SetRelative(st.chainsaw, ChainsawModule::HIGH, tone);
    SetRelative(st.chainsaw, ChainsawModule::LOW, chr);

    if (type != st.type) { // crossfade from the old type to the new one
        st.prevType = st.type;
        st.type = type;
        st.fade = 0.0f;
    }
}

void DistBenchModule::RunEffect(Stage &st, int t, const float *in, float *out, size_t n) {
    BaseEffectModule *e = st.Effect(t);
    if (e == nullptr) {
        std::copy(in, in + n, out);
        return;
    }
    e->ProcessMonoBlock(in, out, m_bufR, n);
}

void DistBenchModule::Process(const float *in, float *out, size_t n) {
    if (m_dirty) {
        m_dirty = false;
        m_outTarget = dbToGain(LevelDb(GetParameterAsFloat(OUT_LEVEL)));
        m_mixTarget = GetParameterAsFloat(MIX);
        for (int s = 0; s < kStages; s++) {
            if (m_stage[s].dirty) {
                m_stage[s].dirty = false;
                ApplyStage(s);
            }
        }
    }

    float x[kChunk];
    for (size_t off = 0; off < n; off += kChunk) {
        const size_t c = std::min(kChunk, n - off);
        std::copy(in + off, in + off + c, x);

        for (Stage &st : m_stage) {
            if (st.fade >= 1.0f) {
                if (st.type == OFF) continue; // an Off stage passes the signal unchanged
                RunEffect(st, st.type, x, m_bufA, c);
                for (size_t i = 0; i < c; i++) {
                    st.gain += (st.gainTarget - st.gain) * kSmoothing;
                    x[i] = m_bufA[i] * st.gain;
                }
            } else {
                // The old type runs only for the k samples the fade still needs, and the rest of the chunk is processed as after the fade, so the
                // output and the old effect's state do not depend on where the fade ends within a block (the per-sample path is identical).
                size_t k = 0;
                for (float f = st.fade; k < c && f < 1.0f; k++) f = std::min(1.0f, f + m_fadeStep);
                RunEffect(st, st.prevType, x, m_bufA, k);
                RunEffect(st, st.type, x, m_bufB, c);
                for (size_t i = 0; i < k; i++) {
                    st.gain += (st.gainTarget - st.gain) * kSmoothing;
                    st.fade = std::min(1.0f, st.fade + m_fadeStep);
                    const float before = st.prevType == OFF ? x[i] : m_bufA[i] * st.gain;
                    const float after = st.type == OFF ? x[i] : m_bufB[i] * st.gain;
                    x[i] = before + (after - before) * st.fade;
                }
                if (st.type != OFF) {
                    for (size_t i = k; i < c; i++) {
                        st.gain += (st.gainTarget - st.gain) * kSmoothing;
                        x[i] = m_bufB[i] * st.gain;
                    }
                }
            }
        }

        for (size_t i = 0; i < c; i++) {
            m_out += (m_outTarget - m_out) * kSmoothing;
            m_mix += (m_mixTarget - m_mix) * kSmoothing;
            out[off + i] = (in[off + i] * (1.0f - m_mix) + x[i] * m_mix * kMakeup) * m_out;
        }
    }
}
