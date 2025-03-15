#pragma once
#ifndef FM_KEYS_MODULE_H
#define FM_KEYS_MODULE_H

#include "../Util/operator.h"
#include "base_effect_module.h"
#include "daisysp.h"
#include <stdint.h>
#ifdef __cplusplus

/** @file midi_keys_module.h */
// Implements the DaisyExamples Midi effect for DaisyField, but on the 125B pedal
// Polyphonic MIDI Synth
// 12 (changed from 24, didn't work) voices of polyphony. Each voice is a band-limited saw waveform running through a state variable
// filter.

using namespace daisysp;

namespace bkshepherd {

class VoiceFm {
  public:
    VoiceFm() {}
    ~VoiceFm() {}
    void Init(float samplerate) {
        active_ = false;
        op1_.Init(samplerate, false);
        op2_.Init(samplerate, true);

        env_.Init(samplerate);
        env_.SetSustainLevel(0.5f);
        env_.SetTime(ADSR_SEG_ATTACK, 0.005f);
        env_.SetTime(ADSR_SEG_DECAY, 0.005f);
        env_.SetTime(ADSR_SEG_RELEASE, 0.2f);

        env2_.Init(samplerate);
        env2_.SetSustainLevel(0.5f);
        env2_.SetTime(ADSR_SEG_ATTACK, 0.005f);
        env2_.SetTime(ADSR_SEG_DECAY, 0.005f);
        env2_.SetTime(ADSR_SEG_RELEASE, 0.2f);
    }

    float Process() {
        if (active_) {
            float sig, sig2, amp, amp2;
            amp = env_.Process(env_gate_);   // Carruer envelope
            amp2 = env2_.Process(env_gate_); // Modulator envelope
            if (!env_.IsRunning())
                active_ = false;
            sig = op1_.Process();
            op2_.setPhaseInput(sig * amp2);
            sig2 = op2_.Process();

            return sig2 * amp;
        }
        return 0.f;
    }

    void OnNoteOn(float note, float velocity) {
        note_ = note;
        velocity_ = velocity;
        op1_.OnNoteOn(note, velocity);
        op2_.OnNoteOn(note, velocity);
        active_ = true;
        env_gate_ = true;
    }

    void OnNoteOff() { env_gate_ = false; }

    void SetModulatorRatio(float val) { op1_.SetRatio(val); }

    void SetCarrierRatio(float val) { op2_.SetRatio(val); }

    void SetModulatorLevel(float val) { op1_.SetLevel(val); }

    void SetCarrierLevel(float val) { op2_.SetLevel(val); }

    void SetCarrierAttack(float val) { env_.SetTime(ADSR_SEG_ATTACK, val); }

    void SetCarrierDecay(float val) { env_.SetTime(ADSR_SEG_DECAY, val); }

    void SetCarrierSustain(float val) { env_.SetSustainLevel(val); }

    void SetCarrierRelease(float val) { env_.SetTime(ADSR_SEG_RELEASE, val); }

    void SetModAttack(float val) { env2_.SetTime(ADSR_SEG_ATTACK, val); }

    void SetModDecay(float val) { env2_.SetTime(ADSR_SEG_DECAY, val); }

    void SetModSustain(float val) { env2_.SetSustainLevel(val); }

    void SetModRelease(float val) { env2_.SetTime(ADSR_SEG_RELEASE, val); }

    inline bool IsActive() const { return active_; }
    inline float GetNote() const { return note_; }

  private:
    // Oscillator osc_;
    Operator op1_;
    Operator op2_;
    Adsr env_;
    Adsr env2_;
    float note_, velocity_;
    bool active_;
    bool env_gate_;
};

template <size_t max_voices> class VoiceManagerFm {
  public:
    VoiceManagerFm() {}
    ~VoiceManagerFm() {}

    void Init(float samplerate) {
        for (size_t i = 0; i < max_voices; i++) {
            voices[i].Init(samplerate);
        }
    }

    float Process() {
        float sum;
        sum = 0.f;
        for (size_t i = 0; i < max_voices; i++) {
            sum += voices[i].Process();
        }
        return sum;
    }

    void OnNoteOn(float notenumber, float velocity) {
        VoiceFm *v = FindFreeVoice();
        if (v == NULL)
            return;
        v->OnNoteOn(notenumber, velocity);
    }

    void OnNoteOff(float notenumber, float velocity) {
        for (size_t i = 0; i < max_voices; i++) {
            VoiceFm *v = &voices[i];
            if (v->IsActive() && v->GetNote() == notenumber) {
                v->OnNoteOff();
            }
        }
    }

    void FreeAllVoices() {
        for (size_t i = 0; i < max_voices; i++) {
            voices[i].OnNoteOff();
        }
    }

    void SetModulatorLevel(float all_val) {
        for (size_t i = 0; i < max_voices; i++) {
            voices[i].SetModulatorLevel(all_val);
        }
    }

    void SetModulatorRatio(float all_val) {
        for (size_t i = 0; i < max_voices; i++) {
            voices[i].SetModulatorRatio(all_val);
        }
    }

    void SetCarrierLevel(float all_val) {
        for (size_t i = 0; i < max_voices; i++) {
            voices[i].SetCarrierLevel(all_val);
        }
    }

    void SetCarrierRatio(float all_val) {
        for (size_t i = 0; i < max_voices; i++) {
            voices[i].SetCarrierRatio(all_val);
        }
    }

    void SetCarrierAttack(float all_val) {
        for (size_t i = 0; i < max_voices; i++) {
            voices[i].SetCarrierAttack(all_val);
        }
    }

    void SetCarrierDecay(float all_val) {
        for (size_t i = 0; i < max_voices; i++) {
            voices[i].SetCarrierDecay(all_val);
        }
    }

    void SetCarrierSustain(float all_val) {
        for (size_t i = 0; i < max_voices; i++) {
            voices[i].SetCarrierSustain(all_val);
        }
    }

    void SetCarrierRelease(float all_val) {
        for (size_t i = 0; i < max_voices; i++) {
            voices[i].SetCarrierRelease(all_val);
        }
    }

    void SetModAttack(float all_val) {
        for (size_t i = 0; i < max_voices; i++) {
            voices[i].SetModAttack(all_val);
        }
    }

    void SetModDecay(float all_val) {
        for (size_t i = 0; i < max_voices; i++) {
            voices[i].SetModDecay(all_val);
        }
    }

    void SetModSustain(float all_val) {
        for (size_t i = 0; i < max_voices; i++) {
            voices[i].SetModSustain(all_val);
        }
    }

    void SetModRelease(float all_val) {
        for (size_t i = 0; i < max_voices; i++) {
            voices[i].SetModRelease(all_val);
        }
    }

  private:
    VoiceFm voices[max_voices];
    VoiceFm *FindFreeVoice() {
        VoiceFm *v = NULL;
        for (size_t i = 0; i < max_voices; i++) {
            if (!voices[i].IsActive()) {
                v = &voices[i];
                break;
            }
        }
        return v;
    }
};

class FmKeysModule : public BaseEffectModule {
  public:
    FmKeysModule();
    ~FmKeysModule();

    void Init(float sample_rate) override;
    void ParameterChanged(int parameter_id) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    void OnNoteOn(float notenumber, float velocity) override;
    void OnNoteOff(float notenumber, float velocity) override;

    float GetBrightnessForLED(int led_id) const override;

  private:
    float m_freqMin;
    float m_freqMax;

    VoiceManagerFm<16> m_voice_handler; // starting with 1 voice for testing, 16 WORKS, 18 freezes
                                      // With added carrier env control, 16 glitches, 12 works

    float m_cachedEffectMagnitudeValue;
};
} // namespace bkshepherd
#endif
#endif
