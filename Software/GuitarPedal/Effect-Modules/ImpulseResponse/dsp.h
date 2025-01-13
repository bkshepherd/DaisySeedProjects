#pragma once

#include <vector>

// A class where a longer buffer of history is needed to correctly calculate
// the DSP algorithm (e.g. algorithms involving convolution).
//
// Hacky stuff:
// * Mono
// * Single-precision floats.
class History 
{
public:
  History();
  ~History();
protected:
  // Called at the end of the DSP, advance the hsitory index to the next open
  // spot.  Does not ensure that it's at a valid address.
  void _AdvanceHistoryIndex(const size_t bufferSize);
  // Drop the new samples into the history array.
  // Manages history array size
  void _UpdateHistory(float inputs);

  // The history array that's used for DSP calculations.
  std::vector<float> mHistory;
  // How many samples previous are required.
  // Zero means that no history is required--only the current sample.
  size_t mHistoryRequired = 0;
  // Location of the first sample in the current buffer.
  // Shall always be in the range [mHistoryRequired, mHistory.size()).
  size_t mHistoryIndex = 0;

private:

  // Copy the end of the history back to the front and reset mHistoryIndex
  void _RewindHistory();
};
