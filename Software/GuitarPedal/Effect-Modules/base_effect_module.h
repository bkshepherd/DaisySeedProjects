#pragma once
#ifndef BASE_EFFECT_MODULE_H
#define BASE_EFFECT_MODULE_H

#include "daisy_seed.h"
#include <stdint.h>
#ifdef __cplusplus

/** @file base_effect_module.h */

using namespace daisy;

namespace bkshepherd {

/** Parameter Value Types */
enum ParameterValueType {
    Raw,    // Raw 32bit Unsigned Int Parameter Value
    Float,  // Float Value
    Bool,   // Boolean Value
    Binned, // Binned Value (1 to valueBinCount)
    Unknown,
    ParameterValueType_LAST, // Last enum item
};

union ParameterValue {
    uint32_t uint_value;
    float float_value;
};

enum ParameterValueCurve {
    Linear,
    Log,
    ParameterValueCurve_LAST, // Last enum item
};

// Meta data for an individual Effect Parameter.  Effects may have zero or more Parameters.
// This data structure contains information about the Effect Parameter.
struct ParameterMetaData {
    const char *name;             // The Name of this Parameter that gets displayed on the Screen when editing the parameters value
    ParameterValueType valueType; // The Type of this Parameter value.
    ParameterValueCurve valueCurve =
        ParameterValueCurve::Linear; // The curve used to move between the min and max value for this parameter
    int valueBinCount;               // The number of distinct choices allowed for this parameter value
    const char **valueBinNames;      // The human readable display names for the bins
    ParameterValue defaultValue;     // The Raw Default Value set for this parameter the first time the device is powered up
    int knobMapping;   // The ID of the Physical Knob mapped to this Parameter. -1 if this Parameter is not controlled by a Knob
    int midiCCMapping; // The Midi CC ID mapped to this Parameter. -1 of this Parameter is not controllable via Midi CC messages
    int minValue = 0;  // The minimum value of the parameter
    int maxValue = 1;  // The maximum value of the parameter
    float fineStepSize = 0.01f; // For Float Parameters, this will set the fineStepSize multiple in the menu
};

class BaseEffectModule {
  public:
    BaseEffectModule();
    virtual ~BaseEffectModule();

    /** Initializes the module
        \param sample_rate  The sample rate of the audio engine being run.
    */
    virtual void Init(float sample_rate);

    /** Gets the Name of the Effect to display
        \return Value Name of the Effect
    */
    const char *GetName();

    /** Gets the total number of Effect Parameters
     \return the number of parameters for this effect.
    */
    uint16_t GetParameterCount() const;

    /** Gets the total number of stored presets for this effect
     \return the number of presets for this effect.
    */
    uint16_t GetPresetCount() const;

    /** Sets the total number of stored presets for this effect, called by guitar_pedal_storage
     \param the number of presets for this effect.
    */
    void SetPresetCount(uint16_t preset_count);

    /** Sets the start index of the Settings Array
     \param the array index where settings for the effect start
    */
    void SetSettingsArrayStartIdx(uint32_t start_idx);

    /** Gets the start position in settings for this effect
     \return the the start position in settings array for this effect.
    */
    uint32_t GetSettingsArrayStartIdx() const;

    /** Sets the start index of the Settings Array
     \param the array index where settings for the effect start
    */
    void SetCurrentPreset(uint32_t currentPreset);

    /** Gets the start position in settings for this effect
     \return the the start position in settings array for this effect.
    */
    uint32_t GetCurrentPreset() const;

    /** Gets the Name of an Effect Parameter
     \return Value Name of the Effect Parameter
    */
    const char *GetParameterName(int parameter_id) const;

    /** Gets the Type of an Effect Parameter
     \return a ParameterValueType representing Type of the Effect Parameter. 0 is raw u_int32_t, 1 is Float, 2 is Bool, 3 is Binned Int
     (return "ParameterValueType::Unknown" is this is unknown)
    */
    ParameterValueType GetParameterType(int parameter_id) const;

    /** Gets the curve of an Effect Parameter
     \return a ParameterValueType representing curve of the Effect Parameter.
    */
    ParameterValueCurve GetParameterValueCurve(int parameter_id) const;

    /** Gets the Bin Count of a Binned Int Effect Parameter
     \return the number of Bins for this Binned Int type Effect Parameter or -1 if this isn't a Binned Int type parameter
    */
    int GetParameterBinCount(int parameter_id) const;

    /** Gets the Bin Name of a Binned Int Effect Parameter
     \param parameter_id Id of the parameter to retrieve.
     \return the Names of the Bins for this Binned Int type Efffect Parameter or NULL if there aren't any specified
    */
    const char **GetParameterBinNames(int parameter_id) const;

    /** Gets the Default Value for an Effect Parameter as a Float value
     \param parameter_id Id of the parameter to retrieve.
     \return the float Value of the specified parameter.
    */
    const float GetParameterDefaultValueAsFloat(int parameter_id) const;

    /** Gets the Raw uint32_t value of an Effect Parameter
     \param parameter_id Id of the parameter to retrieve.
     \return the raw Value of the specified parameter.
    */
    uint32_t GetParameterRaw(int parameter_id) const;

    /** Gets the value of an Effect Parameter as a float
        \param parameter_id Id of the parameter to retrieve.
        \return the float Value for given parameter.
    */
    float GetParameterAsFloat(int parameter_id) const;

    /** Gets the value of an Effect Parameter as a bool (True of False)
     \param parameter_id Id of the parameter to retrieve.
     \return the Value of the specified parameter mapped to True / False
    */
    bool GetParameterAsBool(int parameter_id) const;

    /** Gets the value of an Effect Parameter as a Binned Integer Value (1..Bin Count)
     \param parameter_id Id of the parameter to retrieve.
     \return the bin number as an int value (1..Bin Count) of the specified parameter
    */
    int GetParameterAsBinnedValue(int parameter_id) const;

    /** Gets the Parameter ID of the Effect Parameter mapped to Knob.
     \param knob_id Id of the Knob to retrieve.
     \return the Parameter ID mapped to the specified knob, or -1 if no Parameter is Mapped to that Knob
    */
    int GetMappedParameterIDForKnob(int knob_id) const;

    /** Gets the Parameter ID of the Effect Parameter mapped to this Midi CC ID.
     \param midiCC_id Id of the MidiCC to retrieve.
     \return the Parameter ID mapped to the specified MidiCC, or -1 if no Parameter is Mapped to that Midi CC
    */
    int GetMappedParameterIDForMidiCC(int midiCC_id) const;

    /** Sets the Raw Value for a Particular Effect Parameter.  If the Parameter ID isn't valid, there is no effect.
        \param parameter_id Id of the parameter to set (0 .. m_paramCount - 1).
        \param value the uint32_t Value to set on the parameter.
    */
    void SetParameterRaw(int parameter_id, uint32_t value);

    /** Sets the Value for a Particular Effect Parameter using a float magnitude value ranging from 0.0f to 1.0f.
     * If the Parameter ID isn't valid or if the Parameter Type is RAW, there is no effect. This function map the float
     * magnitude value onto the underlying Parameter Type in the following ways:
     * RAW Parameters - NOT SUPPORTED, there will be no effect.
     * Bool Parameters - values < 0.5f are mapped to False, values >= 0.5 are mapped to True.
     * Float Parameters - values between 0.0f and 1.0f are mapped to a float value between the Min and Max Parameter values
     * Binned Parameters - values between 0.0f and 1.0f are divided equally between the total number of Bins available and mapped
     accordingly.
        \param parameter_id Id of the parameter to set. \param value the float magnitude value to set the parameter to.
    */
    void SetParameterAsMagnitude(int parameter_id, float value);

    /** Sets the Value for a Particular Effect Parameter using a float.  If the Parameter ID isn't valid, there is no effect.
        \param parameter_id Id of the parameter to set.
        \param value the Float value to set the paramter to.
    */
    void SetParameterAsFloat(int parameter_id, float value);

    /** Sets the Value for a Particular Effect Parameter using a bool.  If the Parameter ID isn't valid, there is no effect.
        \param parameter_id Id of the parameter to set.
        \param value the bool value to set the paramter to.
    */
    void SetParameterAsBool(int parameter_id, bool value);

    /** Sets the Value for a Particular Effect Parameter using a Binned Value.  If the Parameter ID isn't valid, there is no effect.
        \param parameter_id Id of the parameter to set.
        \param value the int value (1..Bin Count) of the bin to set the paramter to.
    */
    void SetParameterAsBinnedValue(int parameter_id, int value);

    /** Processes the Effect in Mono for a single sample.  This should only be called once per sample. Also, if this is called, don't
     call ProcessStereo too. \param in Input sample.
    */
    virtual void ProcessMono(float in);

    /** Processes the Effect in Stereo for a single left & right sample.  This should only be called once per sample. Also, if this is
     called, don't call ProcessMono too. \param inL, inR Input sample Left and Right.
    */
    virtual void ProcessStereo(float inL, float inR);

    /**  Gets the most recently calculated Sample Value for the Left Stereo Channel (or Mono)
     \return Last floating point sample for the left channel.
    */
    float GetAudioLeft() const;

    /** Gets the most recently calculated Sample Value for the Right Stereo Channel
     \return Last floating point sample for the right channel.
    */
    float GetAudioRight() const;

    /** Returns a value that can be used to drive the Effect LED brightness for a specific led.
     \return float value 0..1 for the intended LED brightness.
    */
    virtual float GetBrightnessForLED(int led_id) const;

    /** Sets the state of this Effect
     * @param isEnabled True for Enabled, False for Bypassed.
     */
    virtual void SetEnabled(bool isEnabled);

    /** Returns the status of the Effect
     \return Value True if the Effect is Enabled and False if the Effect is Bypassed
    */
    bool IsEnabled() const;

    /** Sets the Tempo of this Effect
     * @param bpm the Tempo in Beats Per Minute (BPM) as a uint32_t
     */
    virtual void SetTempo(uint32_t bpm);

    /** Handles updating the custom UI for this Effect.
     * @param elapsedTime a float value of how much time (in seconds) has elapsed since the last update
     */
    virtual void UpdateUI(float elapsedTime);

    /** Handles drawing the custom UI for this Effect.
     * @param display           The display to draw to
     * @param currentIndex      The index in the menu
     * @param numItemsTotal     The total number of items in the menu
     * @param boundsToDrawIn    The Rectangle to draw the item into
     * @param isEditing         True if the enter button was pressed and the value is being edited directly.
     */
    virtual void DrawUI(OneBitGraphicsDisplay &display, int currentIndex, int numItemsTotal, Rectangle boundsToDrawIn, bool isEditing);

    /** Gets the minimum value for the parameter
        \param parameter_id Id of the parameter to set (0 .. m_paramCount - 1).
        \return int value for minimum parameter value.
    */
    int GetParameterMin(int parameter_id) const;

    /** Gets the maximum value for the parameter
        \param parameter_id Id of the parameter to set (0 .. m_paramCount - 1).
        \return int value for maximum parameter value.
    */
    int GetParameterMax(int parameter_id) const;

    /** Gets the Fine Step size for the parameter id as a float value
        \param parameter_id Id of the parameter to set (0 .. m_paramCount - 1).
        \return Fine step size for given parameter.
    */
    float GetParameterFineStepSize(int parameter_id) const;

    /** This function gets called when a MIDI CC message is received by the pedal when this effect is active.  The default
     * implementation checks to see if any Midi Parameters on this Effect map to the midi CC number and then it maps the Midi Values
     * 0..127 to a float 0.0f - 1.0f and then calls the SetParameterAsMagnitude helper function to handle how the midi values should
     * map to the underlying parameter type. \param control_num, midicc number \param value, midi value
     */
    virtual void MidiCCValueNotification(uint8_t control_num, uint8_t value);

    /** Effect can override this and return false to disable the tap tempo functionality to utilize the switch (loopers, momentary
     * effects, etc) \return Value True if the Effect uses the alternate footswitch for tap tempo
     */
    virtual bool AlternateFootswitchForTempo() const { return true; };
    /** Overridable callback when alternate footswitch is pressed */
    virtual void AlternateFootswitchPressed() {};
    /** Overridable callback when alternate footswitch is released */
    virtual void AlternateFootswitchReleased() {};
    /** Overridable callback when alternate footswitch is held for 1 second */
    virtual void AlternateFootswitchHeldFor1Second() {};

  protected:
    /** Initializes the Parameter Storage and creates space for the specified number of stored Effect Parameters
        \param count  The number of stored parameters
    */
    virtual void InitParams(int count);

    /** This function gets called every time a parameter changes values.
     * By default it does nothing, but it's a good one to override if your effect
     * needs to do things when specific parameters change.
     * @param parameter_id  The id of the parameter that changed.
     */
    virtual void ParameterChanged(int parameter_id);

    float GetSampleRate() const { return m_sampleRate; }

    const char *m_name;                       // Name of the Effect
    int m_paramCount;                         // Number of Effect Parameters
    int m_presetCount;                        // Number of Stored Presets
    uint16_t m_currentPreset;                 // Current Preset in use
    uint32_t *m_params;                       // Dynamic Array of Effect Parameter Values
    const ParameterMetaData *m_paramMetaData; // Dynamic Array of the Meta Data for each Effect Parameter
    float m_audioLeft;                        // Last Audio Sample value for the Left Stereo Channel (or Mono)
    float m_audioRight;                       // Last Audio Sample value for the Right Stereo Channel
    uint32_t m_settingsArrayStartIdx;         // Start index of settings persistent storage struct
  private:
    bool m_isEnabled;
    float m_sampleRate; // Current Sample Rate this Effect was initialized for.
};
} // namespace bkshepherd
#endif
#endif
