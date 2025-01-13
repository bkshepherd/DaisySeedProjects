/*
 * File: dsp.cpp
 * Created Date: March 17, 2023
 * Author: Steven Atkinson (steven@atkinson.mn)
 */
//  Modified by Keith Bloemer on 12/28/23
//    Greatly simplified by assuming 1 channel, 1 input per Process call, and constant samplerate.
//    For initial investigation into running IR's on the Daisy Seed

#include "dsp.h"


History::History()
{
}

// Destructor
History::~History()
{
    // No Code Needed
}


void History::_AdvanceHistoryIndex(const size_t bufferSize)
{
  mHistoryIndex += bufferSize;
}


void History::_RewindHistory()
{
  // TODO memcpy?  Should be fine w/ history array being >2x the history length.
  for (size_t i = 0, j = mHistoryIndex - mHistoryRequired; i < mHistoryRequired; i++, j++)
    mHistory[i] = mHistory[j];
  mHistoryIndex = mHistoryRequired;
}

void History::_UpdateHistory(float inputs)
{
  if (mHistoryIndex + 1 >= mHistory.size())
    _RewindHistory();

  mHistory[mHistoryIndex] = inputs;
}
