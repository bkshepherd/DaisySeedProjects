// https://github.com/ashokfernandez/Yin-Pitch-Tracking
#ifndef Yin_h
#define Yin_h

#include <stdint.h>

#define YIN_DEFAULT_THRESHOLD 0.15

/**
 * @struct  Yin
 * @breif	Object to encapsulate the parameters for the Yin pitch detection
 * algorithm
 */
typedef struct _Yin {
  int16_t bufferSize;     /**< Size of the audio buffer to be analysed */
  int16_t halfBufferSize; /**< Half the buffer length */
  float *yinBuffer;  /**< Buffer that stores the results of the intermediate
                        processing steps of the algorithm */
  float probability; /**< Probability that the pitch found is correct as a
                        decimal (i.e 0.85 is 85%) */
  float threshold; /**< Allowed uncertainty in the result as a decimal (i.e 0.15
                      is 15%) */
} Yin;

/**
 * Initialise the Yin pitch detection object
 * @param yin        Yin pitch detection object to initialise
 * @param bufferSize Length of the audio buffer to analyse
 * @param threshold  Allowed uncertainty (e.g 0.05 will return a pitch with ~95%
 * probability)
 */
void Yin_init(Yin *yin, int16_t bufferSize, float threshold);

/**
 * Runs the Yin pitch detection algortihm
 * @param  yin    Initialised Yin object
 * @param  buffer Buffer of samples to analyse
 * @param  sampleRate Sample rate
 * @return        Fundamental frequency of the signal in Hz. Returns -1 if pitch
 * can't be found
 */
float Yin_getPitch(Yin *yin, float *buffer, uint32_t sampleRate);

/**
 * Certainty of the pitch found
 * @param  yin Yin object that has been run over a buffer
 * @return     Returns the certainty of the note found as a decimal (i.e 0.3 is
 * 30%)
 */
float Yin_getProbability(Yin *yin);

#endif
