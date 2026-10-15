// Startup-state audit of one effect module. Modes (argv[1]):
//   startup  - construct, Init() only (as on the pedal), then process a test signal
//   touched  - construct, Init(), then touch every parameter away from its default and straight back (which fires ParameterChanged for each), then process
//   poison   - like startup, but the object's memory is pre-filled with 0xFF bytes first, to expose reads of uninitialised members
// Writes the stereo output to argv[2]. A difference between startup and touched means state derived from parameters is not set up for the defaults.
#include EFFECT_HDR
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>
using namespace bkshepherd;

// Effects that fill tables with rand() (e.g. Spectral Delay's Random modes) would otherwise differ between runs only because the number of calls differs.
// Make rand() constant so the audit compares the effects' logic, not their random draws. The test signal uses its own generator below.
extern "C" int rand(void) { return RAND_MAX / 2; }
extern "C" void srand(unsigned) {}
static unsigned g_lcg = 12345u;
static float noise() { g_lcg = 1664525u * g_lcg + 1013904223u; return ((g_lcg >> 8) & 0xFFFF) / 32768.0f - 1.0f; }

static void touchOne(BaseEffectModule &m, int p) {
    {
        switch (m.GetParameterType(p)) {
        case ParameterValueType::Float: {
            const float def = m.GetParameterDefaultValueAsFloat(p);
            m.SetParameterAsMagnitude(p, 0.0f);
            if (m.GetParameterAsFloat(p) == def) m.SetParameterAsMagnitude(p, 1.0f);
            m.SetParameterAsFloat(p, def);
            break;
        }
        case ParameterValueType::Binned: {
            const uint32_t n = (uint32_t)m.GetParameterBinCount(p), def = m.GetParameterDefaultValueRaw(p);
            if (n > 1) { m.SetParameterRaw(p, (def + 1) % n); m.SetParameterRaw(p, def); }
            break;
        }
        case ParameterValueType::Bool: {
            const uint32_t def = m.GetParameterDefaultValueRaw(p);
            m.SetParameterRaw(p, def ? 0u : 1u);
            m.SetParameterRaw(p, def);
            break;
        }
        default: break; // Raw/unknown parameters are not touched
        }
    }
}

static void touchAll(BaseEffectModule &m) {
    for (int p = 0; p < m.GetParameterCount(); p++) touchOne(m, p);
}

int main(int argc, char **argv) {
    const std::string mode = argv[1];
    alignas(64) static unsigned char mem[sizeof(EFFECT_CLASS)];
    std::memset(mem, mode == "poison" ? 0xFF : 0x00, sizeof mem);
    EFFECT_CLASS *m = new (mem) EFFECT_CLASS();
    m->Init(48000.0f);
    if (mode == "list") {
        for (int p = 0; p < m->GetParameterCount(); p++) printf("%d %s\n", p, m->GetParameterName(p));
        return 0;
    }
    if (mode == "touched") touchAll(*m);
    if (mode.rfind("touch:", 0) == 0) touchOne(*m, atoi(mode.c_str() + 6));

    const size_t total = 48000, B = 48;
    std::vector<float> in(total), outL(total), outR(total);
    for (size_t i = 0; i < total; i++) {
        float burst = (i < 9600) ? 0.3f * noise() : 0.0f;
        in[i] = burst + 0.25f * std::sin(2.0f * 3.14159265f * 196.0f * i / 48000.0f) * (i < 24000 ? 1.0f : 0.3f);
    }
    for (size_t o = 0; o < total; o += B) m->ProcessStereoBlock(&in[o], &in[o], &outL[o], &outR[o], B);
    FILE *f = fopen(argv[2], "wb");
    fwrite(outL.data(), 4, total, f); fwrite(outR.data(), 4, total, f);
    fclose(f);
    return 0;
}
