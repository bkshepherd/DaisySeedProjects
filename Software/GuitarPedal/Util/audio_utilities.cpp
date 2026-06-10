#include "audio_utilities.h"

float tempo_to_freq(uint32_t tempo) { return static_cast<float>(tempo) / 60.0f; }

uint32_t freq_to_tempo(float freq) { return freq * 60.0f; }

uint32_t ms_to_tempo(uint32_t ms) {
    if (ms == 0) {
        return 0;
    }
    return 60000 / ms;
}

uint32_t s_to_tempo(float seconds) {
    // Guard division by zero; the inf would be undefined behavior when
    // converted to uint32_t. Callers clamp the returned BPM to their range.
    if (seconds <= 0.0f) {
        return 0;
    }
    return 60 / seconds;
}