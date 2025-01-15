#include "polyoctave_module.h"
#include "../Util/audio_utilities.h"

#include <q/fx/biquad.hpp>
#include <q/support/literals.hpp>

//#include "../Util/EffectState.h"
#include "../Util/Multirate.h"
#include "../Util/OctaveGenerator.h"

using namespace bkshepherd;
namespace q = cycfi::q;
using namespace q::literals;

static Decimator2 decimate;
static Interpolator interpolate;
static const auto sample_rate_temp =
    48000; // hard code for now                          // NOTE: the sample_rate must be divisible by the resample_factor (48/6 = 8)
static OctaveGenerator octave(sample_rate_temp / resample_factor); // resample_factor is defined in Multirate.h and equals 6
static q::highshelf eq1(-11, 140_Hz, sample_rate_temp);
static q::lowshelf eq2(5, 160_Hz, sample_rate_temp);

static const int s_paramCount = 4;
static const ParameterMetaData s_metaData[s_paramCount] = {
    {name : "Dry", valueType : ParameterValueType::Float, defaultValue : {.float_value = 0.5f}, knobMapping : 0, midiCCMapping : 14},
    {
        name : "2 OctDown",
        valueType : ParameterValueType::Float,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 1,
        midiCCMapping : 15
    },
    {
        name : "1 OctDown",
        valueType : ParameterValueType::Float,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 2,
        midiCCMapping : 16
    },
    {
        name : "1 OctUp",
        valueType : ParameterValueType::Float,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 3,
        midiCCMapping : 17
    },
};

// Default Constructor
PolyOctaveModule::PolyOctaveModule()
    : BaseEffectModule(), m_tremoloFreqMin(1.0f), m_tremoloFreqMax(20.0f), m_freqOscFreqMin(0.01f), m_freqOscFreqMax(1.0f),
      m_cachedEffectMagnitudeValue(1.0f) {
    // Set the name of the effect
    m_name = "PolyOctave";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData;

    // Initialize Parameters for this Effect
    this->InitParams(s_paramCount);
}

// Destructor
PolyOctaveModule::~PolyOctaveModule() {
    // No Code Needed
}

void PolyOctaveModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);

    // Initialize buffers to 0
    for (int j = 0; j < 6; ++j) {
        buff[j] = 0.0;
        buff_out[j] = 0.0;
    }
}

void PolyOctaveModule::ProcessMono(float in) {
    BaseEffectModule::ProcessMono(in);

    float input = m_audioLeft;
    buff[bin_counter] =
        m_audioLeft; // making a workaround for only processing sample by sample instead of block, will add 6 samples of latency

    float dryLevel = GetParameterAsFloat(0);
    float down1Level = GetParameterAsFloat(1);
    float down2Level = GetParameterAsFloat(2);
    float up1Level = GetParameterAsFloat(3);

    // for (size_t i = 0; i <= (size - resample_factor); i += resample_factor)  // Every 6 samples until block size
    //{

    // do calculation every 6 samples
    if (bin_counter > 4) {

        std::span<const float, resample_factor> in_chunk(&(buff[0]), resample_factor); // std::span is c++ 20 feature

        const auto sample = decimate(in_chunk);

        float octave_mix = 0;
        octave.update(sample);
        octave_mix += up1Level * octave.up1() * 4.0; // TODO May need to update parameter scaling from 0 to 1 to 1 to 20?
        octave_mix += down1Level * octave.down1() * 4.0;
        octave_mix += down2Level * octave.down2() * 4.0;

        auto out_chunk = interpolate(octave_mix);
        for (size_t j = 0; j < out_chunk.size(); ++j) {
            float mix = eq2(eq1(out_chunk[j]));

            const auto dry_signal = buff[j];
            mix += dryLevel * buff[j];

            buff_out[j] = mix;
        }
    }

    // Sets increments the buffer index from 0 to 5 (workaround to adapt code)
    bin_counter += 1;
    if (bin_counter > 5)
        bin_counter = 0;

    m_audioLeft = buff_out[bin_counter];
    m_audioRight = m_audioLeft;
}

void PolyOctaveModule::ProcessStereo(float inL, float inR) {
    // Calculate the mono effect
    ProcessMono(inL);

    // Do the base stereo calculation (which resets the right signal to be the inputR instead of combined mono)
    // BaseEffectModule::ProcessStereo(m_audioLeft, inR);  // Mono only for now

    // Use the same magnitude as already calculated for the Left Audio
    // m_audioRight = m_audioRight * m_cachedEffectMagnitudeValue;
}

float PolyOctaveModule::GetBrightnessForLED(int led_id) const {
    float value = BaseEffectModule::GetBrightnessForLED(led_id);

    if (led_id == 1) {
        return value * m_cachedEffectMagnitudeValue;
    }

    return value;
}
