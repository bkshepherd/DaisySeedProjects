#pragma once
#ifndef AUDIO_UTILITIES_H
#define AUDIO_UTILITIES_H

/** Converts Tempo to Frequency
     \param tempo the Tempo in Beats Per Minute (BPM)
     \return the Frequency as floating point value measure of cycles per second.
*/
float tempo_to_freq(uint32_t tempo);

/** Converts Frequency to Tempo
     \param freq the Frequency as a floating point value measure of cycle per second.
     \return the Tempo as uint32_t value measure of Beats Per Minute (BPM)
*/
uint32_t freq_to_tempo(float freq);

/** Converts Time per Beat (ms) in to Tempo in Beats Per Minute (BPM)
     \param ms the time per beat in ms
     \return the Tempo as integer value measure of Beats Per Minute (BPM)
*/
uint32_t ms_to_tempo(uint32_t ms);

/** Converts Time per Beat (s) in to Tempo in Beats Per Minute (BPM)
     \param seconds time per beat in seconds
     \return the Tempo as integer value measure of Beats Per Minute (BPM)
*/
uint32_t s_to_tempo(float seconds);

#endif