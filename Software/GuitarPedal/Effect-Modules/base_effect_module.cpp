#include "base_effect_module.h"
#include "../Util/audio_utilities.h"

using namespace bkshepherd;

// Default Constructor
BaseEffectModule::BaseEffectModule() : m_paramCount(0),
                                        m_params(NULL),
                                        m_audioLeft(0.0f),
                                        m_audioRight(0.0f),
                                        m_isEnabled(false)
{
    m_name = "Base";
    m_paramMetaData = NULL;
}

// Destructor
BaseEffectModule::~BaseEffectModule()
{
    if (m_params != NULL) {
        delete [] m_params;
	}
}

void BaseEffectModule::Init(float sample_rate)
{
    m_sampleRate = sample_rate;
}

const char *BaseEffectModule::GetName()
{
    return m_name;
}

void BaseEffectModule::InitParams(int count)
{
    // Remove any existing parameter storage
    if (m_params != NULL) {
        delete [] m_params;
        m_params = NULL;
	}

    m_paramCount = 0;

    if (count > 0)
    {
        // Create new parameter storage
        m_paramCount = count;
        m_params = new uint8_t[m_paramCount];

        // Init all parameters to their default value or zero if there is no meta data
        for (int i = 0; i < m_paramCount; i++)
        {
            if (m_paramMetaData != NULL)
            {
                m_params[i] = m_paramMetaData[i].defaultValue;
            }
            else
            {
                m_params[i] = 0;
            }
        }
	}
}

uint8_t BaseEffectModule::GetParameterCount()
{
    return m_paramCount;
}

const char *BaseEffectModule::GetParameterName(int parameter_id)
{
    // Make sure parameter_id is valid.
    if (m_params == NULL || parameter_id < 0 || parameter_id >= m_paramCount || m_paramMetaData == NULL)
    {
        return "Unknown";
    }
    
    return m_paramMetaData[parameter_id].name;
}

int BaseEffectModule::GetParameterType(int parameter_id)
{
    // Make sure parameter_id is valid.
    if (m_params == NULL || parameter_id < 0 || parameter_id >= m_paramCount || m_paramMetaData == NULL)
    {
        return -1;
    }
    
    return m_paramMetaData[parameter_id].valueType;
}

int BaseEffectModule::GetParameterBinCount(int parameter_id)
{
    // Make sure parameter_id is valid.
    if (m_params == NULL || parameter_id < 0 || parameter_id >= m_paramCount || m_paramMetaData == NULL)
    {
        return -1;
    }
    
    // Make sure this is a Binned Int type parameter
    if (m_paramMetaData[parameter_id].valueType != 3)
    {
        return -1;
    }

    return m_paramMetaData[parameter_id].valueBinCount;
}

const char** BaseEffectModule::GetParameterBinNames(int parameter_id)
{
    // Make sure parameter_id is valid.
    if (m_params == NULL || parameter_id < 0 || parameter_id >= m_paramCount || m_paramMetaData == NULL)
    {
        return NULL;
    }
    
    // Make sure this is a Binned Int type parameter
    if (m_paramMetaData[parameter_id].valueType != 3)
    {
        return NULL;
    }

    return m_paramMetaData[parameter_id].valueBinNames;
}

uint8_t BaseEffectModule::GetParameterRaw(int parameter_id)
{
    // Make sure parameter_id is valid.
    if (m_params == NULL || parameter_id < 0 || parameter_id >= m_paramCount)
    {
        return 0;
    }

    return m_params[parameter_id];
}

float BaseEffectModule::GetParameterAsMagnitude(int parameter_id)
{
    return (float)GetParameterRaw(parameter_id) / 127.0f;
}

bool BaseEffectModule::GetParameterAsBool(int parameter_id)
{
    return (GetParameterRaw(parameter_id) > 63);
}

int BaseEffectModule::GetParameterAsBinnedValue(int parameter_id)
{
    int binCount = GetParameterBinCount(parameter_id);

    // Make this is a binned value
    if (binCount == -1)
    {
        return 1;
    }

    // Get the Bin number from a raw value stored as 0..127
    float binSize = 128.0f / binCount;
    float midPoint = (0.5f - (1.0f / 128.0f));
    float offset = (1.0f / 128.0f);

    // Calculate the bin
    int bin = (int)(((GetParameterRaw(parameter_id) + midPoint) / binSize) + offset) + 1;

    // A little sanity checking, make sure the bin is clamped to 1..BinCount
    if (bin < 1)
    {
        bin = 1;
    }

    if (bin > binCount)
    {
        bin = binCount;
    }

    return bin;
}

int BaseEffectModule::GetMappedParameterIDForKnob(int knob_id)
{
    if (m_paramMetaData != NULL)
    {
        for (int i = 0; i < m_paramCount; i++)
        {
            if (m_paramMetaData[i].knobMapping == knob_id)
            {
                return i;
            }
        }
    }

    return -1;
}

int BaseEffectModule::GetMappedParameterIDForMidiCC(int midiCC_id)
{
    if (m_paramMetaData != NULL)
    {
        for (int i = 0; i < m_paramCount; i++)
        {
            if (m_paramMetaData[i].midiCCMapping == midiCC_id)
            {
                return i;
            }
        }
    }

    return -1;
}

void BaseEffectModule::SetParameterRaw(int parameter_id, uint8_t value)
{
    // Make sure parameter_id is valid.
    if (m_params == NULL || parameter_id < 0 || parameter_id >= m_paramCount)
    {
        return;
    }

    // Make sure the value is valid.
    if (value < 0 || value > 127)
    {
        return;
    }

    // Only update the value if it changed.
    if (value != m_params[parameter_id])
    {
        m_params[parameter_id] = value;

        // Notify anyone listening if the parameter actually changed.
        ParameterChanged(parameter_id);
    }
}

void BaseEffectModule::SetParameterAsMagnitude(int parameter_id, float value)
{
    // Make sure the value is in the valid range.
    if (value < 0.0f)
    {
        SetParameterRaw(parameter_id, 0);
        return;
    }
    else if (value > 1.0f)
    {
        SetParameterRaw(parameter_id, 127);
        return;
    }

    SetParameterRaw(parameter_id, (uint8_t)((127.0f * value) + 0.35f)); 
}

void BaseEffectModule::SetParameterAsBool(int parameter_id, bool value)
{
    if (value)
    {
        SetParameterRaw(parameter_id, 127);
    }
    else
    {
        SetParameterRaw(parameter_id, 0);
    }
    
}

void BaseEffectModule::SetParameterAsBinnedValue(int parameter_id, u_int8_t bin)
{
    int binCount = GetParameterBinCount(parameter_id);

    // Make sure that this is a binned type value and we're within range
    if (binCount == -1 || bin < 1 || bin > binCount)
    {
        return;
    }

    // Map the Bin number into a raw value mapped in 0..127
    float binSize = 128.0f / binCount;

    // Calculate the raw value
    int rawValue = (int)(((bin - 1) * binSize) + (binSize / 2.0f));

    // A little sanity checking to make sure the raw value is within proper range
    if (rawValue < 0)
    {
        rawValue = 0;
    }

    if (rawValue > 127)
    {
        rawValue = 127;
    }

    SetParameterRaw(parameter_id, rawValue);
}

void BaseEffectModule::ProcessMono(float in)
{
    m_audioLeft = in;
    m_audioRight = in;
}

void BaseEffectModule::ProcessStereo(float inL, float inR)
{
    m_audioLeft = inL;
    m_audioRight = inR;
}

float BaseEffectModule::GetAudioLeft()
{
    return m_audioLeft;
}

float BaseEffectModule::GetAudioRight()
{
    return m_audioRight;
}

float BaseEffectModule::GetBrightnessForLED(int led_id)
{
    // By default will always return 1.0f if the effect is enabled and 0.0f if the effect is bypassed.
    // Each effect module is expected to override this function to treat the LEDs apropriately.
    // By convention LED_ID 0 should always reflect the status of the Effect as Enabled or Bypassed. 

    if (m_isEnabled)
    {
        return 1.0f;
    }

    return 0.0f;
}

void BaseEffectModule::SetEnabled(bool isEnabled)
{
    m_isEnabled = isEnabled;
}

bool BaseEffectModule::IsEnabled()
{
    return m_isEnabled;
}

void BaseEffectModule::SetTempo(uint32_t bpm)
{
    // Do nothing.
    
    // Not all Effects are time based.
    
    // Effect modules are expected to override this fucntion if they are time based.
}

void BaseEffectModule::ParameterChanged(int parameter_id)
{
    // Do nothing.
}

void BaseEffectModule::UpdateUI(float elapsedTime)
{
    // Do nothing.

    // Not all Effects will have custom UI that needs to udpate based on the passage of time.

    // Effect modules are expected to override this fucntion if they have custom UI requiring time based changes.
}

void BaseEffectModule::DrawUI(OneBitGraphicsDisplay& display, int currentIndex, int numItemsTotal, Rectangle boundsToDrawIn, bool isEditing)
{
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

    if(hasPrev)
    {
        for(int16_t x = leftArrowRect.GetRight() - 1; x >= leftArrowRect.GetX(); x--)
        {
            display.DrawLine(x, leftArrowRect.GetY(), x, leftArrowRect.GetBottom(), true);

            leftArrowRect = leftArrowRect.Reduced(0, 1);
            if(leftArrowRect.IsEmpty())
                break;
        }
    }

    if(hasNext)
    {
        for(int16_t x = rightArrowRect.GetX(); x < rightArrowRect.GetRight(); x++)
        {
            display.DrawLine(x, rightArrowRect.GetY(), x, rightArrowRect.GetBottom(), true);

            rightArrowRect = rightArrowRect.Reduced(0, 1);
            if(rightArrowRect.IsEmpty())
                break;
        }
    }

    display.WriteStringAligned(m_name, Font_11x18, topRowRect, Alignment::centered, true);
    display.WriteStringAligned("...", Font_11x18, boundsToDrawIn, Alignment::centered, true);
}

