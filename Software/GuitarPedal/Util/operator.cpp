#include "operator.h"

using namespace daisysp;



void Operator::Init(float samplerate, bool isCarrier)
{
    //init oscillators
    osc_.Init(samplerate);

    //set some reasonable values
    lfreq_ = freq_ = 440.f;
    lratio_ = ratio_ = 2.f;
    SetFrequency(lfreq_);
    SetRatio(lratio_);

    llevel_ = level_ = 1.0;
    osc_.SetAmp(llevel_);

    osc_.SetWaveform(Oscillator::WAVE_SIN);

    iscarrier_ = isCarrier;

    modval_ = 0.0;

}

float Operator::Process()
{
    if(lratio_ != ratio_ || lfreq_ != freq_)
    {
        lratio_ = ratio_;
        lfreq_  = freq_;
        osc_.SetFreq(lfreq_ * lratio_);
    }

    if (llevel_ != level_) {
        llevel_ = level_;
        osc_.SetAmp(llevel_);
    }

    if (iscarrier_)
        osc_.PhaseAdd(modval_);

    // Return the processed oscillator, scaled by the velocity of the keystroke and the envelope
    return osc_.Process() * velocity_amp_;
}

void Operator::SetFrequency(float freq) ///Currently unused
{
    freq_ = fabsf(freq);
}

void Operator::SetRatio(float ratio)
{
    ratio_ = fabsf(ratio);
}

void Operator::SetLevel(float level)
{
    level_ = fabsf(level);
}

void Operator::setPhaseInput(float modval) // TODO should I use this function to just do the osc phase add instead?
{
    modval_ = modval;
}


void Operator::Reset()
{
    osc_.Reset();
}

////////////////////////////////////// NEW STUFF
void Operator::OnNoteOn(float note, float velocity)
{
    velocity_ = velocity;
    velocity_amp_ = velocity_ / 127.f;
    freq_ = mtof(note);  /// The base frequency of the note is set here, modified by the ratio in the Process function
}

