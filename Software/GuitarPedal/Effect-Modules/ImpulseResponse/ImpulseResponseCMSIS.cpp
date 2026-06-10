#include "ImpulseResponseCMSIS.h"

ImpulseResponseCMSIS::ImpulseResponseCMSIS()
{
    InitEmpty();
}

void ImpulseResponseCMSIS::InitEmpty()
{
    std::fill(std::begin(m_coeffs), std::end(m_coeffs), 0.0f);
    std::fill(std::begin(m_state), std::end(m_state), 0.0f);

    m_coeffs[0]       = 1.0f;
    m_tapCount        = 1;
    m_droppedTapCount = 0;

    arm_fir_init_f32(
        &m_fir,
        m_tapCount,
        m_coeffs,
        m_state,
        kMaxBlockSize);

    m_initialized = true;
}

void ImpulseResponseCMSIS::Init(const std::vector<float>& ir)
{
    Init(ir.data(), static_cast<uint32_t>(ir.size()));
}

void ImpulseResponseCMSIS::Init(const float* ir, uint32_t irLength)
{
    if (ir == nullptr || irLength == 0)
    {
        InitEmpty();
        return;
    }

    const uint32_t tapsToUse = std::min<uint32_t>(irLength, kMaxTaps);

    m_tapCount        = tapsToUse;
    m_droppedTapCount = irLength - tapsToUse;

    std::fill(std::begin(m_coeffs), std::end(m_coeffs), 0.0f);
    std::fill(std::begin(m_state), std::end(m_state), 0.0f);

    // CMSIS FIR expects coefficients in reversed order:
    // { b[numTaps - 1], ..., b[1], b[0] }
    //
    // Your existing implementation also reverses the IR into mWeight,
    // so this should match the old behavior.
    for (uint32_t i = 0; i < tapsToUse; ++i)
    {
        m_coeffs[tapsToUse - 1U - i] = ir[i];
    }

    arm_fir_init_f32(
        &m_fir,
        m_tapCount,
        m_coeffs,
        m_state,
        kMaxBlockSize);

    m_initialized = true;
}

float ImpulseResponseCMSIS::Process(float input)
{
    float output = 0.0f;
    ProcessBlock(&input, &output, 1);
    return output;
}

void ImpulseResponseCMSIS::ProcessBlock(const float* input,
                                        float*       output,
                                        uint32_t     blockSize)
{
    if (input == nullptr || output == nullptr || blockSize == 0)
    {
        return;
    }

    if (!m_initialized)
    {
        InitEmpty();
    }

    while (blockSize > 0)
    {
        const uint32_t n = std::min<uint32_t>(blockSize, kMaxBlockSize);

        arm_fir_f32(
            &m_fir,
            const_cast<float*>(input),
            output,
            n);

        input += n;
        output += n;
        blockSize -= n;
    }
}

void ImpulseResponseCMSIS::Reset()
{
    std::fill(std::begin(m_state), std::end(m_state), 0.0f);

    arm_fir_init_f32(
        &m_fir,
        m_tapCount == 0 ? 1 : m_tapCount,
        m_coeffs,
        m_state,
        kMaxBlockSize);
}