#ifndef BIQUAD2
#define BIQUAD2

#include <vector>
using namespace std;

namespace AudioLib {
class Biquad2 {
  public:
    enum class FilterType { LowPass = 0, HighPass, BandPass, Notch, Peak, LowShelf, HighShelf };

  private:
    float samplerate = 0.0f;
    float _gainDb = 0.0f;
    float _q = 0.0f;

    float a0 = 0.0f;
    float a1 = 0.0f;
    float a2 = 0.0f;

    float b0 = 0.0f;
    float b1 = 0.0f;
    float b2 = 0.0f;

    float x1 = 0.0f;
    float x2 = 0.0f;
    float y = 0.0f;
    float y1 = 0.0f;
    float y2 = 0.0f;

    float gain = 0.0f;

  public:
    FilterType Type;
    float Output = 0.0f;
    float Frequency = 0.0f;
    float Slope = 0.0f;

    Biquad2();
    Biquad2(FilterType filterType, float samplerate);
    ~Biquad2();

    float GetSamplerate();
    void SetSamplerate(float samplerate);
    float GetGainDb();
    void SetGainDb(float value);
    float GetGain();
    void SetGain(float value);
    float GetQ();
    void SetQ(float value);
    vector<float> GetA();
    vector<float> GetB();

    void Update();
    float GetResponse(float freq);

    float inline Process(float x) {
        y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1;
        y2 = y1;
        x1 = x;
        y1 = y;

        Output = y;
        return Output;
    }

    void inline Process(float *input, float *output, int len) {
        for (int i = 0; i < len; i++)
            output[i] = Process(input[i]);
    }

    void ClearBuffers();
};
} // namespace AudioLib

#endif