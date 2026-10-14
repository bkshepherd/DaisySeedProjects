// Saturn: spectral delay, adapted from the Saturn pedal of the FunBox project (https://github.com/GuitarML/Funbox, MIT licence, Copyright (c) 2024 Keith Bloemer),
// which adapts DaisySTFT (https://github.com/amcerbu/DaisySTFT). See Saturn/README.md for the licences and the list of changes.
#include "saturn_module.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

using namespace bkshepherd;
using namespace daisysp;

namespace {
constexpr float kPi = 3.14159265358979f;
constexpr int kSlots = 2 * (int)saturn::kLaps + 1;
constexpr int kLines = saturn::kLinesPerSet;
constexpr int kRef = saturn::kReferenceBins;
// The transform frames are 256 samples apart, which is when the delay lines are advanced: 187.5 times per second at 48 kHz. The longest delay is 4 seconds.
constexpr size_t kMaxDelay = 188 * 4;
constexpr size_t kMaxPreDelay = 96000;
constexpr float kPreDelayMin = 0.05f; // seconds
constexpr float kPreDelayMax = 1.0f;  // seconds
constexpr float kDelayGain = 3.0f;    // the original's constant gain on the wet signal
constexpr int kDriftIntervalSamples = 288; // the drift oscillator (0 .. 1 Hz) is advanced once per this many samples (a multiple of the control interval)
constexpr int kControlIntervalSamples = 48; // the controls are turned into delay settings at most this often
// The stages of one frame (the forward FFT, kChunks chunks of bins, the inverse FFT) are issued at 3/2 of the rate they need (see venus_module.cpp)
constexpr int kStepsPerHop = (int)((saturn::kChunks + 2) * 3 / 2);
constexpr int kClearPerTick = 25; // delay lines cleared per control update after a switch between mono and stereo

// Output makeup gain. Calibrated with tests/host/run_level_tests.sh so that the default settings give about +2 dB, like the other effects.
constexpr float kOutputGain = 1.75f;

// On-chip RAM that is otherwise unused (see nam_a2_sections.lds): the transforms read and write their frames many times, which is far slower in SDRAM.
#define SATURN_FAST_DATA __attribute__((section(".sram_d2_bss")))

// Frames of the two channels: `in` holds every overlapping frame (the input, then the transform's output); the frequency-domain work buffers are shared by the frames
// of one channel, since only one is processed at a time. SDRAM and the fast RAM are not zeroed at startup, Init() clears them.
SATURN_FAST_DATA float s_in[2][saturn::kN * kSlots];
SATURN_FAST_DATA float s_middle[2][saturn::kN];
SATURN_FAST_DATA float s_out[2][saturn::kN];
SATURN_FAST_DATA soundmath::CmsisRealFft<saturn::kN> s_fft; // shared by both channels (a transform runs to its end inside one pipeline step)
SATURN_FAST_DATA float s_hannTable[saturn::kN]; // the window at the frame positions (see SlicedFourier)
SATURN_FAST_DATA soundmath::Wave<float> s_hann([](float phase) -> float { return 0.5f * (1.0f - std::cos(2.0f * kPi * phase)); });

// A delay line with feedback whose samples are in SDRAM (they are too many for the on-chip RAM) but whose bookkeeping is in the fast on-chip RAM. The same delay line
// as DaisySP's DelayLine (written before it is read, interpolated read, the write position counts down), but DelayLine keeps its position, delay and fractional part in the
// same object as the samples: in SDRAM, every update of a line took about ten slow accesses on top of the two reads and one write that are needed (measured on the pedal:
// 2 microseconds per line, so one chunk of 32 bins in both channels took a quarter of an audio block).
template <size_t Size> struct SdramDelay {
    float *data;
    uint32_t writePos;
    float currentDelay;
    float delayTarget;
    float feedback;

    void Init(float *samples) {
        data = samples;
        Clear();
        currentDelay = 0.0f;
        delayTarget = 0.0f;
        feedback = 0.0f;
    }
    void Clear() {
        std::memset(data, 0, Size * sizeof(float));
        writePos = 0;
    }
    float Process(float in) {
        fonepole(currentDelay, delayTarget, 0.0002f); // glide to the target delay
        // At least one call: the line is read before it is written, so a delay of 0 would read the oldest content of the whole line (4 seconds ago), not the newest
        const float delay = std::max(currentDelay, 1.0f);
        int32_t whole = static_cast<int32_t>(delay);
        const float frac = delay - static_cast<float>(whole);
        if (static_cast<size_t>(whole) >= Size) whole = (int32_t)Size - 1;
        uint32_t a = writePos + (uint32_t)whole;
        if (a >= Size) a -= Size;
        const uint32_t b = (a + 1 >= Size) ? a + 1 - Size : a + 1;
        const float read = data[a] + (data[b] - data[a]) * frac;
        data[writePos] = feedback * read + in;
        writePos = (writePos == 0) ? (uint32_t)Size - 1 : writePos - 1;
        return read;
    }
};

// One delay line per bin and part (real, imaginary) of the spectrum. [set][part][bin]: set 0 serves the left channel (and the lower bins in mono mode), set 1 the right
// channel (and the upper bins in mono mode).
using SpectLine = SdramDelay<kMaxDelay>;
using PreLine = SdramDelay<kMaxPreDelay>;
SATURN_FAST_DATA SpectLine s_lines[2][2][kLines];
SATURN_FAST_DATA PreLine s_pre[2];
float DSY_SDRAM_BSS s_lineData[2][2][kLines][kMaxDelay]; // 1.5 MB
float DSY_SDRAM_BSS s_preData[2][kMaxPreDelay];          // 768 KB

const char *s_timeModeNames[3] = {"Linear", "Sine", "Random"};
const char *s_fdbkModeNames[3] = {"Same", "Linear", "Random"};
const char *s_driftNames[3] = {"Delay", "Off", "+PreDelay"};

bool knobMoved(float oldValue, float newValue) { return std::fabs(newValue - oldValue) > 0.005f; }
} // namespace

static const auto s_metaData = [] {
    std::array<ParameterMetaData, SaturnModule::PARAM_COUNT> params{};

    params[SaturnModule::PREDELAY] = {
        name : "PreDelay",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.0f}, // 50 ms .. 1 s of delay before the spectral delay
        knobMapping : 0,
        midiCCMapping : 14
    };
    params[SaturnModule::MIX] = {
        name : "Mix",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.4f},
        knobMapping : 1,
        midiCCMapping : 15
    };
    params[SaturnModule::FILTER] = {
        name : "Filter",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f}, // low-pass to the left of the middle, high-pass to the right, nothing in the middle
        knobMapping : 2,
        midiCCMapping : 16
    };
    params[SaturnModule::TIME] = {
        name : "Time",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f}, // 0 .. 4 seconds of delay spread over the bins; what it does depends on Time Mode
        knobMapping : 3,
        midiCCMapping : 17
    };
    params[SaturnModule::FEEDBACK] = {
        name : "Feedback",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.3f},
        knobMapping : 4,
        midiCCMapping : 18
    };
    params[SaturnModule::DRIFT_RATE] = {
        name : "DriftRate",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.1f}, // 0 .. 1 Hz; only matters when Drift is on
        knobMapping : 5,
        midiCCMapping : 19
    };
    params[SaturnModule::TIME_MODE] = {
        name : "Time Mode",
        valueType : ParameterValueType::Binned,
        valueBinCount : 3,
        valueBinNames : s_timeModeNames,
        defaultValue : {.uint_value = 0}, // Linear: the delay grows with frequency; Sine: a sine wave across the bins; Random: a random delay for every bin
        knobMapping : -1,
        midiCCMapping : 20
    };
    params[SaturnModule::FDBK_MODE] = {
        name : "Fdbk Mode",
        valueType : ParameterValueType::Binned,
        valueBinCount : 3,
        valueBinNames : s_fdbkModeNames,
        defaultValue : {.uint_value = 0}, // Same: the same feedback for every bin; Linear: less feedback towards high frequencies; Random
        knobMapping : -1,
        midiCCMapping : 21
    };
    params[SaturnModule::DRIFT] = {
        name : "Drift",
        valueType : ParameterValueType::Binned,
        valueBinCount : 3,
        valueBinNames : s_driftNames,
        defaultValue : {.uint_value = 1}, // Delay: modulate the delay time and the feedback; Off; +PreDelay: the pre-delay too (warps the pitch)
        knobMapping : -1,
        midiCCMapping : 22
    };
    params[SaturnModule::MONO] = { // Mono processes twice as many bins with one transform and sends the result to both outputs
        name : "Mono",
        valueType : ParameterValueType::Bool,
        valueBinCount : 0,
        defaultValue : {.uint_value = 0},
        knobMapping : -1,
        midiCCMapping : 23
    };
    params[SaturnModule::STEREO_IN] = { // off = mono in (the left input feeds both sides), on = true stereo in; ignored in mono mode
        name : "Stereo In",
        valueType : ParameterValueType::Bool,
        valueBinCount : 0,
        defaultValue : {.uint_value = 0},
        knobMapping : -1,
        midiCCMapping : 24
    };
    params[SaturnModule::STEREO_SPREAD] = { // every bin gets a random level (see Saturn/README.md)
        name : "St Spread",
        valueType : ParameterValueType::Bool,
        valueBinCount : 0,
        defaultValue : {.uint_value = 0},
        knobMapping : -1,
        midiCCMapping : 25
    };
    params[SaturnModule::PRE_FDBK] = { // feedback of 0.75 on the pre-delay
        name : "PreFdbk",
        valueType : ParameterValueType::Bool,
        valueBinCount : 0,
        defaultValue : {.uint_value = 0},
        knobMapping : -1,
        midiCCMapping : 26
    };
    return params;
}();

SaturnModule::SaturnModule()
    : BaseEffectModule(), m_fft(nullptr), m_stft{nullptr, nullptr}, m_sampleRate(48000.0f), m_framesPerSecond(187.5f), m_stepAccum(0), m_driftAccum(0), m_controlAccum(0),
      m_driftMultiplier(1.0f), m_prevTime(0.0f), m_prevFdbk(0.0f), m_prevTimeMode(0), m_prevFdbkMode(0), m_prevMono(false), m_forceUpdate(true), m_clearRemaining(0),
      m_preTarget(2400.0f), m_preFeedback(0.0f), m_rng(0x2545F491u), m_tStepsMax(0), m_tOneStepMax(0), m_tLoopMax(0), m_tCtlMax(0), m_tSteps(0), m_tOneStep(0), m_tLoop(0),
      m_tCtl(0), m_tWindowSamples(0) {
    m_name = "Saturn";
    m_paramMetaData = s_metaData.data();
    this->InitParams(static_cast<int>(s_metaData.size()));
}

SaturnModule::~SaturnModule() {
    delete m_stft[0];
    delete m_stft[1];
}

float SaturnModule::NextRandom() {
    m_rng ^= m_rng << 13; // xorshift32
    m_rng ^= m_rng >> 17;
    m_rng ^= m_rng << 5;
    return (float)(m_rng >> 8) / 16777216.0f;
}

void SaturnModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);

    m_sampleRate = sample_rate;
    m_framesPerSecond = sample_rate / (float)saturn::kStride;
    m_stepAccum = m_driftAccum = m_controlAccum = 0;
    m_driftMultiplier = 1.0f;
    m_clearRemaining = 0;

    std::memset(s_in, 0, sizeof(s_in));
    std::memset(s_middle, 0, sizeof(s_middle));
    std::memset(s_out, 0, sizeof(s_out));

    m_fft = &s_fft;
    m_fft->Init();
    for (size_t i = 0; i < saturn::kN; i++) s_hannTable[i] = s_hann((float)i / (float)saturn::kN);
    for (int ch = 0; ch < 2; ch++) {
        delete m_stft[ch];
        m_stft[ch] = new soundmath::SlicedFourier<float, saturn::kN, soundmath::CmsisRealFft<saturn::kN>>(
            ch == 0 ? &SaturnModule::ProcessChunk0 : &SaturnModule::ProcessChunk1, ch == 0 ? &SaturnModule::BeginFrame0 : &SaturnModule::BeginFrame1, this, saturn::kChunks,
            m_fft, &s_hann, saturn::kLaps, s_in[ch], s_middle[ch], s_out[ch], s_hannTable);
    }

    for (int set = 0; set < 2; set++) {
        for (int part = 0; part < 2; part++) {
            for (int i = 0; i < kLines; i++) {
                s_lines[set][part][i].Init(s_lineData[set][part][i]);
            }
        }
        s_pre[set].Init(s_preData[set]);
    }

    for (int i = 0; i < kLines; i++) m_stereoField[i] = NextRandom();

    m_drift.Init(sample_rate / kDriftIntervalSamples); // advanced once per kDriftIntervalSamples
    m_drift.SetFreq(0.1f);
    m_drift.SetWaveform(Oscillator::WAVE_SIN);
    m_drift.SetAmp(0.5f);

    // ParameterChanged only fires on a change, so everything derived from the controls is set up here
    m_forceUpdate = true;
    m_prevMono = GetParameterAsBool(MONO);
    UpdateControls();
    // the first frames use the controls as they are now, and the delays start at the settings of the controls (the original's start at 0 and glide up to them
    // over many seconds, see SpectLine::Process)
    m_frame[0] = m_frame[1] = m_pendingFrame;
    for (int set = 0; set < 2; set++) {
        for (int part = 0; part < 2; part++)
            for (int i = 0; i < kLines; i++) s_lines[set][part][i].currentDelay = s_lines[set][part][i].delayTarget;
        s_pre[set].currentDelay = s_pre[set].delayTarget;
    }
}

void SaturnModule::ParameterChanged(int /*parameter_id*/) {
    // Nothing to do immediately: UpdateControls() reads the controls and only recomputes the delay settings that depend on what changed
}

// Turns the controls into the delay settings, the filter and the pre-delay. Called at most once per kControlIntervalSamples samples from the audio path.
void SaturnModule::UpdateControls() {
    float vPre = GetParameterAsFloat(PREDELAY);
    const float vFilter = GetParameterAsFloat(FILTER);
    float vTime = GetParameterAsFloat(TIME);
    float vFdbk = GetParameterAsFloat(FEEDBACK);
    const int timeMode = GetParameterAsBinnedValue(TIME_MODE) - 1;
    const int fdbkMode = GetParameterAsBinnedValue(FDBK_MODE) - 1;
    const int driftMode = GetParameterAsBinnedValue(DRIFT) - 1;
    const bool mono = GetParameterAsBool(MONO);

    // Drift: slow modulation of the delay time and feedback (and the pre-delay with +PreDelay)
    m_drift.SetFreq(GetParameterAsFloat(DRIFT_RATE)); // 0 .. 1 Hz
    if (driftMode != 1) {
        if (driftMode == 2) vPre *= m_driftMultiplier;
        vTime *= m_driftMultiplier;
        vFdbk *= (1.0f - m_driftMultiplier); // the feedback drifts the other way
    }

    // A switch between mono and stereo moves bins between the delay lines, so what they hold would ring at the wrong frequencies: clear them, a few per call
    if (mono != m_prevMono) {
        m_clearRemaining = 4 * kLines;
        m_prevMono = mono;
    }
    if (m_clearRemaining > 0) {
        for (int n = 0; n < kClearPerTick && m_clearRemaining > 0; n++, m_clearRemaining--) {
            const int idx = m_clearRemaining - 1; // 0 .. 4 * kLines - 1
            s_lines[idx / (2 * kLines)][(idx / kLines) % 2][idx % kLines].Clear();
        }
    }

    // The filter works on the bins: low-pass left of the middle, high-pass right of it. Its range is a fraction of the bins that are delayed (as in the original, where mono
    // mode, with twice the bins, reaches twice the frequency)
    const int size = mono ? 2 * kLines : kLines;
    FrameParams f;
    f.mono = mono;
    f.activeBins = size;
    f.spread = GetParameterAsBool(STEREO_SPREAD);
    if (vFilter < 0.45f) {
        f.filterMode = 0;
        f.filterBin = (unsigned)std::floor(vFilter * 2.22f * (size - 5) + 4);
    } else if (vFilter > 0.55f) {
        f.filterMode = 1;
        f.filterBin = (unsigned)std::floor((vFilter - 0.55f) * 0.5f * size);
    } else {
        f.filterMode = 2;
        f.filterBin = 0;
    }
    m_pendingFrame = f;

    // Pre-delay: 50 ms .. 1 s, optionally with feedback
    m_preTarget = m_sampleRate * (kPreDelayMin + (kPreDelayMax - kPreDelayMin) * vPre);
    m_preFeedback = GetParameterAsBool(PRE_FDBK) ? 0.75f : 0.0f;
    for (int c = 0; c < 2; c++) {
        s_pre[c].delayTarget = m_preTarget;
        s_pre[c].feedback = m_preFeedback;
    }

    const float fps = m_framesPerSecond;
    const bool modesChanged = timeMode != m_prevTimeMode || fdbkMode != m_prevFdbkMode;

    if (m_forceUpdate || modesChanged || knobMoved(vTime, m_prevTime)) {
        const float cycles = 4.0f; // in Sine mode, how many sine waves fit across the bins
        for (int i = 0; i < kLines; i++) {
            float target;
            if (timeMode == 2) {
                target = NextRandom() * 4.0f * fps * vTime; // random delay for every bin, up to 4 seconds
            } else if (timeMode == 1) {
                target = (std::sin((i * cycles / kRef) * 2.0f * kPi) + 1.0f) * vTime * fps * 2.0f; // a sine wave, 0 .. 4 seconds
            } else {
                target = vTime * 4.0f * fps * i / kRef; // linear: 0 up to 4 seconds over the original's 175 bins (so 2.9 s at the top of ours), Time sets the slope
            }
            for (int set = 0; set < 2; set++)
                for (int part = 0; part < 2; part++) s_lines[set][part][i].delayTarget = target;
        }
        m_prevTime = vTime;
    }

    if (m_forceUpdate || modesChanged || knobMoved(vFdbk, m_prevFdbk)) {
        for (int i = 0; i < kLines; i++) {
            float fb;
            if (fdbkMode == 2) {
                fb = NextRandom();
                for (int set = 0; set < 2; set++)
                    for (int part = 0; part < 2; part++) s_lines[set][part][i].feedback = fb;
            } else if (fdbkMode == 1) { // linear: less feedback towards high frequencies, over the original's 175 bins
                fb = vFdbk * (kRef - 1 - i) / kRef;
                for (int set = 0; set < 2; set++)
                    for (int part = 0; part < 2; part++) s_lines[set][part][i].feedback = fb;
            } else {
                for (int set = 0; set < 2; set++)
                    for (int part = 0; part < 2; part++) s_lines[set][part][i].feedback = vFdbk;
            }
        }
        m_prevFdbk = vFdbk;
    }

    m_prevTimeMode = timeMode;
    m_prevFdbkMode = fdbkMode;
    m_forceUpdate = false;
}

// shy_fft's layout is [real, real, ..., imag, imag, ...]; only the first kLines (or twice that in mono mode) bins get a delay line
void SaturnModule::ProcessBins(int channel, const float *in, float *out, size_t first, size_t last) {
    const FrameParams &f = m_frame[channel];
    const size_t offset = saturn::kBins; // offset of the imaginary parts

    for (size_t i = first; i < last; i++) {
        float real = 0.0f;
        float imag = 0.0f;

        if ((int)i < f.activeBins && ((f.filterMode == 0 && i < f.filterBin) || (f.filterMode == 1 && i > f.filterBin) || f.filterMode == 2)) {
            real = in[i];
            imag = in[i + offset];

            if (!f.mono) {
                const float gain = f.spread ? 1.0f - m_stereoField[i] : 1.0f;
                real = s_lines[channel][0][i].Process(real) * gain;
                imag = s_lines[channel][1][i].Process(imag) * gain;
            } else if ((int)i < kLines) { // mono: the lower bins use the left delay lines, the upper bins the right ones
                real = s_lines[0][0][i].Process(real);
                imag = s_lines[0][1][i].Process(imag);
            } else {
                real = s_lines[1][0][i - kLines].Process(real);
                imag = s_lines[1][1][i - kLines].Process(imag);
            }
        }
        out[i] = real;
        out[i + offset] = imag;
    }
}

void SaturnModule::ProcessSamples(const float *inL, const float *inR, float *outL, float *outR, size_t n, bool stereoInput) {
    const bool mono = GetParameterAsBool(MONO);
    const bool stereo = !mono && stereoInput && GetParameterAsBool(STEREO_IN);
    const float mix = GetParameterAsFloat(MIX);

    // The drift oscillator and the controls are updated at the start of a call when a whole interval has passed since the last update (the intervals are multiples
    // of 48, the pedal's block size, so that the update happens at the same sample however the samples are divided into calls)
    if (m_driftAccum == 0) m_driftMultiplier = m_drift.Process() + 0.5f; // 0 .. 1
    m_driftAccum = (m_driftAccum + (int)n >= kDriftIntervalSamples) ? 0 : m_driftAccum + (int)n;
    const uint32_t tStart = System::GetTick();
    if (m_controlAccum == 0) UpdateControls();
    const uint32_t tCtl = System::GetTick() - tStart;
    m_controlAccum = (m_controlAccum + (int)n >= kControlIntervalSamples) ? 0 : m_controlAccum + (int)n;

    // Run the stages of the frames being processed: kStepsPerHop of them per hop, however the samples are divided into blocks
    m_stepAccum += (int)n * kStepsPerHop;
    uint32_t tOneStep = 0;
    const uint32_t tSteps0 = System::GetTick();
    while (m_stepAccum >= (int)saturn::kStride) {
        m_stepAccum -= (int)saturn::kStride;
        const uint32_t t = System::GetTick();
        m_stft[0]->Step();
        if (!mono) m_stft[1]->Step();
        tOneStep = std::max(tOneStep, System::GetTick() - t);
    }
    const uint32_t tSteps = System::GetTick() - tSteps0;
    const uint32_t tLoop0 = System::GetTick();

    for (size_t i = 0; i < n; i++) {
        const float l = inL[i];
        const float r = stereo ? inR[i] : l; // MISO: the left input feeds both sides
        const float preL = s_pre[0].Process(l);
        if (mono) {
            const float wet = m_stft[0]->Process(preL);
            const float y = (wet * mix * kDelayGain + l * (1.0f - mix)) * kOutputGain;
            outL[i] = y;
            outR[i] = y;
        } else {
            const float preR = s_pre[1].Process(r);
            const float wetL = m_stft[0]->Process(preL);
            const float wetR = m_stft[1]->Process(preR);
            outL[i] = (wetL * mix * kDelayGain + l * (1.0f - mix)) * kOutputGain;
            outR[i] = (wetR * mix * kDelayGain + r * (1.0f - mix)) * kOutputGain;
        }
    }

    // Timing: the slowest call of each part, as a percentage of the time n samples last
    const uint32_t tLoop = System::GetTick() - tLoop0;
    const float pct = 100.0f * m_sampleRate / ((float)System::GetTickFreq() * (float)n);
    m_tCtlMax = std::max(m_tCtlMax, (float)tCtl * pct);
    m_tStepsMax = std::max(m_tStepsMax, (float)tSteps * pct);
    m_tOneStepMax = std::max(m_tOneStepMax, (float)tOneStep * pct);
    m_tLoopMax = std::max(m_tLoopMax, (float)tLoop * pct);
    m_tWindowSamples += (int)n;
    if (m_tWindowSamples >= 2 * (int)m_sampleRate) {
        m_tWindowSamples = 0;
        m_tCtl = m_tCtlMax;
        m_tSteps = m_tStepsMax;
        m_tOneStep = m_tOneStepMax;
        m_tLoop = m_tLoopMax;
        m_tCtlMax = m_tStepsMax = m_tOneStepMax = m_tLoopMax = 0.0f;
    }
}

void SaturnModule::GetDebugText(char *buffer, size_t size) const {
    // slowest call in the last two seconds, % of the block time: stg = all pipeline stages in one call, one = the slowest single stage (both channels), smp = the per-sample loop, ctl = control update
    snprintf(buffer, size, "stg%d one%d\nsmp%d ctl%d", (int)(m_tSteps + 0.5f), (int)(m_tOneStep + 0.5f), (int)(m_tLoop + 0.5f), (int)(m_tCtl + 0.5f));
}

void SaturnModule::ProcessMono(float in) {
    BaseEffectModule::ProcessMono(in);
    float outL, outR;
    ProcessSamples(&in, &in, &outL, &outR, 1, false);
    m_audioLeft = outL;
    m_audioRight = outR;
}

void SaturnModule::ProcessStereo(float inL, float inR) {
    float outL, outR;
    ProcessSamples(&inL, &inR, &outL, &outR, 1, true);
    m_audioLeft = outL;
    m_audioRight = outR;
}

void SaturnModule::ProcessMonoBlock(const float *in, float *outL, float *outR, size_t n) {
    ProcessSamples(in, in, outL, outR, n, false);
    m_audioLeft = outL[n - 1];
    m_audioRight = outR[n - 1];
}

void SaturnModule::ProcessStereoBlock(const float *inL, const float *inR, float *outL, float *outR, size_t n) {
    ProcessSamples(inL, inR, outL, outR, n, true);
    m_audioLeft = outL[n - 1];
    m_audioRight = outR[n - 1];
}
