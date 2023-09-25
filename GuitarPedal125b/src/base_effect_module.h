#pragma once
#ifndef BASE_EFFECT_MODULE_H
#define BASE_EFFECT_MODULE_H

#include <stdint.h>
#ifdef __cplusplus

/** @file base_effect_module.h */

namespace bkshepherd
{

struct ParameterMetaData
{
  const char* name;
  int knobMapping;
  uint8_t defaultValue;
};

class BaseEffectModule
{
  public:
    BaseEffectModule();
    virtual ~BaseEffectModule();

    /** Initializes the module
        \param sample_rate  The sample rate of the audio engine being run.
    */
    virtual void Init(float sample_rate);

    /**
     \return the number of parameters for this effect.
    */
    uint8_t GetParameterCount();

    /**
     \return Value Name of the Effect Parameter
    */
    const char *GetParameterName(int parameter_id);

    /**
     \param parameter_id Id of the parameter to retrieve.
     \return the Value of the specified parameter.
    */
    uint8_t GetParameter(int parameter_id);

    /**
     \param parameter_id Id of the parameter to retrieve.
     \return the Value of the specified parameter as a float 0..1
    */
    float GetParameterAsMagnitude(int parameter_id);

    /**
     \param knob_id Id of the Knob to retrieve.
     \return the Parameter ID mapped to the specified knob.
    */
    int GetMappedParameterIDForKnob(int knob_id);

    /**
        \param parameter_id Id of the parameter to set.
        \param value Value to set on the parameter.
    */
    void SetParameter(int parameter_id, uint8_t value);

        /**
        \param parameter_id Id of the parameter to set.
        \param value Parameter will map a float 0..1 to the value 0..255
    */
    void SetParameterAsMagnitude(int parameter_id, float floatValue);

    /** 
     \param in Input sample.
     \return Next floating point sample for a mono effect.
    */
    virtual float ProcessMono(float in);

    /** 
     \param in Input sample.
     \return Next floating point sample for the left channel of a stereo effect.
    */
    virtual float ProcessStereoLeft(float in);

    /** 
     \param in Input sample.
     \return Next floating point sample for the right channel of a stereo effect.
    */
    virtual float ProcessStereoRight(float in);

    /**
     \return Value 0..1 for the intended LED brightness.
    */
    virtual float GetOutputLEDBrightness();

    /**
     \return Value Name of the Effect
    */
    const char *GetName();

  protected:
    /** Initializes the Parameter Storage
        \param count  The number of stored parameters
    */
    virtual void InitParams(int count);

    const char *m_name;
    int m_paramCount;
    uint8_t *m_params;
    const ParameterMetaData *m_paramMetaData;

  private:
    float   m_sampleRate;

};
} // namespace bkshepherd
#endif
#endif
