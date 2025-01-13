//
//  ImpulseResponse.cpp
//  NeuralAmpModeler-macOS
//
//  Created by Steven Atkinson on 12/30/22.
//
//  Modified by Keith Bloemer on 12/28/23
//    Greatly simplified by assuming 1 channel, 1 input per Process call, and constant samplerate.
//    For initial investigation into running IR's on the Daisy Seed

#include "ImpulseResponse.h"


ImpulseResponse::ImpulseResponse()
{
}

// Destructor
ImpulseResponse::~ImpulseResponse()
{
    // No Code Needed
}


void ImpulseResponse::Init(std::vector<float> irData)
{
  mRawAudio = irData;
  _SetWeights();
}

float ImpulseResponse::Process(float inputs)
{

  _UpdateHistory(inputs);

  int j = mHistoryIndex - mHistoryRequired;
  auto input = Eigen::Map<const Eigen::VectorXf>(&mHistory[j], mHistoryRequired + 1);
  
  _AdvanceHistoryIndex(1); // KAB MOD - for Daisy implementation numFrames is always 1

  return (float)mWeight.dot(input);

}

void ImpulseResponse::_SetWeights()
{

  const size_t irLength = std::min(mRawAudio.size(), mMaxLength);
  mWeight.resize(irLength);
  // Gain reduction.
  // https://github.com/sdatkinson/NeuralAmpModelerPlugin/issues/100#issuecomment-1455273839
  // Add sample rate-dependence
  //const float gain = pow(10, -18 * 0.05) * 48000 / mSampleRate;  //KAB NOTE: This made a very bad/loud sound on Daisy Seed
  for (size_t i = 0, j = irLength - 1; i < irLength; i++, j--)
    //mWeight[j] = gain * mRawAudio[i];  
    mWeight[j] = mRawAudio[i];
  mHistoryRequired = irLength - 1;

  // Moved from HISTORY::EnsureHistorySize since only doing once for this module (assuming same size IR's)
  //   TODO: Maybe find a more efficient method of indexing mHistory,
  //         rather than copying the end of the vector (length of IR) back to the beginning all at once.
  const size_t requiredHistoryArraySize = 5 * mHistoryRequired; // Just so we don't spend too much time copying back. // KAB NOTE: was 10 *
  mHistory.resize(requiredHistoryArraySize);
  std::fill(mHistory.begin(), mHistory.end(), 0.0f);
  mHistoryIndex = mHistoryRequired; 

}
