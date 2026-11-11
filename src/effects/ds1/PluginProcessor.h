// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include <JuceHeader.h>
#include "DS1DSP.h"

class DS1AudioProcessor : public juce::AudioProcessor
{
public:
    DS1AudioProcessor();
    ~DS1AudioProcessor() override = default;

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

    std::atomic<float>* pTone, *pLevel, *pDist;
    juce::AudioParameterBool* bypassParam = nullptr;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    ds1::DistChannel dist[kMaxChannels];
    ds1::ToneChannel tone[kMaxChannels];

    // dry path, delayed by the oversampler's latency so bypass and the effect line up
    std::vector<float> dryBuf[kMaxChannels], dryOut[kMaxChannels];
    int dryPos = 0, latency = 0;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> gainSmooth { 1.4f };
    juce::SmoothedValue<float> toneSmooth { 0.5f }, levelSmooth { 0.5f }, wetSmooth { 1.0f };
    std::vector<float> gainOs, toneBlock, levelBlock, wetBlock;   // per-sample control signals for one block
    double sr = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DS1AudioProcessor)
};
