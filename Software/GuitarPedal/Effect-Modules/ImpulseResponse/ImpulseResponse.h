#pragma once
#include <cstdint>

#include "arm_math.h"

class ImpulseResponse {
  public:
    ImpulseResponse();
    void init(const float *ir, uint32_t len, bool normalize);
    void processBlock(const float *in, float *out, uint32_t n);
    void setImpulseResponse(const float *ir, uint32_t len, bool norm);

  private:
    static constexpr uint32_t DEFAULT_FIR_LEN = 1024;
    static constexpr uint32_t MAX_BLOCK = 128;
    static constexpr uint32_t FIR_STATE_LEN = DEFAULT_FIR_LEN + MAX_BLOCK - 1;

    arm_fir_instance_f32 fir_{};
    float fir_state_[FIR_STATE_LEN]{};
    float fir_coeffs_[DEFAULT_FIR_LEN]{};

    static void normalise(float *c, uint32_t len);
};
