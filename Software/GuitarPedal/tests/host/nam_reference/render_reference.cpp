// Renders a float32 mono file through the NeuralAmpModelerCore reference: render_reference <model.nam> <in.f32> <out.f32>
#include "NAM/dsp.h"
#include "NAM/get_dsp.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <vector>
int main(int argc, char **argv) {
    auto model = nam::get_dsp(std::filesystem::path(argv[1]));
    if (!model) { fprintf(stderr, "cannot load %s\n", argv[1]); return 1; }
    std::vector<float> in;
    { FILE *f = fopen(argv[2], "rb"); float x; while (fread(&x, 4, 1, f) == 1) in.push_back(x); fclose(f); }
    const int block = 48;
    model->Reset(48000.0, block);
    std::vector<NAM_SAMPLE> bi(block), bo(block);
    std::vector<float> out(in.size());
    for (size_t o = 0; o < in.size(); o += block) {
        size_t n = std::min<size_t>(block, in.size() - o);
        for (size_t i = 0; i < n; i++) bi[i] = in[o + i];
        NAM_SAMPLE *ip = bi.data(), *op = bo.data();
        model->process(&ip, &op, (int)n);
        for (size_t i = 0; i < n; i++) out[o + i] = (float)bo[i];
    }
    FILE *f = fopen(argv[3], "wb"); fwrite(out.data(), 4, out.size(), f); fclose(f);
    return 0;
}
