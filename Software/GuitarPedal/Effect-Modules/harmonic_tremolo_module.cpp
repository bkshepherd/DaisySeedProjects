#include "harmonic_tremolo_module.h"
#include "../Util/audio_utilities.h"
#include <array>

using namespace bkshepherd;

namespace {

// LFO range. Slower than the plain tremolo module because the band sweep stays
// musical well below 1 Hz.
constexpr float kSpeedMin = 0.2f;
constexpr float kSpeedMax = 16.0f;

// The depth knob is scaled past unity on purpose. Above 0.8 the high band
// briefly inverts at the LFO peaks, which is where the throaty, almost phasey
// character of this effect comes from.
constexpr float kDepthScale = 1.25f;

// Recombining the two bands loses a little level, so the output is trimmed
// back up. Flick uses a flat 1.2 here, but it can afford to: downstream it
// feeds a reverb and a hard limiter. Standing alone the flat value is a
// problem, because the two counter phase bands sum to more energy as depth
// rises. Measured over the depth range that is about 3.8dB of drift, ending
// at a peak of 1.49, and the pedal writes straight out to the codec with
// nothing to catch it.
//
// So the gain is scaled by the RMS of the modulators instead, sqrt(1 + m^2/2)
// for a bipolar sine of amplitude m. That holds the output inside a 1.3dB
// window at every depth setting and keeps peaks under about 1.12. Turning
// depth up then changes the character rather than the volume.
constexpr float kMakeupGainBase = 1.2f;

// Band split, taken from the Fender 6G12-A schematic.
constexpr float kBandSplitLowCutoff = 144.0f;  // 220K and 5nF low pass
constexpr float kBandSplitHighCutoff = 636.0f; // 1M and 250pF high pass

// Fixed EQ voicing applied after the bands are recombined.
constexpr float kEqHighPassCutoff = 63.0f;
constexpr float kEqLowPassCutoff = 11200.0f;
constexpr float kEqLowShelfFreq = 37.0f;
constexpr float kEqLowShelfGain = -10.5f; // in dB
constexpr float kEqLowShelfQ = 1.0f;
constexpr float kEqPeakLowMidFreq = 254.0f;
constexpr float kEqPeakLowMidGain = 2.0f; // in dB
constexpr float kEqPeakLowMidQ = 0.707f;
constexpr float kEqPeakPresenceFreq = 7500.0f;
constexpr float kEqPeakPresenceGain = -3.37f; // in dB
constexpr float kEqPeakPresenceQ = 0.263f;

// Dummy values that get overwritten in Init
constexpr float kDefaultSampleRate = 48000.0f;

/** Configures a low shelf from an explicit Q.
 *
 * cycfi::q::lowshelf accepts a q argument but discards it, hardcoding a shelf
 * slope of S = 1 (its beta is sqrt(2A)). At -10.5dB that is a noticeably
 * steeper shelf than the Q = 1 this effect is voiced around, so the standard
 * audio EQ cookbook coefficients are computed here and pushed into a plain
 * biquad instead. cycfi::q::peaking needs no such treatment, it already
 * derives alpha from q the same way.
 */
void ConfigureLowShelf(cycfi::q::biquad &filter, float gainDb, float freq, float q, float sample_rate) {
    const float a = powf(10.0f, gainDb / 40.0f);
    const float omega = 2.0f * PI_F * freq / sample_rate;
    const float sinOmega = sinf(omega);
    const float cosOmega = cosf(omega);
    const float alpha = sinOmega / (2.0f * q);
    const float beta = 2.0f * sqrtf(a) * alpha;

    const float b0 = a * ((a + 1.0f) - (a - 1.0f) * cosOmega + beta);
    const float b1 = 2.0f * a * ((a - 1.0f) - (a + 1.0f) * cosOmega);
    const float b2 = a * ((a + 1.0f) - (a - 1.0f) * cosOmega - beta);
    const float a0 = (a + 1.0f) + (a - 1.0f) * cosOmega + beta;
    const float a1 = -2.0f * ((a - 1.0f) + (a + 1.0f) * cosOmega);
    const float a2 = (a + 1.0f) + (a - 1.0f) * cosOmega - beta;

    filter.config(b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0);
}

/** Makeup gain for a given modulation amount, see kMakeupGainBase. */
float MakeupGainForDepth(float modulation) {
    return kMakeupGainBase / sqrtf(1.0f + modulation * modulation * 0.5f);
}

} // namespace

static const auto s_metaData = [] {
    std::array<ParameterMetaData, HarmonicTremoloModule::PARAM_COUNT> params{};

    params[HarmonicTremoloModule::SPEED] = {
        name : "Speed",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.25f},
        knobMapping : 0,
        midiCCMapping : 14
    };

    params[HarmonicTremoloModule::DEPTH] = {
        name : "Depth",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 1,
        midiCCMapping : 15
    };

    return params;
}();

// Default Constructor
HarmonicTremoloModule::HarmonicTremoloModule()
    : BaseEffectModule(), m_eqLowShelf{cycfi::q::biquad(1.0f, 0.0f, 0.0f, 0.0f, 0.0f), cycfi::q::biquad(1.0f, 0.0f, 0.0f, 0.0f, 0.0f)},
      m_eqPeakLowMid{cycfi::q::peaking(kEqPeakLowMidGain, cycfi::q::frequency{kEqPeakLowMidFreq}, kDefaultSampleRate, kEqPeakLowMidQ),
                     cycfi::q::peaking(kEqPeakLowMidGain, cycfi::q::frequency{kEqPeakLowMidFreq}, kDefaultSampleRate, kEqPeakLowMidQ)},
      m_eqPeakPresence{
          cycfi::q::peaking(kEqPeakPresenceGain, cycfi::q::frequency{kEqPeakPresenceFreq}, kDefaultSampleRate, kEqPeakPresenceQ),
          cycfi::q::peaking(kEqPeakPresenceGain, cycfi::q::frequency{kEqPeakPresenceFreq}, kDefaultSampleRate, kEqPeakPresenceQ)},
      m_speedSmoothed(kSpeedMin), m_depthSmoothed(0.0f), m_makeupGain(kMakeupGainBase), m_lastLfoValue(0.0f) {
    // Set the name of the effect
    m_name = "Harm Trem";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData.data();

    // Initialize Parameters for this Effect
    this->InitParams(static_cast<int>(s_metaData.size()));
}

// Destructor
HarmonicTremoloModule::~HarmonicTremoloModule() {
    // No Code Needed
}

void HarmonicTremoloModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);

    m_lfo.Init(sample_rate);
    m_lfo.SetWaveform(Oscillator::WAVE_SIN);

    for (int channel = 0; channel < 2; channel++) {
        m_bandLow[channel].Init(kBandSplitLowCutoff, sample_rate);
        m_bandHigh[channel].Init(kBandSplitHighCutoff, sample_rate);

        m_eqHighPass[channel].Init(kEqHighPassCutoff, sample_rate);
        m_eqLowPass[channel].Init(kEqLowPassCutoff, sample_rate);

        ConfigureLowShelf(m_eqLowShelf[channel], kEqLowShelfGain, kEqLowShelfFreq, kEqLowShelfQ, sample_rate);
        m_eqPeakLowMid[channel].config(kEqPeakLowMidGain, cycfi::q::frequency{kEqPeakLowMidFreq}, sample_rate, kEqPeakLowMidQ);
        m_eqPeakPresence[channel].config(kEqPeakPresenceGain, cycfi::q::frequency{kEqPeakPresenceFreq}, sample_rate, kEqPeakPresenceQ);
    }

    // Seed the smoothed values so the first buffer starts at the knob
    // positions instead of sliding up to them.
    m_speedSmoothed = kSpeedMin + GetParameterAsFloat(SPEED) * (kSpeedMax - kSpeedMin);
    m_depthSmoothed = GetParameterAsFloat(DEPTH) * kDepthScale;
    m_makeupGain = MakeupGainForDepth(m_depthSmoothed);
    m_lastLfoValue = 0.0f;
}

float HarmonicTremoloModule::ProcessChannel(int channel, float in, float lfo) {
    // Split into the two bands. These overlap in the middle, which is what
    // keeps the recombined signal from sounding scooped.
    const float low = m_bandLow[channel].Process(in);
    const float high = m_bandHigh[channel].Process(in);

    // Modulate the bands against each other rather than modulating amplitude.
    float out = low * (1.0f + lfo) + high * (1.0f - lfo);

    // Fixed voicing. Order matters here, the shelf and peaks are trimming what
    // the band pair leaves behind.
    out = m_eqHighPass[channel].Process(out);
    out = m_eqLowPass[channel].Process(out);
    out = m_eqLowShelf[channel](out);
    out = m_eqPeakLowMid[channel](out);
    out = m_eqPeakPresence[channel](out);

    return out;
}

void HarmonicTremoloModule::ProcessMono(float in) {
    BaseEffectModule::ProcessMono(in);

    const float speedTarget = kSpeedMin + GetParameterAsFloat(SPEED) * (kSpeedMax - kSpeedMin);
    const float depthTarget = GetParameterAsFloat(DEPTH) * kDepthScale;

    // Ease both into their targets to avoid zipper noise on knob moves.
    fonepole(m_speedSmoothed, speedTarget, .01f);
    fonepole(m_depthSmoothed, depthTarget, .01f);

    m_lfo.SetFreq(m_speedSmoothed);
    m_lfo.SetAmp(m_depthSmoothed);
    m_lastLfoValue = m_lfo.Process();

    // Track the gain against the smoothed depth so it never steps.
    m_makeupGain = MakeupGainForDepth(m_depthSmoothed);

    m_audioLeft = ProcessChannel(0, m_audioLeft, m_lastLfoValue) * m_makeupGain;
    m_audioRight = m_audioLeft;
}

void HarmonicTremoloModule::ProcessStereo(float inL, float inR) {
    // Calculate the mono effect, which also advances the LFO for both channels
    ProcessMono(inL);

    // Do the base stereo calculation (which resets the right signal to be the
    // inputR instead of combined mono)
    BaseEffectModule::ProcessStereo(m_audioLeft, inR);

    // Reuse the LFO value and gain from the left channel so the two stay
    // locked together instead of drifting into an unintended auto-pan.
    m_audioRight = ProcessChannel(1, m_audioRight, m_lastLfoValue) * m_makeupGain;
}

void HarmonicTremoloModule::SetTempo(uint32_t bpm) {
    float freq = tempo_to_freq(bpm);

    if (freq <= kSpeedMin) {
        SetParameterAsMagnitude(SPEED, 0.0f);
    } else if (freq >= kSpeedMax) {
        SetParameterAsMagnitude(SPEED, 1.0f);
    } else {
        float magnitude = (freq - kSpeedMin) / (kSpeedMax - kSpeedMin);
        SetParameterAsMagnitude(SPEED, magnitude);
    }
}

float HarmonicTremoloModule::GetBrightnessForLED(int led_id) const {
    float value = BaseEffectModule::GetBrightnessForLED(led_id);

    if (led_id == 1) {
        // The LFO is bipolar and can exceed unity at high depth, so fold it
        // into 0..1 for the LED.
        return value * fclamp(0.5f + 0.5f * m_lastLfoValue, 0.0f, 1.0f);
    }

    return value;
}
