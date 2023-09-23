#include "base_effect_module.h"

using namespace bkshepherd;

// Default Constructor
BaseEffectModule::BaseEffectModule() : m_paramCount(0),
                                        m_params(NULL)
{

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

uint8_t BaseEffectModule::GetParameter(int parameter_id)
{
    if (m_params == NULL)
    {
        return 0;
    }

    return m_params[parameter_id];
}

float BaseEffectModule::GetParameterAsMagnitude(int parameter_id)
{
    return (float)GetParameter(parameter_id) / 255.0f;
}

void BaseEffectModule::SetParameter(int parameter_id, uint8_t value)
{
    // Make sure parameter_id is valid.
    if (m_params == NULL || parameter_id < 0 || parameter_id >= m_paramCount)
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
        SetParameter(parameter_id, 255);
        return;
    }

    SetParameter(parameter_id, (uint8_t)(255.0f * floatValue)); 
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
