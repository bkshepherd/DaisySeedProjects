#include "fm_keys_module.h"
#include "../Util/audio_utilities.h"

using namespace bkshepherd;

// static VoiceManager<18> voice_handler; // starting with 1 voice for testing, 16 WORKS, 18 freezes
//  With added carrier env control, 16 glitches, 12 works
//  moved param changing to outside function, can run 18

static const int s_paramCount = 12;
static const ParameterMetaData s_metaData[s_paramCount] = {
    {name : "Level", valueType : ParameterValueType::Float, defaultValue : {.float_value = 0.5f}, knobMapping : 0, midiCCMapping : 14},
    {
        name : "Ratio",
        valueType : ParameterValueType::Float,
        defaultValue : {.float_value = 1.0f},
        knobMapping : 1,
        midiCCMapping : 15,
        minValue : 0,
        maxValue : 16
    },

    {
        name : "ModLevel",
        valueType : ParameterValueType::Float,
        defaultValue : {.float_value = 0.2f},
        knobMapping : 2,
        midiCCMapping : 16
    },
    {
        name : "ModRatio",
        valueType : ParameterValueType::Float,
        defaultValue : {.float_value = 16.0f},
        knobMapping : 3,
        midiCCMapping : 17,
        minValue : 0,
        maxValue : 16
    }, // try fine/coarse step sizes 0.5 (not in lighthouse yet?)

    {
        name : "CarAttack",
        valueType : ParameterValueType::Float,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 4,
        midiCCMapping : 18
    },
    {
        name : "CarDecay",
        valueType : ParameterValueType::Float,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 5,
        midiCCMapping : 19
    },
    {
        name : "CarSustain",
        valueType : ParameterValueType::Float,
        defaultValue : {.float_value = 0.5f},
        knobMapping : -1,
        midiCCMapping : 20
    },
    {
        name : "CarRelease",
        valueType : ParameterValueType::Float,
        defaultValue : {.float_value = 0.5f},
        knobMapping : -1,
        midiCCMapping : 21
    },

    {
        name : "ModAttack",
        valueType : ParameterValueType::Float,
        defaultValue : {.float_value = 0.5f},
        knobMapping : -1,
        midiCCMapping : 22
    },
    {
        name : "ModDecay",
        valueType : ParameterValueType::Float,
        defaultValue : {.float_value = 0.5f},
        knobMapping : -1,
        midiCCMapping : 23
    },
    {
        name : "ModSustain",
        valueType : ParameterValueType::Float,
        defaultValue : {.float_value = 0.5f},
        knobMapping : -1,
        midiCCMapping : 24
    },
    {
        name : "ModRelease",
        valueType : ParameterValueType::Float,
        defaultValue : {.float_value = 0.5f},
        knobMapping : -1,
        midiCCMapping : 25
    },
};

// Default Constructor
FmKeysModule::FmKeysModule() : BaseEffectModule(), m_freqMin(250.0f), m_freqMax(16000.0f), m_cachedEffectMagnitudeValue(1.0f) {
    // Set the name of the effect
    m_name = "FmKeys";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData;

    // Initialize Parameters for this Effect
    this->InitParams(s_paramCount);
}

// Destructor
FmKeysModule::~FmKeysModule() {
    // No Code Needed
}

void FmKeysModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);

    m_voice_handler.Init(sample_rate);
}

void FmKeysModule::ParameterChanged(int parameter_id) {

    if (parameter_id == 0) {
        m_voice_handler.SetCarrierLevel(GetParameterAsFloat(0));

    } else if (parameter_id == 1) {
        float temp = GetParameterAsFloat(1) * 2; // scale 0 to 16... 0 to 32
        int temp2 = static_cast<int>(temp);      // round by converting to int
        float temp3 = static_cast<float>(temp2);
        float float_val = temp3 / 2.0; // 0 to 16 increments of 0.5
        m_voice_handler.SetCarrierRatio(float_val);

    } else if (parameter_id == 2) {
        m_voice_handler.SetModulatorLevel(GetParameterAsFloat(2) * GetParameterAsFloat(2)); // exponential

    } else if (parameter_id == 3) {
        float temp = GetParameterAsFloat(3) * 2; // scale 0 to 16... 0 to 32
        int temp2 = static_cast<int>(temp);      // round by converting to int
        float temp3 = static_cast<float>(temp2);
        float float_val = temp3 / 2.0; // 0 to 16 increments of 0.5
        m_voice_handler.SetModulatorRatio(float_val);

    } else if (parameter_id == 4) {
        m_voice_handler.SetCarrierAttack(GetParameterAsFloat(4) * GetParameterAsFloat(4) * 0.9999 + 0.0001); // exponential

    } else if (parameter_id == 5) {
        m_voice_handler.SetCarrierDecay(GetParameterAsFloat(5) * GetParameterAsFloat(5) * 0.9999 + 0.0001); // exponential

    } else if (parameter_id == 6) {
        m_voice_handler.SetCarrierSustain(GetParameterAsFloat(6) * 0.999 + 0.001);

    } else if (parameter_id == 7) {
        m_voice_handler.SetCarrierRelease(GetParameterAsFloat(7) * 0.999 + 0.001);

    } else if (parameter_id == 8) {
        m_voice_handler.SetModAttack(GetParameterAsFloat(8) * GetParameterAsFloat(8) * 0.9999 + 0.0001); // exponential

    } else if (parameter_id == 9) {
        m_voice_handler.SetModDecay(GetParameterAsFloat(9) * GetParameterAsFloat(9) * 0.9999 + 0.0001); // exponential

    } else if (parameter_id == 10) {
        m_voice_handler.SetModSustain(GetParameterAsFloat(10) * 0.999 + 0.001);

    } else if (parameter_id == 11) {
        m_voice_handler.SetModRelease(GetParameterAsFloat(11) * 0.999 + 0.001);
    }
}

void FmKeysModule::OnNoteOn(float notenumber, float velocity) {
    // Note Off can come in as Note On w/ 0 Velocity
    if (velocity == 0.f) {
        m_voice_handler.OnNoteOff(notenumber, velocity);
    } else {
        m_voice_handler.OnNoteOn(notenumber, velocity);
    }
}

void FmKeysModule::OnNoteOff(float notenumber, float velocity) { m_voice_handler.OnNoteOff(notenumber, velocity); }

void FmKeysModule::ProcessMono(float in) {
    BaseEffectModule::ProcessMono(in);

    // float through_audioL = m_audioLeft;

    float sum = 0.f;
    sum = m_voice_handler.Process() * 0.3f; // 0.3 for volume reduction
    m_audioLeft = sum;
    m_audioRight = m_audioLeft;
}

void FmKeysModule::ProcessStereo(float inL, float inR) {
    // Calculate the mono effect
    ProcessMono(inL);
}

float FmKeysModule::GetBrightnessForLED(int led_id) const {
    float value = BaseEffectModule::GetBrightnessForLED(led_id);

    if (led_id == 1) {
        return value * m_cachedEffectMagnitudeValue;
    }

    return value;
}
