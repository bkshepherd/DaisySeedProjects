#include "base_effect_module.h"

using namespace bkshepherd;

// Default Constructor
BaseEffectModule::BaseEffectModule() : m_paramCount(0),
                                        m_params(NULL)
{
    m_name = "Base";
    m_paramNames = NULL;
    m_knobMappings = NULL;
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

        // Init all parameters to zero
        for (int i = 0; i < m_paramCount; i++)
        {
            m_params[i] = 0;
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
    if (m_params == NULL || parameter_id < 0 || parameter_id >= m_paramCount || m_paramNames == NULL)
    {
        return "Unknown";
    }
    
    return m_paramNames[parameter_id];
}

uint8_t BaseEffectModule::GetParameter(int parameter_id)
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
    return (float)GetParameter(parameter_id) / 127.0f;
}

int BaseEffectModule::GetMappedParameterIDForKnob(int knob_id)
{
    for (int i = 0; i < m_paramCount; i++)
    {
        if (m_knobMappings[i] == knob_id)
        {
            return i;
        }
    }

    return -1;
}

void BaseEffectModule::SetParameter(int parameter_id, uint8_t value)
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

    m_params[parameter_id] = value;
}

void BaseEffectModule::SetParameterAsMagnitude(int parameter_id, float floatValue)
{
    // Make sure the floatValue is in the valid range.
    if (floatValue < 0.0f)
    {
        SetParameter(parameter_id, 0);
        return;
    }
    else if (floatValue > 1.0f)
    {
        SetParameter(parameter_id, 127);
        return;
    }

    SetParameter(parameter_id, (uint8_t)(127.0f * floatValue)); 
}

float BaseEffectModule::ProcessMono(float in)
{
    return in;
}

float BaseEffectModule::ProcessStereoLeft(float in)
{
    return in;
}

float BaseEffectModule::ProcessStereoRight(float in)
{
    return in;
}

float BaseEffectModule::GetOutputLEDBrightness()
{
    return 1.0f;
}

const char *BaseEffectModule::GetName()
{
    return m_name;
}
