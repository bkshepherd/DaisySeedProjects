#include "audio_utilities.h"

float tempo_to_freq(uint32_t tempo)
{
    return tempo / 60.0f;
}

uint32_t freq_to_tempo(float freq)
{
    return freq * 60.0f;
}

uint32_t ms_to_tempo(uint32_t ms)
{
    return 60000 / ms;
}

uint32_t s_to_tempo(float seconds)
{
    return 60 / seconds;
}