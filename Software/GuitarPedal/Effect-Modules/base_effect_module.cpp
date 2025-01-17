#include "base_effect_module.h"
#include "../Util/audio_utilities.h"

using namespace bkshepherd;

// Default Constructor
BaseEffectModule::BaseEffectModule()
    : m_paramCount(0), m_presetCount(1), m_currentPreset(0), m_params(nullptr), m_audioLeft(0.0f), m_audioRight(0.0f),
      m_settingsArrayStartIdx(0), m_isEnabled(false) {
    m_name = "Base";
    m_paramMetaData = nullptr;
}

// Destructor
BaseEffectModule::~BaseEffectModule() {
    if (m_params != nullptr) {
        delete[] m_params;
    }
}

void BaseEffectModule::Init(float sample_rate) { m_sampleRate = sample_rate; }

const char *BaseEffectModule::GetName() { return m_name; }

void BaseEffectModule::InitParams(int count) {
    // Remove any existing parameter storage
    if (m_params != nullptr) {
        delete[] m_params;
        m_params = nullptr;
    }

    m_paramCount = 0;

    if (count > 0) {
        // Create new parameter storage
        m_paramCount = count;
        m_params = new uint32_t[m_paramCount];

        // Init all parameters to their default value or zero if there is no meta data
        for (int i = 0; i < m_paramCount; i++) {
            if (m_paramMetaData != nullptr) {
                if (GetParameterType(i) == ParameterValueType::Float) {
                    uint32_t tmp;
                    float value = m_paramMetaData[i].defaultValue.float_value;
                    std::memcpy(&tmp, &value, sizeof(float));
                    m_params[i] = tmp;
                } else {
                    m_params[i] = m_paramMetaData[i].defaultValue.uint_value;
                }

            } else {
                m_params[i] = 0;
            }
        }
    }
}

uint16_t BaseEffectModule::GetParameterCount() const { return m_paramCount; }

uint16_t BaseEffectModule::GetPresetCount() const { return m_presetCount; }

void BaseEffectModule::SetPresetCount(uint16_t preset_count) { m_presetCount = preset_count; }

void BaseEffectModule::SetCurrentPreset(uint32_t preset) { m_currentPreset = preset; }

uint32_t BaseEffectModule::GetCurrentPreset() const { return m_currentPreset; }

void BaseEffectModule::SetSettingsArrayStartIdx(uint32_t start_idx) { m_settingsArrayStartIdx = start_idx; }

uint32_t BaseEffectModule::GetSettingsArrayStartIdx() const { return m_settingsArrayStartIdx; }

const char *BaseEffectModule::GetParameterName(int parameter_id) const {
    // Make sure parameter_id is valid.
    if (m_params == nullptr || parameter_id < 0 || parameter_id >= m_paramCount || m_paramMetaData == nullptr) {
        return "Unknown";
    }

    return m_paramMetaData[parameter_id].name;
}

ParameterValueType BaseEffectModule::GetParameterType(int parameter_id) const {
    // Make sure parameter_id is valid.
    if (m_params == nullptr || parameter_id < 0 || parameter_id >= m_paramCount || m_paramMetaData == nullptr) {
        return ParameterValueType::Unknown;
    }

    return m_paramMetaData[parameter_id].valueType;
}

ParameterValueCurve BaseEffectModule::GetParameterValueCurve(int parameter_id) const {
    // Make sure parameter_id is valid.
    if (m_params == nullptr || parameter_id < 0 || parameter_id >= m_paramCount || m_paramMetaData == nullptr) {
        return ParameterValueCurve::Linear;
    }

    return m_paramMetaData[parameter_id].valueCurve;
}

int BaseEffectModule::GetParameterBinCount(int parameter_id) const {
    // Make sure parameter_id is valid.
    if (m_params == nullptr || parameter_id < 0 || parameter_id >= m_paramCount || m_paramMetaData == nullptr) {
        return -1;
    }

    // Check to see if this is a binned type, if not return -1
    if (GetParameterType(parameter_id) != ParameterValueType::Binned) {
        return -1;
    }

    return m_paramMetaData[parameter_id].valueBinCount;
}

const char **BaseEffectModule::GetParameterBinNames(int parameter_id) const {
    // Make sure parameter_id is valid.
    if (m_params == nullptr || parameter_id < 0 || parameter_id >= m_paramCount || m_paramMetaData == nullptr) {
        return nullptr;
    }

    // Make sure this is a Binned Int type parameter
    int binCount = GetParameterBinCount(parameter_id);

    // If this is not a binned value type aways return nullptr
    if (binCount == -1) {
        return nullptr;
    }

    return m_paramMetaData[parameter_id].valueBinNames;
}

const float BaseEffectModule::GetParameterDefaultValueAsFloat(int parameter_id) const {
    // Make sure parameter_id is valid.
    if (m_params == nullptr || parameter_id < 0 || parameter_id >= m_paramCount || m_paramMetaData == nullptr) {
        return 0.0f;
    }

    return m_paramMetaData[parameter_id].defaultValue.float_value;
}

uint32_t BaseEffectModule::GetParameterRaw(int parameter_id) const {
    // Make sure parameter_id is valid.
    if (m_params == nullptr || parameter_id < 0 || parameter_id >= m_paramCount) {
        return 0;
    }

    return m_params[parameter_id];
}

float BaseEffectModule::GetParameterAsFloat(int parameter_id) const {
    if (parameter_id >= 0 || parameter_id < m_paramCount) {
        float ret;
        uint32_t tmp = m_params[parameter_id];
        std::memcpy(&ret, &tmp, sizeof(float));
        return ret;
    }
    return -1.0f;
}

bool BaseEffectModule::GetParameterAsBool(int parameter_id) const { return (GetParameterRaw(parameter_id) > 0); }

int BaseEffectModule::GetParameterAsBinnedValue(int parameter_id) const {
    int binCount = GetParameterBinCount(parameter_id);

    // If this is not a binned value type aways return 1
    if (binCount == -1) {
        return 1;
    }

    // Calculate the bin as raw stored value + 1
    int bin = (int)(GetParameterRaw(parameter_id) + 1);

    // A little sanity checking, make sure the bin is clamped to 1..BinCount
    if (bin < 1) {
        bin = 1;
    }

    if (bin > binCount) {
        bin = binCount;
    }

    return bin;
}

int BaseEffectModule::GetMappedParameterIDForKnob(int knob_id) const {
    if (m_paramMetaData != nullptr) {
        for (int i = 0; i < m_paramCount; i++) {
            if (m_paramMetaData[i].knobMapping == knob_id) {
                return i;
            }
        }
    }

    return -1;
}

int BaseEffectModule::GetMappedParameterIDForMidiCC(int midiCC_id) const {
    if (m_paramMetaData != nullptr) {
        for (int i = 0; i < m_paramCount; i++) {
            if (m_paramMetaData[i].midiCCMapping == midiCC_id) {
                return i;
            }
        }
    }

    return -1;
}

void BaseEffectModule::OnNoteOn(float notenumber, float velocity) {
    // Triggered when a NoteOn midi message is received.
    // Do nothing.
    // Effect modules are expected to override this fucntion if they use a midi keyboard.
}

void BaseEffectModule::OnNoteOff(float notenumber, float velocity) {
    // Triggered when a NoteOff midi message is received.
    // Do nothing.
    // Effect modules are expected to override this fucntion if they use a midi keyboard.
}

int BaseEffectModule::GetParameterMin(int parameter_id) const {
    if (m_paramMetaData != nullptr && parameter_id < m_paramCount) {
        return m_paramMetaData[parameter_id].minValue;
    }

    return -1;
}

int BaseEffectModule::GetParameterMax(int parameter_id) const {
    if (m_paramMetaData != nullptr && parameter_id < m_paramCount) {
        return m_paramMetaData[parameter_id].maxValue;
    }

    return -1;
}

float BaseEffectModule::GetParameterFineStepSize(int parameter_id) const {
    if (m_paramMetaData != nullptr && parameter_id < m_paramCount) {
        return m_paramMetaData[parameter_id].fineStepSize;
    }
    return 0.01f;
}

void BaseEffectModule::SetParameterRaw(int parameter_id, uint32_t value) {
    // Make sure parameter_id is valid.
    if (m_params == nullptr || parameter_id < 0 || parameter_id >= m_paramCount) {
        return;
    }

    // Only update the value if it changed.
    if (value != m_params[parameter_id]) {
        m_params[parameter_id] = value;

        // Notify anyone listening if the parameter actually changed.
        ParameterChanged(parameter_id);
    }
}

void BaseEffectModule::SetParameterAsMagnitude(int parameter_id, float value) {
    int min = GetParameterMin(parameter_id);
    int max = GetParameterMax(parameter_id);

    // Handle different ParameterValueTypes Correctly
    ParameterValueType paramType = GetParameterType(parameter_id);

    if (paramType == ParameterValueType::Raw) {
        // This is an unsupported operation, so do nothing.
        return;
    } else if (paramType == ParameterValueType::Float) {
        // Make sure the value is in the valid range.
        if (value < 0.0f) {
            SetParameterAsFloat(parameter_id, min);
            return;
        } else if (value > 1.0f) {
            SetParameterAsFloat(parameter_id, max);
            return;
        }

        // Set the scaled parameter using the magnitude and the curve type
        switch (GetParameterValueCurve(parameter_id)) {
        case ParameterValueCurve::Log: {
            const float minToUse = min > 0 ? (float)min : 0.01f;
            // Logarithmic scaling: interpolate in the log domain
            const float tmp = std::pow(10.0, std::log10(minToUse) + value * (std::log10((float)max) - std::log10(minToUse)));
            SetParameterAsFloat(parameter_id, tmp);
            break;
        }
        case ParameterValueCurve::InverseLog: {
            const float minToUse = min > 0 ? (float)min : 0.01f;
            // Inverse Logarithmic scaling: reverse the mapping
            const float tmp = std::pow(10.0, std::log10(minToUse) + (1.0f - value) * (std::log10((float)max) - std::log10(minToUse)));
            SetParameterAsFloat(parameter_id, tmp);
            break;
        }
        case ParameterValueCurve::Linear:
        default: {
            // Linear scaling: simple interpolation
            const float tmp = (value * ((float)max - (float)min) + (float)min);
            SetParameterAsFloat(parameter_id, tmp);
            break;
        }
        }
    } else if (paramType == ParameterValueType::Bool) {
        // Set the Bool Value to False if the magnitude is less then 0.5f, true otherwise
        if (value < 0.5f) {
            SetParameterAsBool(parameter_id, false);
            return;
        } else {
            SetParameterAsBool(parameter_id, true);
            return;
        }
    } else if (paramType == ParameterValueType::Binned) {
        // Make sure the value is in the valid range.
        if (value < 0.0f) {
            SetParameterAsBinnedValue(parameter_id, 1);
            return;
        } else if (value > 1.0f) {
            SetParameterAsBinnedValue(parameter_id, GetParameterBinCount(parameter_id));
            return;
        }

        // Map values 0..1 to the bins equally
        int mappedBin = (int)(value * GetParameterBinCount(parameter_id) + 1);
        SetParameterAsBinnedValue(parameter_id, mappedBin);
    }
}

void BaseEffectModule::SetParameterAsFloat(int parameter_id, float value) {
    if (parameter_id >= 0 || parameter_id < m_paramCount) {
        uint32_t tmp;
        std::memcpy(&tmp, &value, sizeof(float));

        // Only update the value if it changed.
        if (tmp != m_params[parameter_id]) {
            m_params[parameter_id] = tmp;

            // Notify anyone listening if the parameter actually changed.
            ParameterChanged(parameter_id);
        }
    }
}

void BaseEffectModule::SetParameterAsBool(int parameter_id, bool value) {
    if (value) {
        // Set raw value to 1 for True
        SetParameterRaw(parameter_id, 1U);
    } else {
        // Set raw value to 0 for False
        SetParameterRaw(parameter_id, 0U);
    }
}

void BaseEffectModule::SetParameterAsBinnedValue(int parameter_id, int value) {
    int binCount = GetParameterBinCount(parameter_id);

    // Make sure that this is a binned type value and we're within range
    if (binCount == -1 || value < 1 || value > binCount) {
        return;
    }

    SetParameterRaw(parameter_id, value - 1);
}

void BaseEffectModule::ProcessMono(float in) {
    m_audioLeft = in;
    m_audioRight = in;
}

void BaseEffectModule::ProcessStereo(float inL, float inR) {
    m_audioLeft = inL;
    m_audioRight = inR;
}

float BaseEffectModule::GetAudioLeft() const { return m_audioLeft; }

float BaseEffectModule::GetAudioRight() const { return m_audioRight; }

float BaseEffectModule::GetBrightnessForLED(int led_id) const {
    // By default will always return 1.0f if the effect is enabled and 0.0f if the effect is bypassed.
    // Each effect module is expected to override this function to treat the LEDs apropriately.
    // By convention LED_ID 0 should always reflect the status of the Effect as Enabled or Bypassed.

    if (m_isEnabled) {
        return 1.0f;
    }

    return 0.0f;
}

void BaseEffectModule::SetEnabled(bool isEnabled) { m_isEnabled = isEnabled; }

bool BaseEffectModule::IsEnabled() const { return m_isEnabled; }

void BaseEffectModule::SetTempo(uint32_t bpm) {
    // Do nothing.

    // Not all Effects are time based.

    // Effect modules are expected to override this fucntion if they are time based.
}

void BaseEffectModule::ParameterChanged(int parameter_id) {
    // Do nothing.
}

void BaseEffectModule::MidiCCValueNotification(uint8_t control_num, uint8_t value) {
    // Handle the incoming Midi CC Value Notification if needed
    int effectParamID = GetMappedParameterIDForMidiCC(control_num);

    // If there is a param mapped to this CC value we need to handle it appropriately
    if (effectParamID != -1) {
        // Midi Value gets mapped into a 0.0-1.0 float and the sets the parameter value using the SetParameterAsMagnitude function
        // Essentially Midi values 0..127 will behave the same was as turning a Pot on the guitar pedal from full off to full on.
        // This works for all parameter types except RAW.
        float mag = (float)value / 127.0f;
        SetParameterAsMagnitude(effectParamID, mag);
    }
}

void BaseEffectModule::UpdateUI(float elapsedTime) {
    // Do nothing.

    // Not all Effects will have custom UI that needs to udpate based on the passage of time.

    // Effect modules are expected to override this fucntion if they have custom UI requiring time based changes.
}

void BaseEffectModule::DrawUI(OneBitGraphicsDisplay &display, int currentIndex, int numItemsTotal, Rectangle boundsToDrawIn,
                              bool isEditing) {
    // By Default, the UI for an Effect Module is no different than it would be for the normal
    // FullScreenItemMenu. A specific Effect Module is welcome to override this whole function
    // to take over the full screen. Or overlay additional UI on top of this default UI.

    // Top Half of Screen is for the Effect Name and arrow indicators
    int topRowHeight = boundsToDrawIn.GetHeight() / 2;
    auto topRowRect = boundsToDrawIn.RemoveFromTop(topRowHeight);

    // Determine if there a page before or after this page
    const bool hasPrev = currentIndex > 0;
    const bool hasNext = currentIndex < numItemsTotal - 1;

    // Draw the Arrows before and after
    auto leftArrowRect = topRowRect.RemoveFromLeft(9).WithSizeKeepingCenter(5, 9).Translated(0, -1);
    auto rightArrowRect = topRowRect.RemoveFromRight(9).WithSizeKeepingCenter(5, 9).Translated(0, -1);

    if (hasPrev) {
        for (int16_t x = leftArrowRect.GetRight() - 1; x >= leftArrowRect.GetX(); x--) {
            display.DrawLine(x, leftArrowRect.GetY(), x, leftArrowRect.GetBottom(), true);

            leftArrowRect = leftArrowRect.Reduced(0, 1);
            if (leftArrowRect.IsEmpty())
                break;
        }
    }

    if (hasNext) {
        for (int16_t x = rightArrowRect.GetX(); x < rightArrowRect.GetRight(); x++) {
            display.DrawLine(x, rightArrowRect.GetY(), x, rightArrowRect.GetBottom(), true);

            rightArrowRect = rightArrowRect.Reduced(0, 1);
            if (rightArrowRect.IsEmpty())
                break;
        }
    }

    display.WriteStringAligned(m_name, Font_11x18, topRowRect, Alignment::centered, true);
    display.WriteStringAligned("...", Font_11x18, boundsToDrawIn, Alignment::centered, true);
}
