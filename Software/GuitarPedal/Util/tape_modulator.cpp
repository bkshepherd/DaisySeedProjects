#include "tape_modulator.h"
#include "daisy_seed.h"

// Actual definitions of the static members
DSY_SDRAM_BSS uint8_t TapeModulator::perm_[512];

void TapeModulator::Init(float sample_rate) {
    for(int i = 0; i < 256; i++) {
        perm_[i] = perm_[i + 256] = p_[i];
    }
    t_wow_ = 0.0f;
    t_flutter_ = 0.0f;
    sample_rate_ = sample_rate;
}

float TapeModulator::Perlin1D(float x)
{
    int X = (int)floorf(x) & 255; // lattice coordinate
    x -= floorf(x);               // position inside cell
    float u = fade(x);            // fade curve

    int a = perm_[X];
    int b = perm_[X + 1];

    return lerp(u, grad(a, x), grad(b, x - 1)); // range ~[-1,1]
}

// Fractal Brownian Motion for 1D Perlin
float TapeModulator::Fbm1D(float x, int octaves, float lacunarity, float gain)
{
    float sum = 0.0f;
    float amp = 1.0f;
    float freq = 1.0f;
    float maxSum = 0.0f;

    for(int i = 0; i < octaves; i++)
    {
        sum += Perlin1D(x * freq) * amp;
        maxSum += amp;
        freq *= lacunarity;
        amp *= gain;
    }

    return sum / maxSum; // normalized to ~[-1,1]
}

float TapeModulator::GetTapeSpeed(float wow_rate, float flutter_rate, float wow_depth, float flutter_depth) {
    // Slow WOW component (FBM, smooth)
    float wow = Fbm1D(t_wow_, octaves_wow_, 2.0f, 0.5f);

    // Tiny FLUTTER component (faster, low amplitude)
    float flutter = Fbm1D(t_flutter_, octaves_flutter_, 2.0f, 0.5f);
    
    // Advance time for next sample
    t_wow_ += wow_rate / sample_rate_;
    t_flutter_ += flutter_rate / sample_rate_;

    // Combine, scaled by depth
    // Assume potentiometer controls the main depth for wow; flutter is small fraction
    float speed = wow_depth * wow + (flutter_depth * 0.2f) * flutter;

    return speed;
}