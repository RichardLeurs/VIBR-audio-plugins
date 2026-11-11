// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include <JuceHeader.h>
#include "OD2DSP.h"

class OD2AudioProcessor : public juce::AudioProcessor
{
public:
    OD2AudioProcessor();
    ~OD2AudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    static constexpr int kOsFactor = 2;            // 2^2 = 4x oversampling
    static constexpr int kMaxChannels = 2;

    std::atomic<float>* pType, *pParam, *pDrive, *pLevel, *pLow, *pMiddle, *pHigh;
    std::atomic<float>* pBoostOn, *pBoostPre, *pBoostPost, *pGateThresh, *pGateDecay;
    juce::AudioParameterBool* bypassParam = nullptr;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    od2::DriveChannel drive[kMaxChannels];
    od2::ToneStack tone[kMaxChannels];
    od2::Gate gate[kMaxChannels];

    // dry path, delayed by the oversampler's latency so bypass and the effect line up
    std::vector<float> dryBuf[kMaxChannels], dryOut[kMaxChannels];
    int dryPos = 0, latency = 0;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> gainSmooth { 1.0f };
    juce::SmoothedValue<float> paramSmooth { 0.5f }, lowSmooth { 5.0f }, midSmooth { 5.0f }, highSmooth { 5.0f };
    juce::SmoothedValue<float> levelSmooth { 1.0f }, wetSmooth { 1.0f };
    std::vector<float> gainOs, levelBlock, wetBlock;       // per-sample control signals for one block
    int activeType = 0;
    float typeFade = 1.0f;                                   // dips to 0 while the type changes
    double sr = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OD2AudioProcessor)
};
