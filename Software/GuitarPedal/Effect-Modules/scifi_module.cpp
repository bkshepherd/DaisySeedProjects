#include "scifi_module.h"
#include "../Util/audio_utilities.h"

#include <q/support/literals.hpp>
#include <q/fx/biquad.hpp>

//#include "../Util/EffectState.h"
#include "../Util/Multirate.h"
#include "../Util/OctaveGenerator.h"

using namespace bkshepherd;
namespace q = cycfi::q;
using namespace q::literals;

// naming everything _scifi so it wont conflict with other polyoctave module
static Decimator2 decimate_scifi;
static Interpolator interpolate_scifi;
static const auto sample_rate_temp = 48000; //hard code for now                          // NOTE: the sample_rate must be divisible by the resample_factor (48/6 = 8)
static OctaveGenerator octave_scifi(sample_rate_temp / resample_factor); // resample_factor is defined in Multirate.h and equals 6
static q::highshelf eq1_scifi(-11, 140_Hz, sample_rate_temp);
static q::lowshelf eq2_scifi(5, 160_Hz, sample_rate_temp);


ReverbSc DSY_SDRAM_BSS reverbStereo_scifi;

static const int s_paramCount = 9;
static const ParameterMetaData s_metaData[s_paramCount] = {{name: "Dry", valueType: ParameterValueType::Float, defaultValue: {.float_value = 0.5f}, knobMapping: 0, midiCCMapping: 14},
                                                           {name: "OctDown", valueType: ParameterValueType::Float, defaultValue: {.float_value = 0.5f}, knobMapping: 1, midiCCMapping: 15},
                                                           //{name: "1 OctDown", valueType: ParameterValueType::Float, defaultValue: {.float_value = 0.5f}, knobMapping: 2, midiCCMapping: 16},
                                                           {name: "OctUp", valueType: ParameterValueType::Float, defaultValue: {.float_value = 0.5f}, knobMapping: 2, midiCCMapping: 16},
                                                           {
                                                               name : "Time",
                                                               valueType : ParameterValueType::Float,
                                                               valueBinCount : 0,
                                                               defaultValue : {.float_value = 0.8f},
                                                               knobMapping : 3,
                                                               midiCCMapping : 17
                                                           },
                                                           {
                                                               name : "Drive",
                                                               valueType : ParameterValueType::Float,
                                                               valueBinCount : 0,
                                                               defaultValue : {.float_value = 0.5f},
                                                               knobMapping : 4,
                                                               midiCCMapping : 18
                                                           },
                                                           {
                                                               name : "Mix",
                                                               valueType : ParameterValueType::Float,
                                                               valueBinCount : 0,
                                                               defaultValue : {.float_value = 0.5f},
                                                               knobMapping : 5,
                                                               midiCCMapping : 19
                                                           },
                                                           {
                                                               name : "Damp",
                                                               valueType : ParameterValueType::Float,
                                                               valueBinCount : 0,
                                                               defaultValue : {.float_value = 0.3f},
                                                               knobMapping : -1,
                                                               midiCCMapping : 20
                                                           },

                                                           {
                                                               name : "Level",
                                                               valueType : ParameterValueType::Float,
                                                               valueBinCount : 0,
                                                               defaultValue : {.float_value = 0.5f},
                                                               knobMapping : -1,
                                                               midiCCMapping : 21
                                                           },

                                                           {
                                                               name : "OD Mix",
                                                               valueType : ParameterValueType::Float,
                                                               valueBinCount : 0,
                                                               defaultValue : {.float_value = 1.0f},
                                                               knobMapping : -1,
                                                               midiCCMapping : 22
                                                           }

};

// Default Constructor
SciFiModule::SciFiModule() : BaseEffectModule(),
                                                        m_driveMin(0.4f),
                                                        m_driveMax(0.6f),
                                                        m_levelMin(0.0f),
                                                        m_levelMax(1.0f),
                                                        m_timeMin(0.6f), 
                                                        m_timeMax(1.0f), 
                                                        m_lpFreqMin(500.0f), 
                                                        m_lpFreqMax(16000.0f),
                                                        m_cachedEffectMagnitudeValue(1.0f)
{
    // Set the name of the effect
    m_name = "SciFi";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData;
    
    // Initialize Parameters for this Effect
    this->InitParams(s_paramCount);
}

// Destructor
SciFiModule::~SciFiModule()
{
    // No Code Needed
}

void SciFiModule::Init(float sample_rate)
{
    BaseEffectModule::Init(sample_rate);

    // Initialize buffers to 0
    for (int j = 0; j < 6; ++j) {
        buff[j] = 0.0;
        buff_out[j] = 0.0;
    }

    m_reverbStereo = &reverbStereo_scifi;
    m_reverbStereo->Init(sample_rate);

    m_overdriveLeft.Init();
    m_overdriveRight.Init();

}

void SciFiModule::ProcessMono(float in)
{
    BaseEffectModule::ProcessMono(in);

    float input = m_audioLeft;
    buff[bin_counter] = m_audioLeft; // making a workaround for only processing sample by sample instead of block, will add 6 samples of latency

    float dryLevel = GetParameterAsFloat(0);
    float down1Level = GetParameterAsFloat(1); // Setting 2 oct down and 1 oct down with one parameter
    float down2Level = GetParameterAsFloat(1); // Setting 2 oct down and 1 oct down with one parameter
    float up1Level = GetParameterAsFloat(2);

    // Process PolyOctave //////////////////////////////
    
    // do calculation every 6 samples
    if (bin_counter > 4) {

        std::span<const float, resample_factor> in_chunk(&(buff[0]), resample_factor);  // std::span is c++ 20 feature
            
        const auto sample = decimate_scifi(in_chunk); 

        float octave_mix = 0;
        octave_scifi.update(sample);
        octave_mix += up1Level * octave_scifi.up1() * 2.0;
        octave_mix += down1Level * octave_scifi.down1() * 2.0;
        octave_mix += down2Level * octave_scifi.down2() * 2.0;

        auto out_chunk = interpolate_scifi(octave_mix);
        for (size_t j = 0; j < out_chunk.size(); ++j)
        {
            float mix = eq2_scifi(eq1_scifi(out_chunk[j]));

            const auto dry_signal = buff[j];
            mix += dryLevel * buff[j];

            buff_out[j] = mix;
        }
    }

    // Increments the buffer index from 0 to 5 (workaround to adapt code)
    bin_counter += 1;
    if (bin_counter > 5)
        bin_counter = 0;



    ////////////////////////////////////////////////////////////// 
    // Calculate the Reverb
    float sendl, sendr, wetl, wetr; // Reverb Inputs/Outputs

    sendl = sendr = buff_out[bin_counter];

    m_reverbStereo->SetFeedback(m_timeMin + GetParameterAsFloat(3) * (m_timeMax - m_timeMin));
    float invertedFreq = 1.0 - GetParameterAsFloat(6); // Invert the damping param so that knob left is less dampening, knob right is more dampening
    invertedFreq = invertedFreq * invertedFreq; // also square it for exponential taper (more control over lower frequencies)

    m_reverbStereo->SetLpFreq(m_lpFreqMin + invertedFreq * (m_lpFreqMax - m_lpFreqMin));

    m_reverbStereo->Process(sendl, sendr, &wetl, &wetr);


    //////////////////////////////////////////////////////////////

    // Overdrive the reverb output
    float drive_setting = m_driveMin + (GetParameterAsFloat(4) * (m_driveMax - m_driveMin));
    m_overdriveLeft.SetDrive(drive_setting);
    m_overdriveRight.SetDrive(drive_setting);
 

    float od_out_left = m_overdriveLeft.Process(wetl*0.8) * (1.0 - (drive_setting * drive_setting * 2.8 - 0.1296)); // reduce volume as od drive goes up (otherwise way too loud)
    float od_out_right = m_overdriveRight.Process(wetr*0.8) * (1.0 - (drive_setting * drive_setting * 2.8 - 0.1296)); // reduce volume as od drive goes up (otherwise way too loud)

    // Mix regular reverb with overdriven reverb (default is full overdrive)
    float overdrive_mix_left = od_out_left * GetParameterAsFloat(8) * 0.6 + wetl * (1.0 - GetParameterAsFloat(8));
    float overdrive_mix_right = od_out_right * GetParameterAsFloat(8) * 0.6 + wetr * (1.0 - GetParameterAsFloat(8));

    // Mix in the dry signal and set overall level
    m_audioLeft = (overdrive_mix_left * GetParameterAsFloat(5) + input * (1.0 - GetParameterAsFloat(5))) * GetParameterAsFloat(7);
    m_audioRight = (overdrive_mix_right * GetParameterAsFloat(5) + input * (1.0 - GetParameterAsFloat(5))) * GetParameterAsFloat(7);
}

void SciFiModule::ProcessStereo(float inL, float inR)
{    
    // Calculate the mono effect (Scifi is currently a MISO Mono in stereo out effect)
    ProcessMono(inL);

    // Do the base stereo calculation (which resets the right signal to be the inputR instead of combined mono)
    //BaseEffectModule::ProcessStereo(m_audioLeft, inR);  // Mono only for now

    // Use the same magnitude as already calculated for the Left Audio
    //m_audioRight = m_audioRight * m_cachedEffectMagnitudeValue;
}




float SciFiModule::GetBrightnessForLED(int led_id) const
{    
    float value = BaseEffectModule::GetBrightnessForLED(led_id);

    if (led_id == 1)
    {
        return value * m_cachedEffectMagnitudeValue;
    }

    return value;
}
