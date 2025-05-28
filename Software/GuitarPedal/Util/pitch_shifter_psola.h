#ifndef PITCH_SHIFTER_PSOLA_H
#define PITCH_SHIFTER_PSOLA_H

#include "daisy_seed.h" // DSY_SDRAM_BSS
#include <algorithm>
#include <cmath>

/*----------------------------------------------------------
   Compile-time limits (adapt if you change fs or min F0)
  ----------------------------------------------------------*/
constexpr int kMaxFs = 48000;                 // highest sample-rate you’ll run
constexpr int kMinFund = 30;                  // lowest detectable pitch (Hz)
constexpr int kMaxPeriod = kMaxFs / kMinFund; // 1920 samples
constexpr int kRingLen = kMaxPeriod * 4;      //  7680 samples
constexpr int kFrameLenMax = kMaxPeriod * 2;  //  3840 samples
constexpr float kGainFloor = 1e-6f;

/*----------------------------------------------------------
   SDRAM-resident buffers (big data only)
  ----------------------------------------------------------*/
DSY_SDRAM_BSS float g_psInput[kRingLen];
DSY_SDRAM_BSS float g_psOutput[kRingLen];
DSY_SDRAM_BSS float g_psGain[kRingLen];
DSY_SDRAM_BSS float g_psWindow[kFrameLenMax];
DSY_SDRAM_BSS float g_psFrame[kFrameLenMax];
DSY_SDRAM_BSS float g_psPeriod[kFrameLenMax]; // period-length Hann

/*----------------------------------------------------------
   PitchShifterPSOLA – no heap allocation after Init()
  ----------------------------------------------------------*/
class PitchShifterPSOLA {
  public:
    PitchShifterPSOLA() = default;

    void Init(float sample_rate) {
        m_sampleRate = sample_rate;
        m_maxPeriodSamples = static_cast<int>(sample_rate / kMinFund);
        m_minPeriodSamples = static_cast<int>(sample_rate / 1000.0f); // 1 kHz

        /* build master Hann LUT once */
        for (int i = 0; i < kFrameLenMax; ++i)
            g_psWindow[i] = 0.5f * (1.f - cosf(2.f * M_PI * i / (kFrameLenMax - 1)));

        /* clear rings */
        std::fill_n(g_psInput, kRingLen, 0.f);
        std::fill_n(g_psOutput, kRingLen, 0.f);
        std::fill_n(g_psGain, kRingLen, 0.f);

        m_mask = kRingLen - 1;
        m_pitchPeriod = std::clamp(int(sample_rate / 220.0f), // ~A3
                                   m_minPeriodSamples, m_maxPeriodSamples);

        m_hopIn = m_pitchPeriod;
        m_hopOut = m_pitchPeriod;
        m_pitchShiftRatio = 1.f;
    }

    /* 1.0 = no shift, 2.0 = +12 st, 0.5 = −12 st */
    void SetPitchShiftRatio(float ratio) {
        m_pitchShiftRatio = std::clamp(ratio, 0.5f, 2.0f);
        m_hopOut = std::max(1, int(float(m_pitchPeriod) / m_pitchShiftRatio));
    }

    /* update on every good pitch estimate */
    void SetPitchPeriod(int periodSamples) {
        m_pitchPeriod = std::clamp(periodSamples, m_minPeriodSamples, m_maxPeriodSamples);
        m_hopIn = m_pitchPeriod;
        m_hopOut = std::max(1, int(float(m_pitchPeriod) / m_pitchShiftRatio));

        /* copy correct-length Hann to period window */
        const int frameLen = m_pitchPeriod * 2;
        std::copy_n(g_psWindow, frameLen, g_psPeriod);
    }

    inline float PopOutput() {
        float raw = g_psOutput[m_outRead];
        float g = g_psGain[m_outRead];
        float out = (g > kGainFloor) ? raw / g : raw;

        g_psOutput[m_outRead] = 0.f;
        g_psGain[m_outRead] = 0.f;
        m_outRead = (m_outRead + 1) & m_mask;

        if (((m_outWrite - m_outRead) & m_mask) < m_pitchPeriod)
            m_outRead = m_outWrite;

        return out;
    }

    /* -------- add to private state -------- */
    float m_inBlock[64]; // tiny stack buffer
    int m_blkPos = 0;

    /* -------- replace the very top of ProcessSample() -------- */
    float ProcessSample(float in) {
        m_inBlock[m_blkPos++] = in;

        /* still not at block edge?  just return the next output sample */
        if (m_blkPos < 64)
            return PopOutput();

        /* we have 64 fresh samples – copy them to the ring in one burst */
        const int base = m_writeIndex;
        for (int i = 0; i < 64; ++i)
            g_psInput[(base + i) & m_mask] = m_inBlock[i];
        m_writeIndex = (m_writeIndex + 64) & m_mask;
        m_blkPos = 0;

        /* while we passed ≥ hopIn, emit frame(s) */
        while (((m_writeIndex - m_readIndex) & m_mask) >= m_hopIn) {
            PadOutput(m_hopOut);
            OverlapAdd(m_readIndex);
            m_readIndex = (m_readIndex + m_hopIn) & m_mask;
        }

        return PopOutput();
    }

  private:
    /* zero-pad in output ring before each new frame */
    void PadOutput(int hop) {
        for (int i = 0; i < hop; ++i)
            g_psOutput[(m_outWrite + i) & m_mask] = 0.f;
    }

    /* overlap-add one frame starting at m_outWrite */
    void OverlapAdd(int centre) {
        const int half = m_pitchPeriod;
        const int len = half * 2;

        /* window + copy into scratch frame */
        for (int i = -half; i < half; ++i) {
            int readIdx = (centre + i) & m_mask;
            g_psFrame[i + half] = g_psPeriod[i + half] * g_psInput[readIdx];
        }

        /* add into output & gain rings */
        for (int i = 0; i < len; ++i) {
            int idx = (m_outWrite + i) & m_mask;
            g_psOutput[idx] += g_psFrame[i];
            g_psGain[idx] += g_psPeriod[i];
        }

        /* advance write cursor */
        m_outWrite = (m_outWrite + m_hopOut) & m_mask;
    }

    /* -------- scalar state (in tightly-coupled SRAM) -------- */
    float m_sampleRate = 48000.f;
    float m_pitchShiftRatio = 1.f;

    int m_pitchPeriod = 0;
    int m_maxPeriodSamples = 0;
    int m_minPeriodSamples = 0;

    int m_hopIn = 0;
    int m_hopOut = 0;

    int m_mask = kRingLen - 1;
    int m_writeIndex = 0;
    int m_readIndex = 0;
    int m_outWrite = 0;
    int m_outRead = 0;
};

#endif /* PITCH_SHIFTER_PSOLA_H */
