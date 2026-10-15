#pragma once
/*
    nam_a2_runtime.h

    Allocation-free A2 Nano WaveNet runtime for Daisy / STM32H7.
    Based on NamA2JCM2000Daisy48_minram_v16_k6_current_direct.h by nadavb at
    https://forum.electro-smith.com/t/nam-a2-on-daisy-seed/9186.

    Architecture: A2 WaveNet, C=3 channels, 23 layers, 1871 weights.
    Block size: 48 samples. Mono input -> mono output.

    Usage:
      - Include model_data_nam_a2.h alongside this header.
      - Call A2Player::load_weights() once from Init(), not from the audio callback.
      - Call A2Player::process_block_48(in, out) from the audio callback
        with exactly 48 samples.

    v16 design:
      - branchless leaky ReLU activation
      - K=6 dual-sample kernel uses direct current tap for non-layer-0 layers
      - no heap, no std::vector, no modulo/division in the audio callback
      - C=3 hard-unrolled matrix-vector math
      - 16-tap head uses bitmask wrapping
      - flattened contiguous history arena with exact ring sizes
*/

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#ifndef NAM_A2_ALIGN32
#define NAM_A2_ALIGN32 alignas(32)
#endif

// Storage policy (STM32H750, BOOT_SRAM):
//   - NAM_A2_HOT_DATA:       packed weights/biases   -> DTCMRAM  (.dtcmram_bss)
//   - NAM_A2_HOT_STATE_DATA: small hot work buffers  -> DTCMRAM  (.dtcmram_bss)
//   - NAM_A2_STATE_DATA:     large history buffer    -> RAM_D2   (.sram_d2_bss)
//
// The default .bss lands in DTCMRAM (128 KB) under BOOT_SRAM, so the ~76 KB
// history must be relocated or it crowds everything else out. It goes to the
// otherwise-empty 256 KB D2-domain SRAM (the AXI-SRAM is mostly full of code),
// which is on-chip and cacheable — far faster than external SDRAM. .sram_d2_bss
// is supplied by nam_a2_sections.lds (wired in via the Makefile); the hot data
// is explicitly pinned to DTCMRAM so the tiering is correct regardless of boot
// mode (under BOOT_QSPI the default .bss would otherwise be SRAM, not DTCMRAM).
//
// These macros must be applied to whole variables with static storage duration —
// applying a section attribute to a struct *member* is silently ignored by GCC.
#ifndef NAM_A2_HOT_DATA
#define NAM_A2_HOT_DATA __attribute__((section(".dtcmram_bss")))
#endif

#ifndef NAM_A2_STATE_DATA
#define NAM_A2_STATE_DATA __attribute__((section(".sram_d2_bss")))
#endif

#ifndef NAM_A2_HOT_STATE_DATA
#define NAM_A2_HOT_STATE_DATA __attribute__((section(".dtcmram_bss")))
#endif

// Storage for the read-only model weight arrays. Empty by default, so the
// constexpr arrays land in .rodata — under BOOT_SRAM that is the AXI-SRAM
// (~7.5 KB per model). They are only touched by load_weights() on a model
// switch, never in the audio loop. To move them to QSPI flash (e.g. to reclaim
// SRAM), define this to a QSPI section — but that only works under BOOT_QSPI,
// where the program runs from QSPI; under BOOT_SRAM the bootloader loads only
// the SRAM image, so QSPI-placed data would not be present at runtime.
#ifndef NAM_A2_MODEL_DATA
#define NAM_A2_MODEL_DATA
#endif

#ifndef NAM_A2_FORCE_INLINE
#define NAM_A2_FORCE_INLINE inline
#endif

#ifndef NAM_A2_NOINLINE
#if defined(__GNUC__) || defined(__clang__)
#define NAM_A2_NOINLINE __attribute__((noinline))
#else
#define NAM_A2_NOINLINE
#endif
#endif

#ifndef NAM_A2_TINY_INLINE
#if defined(__GNUC__) || defined(__clang__)
#define NAM_A2_TINY_INLINE inline __attribute__((always_inline))
#else
#define NAM_A2_TINY_INLINE inline
#endif
#endif

#ifndef NAM_A2_ENABLE_PREFETCH
#define NAM_A2_ENABLE_PREFETCH 0
#endif

#if NAM_A2_ENABLE_PREFETCH && (defined(__GNUC__) || defined(__clang__))
  #define NAM_A2_PREFETCH_R(PTR) __builtin_prefetch((PTR), 0, 1)
#else
  #define NAM_A2_PREFETCH_R(PTR) do {} while (false)
#endif

namespace nam_a2_daisy
{
static constexpr int kBlockSize = 48;
static constexpr int kChannels = 3;
static constexpr int kNumLayers = 23;
static constexpr int kHeadKernel = 16;
static constexpr float kLeakySlope = 0.01f;

// process_head() wraps its ring index with a bitmask, which requires kHeadKernel
// to be a power of two.
static_assert((kHeadKernel & (kHeadKernel - 1)) == 0,
              "kHeadKernel must be a power of two for bitmask ring wrapping");

inline constexpr int kKernelSizes[kNumLayers] = {
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 15, 15, 6, 6, 6, 6, 6, 6, 6
};

inline constexpr int kDilations[kNumLayers] = {
    1, 3, 7, 17, 41, 101, 239, 1, 3, 7, 17, 41, 101, 239, 1, 13, 1, 3, 7, 17, 41, 101, 239
};

static constexpr int kKernelSum = 156;
static constexpr int kConvWeightCount = kKernelSum * kChannels * kChannels; // 1404
static constexpr int kLayer1x1WeightCount = kNumLayers * kChannels * kChannels; // 207
static constexpr int kA2WeightCount = 1871;

static constexpr int exact_cols_for_layer(int i) noexcept
{
    return (kKernelSizes[i] - 1) * kDilations[i] + 2;
}

static constexpr int compute_exact_history_columns() noexcept
{
    int total = 0;
    for (int i = 0; i < kNumLayers; ++i)
        total += exact_cols_for_layer(i);
    return total;
}

static constexpr int kHistoryColumns = compute_exact_history_columns(); // 6377
static constexpr int kHistoryFloats  = kHistoryColumns * kChannels;     // 19131
static constexpr int kHistoryBytes   = kHistoryFloats * 4;              // 76524

// -----------------------------------------------------------------------------
// Runtime structures
// -----------------------------------------------------------------------------

inline constexpr int kLayerCols[kNumLayers] = {
    7, 17, 37, 87, 207, 507, 1197, 7, 17, 37, 87, 207, 507, 1197,
    16, 184, 7, 17, 37, 87, 207, 507, 1197
};

inline constexpr int kLayerHistoryOffset[kNumLayers] = {
    0, 21, 72, 183, 444, 1065, 2586, 6177, 6198, 6249,
    6360, 6621, 7242, 8763, 12354, 12402, 12954, 12975, 13026, 13137,
    13398, 14019, 15540
};

inline constexpr int kLayerConvOffset[kNumLayers] = {
    0, 54, 108, 162, 216, 270, 324, 378, 432, 486, 540, 594,
    648, 702, 756, 891, 1026, 1080, 1134, 1188, 1242, 1296, 1350
};

inline constexpr int kLayerL1Offset[kNumLayers] = {
    0, 9, 18, 27, 36, 45, 54, 63, 72, 81, 90, 99,
    108, 117, 126, 135, 144, 153, 162, 171, 180, 189, 198
};

struct A2LayerRuntime
{
    NAM_A2_ALIGN32 float convB[3];
    NAM_A2_ALIGN32 float mixinW[3];
    NAM_A2_ALIGN32 float preCurrent[3];
    NAM_A2_ALIGN32 float l1x1B[3];
};

static constexpr int kRawHeadOffset  = 3 + kConvWeightCount + kNumLayers * 18; // 1821
static constexpr int kRawHeadBOffset = kRawHeadOffset + kHeadKernel * kChannels; // 1869
static constexpr int kRawScaleOffset = kRawHeadBOffset + 1; // 1870

struct A2SharedWeights
{
    NAM_A2_ALIGN32 float rechannel[3];
    NAM_A2_ALIGN32 float convW[kConvWeightCount];
    NAM_A2_ALIGN32 float l1x1W[kLayer1x1WeightCount];
    NAM_A2_ALIGN32 float headW[kHeadKernel][3];
    float headB;
    float headScale;
    A2LayerRuntime layer[kNumLayers];
    bool loaded;
};

struct A2State
{
    // Relocation to AXI-SRAM is done on the whole A2Player instance (see
    // s_nam_a2_model in nam_a2_module.cpp), since a section attribute on a
    // struct member is ignored by GCC. history is the only large member.
    NAM_A2_ALIGN32 float history[kHistoryFloats];
    int layerWritePos[kNumLayers];
};

struct A2HotState
{
    NAM_A2_ALIGN32 float headHistory[kHeadKernel * 3];
    NAM_A2_ALIGN32 float bufA[kBlockSize * 3];
    NAM_A2_ALIGN32 float bufB[kBlockSize * 3];
    NAM_A2_ALIGN32 float headSum[kBlockSize * 3];
    int headWritePos;
};

// -----------------------------------------------------------------------------
// DSP helpers
// -----------------------------------------------------------------------------

#ifndef NAM_A2_BRANCHLESS_LEAKY
#define NAM_A2_BRANCHLESS_LEAKY 1
#endif

NAM_A2_TINY_INLINE float leaky(float x) noexcept
{
#if NAM_A2_BRANCHLESS_LEAKY
    constexpr float p = 0.5f * (1.0f + kLeakySlope);
    constexpr float q = 0.5f * (1.0f - kLeakySlope);
    return p * x + q * __builtin_fabsf(x);
#else
    return x >= 0.0f ? x : x * kLeakySlope;
#endif
}

NAM_A2_TINY_INLINE void acc_tap3(const float* __restrict__ w,
                                  const float* __restrict__ s,
                                  float& z0,
                                  float& z1,
                                  float& z2) noexcept
{
    const float s0 = s[0];
    const float s1 = s[1];
    const float s2 = s[2];

    z0 += s0 * w[0] + s1 * w[3] + s2 * w[6];
    z1 += s0 * w[1] + s1 * w[4] + s2 * w[7];
    z2 += s0 * w[2] + s1 * w[5] + s2 * w[8];
}

inline bool load_weights(A2SharedWeights& dst, const float* src, std::size_t count) noexcept
{
    if (src == nullptr || count != static_cast<std::size_t>(kA2WeightCount))
        return false;

    std::memset(&dst, 0, sizeof(dst));

    std::size_t p = 0;
    auto take = [&]() noexcept -> float {
        return src[p++];
    };

    for (int c = 0; c < 3; ++c)
        dst.rechannel[c] = take();

    int convOff = 0;
    int l1Off = 0;

    for (int li = 0; li < kNumLayers; ++li)
    {
        A2LayerRuntime& L = dst.layer[li];
        const int kernelSize = kKernelSizes[li];

        for (int out = 0; out < 3; ++out)
            for (int in = 0; in < 3; ++in)
                for (int k = 0; k < kernelSize; ++k)
                    dst.convW[convOff + k * 9 + in * 3 + out] = take();

        for (int out = 0; out < 3; ++out)
            L.convB[out] = take();

        for (int out = 0; out < 3; ++out)
            L.mixinW[out] = take();

        for (int out = 0; out < 3; ++out)
            for (int in = 0; in < 3; ++in)
                dst.l1x1W[l1Off + in * 3 + out] = take();

        for (int out = 0; out < 3; ++out)
            L.l1x1B[out] = take();

        if (li == 0 && kernelSize == 6)
        {
            const float* wc = dst.convW + convOff + 5 * 9;
            for (int out = 0; out < 3; ++out)
                L.preCurrent[out] = L.mixinW[out]
                                  + dst.rechannel[0] * wc[0 * 3 + out]
                                  + dst.rechannel[1] * wc[1 * 3 + out]
                                  + dst.rechannel[2] * wc[2 * 3 + out];
        }

        convOff += kernelSize * 9;
        l1Off += 9;
    }

    for (int in = 0; in < 3; ++in)
        for (int k = 0; k < kHeadKernel; ++k)
            dst.headW[k][in] = take();

    dst.headB = take();
    dst.headScale = take();

    dst.loaded = (p == count
               && convOff == kConvWeightCount
               && l1Off == kLayer1x1WeightCount
               && kLayerHistoryOffset[kNumLayers - 1]
                    + kLayerCols[kNumLayers - 1] * kChannels == kHistoryFloats);

    return dst.loaded;
}

inline void reset_state(A2State& st, A2HotState& hot, const A2SharedWeights&) noexcept
{
    std::fill(std::begin(st.history), std::end(st.history), 0.0f);
    std::fill(std::begin(hot.headHistory), std::end(hot.headHistory), 0.0f);
    std::fill(std::begin(hot.bufA), std::end(hot.bufA), 0.0f);
    std::fill(std::begin(hot.bufB), std::end(hot.bufB), 0.0f);
    std::fill(std::begin(hot.headSum), std::end(hot.headSum), 0.0f);

    for (int i = 0; i < kNumLayers; ++i)
        st.layerWritePos[i] = kLayerCols[i] - 1;

    hot.headWritePos = kHeadKernel - 1;
}

inline constexpr int prewarm_samples() noexcept
{
    int total = kHeadKernel - 1;
    for (int i = 0; i < kNumLayers; ++i)
        total += (kKernelSizes[i] - 1) * kDilations[i];
    return total;
}

// -----------------------------------------------------------------------------
// Layer kernels (NAM_A2_NOINLINE keeps code size small in BOOT_SRAM builds)
// -----------------------------------------------------------------------------

template<int K>
NAM_A2_NOINLINE void process_layer_kernel(A2State& st,
                                          A2HotState& hot,
                                          const A2SharedWeights& sw,
                                          int li,
                                          const float* cond,
                                          const float* in,
                                          float* out) noexcept
{
    const A2LayerRuntime& L = sw.layer[li];

    int wp = st.layerWritePos[li];
    const int cols = kLayerCols[li];
    const int dilation = kDilations[li];

    float* const hist = st.history + kLayerHistoryOffset[li];
    const float* const wAll = sw.convW + kLayerConvOffset[li];
    const float* const lx = sw.l1x1W + kLayerL1Offset[li];

    const float cb0 = L.convB[0];
    const float cb1 = L.convB[1];
    const float cb2 = L.convB[2];

    const bool precombineCurrent = (li == 0 && K == 6);
    const float mw0 = precombineCurrent ? L.preCurrent[0] : L.mixinW[0];
    const float mw1 = precombineCurrent ? L.preCurrent[1] : L.mixinW[1];
    const float mw2 = precombineCurrent ? L.preCurrent[2] : L.mixinW[2];

    const float b0 = L.l1x1B[0];
    const float b1 = L.l1x1B[1];
    const float b2 = L.l1x1B[2];

    for (int n = 0; n < kBlockSize; ++n)
    {
        const float* const srcIn = in + n * 3;
        float* const histCol = hist + wp * 3;

        const float x0 = srcIn[0];
        const float x1 = srcIn[1];
        const float x2 = srcIn[2];

        histCol[0] = x0;
        histCol[1] = x1;
        histCol[2] = x2;

        const float c = cond[n];
        float z0 = cb0 + mw0 * c;
        float z1 = cb1 + mw1 * c;
        float z2 = cb2 + mw2 * c;

        for (int k = 0; k < K; ++k)
        {
            if (precombineCurrent && k == K - 1)
                continue;

            const int lag = (K - 1 - k) * dilation;
            int col = wp - lag;
            if (col < 0)
                col += cols;

            const float* const s = hist + col * 3;
            acc_tap3(wAll + k * 9, s, z0, z1, z2);
        }

        const float a0 = leaky(z0);
        const float a1 = leaky(z1);
        const float a2 = leaky(z2);

        float* const hs = hot.headSum + n * 3;
        hs[0] += a0;
        hs[1] += a1;
        hs[2] += a2;

        float* const dst = out + n * 3;
        dst[0] = x0 + b0 + a0 * lx[0] + a1 * lx[3] + a2 * lx[6];
        dst[1] = x1 + b1 + a0 * lx[1] + a1 * lx[4] + a2 * lx[7];
        dst[2] = x2 + b2 + a0 * lx[2] + a1 * lx[5] + a2 * lx[8];

        ++wp;
        if (wp >= cols)
            wp = 0;
    }

    st.layerWritePos[li] = wp;
}

NAM_A2_NOINLINE void process_layer_kernel6_dual(A2State& st,
                                                A2HotState& hot,
                                                const A2SharedWeights& sw,
                                                int li,
                                                const float* __restrict__ cond,
                                                const float* __restrict__ in,
                                                float* __restrict__ out) noexcept
{
    const A2LayerRuntime& L = sw.layer[li];

    int wp = st.layerWritePos[li];
    const int cols = kLayerCols[li];
    const int dilation = kDilations[li];

    float* const hist = st.history + kLayerHistoryOffset[li];
    const float* __restrict__ const wAll = sw.convW + kLayerConvOffset[li];
    const float* __restrict__ const lx = sw.l1x1W + kLayerL1Offset[li];

    const float cb0 = L.convB[0];
    const float cb1 = L.convB[1];
    const float cb2 = L.convB[2];

    const bool precombineCurrent = (li == 0);
    const float mw0 = precombineCurrent ? L.preCurrent[0] : L.mixinW[0];
    const float mw1 = precombineCurrent ? L.preCurrent[1] : L.mixinW[1];
    const float mw2 = precombineCurrent ? L.preCurrent[2] : L.mixinW[2];

    const float b0 = L.l1x1B[0];
    const float b1 = L.l1x1B[1];
    const float b2 = L.l1x1B[2];

    for (int n = 0; n < kBlockSize; n += 2)
    {
        const int wpA = wp;
        int wpB = wpA + 1;
        if (wpB >= cols)
            wpB = 0;

        const float* __restrict__ const srcA = in + n * 3;
        const float* __restrict__ const srcB = srcA + 3;

        const float ax0 = srcA[0];
        const float ax1 = srcA[1];
        const float ax2 = srcA[2];
        const float bx0 = srcB[0];
        const float bx1 = srcB[1];
        const float bx2 = srcB[2];

        float* const histA = hist + wpA * 3;
        float* const histB = hist + wpB * 3;

        histA[0] = ax0; histA[1] = ax1; histA[2] = ax2;
        histB[0] = bx0; histB[1] = bx1; histB[2] = bx2;

        const float ca = cond[n];
        const float cb = cond[n + 1];

        float za0 = cb0 + mw0 * ca;
        float za1 = cb1 + mw1 * ca;
        float za2 = cb2 + mw2 * ca;
        float zb0 = cb0 + mw0 * cb;
        float zb1 = cb1 + mw1 * cb;
        float zb2 = cb2 + mw2 * cb;

        if (!precombineCurrent)
        {
            const float* __restrict__ const wc = wAll + 5 * 9;
            za0 += ax0 * wc[0] + ax1 * wc[3] + ax2 * wc[6];
            za1 += ax0 * wc[1] + ax1 * wc[4] + ax2 * wc[7];
            za2 += ax0 * wc[2] + ax1 * wc[5] + ax2 * wc[8];

            zb0 += bx0 * wc[0] + bx1 * wc[3] + bx2 * wc[6];
            zb1 += bx0 * wc[1] + bx1 * wc[4] + bx2 * wc[7];
            zb2 += bx0 * wc[2] + bx1 * wc[5] + bx2 * wc[8];
        }

        for (int k = 0; k < 5; ++k)
        {
            const int lag = (5 - k) * dilation;

            int colA = wpA - lag;
            if (colA < 0)
                colA += cols;

            int colB = wpB - lag;
            if (colB < 0)
                colB += cols;

            const float* __restrict__ const sA = hist + colA * 3;
            const float* __restrict__ const sB = hist + colB * 3;
            const float* __restrict__ const w = wAll + k * 9;

            acc_tap3(w, sA, za0, za1, za2);
            acc_tap3(w, sB, zb0, zb1, zb2);
        }

        const float aa0 = leaky(za0);
        const float aa1 = leaky(za1);
        const float aa2 = leaky(za2);
        const float ab0 = leaky(zb0);
        const float ab1 = leaky(zb1);
        const float ab2 = leaky(zb2);

        float* __restrict__ const hsA = hot.headSum + n * 3;
        float* __restrict__ const hsB = hsA + 3;

        hsA[0] += aa0; hsA[1] += aa1; hsA[2] += aa2;
        hsB[0] += ab0; hsB[1] += ab1; hsB[2] += ab2;

        float* __restrict__ const dstA = out + n * 3;
        float* __restrict__ const dstB = dstA + 3;

        dstA[0] = ax0 + b0 + aa0 * lx[0] + aa1 * lx[3] + aa2 * lx[6];
        dstA[1] = ax1 + b1 + aa0 * lx[1] + aa1 * lx[4] + aa2 * lx[7];
        dstA[2] = ax2 + b2 + aa0 * lx[2] + aa1 * lx[5] + aa2 * lx[8];

        dstB[0] = bx0 + b0 + ab0 * lx[0] + ab1 * lx[3] + ab2 * lx[6];
        dstB[1] = bx1 + b1 + ab0 * lx[1] + ab1 * lx[4] + ab2 * lx[7];
        dstB[2] = bx2 + b2 + ab0 * lx[2] + ab1 * lx[5] + ab2 * lx[8];

        wp = wpB + 1;
        if (wp >= cols)
            wp = 0;
    }

    st.layerWritePos[li] = wp;
}

NAM_A2_NOINLINE void process_head(A2HotState& hot,
                                  const A2SharedWeights& sw,
                                  float* output) noexcept
{
    int wp = hot.headWritePos;
    float* const hist = hot.headHistory;

    for (int n = 0; n < kBlockSize; ++n)
    {
        const float* const hs = hot.headSum + n * 3;
        float* const hcol = hist + wp * 3;

        hcol[0] = hs[0];
        hcol[1] = hs[1];
        hcol[2] = hs[2];

        float y = sw.headB;

        for (int tap = 0; tap < kHeadKernel; ++tap)
        {
            // kHeadKernel is a power of two (see static_assert below), so the
            // ring index wraps with a bitmask instead of a branch.
            const int lag = kHeadKernel - 1 - tap;
            const int col = (wp - lag) & (kHeadKernel - 1);

            const float* const s = hist + col * 3;
            y += sw.headW[tap][0] * s[0]
               + sw.headW[tap][1] * s[1]
               + sw.headW[tap][2] * s[2];
        }

        output[n] = y * sw.headScale;

        wp = (wp + 1) & (kHeadKernel - 1);
    }

    hot.headWritePos = wp;
}

NAM_A2_NOINLINE void process_block_48(A2State& st,
                                      A2HotState& hot,
                                      const A2SharedWeights& sw,
                                      const float* input,
                                      float* output) noexcept
{
    if (!sw.loaded || input == nullptr || output == nullptr)
    {
        if (output != nullptr)
            std::fill(output, output + kBlockSize, 0.0f);
        return;
    }

    float* in = hot.bufA;
    float* out = hot.bufB;

    const float r0 = sw.rechannel[0];
    const float r1 = sw.rechannel[1];
    const float r2 = sw.rechannel[2];

    for (int n = 0; n < kBlockSize; ++n)
    {
        const float x = input[n];
        float* const dst = in + n * 3;
        dst[0] = r0 * x;
        dst[1] = r1 * x;
        dst[2] = r2 * x;

        float* const hs = hot.headSum + n * 3;
        hs[0] = 0.0f;
        hs[1] = 0.0f;
        hs[2] = 0.0f;
    }

    for (int li = 0; li < kNumLayers; ++li)
    {
        if (kKernelSizes[li] == 6)
            process_layer_kernel6_dual(st, hot, sw, li, input, in, out);
        else
            process_layer_kernel<15>(st, hot, sw, li, input, in, out);

        float* tmp = in;
        in = out;
        out = tmp;
    }

    process_head(hot, sw, output);
}

inline void prewarm(A2State& st, A2HotState& hot, const A2SharedWeights& sw) noexcept
{
    reset_state(st, hot, sw);
    float zeros[kBlockSize] {};
    float discard[kBlockSize] {};
    int remaining = prewarm_samples();
    while (remaining > 0)
    {
        process_block_48(st, hot, sw, zeros, discard);
        remaining -= kBlockSize;
    }
}

// -----------------------------------------------------------------------------
// A2Player: high-level player class.
// Use load_weights() to load a model from model_data_nam_a2.h, then call
// process_block_48() from the audio callback.
// -----------------------------------------------------------------------------
class A2Player
{
public:
    bool load_weights(const float* weights, std::size_t count) noexcept
    {
        const bool ok = nam_a2_daisy::load_weights(weights_, weights, count);
        prewarm(state_, hot_, weights_); // prewarm() resets state internally
        return ok;
    }

    void reset() noexcept
    {
        if (weights_.loaded)
            prewarm(state_, hot_, weights_); // prewarm() resets state internally
        else
            reset_state(state_, hot_, weights_);
    }

    void process_block_48(const float* input, float* output) noexcept
    {
        nam_a2_daisy::process_block_48(state_, hot_, weights_, input, output);
    }

    bool is_loaded() const noexcept { return weights_.loaded; }

    A2SharedWeights& weights() noexcept { return weights_; }
    A2State&         state()   noexcept { return state_; }
    A2HotState&      hot_state() noexcept { return hot_; }

private:
    // weights_ and hot_ are static: shared across all A2Player instances.
    // Only one model can be active at a time (which matches hardware constraints).
    // Pinned to DTCMRAM (zero-wait) — these are touched every block.
    static inline A2SharedWeights weights_ NAM_A2_HOT_DATA;
    static inline A2HotState      hot_     NAM_A2_HOT_STATE_DATA;
    A2State state_;
};

} // namespace nam_a2_daisy
