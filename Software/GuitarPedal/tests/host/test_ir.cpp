// IR module: block path vs per-sample path. The direct block path removes the per-sample path's one block of latency.
#include "ir_module.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace bkshepherd;
static constexpr size_t kB = 48, kN = 400;

static std::vector<float> makeIn() {
    std::vector<float> v(kB * kN); srand(7);
    for (auto &x : v) x = ((rand() / (float)RAND_MAX) - 0.5f) * 1.6f;
    return v;
}
static IrModule *mk(int irBin) {
    IrModule *m = new IrModule(); m->Init(48000.0f);
    m->SetParameterRaw(IrModule::IR, irBin); m->SetParameterAsMagnitude(IrModule::LEVEL, 0.7f);
    return m;
}
static std::vector<float> perSample(const std::vector<float> &in, int bin) {
    IrModule *m = mk(bin); std::vector<float> out(in.size());
    for (size_t i = 0; i < in.size(); i++) { m->ProcessMono(in[i]); out[i] = m->GetAudioLeft(); }
    delete m; return out;
}
// chunks: sizes cycled through, e.g. {48} or {17,31,48,5}
static std::vector<float> blocks(const std::vector<float> &in, int bin, std::vector<size_t> chunks, std::vector<float> *outR = nullptr) {
    IrModule *m = mk(bin); std::vector<float> out(in.size()), r(in.size());
    size_t pos = 0, c = 0;
    while (pos < in.size()) {
        size_t n = std::min(chunks[c++ % chunks.size()], in.size() - pos);
        m->ProcessMonoBlock(&in[pos], &out[pos], &r[pos], n);
        pos += n;
    }
    if (outR) *outR = r;
    delete m; return out;
}
static double maxdiff(const std::vector<float> &a, size_t offA, const std::vector<float> &b, size_t offB, size_t n) {
    double d = 0; for (size_t i = 0; i < n; i++) d = std::max<double>(d, fabs(a[offA + i] - b[offB + i])); return d;
}
int main() {
    int fails = 0; auto in = makeIn();
    for (int bin = 0; bin <= 1; bin++) {
        auto ps = perSample(in, bin);
        auto b48 = blocks(in, bin, {48});
        // per-sample output is the block output delayed by exactly one block (48 samples)
        double d1 = maxdiff(ps, kB, b48, 0, in.size() - kB);
        double peak = 0; for (float x : b48) peak = std::max<double>(peak, fabs(x));
        bool ok1 = d1 == 0.0 && peak > 0.01;
        printf("IR bin %d: aligned 48-blocks == per-sample delayed by 48 samples: maxDiff=%g peak=%.3f %s\n", bin, d1, peak, ok1 ? "OK" : "MISMATCH");
        fails += !ok1;
        // Chunks of <= 48 samples all take the direct path, and a streaming FIR is block-size independent
        auto odd = blocks(in, bin, {17, 31, 5, 48, 9});
        double d2 = maxdiff(b48, 0, odd, 0, in.size());
        bool ok2 = d2 == 0.0;
        printf("IR bin %d: odd chunk sizes <=48 == aligned 48-blocks: maxDiff=%g %s\n", bin, d2, ok2 ? "OK" : "MISMATCH");
        fails += !ok2;
        // Chunks of 60 never realign to 48, so they stay on the per-sample fallback and must equal the per-sample path exactly
        auto big = blocks(in, bin, {60});
        double d4 = maxdiff(ps, 0, big, 0, in.size());
        bool ok4 = d4 == 0.0;
        printf("IR bin %d: oversized chunks (fallback) == per-sample: maxDiff=%g %s\n", bin, d4, ok4 ? "OK" : "MISMATCH");
        fails += !ok4;
        // Switching from the direct path to the fallback mid-stream must not replay stale audio: output must stay bounded
        auto mix = blocks(in, bin, {48, 48, 60, 60, 60, 60, 48});
        double mpeak = 0; for (float x : mix) mpeak = std::max<double>(mpeak, fabs(x));
        bool ok5 = mpeak < peak * 1.5 + 1e-6;
        printf("IR bin %d: direct->fallback switch stays bounded (peak %.3f vs %.3f): %s\n", bin, mpeak, peak, ok5 ? "OK" : "SUSPICIOUS");
        fails += !ok5;
        // Mono output must be duplicated to the right channel
        std::vector<float> r; auto l = blocks(in, bin, {48}, &r);
        bool ok3 = maxdiff(l, 0, r, 0, l.size()) == 0.0;
        printf("IR bin %d: right channel == left channel: %s\n", bin, ok3 ? "OK" : "MISMATCH");
        fails += !ok3;
    }
    printf(fails ? "FAILURES: %d\n" : "ALL OK\n", fails);
    return fails != 0;
}
