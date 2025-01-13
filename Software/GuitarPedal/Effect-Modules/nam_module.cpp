#include "nam_module.h"
#include "../Util/audio_utilities.h"
#include "Nam/model_data_nam.h"
#include <q/fx/biquad.hpp>
//#include <math_approx/math_approx.hpp>  // I dont think this is needed, only used in SIMD NAMMathsProvider
#include "Nam/wavenet/wavenet_model.hpp"

using namespace bkshepherd;

constexpr uint8_t NUM_FILTERS_NAM = 3;



const float minGain = -10.f;
const float maxGain = 10.f;

const float centerFrequencyNam[NUM_FILTERS_NAM] = {180.f, 1200.f, 5000.f}; // Experiment with these freqs and q values
const float q_nam[NUM_FILTERS_NAM] = {0.7f, 0.6f, 0.5f};

cycfi::q::peaking filter_nam[NUM_FILTERS_NAM] = {{0, centerFrequencyNam[0], 48000, q_nam[0]}, {0, centerFrequencyNam[1], 48000, q_nam[1]}, {0, centerFrequencyNam[2], 48000, q_nam[2]}};


//static const char* s_modelBinNames[3] = {"HRClean", "MarshJVM", "Tumnus32k"};
static const char* s_modelBinNames[8] = {"Mesa", "Match30", "DumHighG", "DumLowG", "Ethos", "Splawn", "PRSArch", "JCM800"};
struct NAMMathsProvider
{
#if RTNEURAL_USE_EIGEN
    template <typename Matrix>
    static auto tanh (const Matrix& x)
    {
        // See: math_approx::tanh<3>
        const auto x_poly = x.array() * (1.0f + 0.183428244899f * x.array().square());
        return x_poly.array() * (x_poly.array().square() + 1.0f).array().rsqrt();
        //return x.array().tanh(); // Tried using Eigen's built in tanh(), also works, failed on the same larger models as above custom tanh
    }
#elif RTNEURAL_USE_XSIMD
    template <typename T>
    static T tanh (const T& x)
    {
        return math_approx::tanh<3> (x);
    }
#endif
};


static const int s_paramCount = 8;
static const ParameterMetaData s_metaData[s_paramCount] = {{name: "Gain", valueType: ParameterValueType::Float, defaultValue: {.float_value = 0.5f}, knobMapping: 0, midiCCMapping: 14},
                                                           {name: "Level", valueType: ParameterValueType::Float, defaultValue: {.float_value = 0.5f}, knobMapping: 1, midiCCMapping: 15},
                                                           {name: "Model", valueType: ParameterValueType::Binned, valueBinCount: 8, valueBinNames: s_modelBinNames, defaultValue: {.uint_value = 0}, knobMapping: 2, midiCCMapping: 16},
                                                           {
                                                               name : "Bass",
                                                               valueType : ParameterValueType::Float,
                                                               valueBinCount : 0,
                                                               defaultValue : {.float_value = 0.0f},
                                                               knobMapping : 3,
                                                               midiCCMapping : 17,
                                                               minValue : minGain,
                                                               maxValue : maxGain
                                                           },
                                                           {
                                                               name : "Mid",
                                                               valueType : ParameterValueType::Float,
                                                               valueBinCount : 0,
                                                               defaultValue : {.float_value = 0.0f},
                                                               knobMapping : 4,
                                                               midiCCMapping : 18,
                                                               minValue : minGain,
                                                               maxValue : maxGain
                                                           },
                                                           {
                                                               name : "Treble",
                                                               valueType : ParameterValueType::Float,
                                                               valueBinCount : 0,
                                                               defaultValue : {.float_value = 0.0f},
                                                               knobMapping : 5,
                                                               midiCCMapping : 19,
                                                               minValue : minGain,
                                                               maxValue : maxGain
                                                           },
                                                           {name: "NeuralModel", valueType: ParameterValueType::Bool, valueBinCount: 0, defaultValue: {.uint_value = 1}, knobMapping: -1, midiCCMapping: 20},
                                                           {name: "EQ", valueType: ParameterValueType::Bool, valueBinCount: 0, defaultValue: {.uint_value = 1}, knobMapping: -1, midiCCMapping: 21},
                                                          
};


// NOTE NAM Standard arch?
/*
using Dilations = wavenet::Dilations<1, 2, 4, 8, 16, 32, 64, 128, 256, 512>;
wavenet::Wavenet_Model<float,
                       1,
                       wavenet::Layer_Array<float, 1, 1, 8, 16, 3, Dilations, false, NAMMathsProvider>,
                       wavenet::Layer_Array<float, 16, 1, 1, 8, 3, Dilations, true, NAMMathsProvider>>
    rtneural_wavenet; 
*/

// NOTE NAM "Pico" (unnoficial model type)
using Dilations = wavenet::Dilations<1, 2, 4, 8, 16, 32, 64>;
using Dilations2 = wavenet::Dilations<128, 256, 512, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512>;
wavenet::Wavenet_Model<float,
                       1,
                       wavenet::Layer_Array<float, 1, 1, 2, 2, 3, Dilations, false, NAMMathsProvider>,
                       wavenet::Layer_Array<float, 2, 1, 1, 2, 3, Dilations2, true, NAMMathsProvider>>
    rtneural_wavenet; 

//NOTES:
// nano models:
//   Seems to run (verify sound) for Samplerate 32kHz, Blocksize 64, 1 sample at a time
//   Freezes at Samplerate 48kHz, Blocksize 64, 1 sample at a time
//   Runs at samplerate 32kHz, Blocksize 48, 1 sample, verify sound


// Default Constructor
NamModule::NamModule() : BaseEffectModule(),
                                                        m_gainMin(0.0f),
                                                        m_gainMax(2.0f),
                                                        m_toneFreqMin(400.0f),
                                                        m_toneFreqMax(20000.0f),
                                                        m_cachedEffectMagnitudeValue(1.0f)
{
    // Set the name of the effect
    m_name = "NAM";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData;
    
    // Initialize Parameters for this Effect
    this->InitParams(s_paramCount);
}

// Destructor
NamModule::~NamModule()
{
    // No Code Needed
}

void NamModule::Init(float sample_rate)
{
    BaseEffectModule::Init(sample_rate);
    setupWeightsNam(); // in the model data nam .h file
    SelectModel();

    filter_nam[0].config(GetParameterAsFloat(3), centerFrequencyNam[0], sample_rate, q_nam[0]);
    filter_nam[1].config(GetParameterAsFloat(4), centerFrequencyNam[1], sample_rate, q_nam[1]);
    filter_nam[2].config(GetParameterAsFloat(5), centerFrequencyNam[2], sample_rate, q_nam[2]);
}

void NamModule::ParameterChanged(int parameter_id)
{
    if (parameter_id == 2) {  // Change Model
        SelectModel();
    } else if (parameter_id == 3) {
        filter_nam[0].config(GetParameterAsFloat(3), centerFrequencyNam[0], GetSampleRate(), q_nam[0]);
    } else if (parameter_id == 4) {
        filter_nam[1].config(GetParameterAsFloat(4), centerFrequencyNam[1], GetSampleRate(), q_nam[1]);
    } else if (parameter_id == 5) { 
        filter_nam[2].config(GetParameterAsFloat(5), centerFrequencyNam[2], GetSampleRate(), q_nam[2]);
    }
}

void NamModule::SelectModel()
{
    int modelIndex = GetParameterAsBinnedValue(2) - 1;

    if (m_currentModelindex != modelIndex) {
        float temp = GetParameterAsFloat(1);
        SetParameterAsFloat(1,0.0);
        rtneural_wavenet.load_weights (model_collection_nam[modelIndex].weights);
        static constexpr size_t N = 1; // number of samples sent through model at once
        rtneural_wavenet.prepare (N); // This is needed, including this allowed the led to come on before freezing
        rtneural_wavenet.prewarm();  // Note: looks like this just sends some 0's through the model
        m_currentModelindex = modelIndex;
        SetParameterAsFloat(1,temp);
    }

}



void NamModule::ProcessMono(float in)
{
    BaseEffectModule::ProcessMono(in);

    float ampOut;
    float input_arr[1] = { 0.0 };    // Neural Net Input
    input_arr[0] = m_audioLeft * (m_gainMin + (m_gainMax - m_gainMin) * GetParameterAsFloat(0));

    // NEURAL MODEL //
    if (GetParameterAsBool(6))
    {    
        ampOut = rtneural_wavenet.forward (input_arr[0]) * 0.4;   // TODO Try this again, was sending the whole array, wants just the float
    } else {
        ampOut = input_arr[0];
    }

    // Apply 3 band EQ
    if (GetParameterAsBool(7)) {
        for (uint8_t i = 0; i < NUM_FILTERS_NAM; i++) {
            ampOut = filter_nam[i](ampOut);
        }
    }

    m_audioRight = m_audioLeft = ampOut * GetParameterAsFloat(1);
}

void NamModule::ProcessStereo(float inL, float inR)
{    
    // Calculate the mono effect
    ProcessMono(inL);

    // NOTE: Running the Neural Nets in stereo is currently not feasible due to processing limitations, this will remain a MONO ONLY effect for now.
    //       The left channel output is copied to the right output, but the right input is ignored in this effect module.

    // Do the base stereo calculation (which resets the right signal to be the inputR instead of combined mono)
    //BaseEffectModule::ProcessStereo(m_audioLeft, inR);
    
    //m_audioRight = m_audioLeft;
    
    // Use the same magnitude as already calculated for the Left Audio
    //m_audioRight = m_audioRight * m_cachedEffectMagnitudeValue;
}


float NamModule::GetBrightnessForLED(int led_id) const
{    
    float value = BaseEffectModule::GetBrightnessForLED(led_id);

    if (led_id == 1)
    {
        return value * m_cachedEffectMagnitudeValue;
    }

    return value;
}
