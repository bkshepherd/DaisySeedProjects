// Checks the real CMSIS-DSP real FFT (built with the firmware's table configuration, see run_tests.sh) through venus::CmsisRealFft against ShyFFT and a plain
// DFT: the same spectrum in ShyFFT's layout, an inverse that undoes it, and that the table configuration really includes the 4096-point FFT.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <vector>
#include "Venus/cmsis_fft.h"
#include "../../Util/STFT/shy_fft.h"

static int g_fails = 0;
static void check(bool ok, const char *what) { printf("  %-70s %s\n", what, ok ? "OK" : "FAIL"); if (!ok) g_fails++; }

int main() {
    constexpr size_t N = 4096;
    arm_rfft_fast_instance_f32 probe;
    check(arm_rfft_fast_init_f32(&probe, N) == ARM_MATH_SUCCESS, "the 4096-point real FFT is available in the firmware's table configuration");

    std::vector<float> x(N), a(N), b(N), spectrumShy(N), spectrumCms(N);
    unsigned s = 12345u;
    for (size_t i = 0; i < N; i++) { s = 1664525u * s + 1013904223u; x[i] = ((s >> 8) & 0xFFFF) / 32768.0f - 1.0f + 0.3f * std::sin(0.05f * i); }

    ShyFFT<float, N, RotationPhasor> shy; shy.Init();
    static venus::CmsisRealFft<N> cms; cms.Init();

    a = x; shy.Direct(a.data(), spectrumShy.data());
    b = x; cms.Direct(b.data(), spectrumCms.data());
    double maxAbs = 0, maxDiff = 0;
    for (size_t i = 0; i < N; i++) { maxAbs = std::fmax(maxAbs, std::fabs(spectrumShy[i])); maxDiff = std::fmax(maxDiff, std::fabs(spectrumShy[i] - spectrumCms[i])); }
    printf("  forward: max difference %.3g (largest value %.1f)\n", maxDiff, maxAbs);
    check(maxDiff < 1e-4 * maxAbs, "forward spectrum equals ShyFFT's (same layout)");

    // against a plain DFT for a few bins (layout: Re in [k], Nyquist in [N/2], -Im in [N/2 + k])
    bool dftOk = true;
    for (int k : {1, 2, 7, 100, 1023, 2047}) {
        double re = 0, im = 0;
        for (size_t n = 0; n < N; n++) { re += x[n] * std::cos(2 * M_PI * k * n / N); im -= x[n] * std::sin(2 * M_PI * k * n / N); }
        dftOk &= std::fabs(spectrumCms[k] - re) < 1e-2 && std::fabs(spectrumCms[N / 2 + k] + im) < 1e-2;
    }
    check(dftOk, "bins match a plain DFT (Re at [k], -Im at [N/2 + k])");

    std::vector<float> backShy(N), backCms(N);
    a = spectrumShy; shy.Inverse(a.data(), backShy.data());
    b = spectrumShy; cms.Inverse(b.data(), backCms.data());
    double m = 0, d = 0, rt = 0;
    for (size_t i = 0; i < N; i++) { m = std::fmax(m, std::fabs(backShy[i])); d = std::fmax(d, std::fabs(backShy[i] - backCms[i])); rt = std::fmax(rt, std::fabs(backCms[i] / N - x[i])); }
    printf("  inverse: max difference %.3g (largest value %.1f), round trip error %.3g\n", d, m, rt);
    check(d < 1e-4 * m, "inverse equals ShyFFT's (scaled by N)");
    check(rt < 1e-4, "inverse(forward(x)) / N gives x back");

    // informational: host timing (the ratio is only a rough guide; the Cortex-M7 differs)
    auto timeIt = [&](auto &&f) { const auto t0 = std::chrono::steady_clock::now(); for (int i = 0; i < 200; i++) f(); return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / 200; };
    const double tShy = timeIt([&] { a = x; shy.Direct(a.data(), spectrumShy.data()); });
    const double tCms = timeIt([&] { b = x; cms.Direct(b.data(), spectrumCms.data()); });
    printf("  host time per forward transform: ShyFFT %.4f ms, CMSIS %.4f ms\n", tShy, tCms);

    printf("\n%s\n", g_fails ? "CMSIS FFT: FAILURES" : "CMSIS FFT OK");
    return g_fails != 0;
}
