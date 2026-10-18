#pragma once
// The tap tempo of the second footswitch (guitar_pedal.cpp): three taps set the tempo, from the interval between the last two taps (the
// first interval is dropped, it is usually off), and every further tap of the series sets it again from its own interval. A tap that comes
// too long after the previous one starts a new series. The series logic is here so that it can be tested on the PC.
class TapTempoSeries {
  public:
    /** A tap of the switch.
     *  \param followsPrevious true if the previous tap was close enough to belong to the same series
     *  \param intervalSamples the samples since the previous tap (only used when followsPrevious is true)
     *  \param interval set to the interval of the last two taps when the tempo is to be set
     *  \return true if the tempo is set from this tap
     */
    bool Tap(bool followsPrevious, int intervalSamples, int &interval) {
        if (!followsPrevious) {
            m_count = 1;
            return false;
        }
        if (m_count < 3) {
            m_count++;
        }
        if (m_count < 3) {
            return false;
        }
        interval = intervalSamples;
        return true;
    }

    /** Forget the series (the effect does not use tap tempo). */
    void Reset() { m_count = 0; }

  private:
    int m_count = 0;
};
