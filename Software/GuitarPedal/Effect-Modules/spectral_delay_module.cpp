#include "spectral_delay_module.h"
#include "../Util/audio_utilities.h"

using namespace bkshepherd;
using namespace soundmath;

#define PI 3.1415926535897932384626433832795
// convenient lookup tables
Wave<float> hann([](float phase) -> float {
    return 0.5 * (1 - cos(2 * PI * phase));
}); // see if moving this to sdram significantly slows things down, YES, fails to load if allocated to SDRAM
// Wave<float> halfhann([] (float phase) -> float { return sin(PI * phase); });

// 4 overlapping windows of size 2^12 = 4096
// `N = 4096` and `laps = 4` (higher frequency resolution, greater latency), or when `N = 2048` and `laps = 8` (higher time resolution,
// less latency). For Saturn, I'm using N=1024, N=4

// const size_t order = 10;      // 1024
const size_t order = 8; // 256 // Changing to order 8 from 10 allowed running at 48 blocksize instaed of 256
const size_t N = (1 << order);
const float sqrtN = sqrt(N);
const size_t laps = 4;
const size_t buffsize = 2 * laps * N;

// convenient constant for grabbing imaginary parts
static const size_t offset = N / 2; // equals 512

// buffers for STFT processing
// audio --> in --(fft)--> middle --(process)--> out --(ifft)--> in -->
// each of these is a few circular buffers stacked end-to-end.
float DSY_SDRAM_BSS in[buffsize];     // buffers for input and output (from / to user audio callback)
float DSY_SDRAM_BSS middle[buffsize]; // buffers for unprocessed frequency domain data
float DSY_SDRAM_BSS out[buffsize];    // buffers for processed frequency domain data

ShyFFT<float, N, RotationPhasor> *fft; // fft object
Fourier<float, N> *stft;               // stft object

float fft_size = N / 2;

// Delay
#define MAX_DELAY_SPECTRAL_DELAY                                                                                                      \
    static_cast<size_t>(188 * 4.f) // 4 second max delay (4 second spread plus 1 second predelay), delay called 188 times per second

// const int delay_array_size = 175;
const int delay_array_size = 120;
DelayLine<float, MAX_DELAY_SPECTRAL_DELAY> DSY_SDRAM_BSS delayLine_array_real[delay_array_size];
DelayLine<float, MAX_DELAY_SPECTRAL_DELAY> DSY_SDRAM_BSS delayLine_array_imag[delay_array_size];

float vtone = 0.0;
bool mono_mode = false;

struct delaySpect {
    DelayLine<float, MAX_DELAY_SPECTRAL_DELAY> *del;
    float currentDelay;
    float delayTarget;
    float feedback;
    float active = false;

    float Process(float in) {
        // set delay times
        fonepole(currentDelay, delayTarget, .0002f);
        del->SetDelay(currentDelay);

        float read = del->Read();
        del->Write((feedback * read) + in);
        return read;
    }
};

struct delaySpect delay_array_real[delay_array_size];
struct delaySpect delay_array_imag[delay_array_size];

unsigned int filter_bin = 0;

inline void spectraldelay(const float *in, float *out) {
    for (size_t i = 0; i < N / 2; i++) // loop i from 0 to 511
    {

        float real = 0.0;
        float imag = 0.0;
        float test = vtone * 20;
        int tone_bins = test;
        // each delayline affects 1 bin. 512 total bins. only using first 175 bins in more audible frequencies, more causes dropouts
        if (i < delay_array_size && i > tone_bins) {

            real = in[i];
            imag = in[i + offset];

            real = delay_array_real[i].Process(real);
            imag = delay_array_imag[i].Process(imag);
        }

        out[i] = real;
        out[i + offset] = imag;
    }
}

static const char *s_timeMode[5] = {"Random", "Sine", "LinearUp", "LinearDn", "Const"};
static const char *s_fdbkMode[5] = {"Random", "Sine", "LinearUp", "LinearDn", "Const"};

static const int s_paramCount = 6;
static const ParameterMetaData s_metaData[s_paramCount] = {
    {name : "Mix", valueType : ParameterValueType::Float, defaultValue : {.float_value = 0.5f}, knobMapping : 0, midiCCMapping : 14},
    {name : "Time", valueType : ParameterValueType::Float, defaultValue : {.float_value = 0.5f}, knobMapping : 1, midiCCMapping : 15},
    {name : "FDBK", valueType : ParameterValueType::Float, defaultValue : {.float_value = 0.5f}, knobMapping : 2, midiCCMapping : 16},
    {
        name : "TimeMode",
        valueType : ParameterValueType::Binned,
        valueBinCount : 5,
        valueBinNames : s_timeMode,
        defaultValue : {.uint_value = 0},
        knobMapping : 3,
        midiCCMapping : 17
    },
    {
        name : "FDBK Mode",
        valueType : ParameterValueType::Binned,
        valueBinCount : 5,
        valueBinNames : s_fdbkMode,
        defaultValue : {.uint_value = 0},
        knobMapping : 4,
        midiCCMapping : 18
    },
    {name : "Tone", valueType : ParameterValueType::Float, defaultValue : {.float_value = 0.0f}, knobMapping : 5, midiCCMapping : 19},
};

// Default Constructor
SpectralDelayModule::SpectralDelayModule() : BaseEffectModule(), m_cachedEffectMagnitudeValue(1.0f) {
    // Set the name of the effect
    m_name = "SpctDelay";

    // Setup the meta data reference for this Effect
    m_paramMetaData = s_metaData;

    // Initialize Parameters for this Effect
    this->InitParams(s_paramCount);
}

// Destructor
SpectralDelayModule::~SpectralDelayModule() {
    // No Code Needed
}

void SpectralDelayModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);

    // initialize FFT and STFT objects
    fft = new ShyFFT<float, N, RotationPhasor>();
    fft->Init();
    stft = new Fourier<float, N>(spectraldelay, fft, &hann, laps, in, middle, out);

    // Initialize delay array settings
    for (int i = 0; i < delay_array_size; i++) {
        delayLine_array_real[i].Init();
        delay_array_real[i].del = &delayLine_array_real[i];
        delay_array_real[i].delayTarget = 100; // in samples
        delay_array_real[i].feedback = 0.0;
        delay_array_real[i].active = true;

        delayLine_array_imag[i].Init();
        delay_array_imag[i].del = &delayLine_array_imag[i];
        delay_array_imag[i].delayTarget = 100; // in samples
        delay_array_imag[i].feedback = 0.0;
        delay_array_imag[i].active = true;
    }
}

void SpectralDelayModule::ParameterChanged(int parameter_id) // Somewhere here is causeing issues on start up, if I take them out it
                                                             // works, adding them in breaks, but it worked once???
{

    // static const char *s_timeMode[5] = {"Random", "Sine", "LinearUp", "LinearDn", "Const" };
    // static const char *s_fdbkMode[4] = {"Random", "LinearUp", "LinearDn", "Const"};

    if (parameter_id == 1 || parameter_id == 3) { // Time or Time Mode
        float vdelay_time = GetParameterAsFloat(1);
        int delay_time_mode = (GetParameterAsBinnedValue(3) - 1);
        float cycles = 4.0; // when time mode is sine wave, this changes the frequency of the sine wave across frequency bins

        for (int i = 0; i < delay_array_size; i++) {
            if (delay_time_mode == 0) {
                float r = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX));
                delay_array_real[i].delayTarget = delay_array_imag[i].delayTarget =
                    r * 4 * 188 * vdelay_time; // random delay time for each bin up to 4 seconds, mod

            } else if (delay_time_mode == 1) {
                delay_array_real[i].delayTarget = delay_array_imag[i].delayTarget =
                    (sin((i * cycles / delay_array_size) * 2 * PI) + 1.0) * vdelay_time * 188 *
                    2; // sin wave scaled from 0 up to 4 seconds

            } else if (delay_time_mode == 2) {
                delay_array_real[i].delayTarget = delay_array_imag[i].delayTarget =
                    vdelay_time * 4 * 188 * i / delay_array_size; // linear delay time increase from low to high freq, 0 to 4 seconds,
                                                                  // mod determines steepness of slope

            } else if (delay_time_mode == 3) {
                delay_array_real[delay_array_size - 1 - i].delayTarget = delay_array_imag[delay_array_size - 1 - i].delayTarget =
                    vdelay_time * 4 * 188 * i / delay_array_size; // linear delay time decrease low to high

            } else if (delay_time_mode == 4) {
                delay_array_real[i].delayTarget = delay_array_imag[i].delayTarget = vdelay_time * 4 * 188; // const time delay
            }
        }

    } else if (parameter_id == 2 || parameter_id == 4) { // FDBK or FDBK Mode
        float vdelay_fdbk = GetParameterAsFloat(2);

        int delay_fdbk_mode = (GetParameterAsBinnedValue(4) - 1);
        float cycles =
            4.0; // when fdbk mode is center for sine wave, this changes the frequency of the sine wave across frequency bins

        for (int i = 0; i < delay_array_size; i++) {
            if (delay_fdbk_mode == 0) {
                float r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
                delay_array_real[i].feedback = delay_array_imag[i].feedback = r;

            } else if (delay_fdbk_mode == 1) {
                delay_array_real[i].feedback = delay_array_imag[i].feedback =
                    (sin((i * cycles / delay_array_size) * 2 * PI) + 1.0) * vdelay_fdbk;

            } else if (delay_fdbk_mode == 2) {
                delay_array_real[i].feedback = delay_array_imag[i].feedback = vdelay_fdbk * i / delay_array_size;

            } else if (delay_fdbk_mode == 3) {
                delay_array_real[delay_array_size - 1 - i].feedback = delay_array_imag[delay_array_size - 1 - i].feedback =
                    vdelay_fdbk * i / delay_array_size;

            } else if (delay_fdbk_mode == 4) {
                delay_array_real[i].feedback = delay_array_imag[i].feedback = vdelay_fdbk;
            }
        }

    } else if (parameter_id == 5) { // Tone
        vtone = GetParameterAsFloat(5);
    }
}

void SpectralDelayModule::ProcessMono(float in) {
    BaseEffectModule::ProcessMono(in);

    float inputL = m_audioLeft;
    // float inputR = m_audioLeft;

    float vmix = GetParameterAsFloat(0);
    float delaygain = 3.0;

    stft->write(inputL);                                                   // put a new sample in the STFT
    m_audioLeft = stft->read() * vmix * delaygain + inputL * (1.0 - vmix); // read the next sample from the STFT
    m_audioRight = m_audioLeft;
}

void SpectralDelayModule::ProcessStereo(float inL, float inR) {
    // Calculate the mono effect
    ProcessMono(inL);

    // Do the base stereo calculation (which resets the right signal to be the inputR instead of combined mono)
    // BaseEffectModule::ProcessStereo(m_audioLeft, inR);

    // Use the same magnitude as already calculated for the Left Audio
    // m_audioRight = m_audioRight * m_cachedEffectMagnitudeValue;
}

float SpectralDelayModule::GetBrightnessForLED(int led_id) const {
    float value = BaseEffectModule::GetBrightnessForLED(led_id);

    if (led_id == 1) {
        return value * m_cachedEffectMagnitudeValue;
    }

    return value;
}
