#ifndef PITCH_SHIFTER_PSOLA_H
#define PITCH_SHIFTER_PSOLA_H

#include "daisy_seed.h" // DSY_SDRAM_BSS
#include <algorithm>
#include <cmath>

/* ----------------------------------------------------------
   Tunables (change if you run a different rate / range)
   ---------------------------------------------------------- */
constexpr int kMaxFs = 48000;                 // highest sample-rate you’ll use
constexpr int kMinFund = 30;                  // lowest pitch to detect (Hz)
constexpr int kMaxPeriod = kMaxFs / kMinFund; // 960 samples
constexpr int kRingLen = kMaxPeriod * 4;      // 3840 samples
constexpr int kFrameMax = kMaxPeriod * 2;     // 1920 samples
constexpr int kBlock = 32;                    // I/O block size
constexpr float kGainFloor = 1e-6f;

/* ----------------------------------------------------------
   Large buffers in SDRAM
   ---------------------------------------------------------- */
DSY_SDRAM_BSS float gIn[kRingLen];
DSY_SDRAM_BSS float gOut[kRingLen];
DSY_SDRAM_BSS float gGain[kRingLen];
DSY_SDRAM_BSS float gHann[kFrameMax]; // master Hann table
DSY_SDRAM_BSS float gWin[kFrameMax];  // current period window

/* One frame of scratch in DTCM (fast local RAM)  */
static float gFrame[kFrameMax] __attribute__((section(".dtcm")));

/* ----------------------------------------------------------
   PSOLA shifter (no heap after Init)
   ---------------------------------------------------------- */
class PitchShifterPSOLA {
  public:
    void Init(float fs) {
        m_fs = fs;
        m_minPer = int(fs / 1000.f);
        m_maxPer = int(fs / kMinFund);

        /* build Hann LUT once */
        for (int i = 0; i < kFrameMax; ++i)
            gHann[i] = 0.5f * (1.f - cosf(2.f * M_PI * i / (kFrameMax - 1)));

        std::fill_n(gIn, kRingLen, 0.f);
        std::fill_n(gOut, kRingLen, 0.f);
        std::fill_n(gGain, kRingLen, 0.f);

        m_mask = kRingLen - 1;
        m_period = std::clamp(int(fs / 220.f), m_minPer, m_maxPer);
        m_hopIn = m_period;
        m_hopOut = m_period;
        m_ratio = 1.f;
    }

    /* 1.0 = no shift, 2.0 = +12 st, 0.5 = −12 st */
    void SetPitchShiftRatio(float r) {
        m_ratio = std::clamp(r, 0.5f, 2.f);
        m_hopOut = std::max(1, int(float(m_period) / m_ratio));
        m_half = (m_ratio > 1.25f); // half window when shifting up
    }

    /* call whenever the pitch detector has a stable period (in samples) */
    void SetPitchPeriod(int P) {
        m_period = std::clamp(P, m_minPer, m_maxPer);
        m_hopIn = m_period;
        m_hopOut = std::max(1, int(float(m_period) / m_ratio));
        m_half = (m_ratio > 1.25f);

        const int len = m_half ? m_period : m_period * 2;
        std::copy_n(gHann, len, gWin);
    }

    /* -------------------------------------------------- */
    /*          one-sample API (block-buffered)           */
    /* -------------------------------------------------- */
    float ProcessSample(float in) {
        m_blk[m_blkPos++] = in;
        if (m_blkPos < kBlock) // fast path: nothing heavy to do
            return Pop();

        /* -- copy kBlock samples into input ring -- */
        for (int i = 0; i < kBlock; ++i)
            gIn[(m_write + i) & m_mask] = m_blk[i];
        m_write = (m_write + kBlock) & m_mask;
        m_blkPos = 0;

        /* -- generate frames while we’re ahead -- */
        while (((m_write - m_read) & m_mask) >= m_hopIn) {
            PadZeros(m_hopOut);
            AddFrame(m_read);
            m_read = (m_read + m_hopIn) & m_mask;
        }
        return Pop();
    }

  private:
    /* zero-pad output ring before adding a new frame */
    inline void PadZeros(int hop) {
        for (int i = 0; i < hop; ++i)
            gOut[(m_outW + i) & m_mask] = 0.f;
    }

    /* overlap-add one PSOLA frame */
    inline void AddFrame(int centre) {
        const int half = m_half ? (m_period >> 1) : m_period;
        const int len = half * 2;

        /* window * input  →  gFrame */
        int start = (centre - half) & m_mask;
        int seg1 = std::min(len, kRingLen - start);
        for (int i = 0; i < seg1; ++i)
            gFrame[i] = gWin[i] * gIn[start + i];
        for (int i = seg1; i < len; ++i)
            gFrame[i] = gWin[i] * gIn[i - seg1];

        /* add into output & gain rings */
        int tail = kRingLen - m_outW;
        if (len <= tail) {
            for (int i = 0; i < len; ++i) {
                gOut[m_outW + i] += gFrame[i];
                gGain[m_outW + i] += gWin[i];
            }
        } else {
            int first = tail, second = len - tail;
            for (int i = 0; i < first; ++i) {
                gOut[m_outW + i] += gFrame[i];
                gGain[m_outW + i] += gWin[i];
            }
            for (int i = 0; i < second; ++i) {
                gOut[i] += gFrame[first + i];
                gGain[i] += gWin[first + i];
            }
        }
        m_outW = (m_outW + m_hopOut) & m_mask;
    }

    /* pop one sample from output ring and normalise it */
    inline float Pop() {
        float y = gOut[m_outR];
        float g = gGain[m_outR];
        float out = (g > kGainFloor) ? y / g : y;

        gOut[m_outR] = 0.f;
        gGain[m_outR] = 0.f;
        m_outR = (m_outR + 1) & m_mask;

        /* underrun guard */
        if (((m_outW - m_outR) & m_mask) < m_period)
            m_outR = m_outW;

        return out;
    }

    /* ---------- small state (fits in DTCM) ---------- */
    float m_fs = 48000.f;
    float m_ratio = 1.f;

    int m_period = 0;
    int m_minPer = 0;
    int m_maxPer = 0;
    int m_hopIn = 0;
    int m_hopOut = 0;
    bool m_half = false;

    int m_mask = kRingLen - 1;
    int m_write = 0, m_read = 0;
    int m_outW = 0, m_outR = 0;

    /* block-I/O scratch */
    float m_blk[kBlock]{};
    int m_blkPos = 0;
};

#endif /* PITCH_SHIFTER_PSOLA_H */
