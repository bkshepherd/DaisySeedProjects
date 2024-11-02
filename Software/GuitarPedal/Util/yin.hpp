// Nearly exact copy from:
// https://github.com/ashokfernandez/Yin-Pitch-Tracking
// Changed sample rate to be added to getPitch
// Changed init() to not use malloc so allocation happens in frequency detector
// class

#pragma once
#ifndef YIN_H
#define YIN_H

#include <cmath>
#include <cstdint>

namespace yin {

// Length of audio sample to use for pitch detection/sending to the yin
// algorithm
constexpr uint16_t audioBufferLength = 2048;
/**< Half the buffer length */
constexpr uint16_t halfBufferLength = audioBufferLength / 2;

/**
 * @struct  Yin
 * @brief   Object to encapsulate the parameters for the Yin pitch detection
 * algorithm
 */
typedef struct _Yin {
  /**< Buffer that stores the results of the intermediate processing steps of
   * the algorithm */
  float yinBuffer[halfBufferLength];
  /**< Probability that the pitch found is correct as a decimal (i.e 0.85 is
   * 85%) */
  float probability;
  /**< Allowed uncertainty in the result as a decimal (i.e 0.15 is 15%) */
  float threshold;
} Yin;

/**
 * Step 1: Calculates the squared difference of the signal with a shifted
 * version of itself.
 * @param buffer Buffer of samples to process.
 *
 * This is the Yin algorithms tweak on autocorellation. Read
 * http://audition.ens.fr/adc/pdf/2002_JASA_YIN.pdf for more details on what is
 * in here and why it's done this way.
 */
void difference(Yin *yin, float *buffer) {
  int16_t i;
  int16_t tau;
  float delta;

  /* Calculate the difference for difference shift values (tau) for the half of
   * the samples */
  for (tau = 0; tau < halfBufferLength; tau++) {
    /* Take the difference of the signal with a shifted version of itself, then
     * square it. (This is the Yin algorithm's tweak on autocorellation) */
    for (i = 0; i < halfBufferLength; i++) {
      delta = buffer[i] - buffer[i + tau];
      yin->yinBuffer[tau] += delta * delta;
    }
  }
}

/**
 * Step 2: Calculate the cumulative mean on the normalised difference calculated
 * in step 1
 * @param yin #Yin structure with information about the signal
 *
 * This goes through the Yin autocorellation values and finds out roughly where
 * shift is which produced the smallest difference
 */
void cumulativeMeanNormalizedDifference(Yin *yin) {
  int16_t tau;
  float runningSum = 0;
  yin->yinBuffer[0] = 1;

  /* Sum all the values in the autocorellation buffer and nomalise the result,
   * replacing the value in the autocorellation buffer with a cumulative mean of
   * the normalised difference */
  for (tau = 1; tau < halfBufferLength; tau++) {
    runningSum += yin->yinBuffer[tau];
    yin->yinBuffer[tau] *= tau / runningSum;
  }
}

/**
 * Step 3: Search through the normalised cumulative mean array and find values
 * that are over the threshold
 * @return Shift (tau) which caused the best approximate autocorellation. -1 if
 * no suitable value is found over the threshold.
 */
int16_t absoluteThreshold(Yin *yin) {
  int16_t tau;

  /* Search through the array of cumulative mean values, and look for ones that
   * are over the threshold The first two positions in yinBuffer are always so
   * start at the third (index 2) */
  for (tau = 2; tau < halfBufferLength; tau++) {
    if (yin->yinBuffer[tau] < yin->threshold) {
      while (tau + 1 < halfBufferLength &&
             yin->yinBuffer[tau + 1] < yin->yinBuffer[tau]) {
        tau++;
      }
      /* found tau, exit loop and return
       * store the probability
       * From the YIN paper: The yin->threshold determines the list of
       * candidates admitted to the set, and can be interpreted as the
       * proportion of aperiodic power tolerated
       * within a periodic signal.
       *
       * Since we want the periodicity and and not aperiodicity:
       * periodicity = 1 - aperiodicity */
      yin->probability = 1 - yin->yinBuffer[tau];
      break;
    }
  }

  /* if no pitch found, tau => -1 */
  if (tau == halfBufferLength || yin->yinBuffer[tau] >= yin->threshold) {
    tau = -1;
    yin->probability = 0;
  }

  return tau;
}

/**
 * Step 5: Interpolate the shift value (tau) to improve the pitch estimate.
 * @param  yin         [description]
 * @param  tauEstimate [description]
 * @return             [description]
 *
 * The 'best' shift value for autocorellation is most likely not an interger
 * shift of the signal. As we only autocorellated using integer shifts we should
 * check that there isn't a better fractional shift value.
 */
float parabolicInterpolation(Yin *yin, int16_t tauEstimate) {
  float betterTau;
  int16_t x0;
  int16_t x2;

  /* Calculate the first polynomial coeffcient based on the current estimate of
   * tau */
  if (tauEstimate < 1) {
    x0 = tauEstimate;
  } else {
    x0 = tauEstimate - 1;
  }

  /* Calculate the second polynomial coeffcient based on the current estimate of
   * tau */
  if (tauEstimate + 1 < halfBufferLength) {
    x2 = tauEstimate + 1;
  } else {
    x2 = tauEstimate;
  }

  /* Algorithm to parabolically interpolate the shift value tau to find a better
   * estimate */
  if (x0 == tauEstimate) {
    if (yin->yinBuffer[tauEstimate] <= yin->yinBuffer[x2]) {
      betterTau = tauEstimate;
    } else {
      betterTau = x2;
    }
  } else if (x2 == tauEstimate) {
    if (yin->yinBuffer[tauEstimate] <= yin->yinBuffer[x0]) {
      betterTau = tauEstimate;
    } else {
      betterTau = x0;
    }
  } else {
    float s0, s1, s2;
    s0 = yin->yinBuffer[x0];
    s1 = yin->yinBuffer[tauEstimate];
    s2 = yin->yinBuffer[x2];
    // fixed AUBIO implementation, thanks to Karl Helgason:
    // (2.0f * s1 - s2 - s0) was incorrectly multiplied with -1
    betterTau = tauEstimate + (s2 - s0) / (2 * (2 * s1 - s2 - s0));
  }

  return betterTau;
}

/**
 * Initialise the Yin pitch detection object
 * @param yin        Yin pitch detection object to initialise
 * @param bufferSize Length of the audio buffer to analyse
 * @param threshold  Allowed uncertainty (e.g 0.05 will return a pitch with ~95%
 * probability)
 */
void init(Yin *yin, float threshold) {
  /* Initialise the fields of the Yin structure passed in */
  yin->probability = 0.0;
  yin->threshold = threshold;

  int16_t i;
  for (i = 0; i < halfBufferLength; i++) {
    yin->yinBuffer[i] = 0;
  }
}

/**
 * Runs the Yin pitch detection algortihm
 * @param  yin    Initialised Yin object
 * @param  buffer Buffer of samples to analyse
 * @param  sampleRate Sample rate
 * @return        Fundamental frequency of the signal in Hz. Returns -1 if pitch
 * can't be found
 */
float getPitch(Yin *yin, float *buffer, uint32_t sampleRate) {
  int16_t tauEstimate = -1;
  float pitchInHertz = -1;

  /* Step 1: Calculates the squared difference of the signal with a shifted
   * version of itself. */
  difference(yin, buffer);

  /* Step 2: Calculate the cumulative mean on the normalised difference
   * calculated in step 1 */
  cumulativeMeanNormalizedDifference(yin);

  /* Step 3: Search through the normalised cumulative mean array and find values
   * that are over the threshold */
  tauEstimate = absoluteThreshold(yin);

  /* Step 5: Interpolate the shift value (tau) to improve the pitch estimate. */
  if (tauEstimate != -1) {
    pitchInHertz = static_cast<float>(sampleRate) /
                   parabolicInterpolation(yin, tauEstimate);
  }

  return pitchInHertz;
}

}  // namespace yin

#endif