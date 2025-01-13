//
//  ImpulseResponse.h
//  NeuralAmpModeler-macOS
//
//  Created by Steven Atkinson on 12/30/22.
//
// Impulse response processing
//
//  Modified by Keith Bloemer on 12/28/23
//    Greatly simplified by assuming 1 channel, 1 input per Process call, and constant samplerate.
//    For initial investigation into running IR's on the Daisy Seed

#pragma once

#include <Eigen/Dense>
#include "dsp.h"


class ImpulseResponse : public History
{
public:
  ImpulseResponse();
  ~ImpulseResponse();

  void Init(std::vector<float> irData);
  float Process(float inputs);


private:
  // Set the weights, given that the plugin is running at the provided sample
  // rate.
  void _SetWeights();

  // State of audio
  // Keep a copy of the raw audio that was loaded so that it can be resampled
  std::vector<float> mRawAudio;
  float mRawAudioSampleRate;
  float mSampleRate;

  const size_t mMaxLength = 8192;
  // The weights
  Eigen::VectorXf mWeight;
};



