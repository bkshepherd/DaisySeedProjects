#pragma once
#include "daisysp.h"

/**
    Tape modulation effect that simulates analog tape wow and flutter using Perlin noise.

    Uses 1D Perlin noise with Fractal Brownian Motion (FBM) to generate smooth,
    natural-sounding speed variations that mimic the imperfections of analog tape machines.
*/
class TapeModulator {
    public:
        /** Initializes the TapeModulator module.
            \param sample_rate - The sample rate of the audio engine being run.
        */
        void Init(float sample_rate);

        /** Generates tape speed variation based on wow and flutter parameters.
            \param wow_rate - Rate of the slow wow modulation in Hz
            \param flutter_rate - Rate of the fast flutter modulation in Hz
            \param wow_depth - Depth/amount of wow effect (scaling factor)
            \param flutter_depth - Depth/amount of flutter effect (scaling factor)
            \return Speed variation value based on combined modulation sources
        */
        float GetTapeSpeed(float wow_rate, float flutter_rate, float wow_depth, float flutter_depth);

    private:
        float sample_rate_;         ///< Audio engine sample rate
        float t_wow_;               ///< Time accumulator for wow modulation
        float t_flutter_;           ///< Time accumulator for flutter modulation
        int octaves_wow_ = 2;       ///< Number of octaves for wow FBM
        int octaves_flutter_ = 1;   ///< Number of octaves for flutter FBM

        /** Generates 1D Perlin noise value.
            \param x - Input coordinate for noise lookup
            \return Noise value in approximate range [-1, 1]
        */
        float Perlin1D(float x);

        /** Generates Fractal Brownian Motion using layered Perlin noise.
            \param x - Input coordinate for noise lookup
            \param octaves - Number of noise layers to combine
            \param lacunarity - Frequency multiplier between octaves (typically 2.0)
            \param gain - Amplitude multiplier between octaves (typically 0.5)
            \return Normalized FBM value in range [-1, 1]
        */
        float Fbm1D(float x, int octaves, float lacunarity, float gain);

        // Original permutation table from Ken Perlin
        static inline constexpr uint8_t p_[256] = {
            151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,
            140,36,103,30,69,142,8,99,37,240,21,10,23,190, 6,148,247,120,
            234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,
            33,88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,
            71,134,139,48,27,166,77,146,158,231,83,111,229,122,60,211,
            133,230,220,105,92,41,55,46,245,40,244,102,143,54, 65,25,
            63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,
            135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,
            226,250,124,123, 5,202,38,147,118,126,255,82,85,212,207,206,
            59,227,47,16,58,17,182,189,28,42,223,183,170,213,119,248,
            152, 2,44,154,163, 70,221,153,101,155,167, 43,172, 9,129,22,
            39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,
            97,228,251,34,242,193,238,210,144,12,191,179,162,241, 81,51,
            145,235,249,14,239,107,49,192,214, 31,181,199,106,157,184, 84,
            204,176,115,121,50,45,127, 4,150,254,138,236,205, 93,222,114,
            67,29,24,72,243,141,128,195,78,66,215,61,156,180
        };
        /// Permutation table duplicated for wraparound (perm[512] from p[256])
        static uint8_t perm_[512];

        /// Perlin fade curve: 6t^5 - 15t^4 + 10t^3
        inline float fade(float t) { return t * t * t * (t * (t * 6 - 15) + 10); }

        /// Linear interpolation
        inline float lerp(float t, float a, float b) { return a + t * (b - a); }

        /// Simple 1D gradient function for Perlin noise
        inline float grad(int hash, float x) { return (hash & 1) == 0 ? x : -x; }
};
