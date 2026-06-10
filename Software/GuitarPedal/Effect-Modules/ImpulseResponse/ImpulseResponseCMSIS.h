#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include "arm_math.h"

class ImpulseResponseCMSIS
{
  public:
    static constexpr uint32_t kMaxTaps      = 1024;
    static constexpr uint32_t kMaxBlockSize = 128;

    ImpulseResponseCMSIS();

    void Init(const std::vector<float>& ir);
    void Init(const float* ir, uint32_t irLength);

    template <size_t N>
    void Init(const std::array<float, N>& ir)
    {
        Init(ir.data(), static_cast<uint32_t>(N));
    }

    template <size_t N>
    void Init(const float (&ir)[N])
    {
        Init(ir, static_cast<uint32_t>(N));
    }

    float Process(float input);

    void ProcessBlock(const float* input, float* output, uint32_t blockSize);

    void Reset();

    uint32_t GetTapCount() const { return m_tapCount; }
    uint32_t GetDroppedTapCount() const { return m_droppedTapCount; }

  private:
    void InitEmpty();

    arm_fir_instance_f32 m_fir{};

    alignas(4) float m_coeffs[kMaxTaps]{};
    alignas(4) float m_state[kMaxTaps + kMaxBlockSize - 1]{};

    uint32_t m_tapCount        = 0;
    uint32_t m_droppedTapCount = 0;
    bool     m_initialized     = false;
};