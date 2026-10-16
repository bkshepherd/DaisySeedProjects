// Venus: spectral reverb, adapted from the Venus pedal of the FunBox project (https://github.com/GuitarML/Funbox, MIT licence, Copyright (c) 2024
// Keith Bloemer). See Venus/README.md for the licence text, the acknowledgements and the list of changes.
#include "venus_module.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

using namespace bkshepherd;

namespace {

// The original runs at 32 kHz with 256-sample blocks. Everything that depends on that is rescaled by m_srRatio (sampleRate / 32000).
constexpr float kOriginalSampleRate = 32000.0f;
// The work is scheduled by sample count, not by block, so it does not depend on the audio block size (the pedal can run at 48 or 256 samples per block).
constexpr int kDriftIntervalSamples = 256; // the drift oscillators (0.01 .. 0.035 Hz) are advanced once per this many samples
constexpr int kParamIntervalSamples = 48;  // the controls are turned into frame parameters at least this often
// The stages of one frame (the forward FFT, kChunks chunks of bins, the inverse FFT) are issued at 3/2 of the rate they need, i.e. all of them are done after
// two thirds of a hop; the rest is slack so that a slow block does not leave a frame unfinished when its output is due (which would force the whole frame to
// be finished inside the audio callback).
constexpr int kStepsPerHop = (int)((venus::kChunks + 2) * 3 / 2);
constexpr size_t kSlots = 2 * venus::kLaps + 1;
// Output makeup gain. The dry/wet crossfade attenuates (at the default Mix of 0.4 the dry signal alone is 4.4 dB down, and the reverb's own level is lower
// than the input), so without it the effect is quieter than bypass. Calibrated with tests/host/run_level_tests.sh to about +2 dB at the default settings.
constexpr float kOutputGain = 1.9f;
constexpr float kIntervalSamples = 8192.0f; // ceil(window_samples / laps) in the original, which sets the scale of the shimmer and detune amounts

// audio --> in --(fft)--> middle --(process)--> out --(ifft)--> in --> audio
// `in` holds every overlapping frame (kN floats each): 147 KB, so it lives in SDRAM (SDRAM is not zeroed at startup, Init() clears it). Only one frame is
// processed at a time, so the frequency-domain work buffers are a single frame each (16 KB), in the ordinary (DTCM) RAM: the FFT passes over them several
// times, and that is much faster than going through the cache to SDRAM.
float DSY_SDRAM_BSS s_in[venus::kN * kSlots];
float s_middle[venus::kN]; // unprocessed frequency domain data
float s_out[venus::kN];    // processed frequency domain data
soundmath::CmsisRealFft<venus::kN> s_fft; // the FFT with its two 16 KB workspaces: must be in the ordinary (DTCM) RAM, not SDRAM (see cmsis_fft.h)

soundmath::Wave<float> s_hann([](float phase) -> float { return 0.5f * (1.0f - std::cos(2.0f * 3.14159265358979f * phase)); });

const char *s_shimmerModeNames[3] = {"Oct Down", "Oct Up", "Oct Up+Dn"};
const char *s_lofiNames[3] = {"Mild", "Off", "Heavy"};
const char *s_driftNames[3] = {"Slow", "Off", "Fast"};

} // namespace

static const auto s_metaData = [] {
    std::array<ParameterMetaData, VenusModule::PARAM_COUNT> params{};

    params[VenusModule::DECAY] = {
        name : "Decay",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 0,
        midiCCMapping : 14
    };

    params[VenusModule::MIX] = {
        name : "Mix",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.4f},
        knobMapping : 1,
        midiCCMapping : 15,
        unit : " %",
        decimals : 0,
        // crossfade position
        displayFn : [](float v) { return ParamDisplay::Percent(v); }
    };

    params[VenusModule::DAMP] = {
        name : "Damp",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.7f},
        knobMapping : 2,
        midiCCMapping : 16
    };

    params[VenusModule::SHIMMER] = {
        name : "Shimmer",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.0f},
        knobMapping : 3,
        midiCCMapping : 17,
        unit : " %",
        decimals : 0,
        displayFn : [](float v) { return ParamDisplay::Percent(v); }
    };

    params[VenusModule::SHIMMER_TONE] = {
        name : "Shim Tone",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.0f},
        knobMapping : 4,
        midiCCMapping : 18,
        unit : " %",
        decimals : 0,
        displayFn : [](float v) { return ParamDisplay::Percent(v); }
    };

    params[VenusModule::DETUNE] = { // 0.5 = no detune, below detunes down, above detunes up
        name : "Detune",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 5,
        midiCCMapping : 19
    };

    params[VenusModule::SHIMMER_MODE] = {
        name : "Shim Mode",
        valueType : ParameterValueType::Binned,
        valueBinCount : 3,
        valueBinNames : s_shimmerModeNames,
        defaultValue : {.uint_value = 1},
        knobMapping : -1,
        midiCCMapping : 20
    };

    params[VenusModule::LOFI] = {
        name : "LoFi",
        valueType : ParameterValueType::Binned,
        valueBinCount : 3,
        valueBinNames : s_lofiNames,
        defaultValue : {.uint_value = 1},
        knobMapping : -1,
        midiCCMapping : 21
    };

    params[VenusModule::DRIFT] = {
        name : "Drift",
        valueType : ParameterValueType::Binned,
        valueBinCount : 3,
        valueBinNames : s_driftNames,
        defaultValue : {.uint_value = 1},
        knobMapping : -1,
        midiCCMapping : 22
    };

    params[VenusModule::FREEZE] = {
        name : "Freeze",
        valueType : ParameterValueType::Bool,
        valueBinCount : 0,
        defaultValue : {.uint_value = 0},
        knobMapping : -1,
        midiCCMapping : 23
    };

    return params;
}();

VenusModule::VenusModule()
    : BaseEffectModule(), m_fft(nullptr), m_stft(nullptr), m_sampleRate(48000.0f), m_srRatio(1.5f), m_stepAccum(0), m_driftAccum(0), m_paramAccum(0), m_driftValue{1.0f, 1.0f, 1.0f, 1.0f},
      m_footswitchFreeze(false) {
    m_name = "Venus";
    m_paramMetaData = s_metaData.data();
    this->InitParams(static_cast<int>(s_metaData.size()));
}

VenusModule::~VenusModule() {
    delete m_stft; // the FFT object is a file-scope static
}

void VenusModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);

    m_sampleRate = sample_rate;
    m_srRatio = sample_rate / kOriginalSampleRate;
    m_stepAccum = 0;
    m_driftAccum = 0;
    m_paramAccum = 0;

    // SDRAM is not zeroed at startup, and the transform must not see random memory
    std::memset(s_in, 0, sizeof(s_in));
    std::memset(s_middle, 0, sizeof(s_middle));
    std::memset(s_out, 0, sizeof(s_out));

    m_reverb.Init();

    delete m_stft;
    m_fft = &s_fft;
    m_fft->Init();
    m_stft = new soundmath::SlicedFourier<float, venus::kN, soundmath::CmsisRealFft<venus::kN>>(&venus::SpectralReverb::ProcessChunk, &venus::SpectralReverb::BeginFrame, &m_reverb,
                                                            venus::kChunks, m_fft, &s_hann, venus::kLaps, s_in, s_middle, s_out);

    m_reducer.Init();
    m_lowpass.Init(sample_rate);
    for (int i = 0; i < 4; i++) {
        m_drift[i].Init(sample_rate / kDriftIntervalSamples); // the drift oscillators are advanced once per kDriftIntervalSamples
        m_drift[i].SetAmp(1.0f);
        m_driftValue[i] = 1.0f;
    }

    // ParameterChanged only fires on a change, so apply the defaults that configure the lo-fi and drift modes here
    ApplyLoFiMode();
    ApplyDriftMode();
    UpdateFrameParams();
}

void VenusModule::ParameterChanged(int parameter_id) {
    if (parameter_id == LOFI) {
        ApplyLoFiMode();
    } else if (parameter_id == DRIFT) {
        ApplyDriftMode();
    }
}

void VenusModule::ApplyLoFiMode() {
    // Lo-fi is applied to the wet signal only. The original names are in Hz at 32 kHz: Mild = 9600 Hz sample rate + 8 kHz lowpass, Heavy = 6400 Hz,
    // no lowpass. The reducer's frequency is a fraction of the sample rate, so it is rescaled to keep the same rates in Hz.
    const int mode = GetParameterAsBinnedValue(LOFI) - 1;
    if (mode == 0) {
        m_reducer.SetFreq(9600.0f / m_sampleRate);
        m_lowpass.SetFreq(8000.0f);
    } else if (mode == 2) {
        m_reducer.SetFreq(6400.0f / m_sampleRate);
    }
}

void VenusModule::ApplyDriftMode() {
    // Four slow oscillators, each a little off from the others, so the modulation of Damp, Shimmer, Shim Tone and Detune never quite repeats.
    // Frequencies in Hz (the oscillators run at block rate): slow = sine at about 0.01 Hz, fast = triangle at about 0.02 .. 0.035 Hz.
    static const float slowFreq[4] = {0.009f, 0.01f, 0.011f, 0.012f};
    static const float fastFreq[4] = {0.020f, 0.025f, 0.03f, 0.035f};
    const int mode = GetParameterAsBinnedValue(DRIFT) - 1;
    if (mode == 1) {
        for (int i = 0; i < 4; i++) m_driftValue[i] = 1.0f; // no drift
        return;
    }
    for (int i = 0; i < 4; i++) {
        m_drift[i].SetFreq(mode == 0 ? slowFreq[i] : fastFreq[i]);
        m_drift[i].SetWaveform(mode == 0 ? Oscillator::WAVE_SIN : Oscillator::WAVE_TRI);
    }
}

// Turns the controls into the parameters of the next frame of spectral processing (once per call, like the original's per-callback control update)
void VenusModule::UpdateFrameParams() {
    const float decayParam = GetParameterAsFloat(DECAY);
    const float dampParam = GetParameterAsFloat(DAMP);
    const float detuneParam = (GetParameterAsFloat(DETUNE) - 0.5f) * 0.3f; // -0.15 .. 0.15

    float decay = decayParam * 99.0f + 1.0f; // decay time in frames at 32 kHz
    float damp = dampParam * dampParam;      // exponential knob taper
    float shimmer = GetParameterAsFloat(SHIMMER) * 0.1f;
    float shimmerTone = GetParameterAsFloat(SHIMMER_TONE) * 0.3f;
    float detune = std::fabs(detuneParam);

    // Drift automation: slow modulation of the controls from their current setting towards 0
    if (GetParameterAsBinnedValue(DRIFT) != 2) {
        damp = damp * m_driftValue[0] * 0.7f + 0.3f;
        shimmer *= m_driftValue[1];
        shimmerTone *= m_driftValue[2];
        detune *= m_driftValue[3];
    }

    venus::FrameParams p;
    p.freeze = GetParameterAsBool(FREEZE) || m_footswitchFreeze;
    p.shimmerMode = GetParameterAsBinnedValue(SHIMMER_MODE) - 1; // 0 = octave down, 1 = octave up, 2 = octave up (see venus_spectral.h)

    // Rescale from the original 32 kHz: the frames come m_srRatio times as fast, so keep the decay time in seconds, the same amount of
    // energy per second, and the damp cutoff in Hz.
    p.decayFactor = 1.0f / (decay * m_srRatio);
    p.damp = damp / m_srRatio;
    p.energyScale = 1.0f / (float)venus::kLaps / m_srRatio;

    if (detune > 0.03f) { // gives a 10% knob range at noon for no detuning
        detune -= 0.029f;
        p.detuneMode = detuneParam >= 0.0f ? 2 : 0;
        p.detuneMultiplier = detuneParam >= 0.0f ? 1 : -1;
    } else {
        p.detuneMode = 1;
        p.detuneMultiplier = 1;
    }

    const float octaveUpRatePerSecond = std::pow(8.0f, shimmer) - 1.0f;
    const float octaveUpRatePerInterval = std::min(0.75f, octaveUpRatePerSecond / m_sampleRate * kIntervalSamples);
    // The fifths are independent of the shimmer control
    const float octaveUpRatePerSecond2 = std::pow(8.0f, shimmerTone) - 1.0f;
    const float octaveUpRatePerInterval2 = std::min(0.75f, octaveUpRatePerSecond2 / m_sampleRate * kIntervalSamples);
    p.shimmerDouble = octaveUpRatePerInterval * (1.0f - shimmerTone / 1.58f);
    p.shimmerTriple = (octaveUpRatePerInterval2 / 1.58f) * shimmerTone;
    p.shimmerRemainder = 1.0f - p.shimmerDouble - p.shimmerTriple;

    const float detuneRatePerSecond = std::pow(8.0f, detune) - 1.0f;
    p.detuneDouble = std::min(0.75f, detuneRatePerSecond / m_sampleRate * kIntervalSamples);
    p.detuneRemainder = p.detuneMode == 1 ? 1.0f : 1.0f - p.detuneDouble;

    m_reverb.SetParams(p);
}

void VenusModule::ProcessSamples(const float *in, float *outL, float *outR, size_t n) {
    // Advance the drift oscillators on their own slow schedule
    m_driftAccum += (int)n;
    while (m_driftAccum >= kDriftIntervalSamples) {
        m_driftAccum -= kDriftIntervalSamples;
        if (GetParameterAsBinnedValue(DRIFT) != 2) {
            for (int i = 0; i < 4; i++) m_driftValue[i] = std::fabs(m_drift[i].Process());
        }
    }

    // Run the stages of the frame that is being processed: kStepsPerHop of them per hop, however the samples are divided into blocks
    m_stepAccum += (int)n * kStepsPerHop;
    const int hop = (int)m_stft->stride();
    while (m_stepAccum >= hop) {
        m_stepAccum -= hop;
        m_stft->Step();
    }

    m_paramAccum += (int)n;
    if (m_paramAccum >= kParamIntervalSamples) {
        m_paramAccum = 0;
        UpdateFrameParams();
    }

    const float mix = GetParameterAsFloat(MIX);
    const float dry = 1.0f - mix;
    const int lofi = GetParameterAsBinnedValue(LOFI) - 1;

    for (size_t i = 0; i < n; i++) {
        const float x = in[i];
        float wet = m_stft->Process(x);
        if (lofi == 0) {
            wet = m_lowpass.Process(m_reducer.Process(wet));
        } else if (lofi == 2) {
            wet = m_reducer.Process(wet);
        }
        const float y = (wet * mix + x * dry) * kOutputGain;
        outL[i] = y;
        outR[i] = y; // mono processing
    }
}

void VenusModule::ProcessMono(float in) {
    BaseEffectModule::ProcessMono(in);

    float outL, outR;
    ProcessSamples(&in, &outL, &outR, 1);
    m_audioLeft = outL;
    m_audioRight = outR;
}

void VenusModule::ProcessStereo(float inL, float /*inR*/) {
    // Mono in (the left input), the same signal on both outputs
    ProcessMono(inL);
}

void VenusModule::ProcessMonoBlock(const float *in, float *outL, float *outR, size_t n) {
    ProcessSamples(in, outL, outR, n);
    m_audioLeft = outL[n - 1];
    m_audioRight = outR[n - 1];
}

void VenusModule::ProcessStereoBlock(const float *inL, const float * /*inR*/, float *outL, float *outR, size_t n) {
    ProcessMonoBlock(inL, outL, outR, n);
}

float VenusModule::GetBrightnessForLED(int led_id) const {
    float value = BaseEffectModule::GetBrightnessForLED(led_id);

    // The second LED shows the freeze
    if (led_id == 1) {
        return value * ((GetParameterAsBool(FREEZE) || m_footswitchFreeze) ? 1.0f : 0.0f);
    }

    return value;
}
