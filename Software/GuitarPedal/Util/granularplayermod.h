/*
Copyright (c) 2020 Electrosmith, Corp, Vinícius Fernandes

Use of this source code is governed by an MIT-style
license that can be found in the LICENSE file or at
https://opensource.org/licenses/MIT.

 GranularPlayer class modified by K. Bloemer 2025 (renamed to GranularPlayerMod)
  Changes:
  - Applies the envelope to the individual grains instead of the whole audio sample
  - Adds two additional envelopes (fast attack and slow attack, linear), and envelopes now use 512 points instead of 256
  - Adds a stereo spread for each grain and left/right output getter functions
  - Adds the ability to change the effective audio sample size
  - Adds "width" parameter that starts grains at a random point within the timeframe set by width param (in milliseconds)
*/

#pragma once
#ifndef DSY_GRANULARPLAYERMOD_H
#define DSY_GRANULARPLAYERMOD_H

#include "daisysp.h"
#include <cmath>
#include <stdint.h>
// #include "Control/phasor.h"
#ifdef __cplusplus
#ifndef M_PI
#define M_PI 3.14159265358979323846 /* pi */
#endif

// namespace daisysp_modified
//{
/** GranularPlayerMod Module

    Date: November, 2023

    Author: Vinícius Fernandes

    GranularPlayerMod is a lookup table player that provides independent time stretching and pitch shifting
    via granulation.
    Inspired by the grain.player object from else pure data's library.
*/

class GranularPlayerMod {
  public:
    GranularPlayerMod() {}
    ~GranularPlayerMod() {}

    /** Initializes the GranularPlayerMod module.
        \param sample pointer to the sample to be played
        \param size number of elements in the sample array
        \param sample_rate audio engine sample rate
        \param phase1 starting phase from 0 to 1 of first grain (normally 0)
        \param phase2 starting phase 0 to 1 of second grain (normally 0.5 to overlap halfway with 1st grain)
    */
    void Init(float *sample, int size, float sample_rate, float phase1, float phase2);

    /** Processes the granular player.
        \param speed playback speed. 1 is normal speed, 2 is double speed, 0.5 is half speed, etc. Negative values play the sample
       backwards.
        \param transposition transposition in cents. 100 cents is one semitone. Negative values transpose down, positive values
       transpose up.
        \param grain_size grain size in milliseconds. 1 is 1 millisecond, 1000 is 1 second. Does not accept negative values. Minimum
       value is 1.
        \param width width range in milliseconds. Will start each grain envelope at a random location within width range. 0 for no
       randomized location.
    */
    void Process(float speed, float transposition, float grain_size, float width);

    /* Selects grain envelope mode */
    void setEnvelopeMode(int env_mode);

    /* Sets amount of stereo spread of each grain, 0 for none, 1 for widest */
    void setStereoSpread(float spread);

    /* Sets the current audio sample size */
    void setSampleSize(float sample_size);

    float getLeftOut();

    float getRightOut();

  private:
    // Wraps an index to the size of the sample array
    uint32_t WrapIdx(uint32_t idx, uint32_t size);

    // Converts cents(1/100th of a semitone) to a ratio
    float CentsToRatio(float cents);

    // Converts milliseconds to  number of samples
    float MsToSamps(float ms, float samplerate);

    // Inverts the phase of the phasor if the frequency is negative, mimicking pure data's phasor~ object
    float NegativeInvert(daisysp::Phasor *phs, float frequency);

    /* Generates a new random index modifier based on width setting,
    and updates the stereo panning for the next grain */
    float newRandIndex(bool isFirstGrain);

    float *sample_;       // pointer to the sample to be played
    float sample_rate_;   // audio engine sample rate
    int size_;            // number of elements in the sample array
    float grain_size_;    // grain size in milliseconds
    float speed_;         // processed playback speed.
    float transposition_; // processed transpotion.
    float sample_frequency_;
    float cosEnv_[512] = {0};   // cosine envelope for crossfading between grains
    float linEnv_[512] = {0};   // Trapezoid shape envelope for crossfading between grains
    float adLinEnv_[512] = {0}; // fast attack linear increase, linear decay
    float idxTransp_;           // Adjusted Transposition value contribution to idx of first grain
    float idxTransp2_;          // Adjusted Transposition value contribution to idx of second grain
    float idxSpeed_;            // Adjusted Speed value contribution to idx of first grain
    float idxSpeed2_;           // Adjusted Speed value contribution to idx of second grain
    float sig_;                 // Output of first grain
    float sig2_;                // Output of second grain

    uint32_t idx_;  // Index of first grain
    uint32_t idx2_; // Index of second grain

    daisysp::Phasor phs_;     // Phasor for speed
    daisysp::Phasor phsImp_;  // Phasor for transposition
    daisysp::Phasor phs2_;    // Phasor for speed
    daisysp::Phasor phsImp2_; // Phasor for transposition

    float width_;         // Width for randomized grain starting point in milliseconds.
    float rand_idx_mod_;  // for selecting random location within width parameter. Updated at the beginning of each grain envelope
    float rand_idx_mod2_; // for selecting random location within width parameter. Updated at the beginning of each grain envelope
    float phaseImp_out_;  // need to use this for width setting
    float phaseImp2_out_; // need to use this for width setting

    bool switch1;
    bool switch2;

    int env_mode_;

    float grain_pan1_; // First grain stereo pan, updated for each new grain envelope
    float grain_pan2_; // Second grain stereo pan, updated for each new grain envelope

    float outl_;
    float outr_;

    float spread_; // Amount of stereo spread, 0 for none, 1 for widest

    float variable_size_; // the size of the current variable length sample (size_ is now the max samplesize, i.e. the buffer length)
};
//} // namespace daisysp
#endif
#endif