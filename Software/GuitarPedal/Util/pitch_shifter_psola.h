#ifndef PITCH_SHIFTER_PSOLA_H
#define PITCH_SHIFTER_PSOLA_H

#include <algorithm>
#include <cmath>
#include <deque>
#include <vector>

// PSOLA-based pitch shifter (monophonic)
class PitchShifterPSOLA {
  public:
    PitchShifterPSOLA() : m_pitchShiftRatio(1.0f), m_hopIn(0), m_hopOut(0), m_readIndex(0), m_writeIndex(0) {}

    void Init(float sample_rate) {
        m_sampleRate = sample_rate;
        m_maxPeriodSamples = static_cast<int>(sample_rate / 50.0f);   // 50 Hz
        m_minPeriodSamples = static_cast<int>(sample_rate / 1000.0f); // 1 kHz

        const int bufLen = m_maxPeriodSamples * 4; // power of two
        m_inputBuffer.assign(bufLen, 0.0f);
        m_outputBuffer.assign(bufLen, 0.0f);

        m_pitchPeriod = std::clamp(int(m_sampleRate / 220.0f), // ~A3
                                   m_minPeriodSamples, m_maxPeriodSamples);
        m_hopIn = m_pitchPeriod;  // 1 period
        m_hopOut = m_pitchPeriod; // ratio = 1 until told otherwise
        m_readIndex = 0;
        m_writeIndex = 0;
    }

    // 1.0 = no shift, 2.0 = +12 st, 0.5 = −12 st
    void SetPitchShiftRatio(float ratio) { m_pitchShiftRatio = std::clamp(ratio, 0.5f, 2.0f); }

    // pitch period from external detector, in samples
    void SetPitchPeriod(int periodSamples) {
        m_pitchPeriod = std::clamp(periodSamples, m_minPeriodSamples, m_maxPeriodSamples);

        m_hopIn = m_pitchPeriod;
        m_hopOut = int(float(m_pitchPeriod) / m_pitchShiftRatio);
    }

    // process one sample
    float ProcessSample(float in) {
        if (m_hopIn == 0 || m_hopOut == 0)
            return in;

        /* ---- 1. write input circularly ---- */
        m_inputBuffer[m_writeIndex & (m_inputBuffer.size() - 1)] = in;
        ++m_writeIndex;

        /* ---- 2. analysis epoch driven by m_hopIn ---- */
        if (m_writeIndex >= m_readIndex + m_hopIn) {
            PadOutputDeque(m_hopOut); // make space in output
            OverlapAdd(m_readIndex);  // write frame
            m_readIndex += m_hopIn;   // next analysis epoch
        }

        /* ---- 3. deliver one sample ---- */
        float out = 0.f;
        if (!m_outputBuffer.empty()) {
            out = m_outputBuffer.front();
            m_outputBuffer.pop_front();
        }
        return out;
    }

  private:
    void PadOutputDeque(int hop) {
        if (hop <= 0)
            return; // nothing to pad

        // push <hop> zeros so the next frame lands hop samples ahead
        m_outputBuffer.insert(m_outputBuffer.end(), hop, 0.0f);
    }

    void OverlapAdd(int centre /*analysis centre in input time*/) {
        const int halfWindow = m_pitchPeriod;
        std::vector<float> frame(halfWindow * 2, 0.0f);

        for (int i = -halfWindow; i < halfWindow; ++i) {
            int readIndex = centre + i;
            if (readIndex < 0 || readIndex >= int(m_inputBuffer.size()))
                continue;

            const float w = 0.5f * (1.0f - std::cos(2.0f * M_PI * (i + halfWindow) / (2.0f * halfWindow)));

            frame[i + halfWindow] = w * m_inputBuffer[readIndex & (m_inputBuffer.size() - 1)];
        }

        // overlap-add into output deque
        for (size_t i = 0; i < frame.size(); ++i) {
            if (i >= m_outputBuffer.size())
                m_outputBuffer.push_back(frame[i]);
            else
                m_outputBuffer[i] += frame[i];
        }
    }

    /* ---------- state ---------- */
    float m_sampleRate;
    float m_pitchShiftRatio = 1.f;
    int m_pitchPeriod;
    int m_maxPeriodSamples;
    int m_minPeriodSamples;

    std::vector<float> m_inputBuffer;
    std::deque<float> m_outputBuffer;

    int m_hopIn;
    int m_hopOut;
    int m_readIndex;
    int m_writeIndex;
};

#endif // PITCH_SHIFTER_PSOLA_H
