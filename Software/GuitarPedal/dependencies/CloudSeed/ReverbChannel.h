
#ifndef REVERBCHANNEL
#define REVERBCHANNEL

#include "AllpassDiffuser.h"
#include "AudioLib/Hp1.h"
#include "AudioLib/Lp1.h"
#include "AudioLib/ShaRandom.h"
#include "DelayLine.h"
#include "ModulatedDelay.h"
#include "MultitapDiffuser.h"
#include "Parameter.h"
#include <map>
#include <memory>

#include "AudioLib/ShaRandom.h"
#include "ReverbChannel.h"
#include "Utils.h"
#include <cmath>

extern void *custom_pool_allocate(size_t size);

using namespace std;

namespace CloudSeed {
enum class ChannelLR { Left, Right };

class ReverbChannel {
  private:
    // IMPORTANT: CHANGE "TotalLineCount" FOR DAISY SEED HARDWARE
    //            Original CloudSeed plugin uses 8 Delay Lines, or 12 delay lines?
    //            DaisyCloudSeed adjusted to 2 to use with Stereo on DaisyPatch hardware (otherwise causes buffer underruns for most
    //            presets (except ChorusDelay) 4/26/2023 GuitarML fork of DaisyCloudSeed uses 4, able to increase for Mono Only
    //            Terrarium platform (mono guitar pedal using Daisy Seed)
    static const int TotalLineCount = 3;

    map<Parameter2, float> parameters;
    int samplerate;
    int bufferSize;

    ModulatedDelay preDelay;
    MultitapDiffuser multitap;
    AllpassDiffuser diffuser;
    vector<DelayLine *> lines;
    AudioLib::ShaRandom rand;
    AudioLib::Hp1 highPass;
    AudioLib::Lp1 lowPass;
    float *tempBuffer;
    float *lineOutBuffer;
    float *outBuffer;
    int delayLineSeed;
    int postDiffusionSeed;

    // Used the the main process loop
    int lineCount;

    bool highPassEnabled;
    bool lowPassEnabled;
    bool diffuserEnabled;
    float dryOut;
    float predelayOut;
    float earlyOut;
    float lineOut;
    float crossSeed;
    ChannelLR channelLr;

  public:
    ReverbChannel(int bufferSize, int samplerate, ChannelLR leftOrRight)
        : preDelay(bufferSize, (int)(samplerate * 1.0), 100) // 1 second delay buffer
          ,
          multitap(samplerate) // use samplerate = 1 second delay buffer
          ,
          highPass(samplerate), lowPass(samplerate), diffuser(samplerate, 150) // 150ms buffer, to allow for 100ms + modulation time
    {
        this->channelLr = leftOrRight;

        for (int i = 0; i < TotalLineCount; i++)
            lines.push_back(new (custom_pool_allocate(sizeof(DelayLine))) DelayLine(bufferSize, samplerate));

        this->bufferSize = bufferSize;

        for (auto value = 0; value < (int)Parameter2::Count; value++)
            this->parameters[static_cast<Parameter2>(value)] = 0.0;

        crossSeed = 0.0;
        lineCount = TotalLineCount;
        diffuser.SetInterpolationEnabled(true);
        highPass.SetCutoffHz(20);
        lowPass.SetCutoffHz(20000);

        tempBuffer = new (custom_pool_allocate(sizeof(float) * bufferSize)) float[bufferSize];
        lineOutBuffer = new (custom_pool_allocate(sizeof(float) * bufferSize)) float[bufferSize];
        outBuffer = new (custom_pool_allocate(sizeof(float) * bufferSize)) float[bufferSize];

        this->samplerate = samplerate;
    }

    ~ReverbChannel() {
        for (auto line : lines)
            delete line;

        delete tempBuffer;
        delete lineOutBuffer;
        delete outBuffer;
    }

    int GetSamplerate() { return samplerate; }

    void SetSamplerate(int samplerate) {
        this->samplerate = samplerate;
        highPass.SetSamplerate(samplerate);
        lowPass.SetSamplerate(samplerate);

        for (int i = 0; i < lines.size(); i++) {
            lines[i]->SetSamplerate(samplerate);
        }

        auto update = [&](Parameter2 p) { SetParameter(p, parameters[p]); };
        update(Parameter2::PreDelay);
        update(Parameter2::TapLength);
        update(Parameter2::DiffusionDelay);
        update(Parameter2::LineDelay);
        update(Parameter2::LateDiffusionDelay);
        update(Parameter2::EarlyDiffusionModRate);
        update(Parameter2::LineModRate);
        update(Parameter2::LateDiffusionModRate);
        update(Parameter2::LineModAmount);
        UpdateLines();
    }

    float *GetOutput() { return outBuffer; }

    float *GetLineOutput() { return lineOutBuffer; }

    void SetParameter(Parameter2 para, float value) {
        parameters[para] = value;

        switch (para) {
        case Parameter2::PreDelay:
            preDelay.SampleDelay = (int)Ms2Samples(value);
            break;
        case Parameter2::HighPass:
            highPass.SetCutoffHz(value);
            break;
        case Parameter2::LowPass:
            lowPass.SetCutoffHz(value);
            break;

        case Parameter2::TapCount:
            multitap.SetTapCount((int)value);
            break;
        case Parameter2::TapLength:
            multitap.SetTapLength((int)Ms2Samples(value));
            break;
        case Parameter2::TapGain:
            multitap.SetTapGain(value);
            break;
        case Parameter2::TapDecay:
            multitap.SetTapDecay(value);
            break;

        case Parameter2::DiffusionEnabled: {
            auto newVal = value >= 0.5;
            if (newVal != diffuserEnabled)
                diffuser.ClearBuffers();
            diffuserEnabled = newVal;
            break;
        }
        case Parameter2::DiffusionStages:
            diffuser.Stages = (int)value;
            break;
        case Parameter2::DiffusionDelay:
            diffuser.SetDelay((int)Ms2Samples(value));
            break;
        case Parameter2::DiffusionFeedback:
            diffuser.SetFeedback(value);
            break;

        case Parameter2::LineCount:
            lineCount = (int)value; // Originally commented out
                                    // perLineGain = GetPerLineGain();  // In original Cloud Seed
            break;
        case Parameter2::LineDelay:
            UpdateLines();
            break;
        case Parameter2::LineDecay:
            UpdateLines();
            break;

        case Parameter2::LateDiffusionEnabled:
            for (auto line : lines) {
                auto newVal = value >= 0.5;
                if (newVal != line->DiffuserEnabled)
                    line->ClearDiffuserBuffer();
                line->DiffuserEnabled = newVal;
            }
            break;
        case Parameter2::LateDiffusionStages:
            for (auto line : lines)
                line->SetDiffuserStages((int)value);
            break;
        case Parameter2::LateDiffusionDelay:
            for (auto line : lines)
                line->SetDiffuserDelay((int)Ms2Samples(value));
            break;
        case Parameter2::LateDiffusionFeedback:
            for (auto line : lines)
                line->SetDiffuserFeedback(value);
            break;

        case Parameter2::PostLowShelfGain:
            for (auto line : lines)
                line->SetLowShelfGain(value);
            break;
        case Parameter2::PostLowShelfFrequency:
            for (auto line : lines)
                line->SetLowShelfFrequency(value);
            break;
        case Parameter2::PostHighShelfGain:
            for (auto line : lines)
                line->SetHighShelfGain(value);
            break;
        case Parameter2::PostHighShelfFrequency:
            for (auto line : lines)
                line->SetHighShelfFrequency(value);
            break;
        case Parameter2::PostCutoffFrequency:
            for (auto line : lines)
                line->SetCutoffFrequency(value);
            break;

        case Parameter2::EarlyDiffusionModAmount:
            diffuser.SetModulationEnabled(value > 0.0);
            diffuser.SetModAmount(Ms2Samples(value));
            break;
        case Parameter2::EarlyDiffusionModRate:
            diffuser.SetModRate(value);
            break;
        case Parameter2::LineModAmount:
            UpdateLines();
            break;
        case Parameter2::LineModRate:
            UpdateLines();
            break;
        case Parameter2::LateDiffusionModAmount:
            UpdateLines();
            break;
        case Parameter2::LateDiffusionModRate:
            UpdateLines();
            break;

        case Parameter2::TapSeed:
            multitap.SetSeed((int)value);
            break;
        case Parameter2::DiffusionSeed:
            diffuser.SetSeed((int)value);
            break;
        case Parameter2::DelaySeed:
            delayLineSeed = (int)value;
            UpdateLines();
            break;
        case Parameter2::PostDiffusionSeed:
            postDiffusionSeed = (int)value;
            UpdatePostDiffusion();
            break;

        case Parameter2::CrossSeed:

            crossSeed = channelLr == ChannelLR::Right ? value : 0;
            multitap.SetCrossSeed(value);
            diffuser.SetCrossSeed(value);
            UpdateLines();
            UpdatePostDiffusion();
            break;

        case Parameter2::DryOut:
            dryOut = value;
            break;
        case Parameter2::PredelayOut:
            predelayOut = value;
            break;
        case Parameter2::EarlyOut:
            earlyOut = value;
            break;
        case Parameter2::MainOut:
            lineOut = value;
            break;

        case Parameter2::HiPassEnabled:
            highPassEnabled = value >= 0.5;
            break;
        case Parameter2::LowPassEnabled:
            lowPassEnabled = value >= 0.5;
            break;
        case Parameter2::LowShelfEnabled:
            for (auto line : lines)
                line->LowShelfEnabled = value >= 0.5;
            break;
        case Parameter2::HighShelfEnabled:
            for (auto line : lines)
                line->HighShelfEnabled = value >= 0.5;
            break;
        case Parameter2::CutoffEnabled:
            for (auto line : lines)
                line->CutoffEnabled = value >= 0.5;
            break;
        case Parameter2::LateStageTap:
            for (auto line : lines)
                line->LateStageTap = value >= 0.5;
            break;

        case Parameter2::Interpolation:
            for (auto line : lines)
                line->SetInterpolationEnabled(value >= 0.5);
            break;
        }
    }

    void Process(float *input, int sampleCount) {
        int len = sampleCount;
        auto predelayOutput = preDelay.GetOutput();
        auto lowPassInput = highPassEnabled ? tempBuffer : input;

        if (highPassEnabled)
            highPass.Process(input, tempBuffer, len);
        if (lowPassEnabled)
            lowPass.Process(lowPassInput, tempBuffer, len);
        if (!lowPassEnabled && !highPassEnabled)
            Utils::Copy(input, tempBuffer, len);

        // completely zero if no input present
        // Previously, the very small values were causing some really strange CPU spikes
        for (int i = 0; i < len; i++) {
            auto n = tempBuffer[i];
            if (n * n < 0.000000001)
                tempBuffer[i] = 0;
        }

        preDelay.Process(tempBuffer, len);
        multitap.Process(preDelay.GetOutput(), len);

        auto earlyOutStage = diffuserEnabled ? diffuser.GetOutput() : multitap.GetOutput();

        if (diffuserEnabled) {
            diffuser.Process(multitap.GetOutput(), len);
            Utils::Copy(diffuser.GetOutput(), tempBuffer, len);
        } else {
            Utils::Copy(multitap.GetOutput(), tempBuffer, len);
        }

        // mix in the feedback from the other channel
        // for (int i = 0; i < len; i++)
        //	tempBuffer[i] += crossMix[i];

        for (int i = 0; i < lineCount; i++)
            lines[i]->Process(tempBuffer, len);

        for (int i = 0; i < lineCount; i++) {
            auto buf = lines[i]->GetOutput();

            if (i == 0) {
                for (int j = 0; j < len; j++)
                    tempBuffer[j] = buf[j];
            } else {
                for (int j = 0; j < len; j++)
                    tempBuffer[j] += buf[j];
            }
        }

        auto perLineGain = GetPerLineGain();
        Utils::Gain(tempBuffer, perLineGain, len);
        Utils::Copy(tempBuffer, lineOutBuffer, len);

        for (int i = 0; i < len; i++) {
            outBuffer[i] = dryOut * input[i] + predelayOut * predelayOutput[i] + earlyOut * earlyOutStage[i] + lineOut * tempBuffer[i];
        }
    }

    void ClearBuffers() {
        for (int i = 0; i < bufferSize; i++) {
            tempBuffer[i] = 0.0;
            lineOutBuffer[i] = 0.0;
            outBuffer[i] = 0.0;
        }

        lowPass.Output = 0;
        highPass.Output = 0;

        preDelay.ClearBuffers();
        multitap.ClearBuffers();
        diffuser.ClearBuffers();
        for (auto line : lines)
            line->ClearBuffers();
    }

  private:
    float GetPerLineGain() { return 1.0 / std::sqrt(lineCount); }

    void UpdateLines() {
        auto lineDelaySamples = (int)Ms2Samples(parameters[Parameter2::LineDelay]);
        auto lineDecayMillis = parameters[Parameter2::LineDecay] * 1000;
        auto lineDecaySamples = Ms2Samples(lineDecayMillis);

        auto lineModAmount = Ms2Samples(parameters[Parameter2::LineModAmount]);
        auto lineModRate = parameters[Parameter2::LineModRate];

        auto lateDiffusionModAmount = Ms2Samples(parameters[Parameter2::LateDiffusionModAmount]);
        auto lateDiffusionModRate = parameters[Parameter2::LateDiffusionModRate];

        auto delayLineSeeds = ShaRandom::Generate(delayLineSeed, (int)lines.size() * 3, crossSeed);
        int count = (int)lines.size();

        for (int i = 0; i < count; i++) {
            auto modAmount = lineModAmount * (0.7 + 0.3 * delayLineSeeds[i + count]);
            auto modRate = lineModRate * (0.7 + 0.3 * delayLineSeeds[i + 2 * count]) / samplerate;

            auto delaySamples = (0.5 + 1.0 * delayLineSeeds[i]) * lineDelaySamples;
            if (delaySamples < modAmount + 2) // when the delay is set really short, and the modulation is very high
                delaySamples = modAmount + 2; // the mod could actually take the delay time negative, prevent that! -- provide 2 extra
                                              // sample as margin of safety

            auto dbAfter1Iteration = delaySamples / lineDecaySamples * (-60); // lineDecay is the time it takes to reach T60
            auto gainAfter1Iteration = Utils::DB2gain(dbAfter1Iteration);

            lines[i]->SetDelay((int)delaySamples);
            lines[i]->SetFeedback(gainAfter1Iteration);
            lines[i]->SetLineModAmount(modAmount);
            lines[i]->SetLineModRate(modRate);
            lines[i]->SetDiffuserModAmount(lateDiffusionModAmount);
            lines[i]->SetDiffuserModRate(lateDiffusionModRate);
        }
    }

    void UpdatePostDiffusion() {
        for (int i = 0; i < lines.size(); i++)
            lines[i]->SetDiffuserSeed(((long long)postDiffusionSeed) * (i + 1), crossSeed);
    }

    float Ms2Samples(float value) { return value / 1000.0 * samplerate; }
};

} // namespace CloudSeed

#endif
