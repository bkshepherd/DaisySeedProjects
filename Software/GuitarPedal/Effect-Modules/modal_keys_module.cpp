#include "modal_keys_module.h"
#include "../Util/audio_utilities.h"
#include <array>

using namespace bkshepherd;

static const auto s_metaData = [] {
    std::array<ParameterMetaData, ModalKeysModule::PARAM_COUNT> params{};

    params[ModalKeysModule::STRUCTURE] = {
        name : "Structure",
        valueType : ParameterValueType::Float,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 0,
        midiCCMapping : 14
    };

    params[ModalKeysModule::BRIGHTNESS] = {
        name : "Brightness",
        valueType : ParameterValueType::Float,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 1,
        midiCCMapping : 15
    };

    params[ModalKeysModule::LEVEL] = {
        name : "Level",
        valueType : ParameterValueType::Float,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 2,
        midiCCMapping : 16
    };

    params[ModalKeysModule::DAMPING] = {
        name : "Damping",
        valueType : ParameterValueType::Float,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 3,
        midiCCMapping : 17
    };

    return params;
}();

// Default Constructor
ModalKeysModule::ModalKeysModule()
    : BaseEffectModule(), m_freqMin(300.0f), m_freqMax(20000.0f),

      m_cachedEffectMagnitudeValue(1.0f) {
    // Set the name of the effect
    m_name = "ModalKeys";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData.data();

    // Initialize Parameters for this Effect
    this->InitParams(static_cast<int>(s_metaData.size()));
}

// Destructor
ModalKeysModule::~ModalKeysModule() {
    // No Code Needed
}

void ModalKeysModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);

    modalvoice.Init(sample_rate);
}

void ModalKeysModule::ParameterChanged(int parameter_id) {
    if (parameter_id == STRUCTURE) {
        modalvoice.SetStructure(GetParameterAsFloat(STRUCTURE));

    } else if (parameter_id == BRIGHTNESS) {
        modalvoice.SetBrightness(GetParameterAsFloat(BRIGHTNESS));

    } else if (parameter_id == DAMPING) {
        modalvoice.SetDamping(GetParameterAsFloat(DAMPING));
    }
}

void ModalKeysModule::OnNoteOn(float notenumber, float velocity) {
    // Note Off can come in as Note On w/ 0 Velocity
    if (velocity == 0.f) {

    } else {
        // Using velocity for accent setting (striking the resonator harder)
        modalvoice.SetAccent(velocity / 128.0);
        modalvoice.SetFreq(mtof(notenumber));
        modalvoice.Trig();
    }
}

void ModalKeysModule::OnNoteOff(float notenumber, float velocity) {
    // Currently note off does nothing
}

void ModalKeysModule::ProcessMono(float in) {
    BaseEffectModule::ProcessMono(in); // This doesn't matter since not using input audio

    float through_audioL = m_audioLeft;
    // float through_audioR = m_audioRight;

    float voice_out = modalvoice.Process();

    m_audioLeft = (voice_out)*GetParameterAsFloat(LEVEL) * 0.1 + through_audioL; // Doing 50/50 mix of dry/reverb, 0.2 is volume reduction
    m_audioRight = (voice_out)*GetParameterAsFloat(LEVEL) * 0.1 + through_audioL;
}

void ModalKeysModule::ProcessStereo(float inL, float inR) {
    // Calculate the mono effect
    // ProcessMono(inL);

    // Do the base stereo calculation (which resets the right signal to be the inputR instead of combined mono)
    BaseEffectModule::ProcessStereo(inL, inR);

    float through_audioL = m_audioLeft;
    float through_audioR = m_audioRight;

    float voice_out = modalvoice.Process();

    m_audioLeft = (voice_out)*GetParameterAsFloat(LEVEL) * 0.1 + through_audioL; // Doing 50/50 mix of dry/reverb, 0.2 is volume reduction
    m_audioRight = (voice_out)*GetParameterAsFloat(LEVEL) * 0.1 + through_audioR;
}

float ModalKeysModule::GetBrightnessForLED(int led_id) const {
    float value = BaseEffectModule::GetBrightnessForLED(led_id);

    if (led_id == 1) {
        return value * m_cachedEffectMagnitudeValue;
    }

    return value;
}
