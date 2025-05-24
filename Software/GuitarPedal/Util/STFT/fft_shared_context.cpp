#include "fft_shared_context.h"
#include "daisy.h"

// buffers for STFT processing (private to this translation unit)
DSY_SDRAM_BSS float in[kBufferSize];
DSY_SDRAM_BSS float middle[kBufferSize];
DSY_SDRAM_BSS float out[kBufferSize];

FFTSharedContext::FFTSharedContext()
    : sample_rate_(48000.0f), initialized_(false), user_data_(nullptr),
      hann_([](float phase) -> float { return 0.5f * (1.0f - std::cos(2.0f * M_PI * phase)); }), stft_(nullptr) {}

FFTSharedContext &FFTSharedContext::Instance() {
    static FFTSharedContext instance;
    return instance;
}

void FFTSharedContext::Init(float sample_rate, FFTProcessCallback processor) {
    if (initialized_)
        return;

    sample_rate_ = sample_rate;

    fft_.Init();
    stft_ = new Fourier<float, kFFTSize>(processor, &fft_, &hann_, kLaps, in, middle, out);
    stft_->processor = processor;

    memset(in, 0, sizeof(in));
    memset(middle, 0, sizeof(middle));
    memset(out, 0, sizeof(out));

    initialized_ = true;
}

Fourier<float, kFFTSize> *FFTSharedContext::GetSTFT() { return stft_; }
ShyFFT<float, kFFTSize, RotationPhasor> *FFTSharedContext::GetFFT() { return &fft_; }

float *FFTSharedContext::GetInputBuffer() { return in; }
float *FFTSharedContext::GetMiddleBuffer() { return middle; }
float *FFTSharedContext::GetOutputBuffer() { return out; }

void FFTSharedContext::SetUserData(void *ptr) { user_data_ = ptr; }
