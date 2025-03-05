#include "granularplayermod.h"

using namespace daisysp;

void GranularPlayerMod::Init(float *sample, int size, float sample_rate, float phase1, float phase2) {
    /*initialize variables to private members*/
    sample_ = sample;
    size_ = size;
    variable_size_ = size_;
    sample_rate_ = sample_rate;
    /*initialize phasors. phs2_ is initialized with a phase offset of 0.5f to create an overlapping effect*/
    phs_.Init(sample_rate_, 0, phase1);
    phsImp_.Init(sample_rate_, 0, 0);
    phs2_.Init(sample_rate_, 0, phase2);
    phsImp2_.Init(sample_rate_, 0, 0);
    /*calculate sample frequency*/
    sample_frequency_ = sample_rate_ / variable_size_;
    /*initialize half cosine envelope*/
    for (int i = 0; i < 512; i++) {
        cosEnv_[i] = sinf((i / 512.0f) * M_PI);
    }

    /*initialize linear attack-release envelope*/

    float c = 0.0;
    for (int i = 0; i < 512; i++) {
        if (i < 384) { // attack to unity 1/4 grain time
            c += 1 / 384.0f;
            linEnv_[i] = c;
        } else { // decay to zero 3/4 grain time
            c -= 1 / 128.0f;
            linEnv_[i] = c;
        }
    }

    /*initialize fast attack- slow decay envelope*/

    c = 0.0;
    for (int i = 0; i < 512; i++) {
        if (i < 128) { // attack to unity 1/4 grain time
            c += 1 / 128.0f;
            adLinEnv_[i] = c;
        } else { // decay to zero 3/4 grain time
            c -= 1 / 384.0f;
            adLinEnv_[i] = c;
        }
    }

    rand_idx_mod_ = 0.0;
    rand_idx_mod2_ = 0.0;
    switch1 = false;
    switch2 = false;

    env_mode_ = 0; // set to cosine envelope for default

    grain_pan1_ = 0.5; // First grain stereo pan, updated for each new grain envelope
    grain_pan2_ = 0.5; //  range of 0 to 1, 0 is full left, 1 is full right, 0.5 is center

    outl_ = 0.0;
    outr_ = 0.0;
}

uint32_t GranularPlayerMod::WrapIdx(uint32_t idx, uint32_t sz) {
    /*wraps idx to sz*/
    // TODO verify change >= from just =
    if (idx >= sz) {
        idx = idx - sz;
        return idx;
    }

    return idx;
}

float GranularPlayerMod::CentsToRatio(float cents) {
    /*converts cents to  ratio*/
    return powf(2.0f, cents / 1200.0f);
}

float GranularPlayerMod::MsToSamps(float ms, float samplerate) {
    /*converts milliseconds to  number of samples*/
    return (ms * 0.001f) * samplerate;
}

float GranularPlayerMod::NegativeInvert(Phasor *phs, float frequency) {
    /*inverts the phase of the phasor if the frequency is negative, mimicking pure data's phasor~ object*/
    return (frequency > 0) ? phs->Process() : ((phs->Process() * -1) + 1);
}

float GranularPlayerMod::newRandIndex(bool isFirstGrain) {
    /* Generates a new random index modifier based on width setting.
        The stereo panning of each grain is also controled by the random
        number genereated here. The farther the width index is from center, the
        more panning is applied. */

    // Generate random float from 0 to 1
    float r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);

    // Use this 0 to 1 float value to set the grain panning (this is a hacky way to do it, maybe change later)
    if (isFirstGrain) {
        grain_pan1_ = 0.5 + ((r - 0.5) * spread_);
    } else {
        grain_pan2_ = 0.5 + ((r - 0.5) * spread_);
    }

    // Move randomized range to -0.5 to 0.5
    float rand_zero_centered = r - 0.5;
    float width_in_samples = MsToSamps(width_, sample_rate_);

    // Calculate random index modifier (range of -width/2 to + width/2)
    float rand_idx_in_samples_float = width_in_samples * rand_zero_centered;

    return rand_idx_in_samples_float;
}

/* Selects grain envelope mode */
void GranularPlayerMod::setEnvelopeMode(int env_mode) { env_mode_ = env_mode; }

void GranularPlayerMod::setStereoSpread(float spread) { spread_ = spread; }

void GranularPlayerMod::setSampleSize(float sample_size) {
    variable_size_ = sample_size;
    sample_frequency_ = sample_rate_ / variable_size_;
}

float GranularPlayerMod::getLeftOut() { return outl_; }

float GranularPlayerMod::getRightOut() { return outr_; }

void GranularPlayerMod::Process(float speed, float transposition, float grain_size, float width) {
    grain_size_ = grain_size;
    speed_ = speed * sample_frequency_;
    transposition_ = (CentsToRatio(transposition) - speed) * (grain_size >= 1 ? 1000 / grain_size_ : 1);
    width_ = width;

    phs_.SetFreq(fabs(speed_));
    phs2_.SetFreq(fabs(speed_));

    phsImp_.SetFreq(fabs(transposition_ / 2)); // Now dividing by 2 because we process the phasor twice
    phsImp2_.SetFreq(fabs(transposition_ / 2));
    idxSpeed_ =
        NegativeInvert(&phs_, speed_) *
        variable_size_; // Speed phasors control the movement through the entire sample, NegativeInvert function processes the phasor
    idxSpeed2_ = NegativeInvert(&phs2_, speed_) * variable_size_;
    idxTransp_ = (NegativeInvert(&phsImp_, transposition_) * MsToSamps(grain_size_, sample_rate_));
    idxTransp2_ = (NegativeInvert(&phsImp2_, transposition_) * MsToSamps(grain_size_, sample_rate_));
    idx_ = WrapIdx((uint32_t)(idxSpeed_ + idxTransp_ + rand_idx_mod_), variable_size_);
    idx2_ = WrapIdx((uint32_t)(idxSpeed2_ + idxTransp2_ + rand_idx_mod2_), variable_size_);

    // Check for when phase output equals 0.0, then generate a new random index modifier for next grain envelope
    float phase_out1_imp = phsImp_.Process();
    float phase_out2_imp = phsImp2_.Process();

    if (phase_out1_imp < 0.01 && switch1 == true) { // TESTING
        rand_idx_mod_ = newRandIndex(true);
        switch1 = false;
    }
    if (phase_out1_imp > 0.9)
        switch1 = true;

    if (phase_out2_imp < 0.01 && switch2 == true) { // TESTING
        rand_idx_mod2_ = newRandIndex(false);
        switch2 = false;
    }
    if (phase_out2_imp > 0.9)
        switch2 = true;

    if (env_mode_ == 0) {

        sig_ = sample_[idx_] * cosEnv_[(uint32_t)(phase_out1_imp * 512)];
        sig2_ = sample_[idx2_] * cosEnv_[(uint32_t)(phase_out2_imp * 512)];

    } else if (env_mode_ == 1) {

        sig_ = sample_[idx_] * linEnv_[(uint32_t)(phase_out1_imp * 512)];
        sig2_ = sample_[idx2_] * linEnv_[(uint32_t)(phase_out2_imp * 512)];

    } else if (env_mode_ == 2) {

        sig_ = sample_[idx_] * adLinEnv_[(uint32_t)(phase_out1_imp * 512)];
        sig2_ = sample_[idx2_] * adLinEnv_[(uint32_t)(phase_out2_imp * 512)];
    }

    outl_ = (sig_ * (1.0 - grain_pan1_) + sig2_ * (1.0 - grain_pan2_)) / 2;
    outr_ = (sig_ * grain_pan1_ + sig2_ * grain_pan2_) / 2;
}