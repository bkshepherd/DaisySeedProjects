#include "string_keys_module.h"
#include "../Util/audio_utilities.h"

using namespace bkshepherd;

static const int s_paramCount = 4;
static const ParameterMetaData s_metaData[s_paramCount] = {
    {
        name : "Structure",
        valueType : ParameterValueType::Float,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 0,
        midiCCMapping : 14
    },
    {
        name : "Brightness",
        valueType : ParameterValueType::Float,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 1,
        midiCCMapping : 15
    },
    {name : "Level", valueType : ParameterValueType::Float, defaultValue : {.float_value = 0.5f}, knobMapping : 2, midiCCMapping : 16},
    {
        name : "Damping",
        valueType : ParameterValueType::Float,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 3,
        midiCCMapping : 17
    }};

// Default Constructor
StringKeysModule::StringKeysModule() : BaseEffectModule(), m_freqMin(300.0f), m_freqMax(20000.0f), m_cachedEffectMagnitudeValue(1.0f) {
    // Set the name of the effect
    m_name = "StringKeys";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData;

    // Initialize Parameters for this Effect
    this->InitParams(s_paramCount);
}

// Destructor
StringKeysModule::~StringKeysModule() {
    // No Code Needed
}

void StringKeysModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);

    modalvoice.Init(sample_rate);
}

void StringKeysModule::ParameterChanged(int parameter_id) {
    if (parameter_id == 0) {
        modalvoice.SetStructure(GetParameterAsFloat(0));

    } else if (parameter_id == 1) {
        modalvoice.SetBrightness(GetParameterAsFloat(1));

    } else if (parameter_id == 3) {
        modalvoice.SetDamping(GetParameterAsFloat(3));
    }
}

void StringKeysModule::OnNoteOn(float notenumber, float velocity) {
    // Note Off can come in as Note On w/ 0 Velocity
    if (velocity == 0.f) {

    } else {
        // Using velocity for accent setting (striking the resonator harder)
        modalvoice.SetAccent(velocity / 128.0);
        modalvoice.SetFreq(mtof(notenumber));
        modalvoice.Trig();
    }
}

void StringKeysModule::OnNoteOff(float notenumber, float velocity) {
    // Currently note off does nothing
}

void StringKeysModule::ProcessMono(float in) {
    BaseEffectModule::ProcessMono(in); // This doesn't matter since not using input audio
    float through_audioL = m_audioLeft;
    // float through_audioR = m_audioRight;

    float voice_out = modalvoice.Process();

    m_audioLeft = voice_out * GetParameterAsFloat(2) * 0.1 + through_audioL; // Doing 50/50 mix of dry/reverb
    m_audioRight = voice_out * GetParameterAsFloat(2) * 0.1 + through_audioL;
}

void StringKeysModule::ProcessStereo(float inL, float inR) {
    // Calculate the mono effect
    // ProcessMono(inL);

    // Do the base stereo calculation (which resets the right signal to be the inputR instead of combined mono)
    BaseEffectModule::ProcessStereo(inL, inR);

    float through_audioL = m_audioLeft;
    float through_audioR = m_audioRight;

    float voice_out = modalvoice.Process();

    m_audioLeft = voice_out * GetParameterAsFloat(2) * 0.1 + through_audioL; // Doing 50/50 mix of dry/reverb
    m_audioRight = voice_out * GetParameterAsFloat(2) * 0.1 + through_audioR;
}

float StringKeysModule::GetBrightnessForLED(int led_id) const {
    float value = BaseEffectModule::GetBrightnessForLED(led_id);

    if (led_id == 1) {
        return value * m_cachedEffectMagnitudeValue;
    }

    return value;
}
