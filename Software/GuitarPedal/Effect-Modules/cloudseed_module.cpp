#include "cloudseed_module.h"
#include "../Util/audio_utilities.h"

// This is used in the modified CloudSeed code for allocating
// delay line memory to SDRAM (64MB available on Daisy)
//#define CUSTOM_POOL_SIZE (48*1024*1024)
//#define CUSTOM_POOL_SIZE (48*512*512) // works
#define CUSTOM_POOL_SIZE (48*384*384) // Works! test more thoroughly with other presets, if it freezes, check here first TODO
//#define CUSTOM_POOL_SIZE (48*256*256) // freezes

DSY_SDRAM_BSS char custom_pool[CUSTOM_POOL_SIZE];
size_t pool_index = 0;
int allocation_count = 0;
void* custom_pool_allocate(size_t size)
{
        if (pool_index + size >= CUSTOM_POOL_SIZE)
        {
                return 0;
        }
        void* ptr = &custom_pool[pool_index];
        pool_index += size;
        return ptr;
}

using namespace bkshepherd;

static const char* s_presetNames[8] = {"FChorus", "DullEchos", "Hyperplane", "MedSpace", "Hallway", "RubiKa", "SmallRoom", "90s"};

static const int s_paramCount = 10;
static const ParameterMetaData s_metaData[s_paramCount] = {{name: "PreDelay", valueType: ParameterValueType::Float, defaultValue: {.float_value = 0.0f}, knobMapping: 0, midiCCMapping: 14},
                                                           {name: "Mix", valueType: ParameterValueType::Float, defaultValue: {.float_value = 0.5f}, knobMapping: 1, midiCCMapping: 15},
                                                           {name: "Decay", valueType: ParameterValueType::Float, defaultValue: {.float_value = 0.5f}, knobMapping: 2, midiCCMapping: 16},
                                                           {name: "Mod Amt", valueType: ParameterValueType::Float, defaultValue: {.float_value = 0.5f}, knobMapping: 3, midiCCMapping: 17},
                                                           {name: "Mod Rate", valueType: ParameterValueType::Float, defaultValue: {.float_value = 0.5f}, knobMapping: 4, midiCCMapping: 18},
                                                           {name: "Tone", valueType: ParameterValueType::Float, defaultValue: {.float_value = 0.5f}, knobMapping: 5, midiCCMapping: 19},
                                                           {name: "Preset", valueType: ParameterValueType::Binned, valueBinCount: 8, valueBinNames: s_presetNames, defaultValue: {.uint_value = 6}, knobMapping: -1, midiCCMapping: 20},
                                                           {name: "Sum2Mono", valueType: ParameterValueType::Bool, defaultValue: {.uint_value = 0}, knobMapping: -1, midiCCMapping: 21},
                                                           {name: "StereoIn", valueType: ParameterValueType::Bool, defaultValue: {.uint_value = 0}, knobMapping: -1, midiCCMapping: 22},
                                                           {name: "KnobsOvrd", valueType: ParameterValueType::Bool, defaultValue: {.uint_value = 0}, knobMapping: -1, midiCCMapping: 23} // "KnobsOverride(Presets)" - Making this True always makes the knob settings override the preset settings. If false, changing the preset changes all settings until a knob is moved.
                                                           };





// NOTES ABOUT THE CLOUDSEED PARAMETERS
// 1. I changed the "Parameter" class to be "Parameter2" to deconflict with the DaisySP Parameter. Likely there was a much easier way to remedy this.
// 2. I have hard coded the presets to work with the limited processing on the Daisy Seed. The main parameters that affect processing are:
//        Mostly these:  LineCount, LateDiffusionStages      And to a lesser degree these:  DiffusionStages, TapCount (Added tapcount as param, halved the full range)
//     Increasing the above params past what I have them set at in "ReverbController.h" may freeze the pedal processing. 
//     Currently the max stereo line count is 2 on the Daisy Seed, so the presets will sound different than the desktop CloudSeed plugin.



// Default Constructor
CloudSeedModule::CloudSeedModule() : BaseEffectModule(),
                                                        m_gainMin(0.0f),
                                                        m_gainMax(1.0f),
                                                        m_cachedEffectMagnitudeValue(1.0f)
{
    // Set the name of the effect
    m_name = "CloudSeed";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData;

    // Initialize Parameters for this Effect
    this->InitParams(s_paramCount);
}

// Destructor
CloudSeedModule::~CloudSeedModule()
{
    // No Code Needed
}

void CloudSeedModule::Init(float sample_rate)
{
    BaseEffectModule::Init(sample_rate);

    AudioLib::ValueTables::Init();
    CloudSeed::FastSin::Init();

    reverb = new CloudSeed::ReverbController(sample_rate);
    reverb->ClearBuffers();
    reverb->initFactoryRubiKaFields(); // Not setting a preset at the beginning allows you to save the previous preset
    reverb->SetParameter(::Parameter2::LineCount, 3); // 2 on factory chorus for stereo is max, 3 froze it
    CalculateMix();
}

void CloudSeedModule::ParameterChanged(int parameter_id)  // Somewhere here is causeing issues on start up, if I take them out it works, adding them in breaks, but it worked once???
{
    if (parameter_id == 6) {  // Preset  
        // Change the preset and then override with current knob settings
        changePreset();    
        if (GetParameterAsBool(9)) { // If true, apply current knob settings over the chosen preset. If false, don't update until knob is moved.     
            reverb->SetParameter(::Parameter2::PreDelay, GetParameterAsFloat(0) * 0.95); // Was freezing pedal when set to 127
            reverb->SetParameter(::Parameter2::LineDecay, GetParameterAsFloat(2));
            reverb->SetParameter(::Parameter2::LineModAmount, GetParameterAsFloat(3));
            reverb->SetParameter(::Parameter2::LineModRate, GetParameterAsFloat(4));
            reverb->SetParameter(::Parameter2::CutoffEnabled, 1.0); // If this knob is moved, turn on the cutoff filter, the presets will reset this on/off as needed
            reverb->SetParameter(::Parameter2::PostCutoffFrequency, GetParameterAsFloat(5));
        }
    } else {

        if (throttle_counter > 5) { // The calls to reverb settings need to be throttled, else it will freeze the pedal on startup
                                    // TODO: This will prevent single param change calls (such as a single programmed midi message) to only work every 5 calls. Figure out a better fix
            if (parameter_id == 0) {  // {PreDelay
                reverb->SetParameter(::Parameter2::PreDelay, GetParameterAsFloat(0) * 0.95); // Was freezing pedal when set to 127
            } else if (parameter_id == 1) {  // Mix
                CalculateMix();
            } else if (parameter_id == 2) {  // Decay
                reverb->SetParameter(::Parameter2::LineDecay, GetParameterAsFloat(2));
            } else if (parameter_id == 3) {  // Mod Amt
                reverb->SetParameter(::Parameter2::LineModAmount, GetParameterAsFloat(3));
            } else if (parameter_id == 4) {  // Mod Rate
                reverb->SetParameter(::Parameter2::LineModRate, GetParameterAsFloat(4));
            } else if (parameter_id == 5) {  // Tone
                reverb->SetParameter(::Parameter2::CutoffEnabled, 1.0); // If this knob is moved, turn on the cutoff filter, the presets will reset this on/off as needed
                reverb->SetParameter(::Parameter2::PostCutoffFrequency, GetParameterAsFloat(5));
            }
            throttle_counter = 0;
        }
        throttle_counter += 1;
    }
}


void CloudSeedModule::changePreset()
{
    
    int c = (GetParameterAsBinnedValue(6) - 1);
    reverb->ClearBuffers();

    if ( c == 0 ) {
            reverb->initFactoryChorus();
    } else if ( c == 1 ) {
            reverb->initFactoryDullEchos();
    } else if ( c == 2 ) {
            reverb->initFactoryHyperplane();
    } else if ( c == 3 ) {
            reverb->initFactoryMediumSpace();
    } else if ( c == 4 ) {
            reverb->initFactoryNoiseInTheHallway();
    } else if ( c == 5 ) {
            reverb->initFactoryRubiKaFields();
    } else if ( c == 6 ) {
           reverb->initFactorySmallRoom();
    } else if ( c == 7 ) {
            reverb->initFactory90sAreBack();
    } 
    
}

void CloudSeedModule::CalculateMix()
{
    //    A computationally cheap mostly energy constant crossfade from SignalSmith Blog
    //    https://signalsmith-audio.co.uk/writing/2021/cheap-energy-crossfade/

    float mixKnob = GetParameterAsFloat(1);
    float x2 = 1.0 - mixKnob;
    float A = mixKnob*x2;
    float B = A * (1.0 + 1.4186 * A);
    float C = B + mixKnob;
    float D = B + x2;

    wetMix = C * C;
    dryMix = D * D;
}


void CloudSeedModule::ProcessMono(float in)
{
    BaseEffectModule::ProcessMono(in);


    float inL[1];
    float outL[1];
    float inR[1];
    float outR[1];

    inL[0] = m_audioLeft;
    inR[0] = m_audioLeft;

    reverb->Process(inL, inR, outL, outR, 1);

    if (GetParameterAsBool(7)) { // If "Sum2Mono" is on, combine L and R signals and half the level
        m_audioLeft = ((outL[0] + outR[0]) / 2.0) * wetMix  + inL[0] * dryMix;
        m_audioRight = m_audioLeft;    
    } else {
        m_audioLeft = outL[0] * wetMix  + inL[0] * dryMix;
        m_audioRight = outR[0] * wetMix  + inR[0] * dryMix;        
    }

}

void CloudSeedModule::ProcessStereo(float inL, float inR)
{
    // Calculate the mono effect
    //ProcessMono(inL);
    BaseEffectModule::ProcessStereo(inL, inR);

    float inL2[1];
    float outL[1];
    float inR2[1];
    float outR[1];

    inL2[0] = m_audioLeft;
    if (GetParameterAsBool(8)) { // If Stereo In is true (TODO Verify this works)
        inR2[0] = m_audioRight;
    } else {
        inR2[0] = m_audioLeft; 
    }

    reverb->Process(inL2, inR2, outL, outR, 1);

    if (GetParameterAsBool(7)) { // If "Sum2Mono" is on, combine L and R signals and half the level
        m_audioLeft = ((outL[0] + outR[0]) / 2.0) * wetMix  + inL2[0] * dryMix;
        m_audioRight = m_audioLeft;    
    } else {
        m_audioLeft = outL[0] * wetMix  + inL2[0] * dryMix;
        m_audioRight = outR[0] * wetMix  + inR2[0] * dryMix;        
    }
}


float CloudSeedModule::GetBrightnessForLED(int led_id) const
{
    float value = BaseEffectModule::GetBrightnessForLED(led_id);

    if (led_id == 1)
    {
        return value * m_cachedEffectMagnitudeValue;
    }

    return value;
}