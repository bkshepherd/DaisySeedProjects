#include "amp_module.h"
#include "../Util/audio_utilities.h"
#include "ImpulseResponse/ir_data.h"
#include "NeuralModels/model_data_gru9.h"

using namespace bkshepherd;
/*
static const char* s_modelBinNames[14] = {"Klon", "Fender57", "TS9", "Bassman", "5150 G75",
                                          "5150 G5", "ENGLInvG5", "ENGLInvG75", "TS7 Hot", "Matchless",
                                          "Mesa Amp", "Victory", "Ethos"};
*/

static const char *s_modelBinNames[7] = {"Fender57", "Matchless", "Klon", "Mesa iic", "Bassman", "5150", "Splawn"};

// static const char *s_irNames[10] = {"Rhythm",  "Lead",    "Clean",   "Marsh",     "Bogn",
//"Proteus", "Rectify", "Rhythm2", "US Deluxe", "British"};

static const char *s_irNames[4] = {"Marsh", "Proteus", "US Deluxe", "British"};

static const int s_paramCount = 8;
static const ParameterMetaData s_metaData[s_paramCount] = {
    {
        name : "Gain",
        valueType : ParameterValueType::Float,
        valueCurve : ParameterValueCurve::Log,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 0,
        midiCCMapping : 14
    },
    {name : "Mix", valueType : ParameterValueType::Float, defaultValue : {.float_value = 1.0f}, knobMapping : 1, midiCCMapping : 15},
    {name : "Level", valueType : ParameterValueType::Float, defaultValue : {.float_value = 0.5f}, knobMapping : 2, midiCCMapping : 16},
    {name : "Tone", valueType : ParameterValueType::Float, defaultValue : {.float_value = 1.0f}, knobMapping : 3, midiCCMapping : 17},
    {
        name : "Model",
        valueType : ParameterValueType::Binned,
        valueBinCount : 7,
        valueBinNames : s_modelBinNames,
        defaultValue : {.uint_value = 0},
        knobMapping : 4,
        midiCCMapping : 18
    },
    {
        name : "IR",
        valueType : ParameterValueType::Binned,
        valueBinCount : 4,
        valueBinNames : s_irNames,
        defaultValue : {.uint_value = 0},
        knobMapping : 5,
        midiCCMapping : 19
    },
    {
        name : "NeuralModel",
        valueType : ParameterValueType::Bool,
        defaultValue : {.uint_value = 1},
        knobMapping : -1,
        midiCCMapping : 20
    },
    {name : "IR On", valueType : ParameterValueType::Bool, defaultValue : {.uint_value = 1}, knobMapping : -1, midiCCMapping : 21}};

RTNeural::ModelT<float, 1, 1, RTNeural::GRULayerT<float, 1, 9>, RTNeural::DenseT<float, 9, 1>> model;
// 12 is currently the max size GRU I was able to get working with OPT flag on, 13 froze it
// 11 seems to be more practical, can add a few quality of life features

// Default Constructor
AmpModule::AmpModule()
    : BaseEffectModule(), m_gainMin(0.0f), m_gainMax(2.0f), m_levelMin(0.0f), m_levelMax(2.0f), m_toneFreqMin(400.0f),
      m_toneFreqMax(20000.0f), m_cachedEffectMagnitudeValue(1.0f) {
    // Set the name of the effect
    m_name = "Amp";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData;

    // Initialize Parameters for this Effect
    this->InitParams(s_paramCount);
}

// Destructor
AmpModule::~AmpModule() {
    // No Code Needed
}

void AmpModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);
    setupWeights(); // in the model data .h file
    SelectModel();
    SelectIR();
    CalculateMix();
    tone.Init(sample_rate);
    // bal.Init(sample_rate);
    CalculateTone();
}

void AmpModule::ParameterChanged(int parameter_id) {
    if (parameter_id == 4) { // Change Model
        SelectModel();
    } else if (parameter_id == 1) {
        CalculateMix();
    } else if (parameter_id == 3) {
        CalculateTone();
    } else if (parameter_id == 5) { // Change IR
        SelectIR();
    }
}
void AmpModule::SelectModel() {
    int modelIndex = GetParameterAsBinnedValue(4) - 1;
    if (m_currentModelindex != modelIndex) {
        auto &gru = (model).template get<0>();
        auto &dense = (model).template get<1>();
        gru.setWVals(model_collection[modelIndex].rec_weight_ih_l0);
        gru.setUVals(model_collection[modelIndex].rec_weight_hh_l0);
        gru.setBVals(model_collection[modelIndex].rec_bias);
        dense.setWeights(model_collection[modelIndex].lin_weight);
        dense.setBias(model_collection[modelIndex].lin_bias.data());
        model.reset();
        nnLevelAdjust = model_collection[modelIndex].levelAdjust;
        m_currentModelindex = modelIndex;
    }
}

void AmpModule::SelectIR() {
    int irIndex = GetParameterAsBinnedValue(5) - 1;
    if (irIndex != m_currentIRindex) {
        mIR.Init(ir_collection[irIndex]); // ir_data is from ir_data.h
    }
    m_currentIRindex = irIndex;
}

void AmpModule::CalculateMix() {
    //    A computationally cheap mostly energy constant crossfade from SignalSmith Blog
    //    https://signalsmith-audio.co.uk/writing/2021/cheap-energy-crossfade/

    float mixKnob = GetParameterAsFloat(1);
    float x2 = 1.0 - mixKnob;
    float A = mixKnob * x2;
    float B = A * (1.0 + 1.4186 * A);
    float C = B + mixKnob;
    float D = B + x2;

    wetMix = C * C;
    dryMix = D * D;
}

void AmpModule::CalculateTone() {
    // Set low pass filter as exponential taper
    tone.SetFreq(m_toneFreqMin + GetParameterAsFloat(3) * GetParameterAsFloat(3) * (m_toneFreqMax - m_toneFreqMin));
}

void AmpModule::ProcessMono(float in) {
    BaseEffectModule::ProcessMono(in);

    // GAIN and PREPARE INPUT //
    // Order of processing is Gain -> Neural Model -> EQ filter w/ level balance -> Wet/Dry mix -> Impulse Response -> Output Level

    float ampOut;
    float input_arr[1] = {0.0}; // Neural Net Input
    input_arr[0] = m_audioLeft * (m_gainMin + (m_gainMax - m_gainMin) * GetParameterAsFloat(0));

    // NEURAL MODEL //
    if (GetParameterAsBool(6)) {
        ampOut = model.forward(input_arr) + input_arr[0]; // Run Model and add Skip Connection
        ampOut *= nnLevelAdjust * 0.4;                    // Level adjustment
    } else {
        ampOut = input_arr[0];
    }

    // TONE //
    float filter_out = tone.Process(ampOut); // Apply tone Low Pass filter
    // float balanced_out = bal.Process(filter_out, ampOut); // Apply level adjustment to increase level of filtered signal

    // MIX //
    float mix_out = filter_out * wetMix + input_arr[0] * dryMix; // Applies model level adjustment ("/4.0"), wet/dry mix

    const float level = m_levelMin + (GetParameterAsFloat(2) * (m_levelMax - m_levelMin));

    // IMPULSE RESPONSE //
    if (GetParameterAsBool(7)) {
        m_audioLeft = mIR.Process(mix_out) * level * 0.2; // 0.2 is level adjust for loud output
    } else {
        m_audioLeft = mix_out * level;
    }

    m_audioRight = m_audioLeft;
}

void AmpModule::ProcessStereo(float inL, float inR) {
    // Calculate the mono effect
    ProcessMono(inL);

    // NOTE: Running the Neural Nets in stereo is currently not feasible due to processing limitations, this will remain a MONO ONLY
    // effect for now.
    //       The left channel output is copied to the right output, but the right input is ignored in this effect module.

    // Do the base stereo calculation (which resets the right signal to be the inputR instead of combined mono)
    // BaseEffectModule::ProcessStereo(m_audioLeft, inR);

    // m_audioRight = m_audioLeft;

    // Use the same magnitude as already calculated for the Left Audio
    // m_audioRight = m_audioRight * m_cachedEffectMagnitudeValue;
}

float AmpModule::GetBrightnessForLED(int led_id) const {
    float value = BaseEffectModule::GetBrightnessForLED(led_id);

    if (led_id == 1) {
        return value * m_cachedEffectMagnitudeValue;
    }

    return value;
}
