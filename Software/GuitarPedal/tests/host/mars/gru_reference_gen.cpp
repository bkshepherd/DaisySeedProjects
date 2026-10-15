// Generates tests/host/mars/gru_reference.bin: the output of the original Mars network (RTNeural, Eigen backend; models 2 to 4 of the FunBox Mars collection) for a fixed input, which
// test_mars.cpp compares the module's own GRU to. Only needed to regenerate the file; FUNBOX is a checkout of https://github.com/GuitarML/Funbox:
//   g++ -std=c++17 -O1 -DRTNEURAL_DEFAULT_ALIGNMENT=8 -DRTNEURAL_NO_DEBUG=1 -DRTNEURAL_USE_EIGEN=1 -I$FUNBOX/RTNeural -I$FUNBOX/RTNeural/modules/Eigen -I$FUNBOX/software/Mars \
//       tests/host/mars/gru_reference_gen.cpp -o /tmp/gru_gen && /tmp/gru_gen tests/host/mars/gru_reference.bin
#include <RTNeural/RTNeural.h>
#include <cmath>
#include <cstdio>
#include <vector>
#include "all_model_data_gru9_4count.h"

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    setupWeights();
    constexpr int kN = 4800;
    std::vector<float> in(kN);
    unsigned s = 12345;
    for (int i = 0; i < kN; i++) { // a sine and noise, 0.8 peak
        s = 1664525u * s + 1013904223u;
        const float noise = ((s >> 8) & 0xFFFF) / 32768.0f - 1.0f;
        in[i] = 0.5f * std::sin(0.05f * i) * (0.4f + 0.6f * std::sin(0.001f * i)) + 0.3f * noise * (i > 2400 ? 1.0f : 0.1f);
    }
    FILE *f = fopen(argv[1], "wb");
    fwrite(in.data(), sizeof(float), kN, f);
    for (int m = 1; m <= 3; m++) { // model_collection[1..3] are what Mars uses
        RTNeural::ModelT<float, 1, 1, RTNeural::GRULayerT<float, 1, 9>, RTNeural::DenseT<float, 9, 1>> model;
        auto &gru = model.template get<0>();
        auto &dense = model.template get<1>();
        gru.setWVals(model_collection[m].rec_weight_ih_l0);
        gru.setUVals(model_collection[m].rec_weight_hh_l0);
        gru.setBVals(model_collection[m].rec_bias);
        dense.setWeights(model_collection[m].lin_weight);
        dense.setBias(model_collection[m].lin_bias.data());
        model.reset();
        std::vector<float> out(kN);
        for (int i = 0; i < kN; i++) { float x[1] = {in[i]}; out[i] = model.forward(x); }
        fwrite(out.data(), sizeof(float), kN, f);
    }
    fclose(f);
    return 0;
}
