// Generates tests/host/mercury/wavenet_reference.bin: the output of the original Mercury network (RTNeural-NAM on RTNeural, Eigen backend, with the NAM maths provider the pedal used) for
// a fixed input, for each of the nine models, which test_mercury.cpp compares the module's own network to. Only needed to regenerate the file; FUNBOX is a checkout of
// https://github.com/GuitarML/Funbox:
//   g++ -std=c++20 -O1 -DRTNEURAL_DEFAULT_ALIGNMENT=8 -DRTNEURAL_NO_DEBUG=1 -DRTNEURAL_USE_EIGEN=1 -I$FUNBOX/RTNeural -I$FUNBOX/RTNeural/modules/Eigen -I$FUNBOX/software/Mercury \
//       tests/host/mercury/wavenet_reference_gen.cpp -o /tmp/wavenet_gen && /tmp/wavenet_gen tests/host/mercury/wavenet_reference.bin
#include <RTNeural/RTNeural.h>
#include <cmath>
#include <cstdio>
#include <vector>
#include "wavenet/wavenet_model.hpp"
#include "model_data_nam.h"

struct NAMMathsProvider {
    template <typename Matrix>
    static auto tanh(const Matrix &x) {
        const auto x_poly = x.array() * (1.0f + 0.183428244899f * x.array().square());
        return x_poly.array() * (x_poly.array().square() + 1.0f).array().rsqrt();
    }
};

using Dilations = wavenet::Dilations<1, 2, 4, 8, 16, 32, 64>;
using Dilations2 = wavenet::Dilations<128, 256, 512, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512>;
static wavenet::Wavenet_Model<float, 1, wavenet::Layer_Array<float, 1, 1, 2, 2, 3, Dilations, false, NAMMathsProvider>,
                              wavenet::Layer_Array<float, 2, 1, 1, 2, 3, Dilations2, true, NAMMathsProvider>> model;

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    setupWeightsNam();
    constexpr int kN = 1000;
    std::vector<float> in(kN);
    unsigned s = 777;
    for (int i = 0; i < kN; i++) { // a sine and noise
        s = 1664525u * s + 1013904223u;
        const float noise = ((s >> 8) & 0xFFFF) / 32768.0f - 1.0f;
        in[i] = 0.4f * std::sin(0.07f * i) * (0.5f + 0.5f * std::sin(0.004f * i)) + 0.15f * noise;
    }
    FILE *f = fopen(argv[1], "wb");
    fwrite(in.data(), sizeof(float), kN, f);
    for (int m = 0; m < 9; m++) {
        model.load_weights(model_collection_nam[m].weights);
        model.prepare(1);
        model.prewarm();
        std::vector<float> out(kN);
        for (int i = 0; i < kN; i++) out[i] = model.forward(in[i]);
        fwrite(out.data(), sizeof(float), kN, f);
    }
    fclose(f);
    return 0;
}
