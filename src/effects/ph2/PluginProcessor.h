// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include <JuceHeader.h>
#include "PH2DSP.h"

class PH2AudioProcessor : public juce::AudioProcessor
{
public:
    PH2AudioProcessor();
    ~PH2AudioProcessor() override = default;

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
    double getTailLengthSeconds() const override { return 0.2; }

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

    static constexpr int kMaxChannels = 2;

    std::atomic<float>* pManual, *pRes, *pDepth, *pRate, *pMode;
    juce::AudioParameterBool* bypassParam = nullptr;

    ph2::PhaserChannel phaser[kMaxChannels];
    juce::SmoothedValue<float> resSmooth { 0.5f }, depthSmooth { 0.5f }, rateSmooth { 0.5f }, wetSmooth { 1.0f };
    juce::SmoothedValue<float> modeSmooth { 0.0f };     // crossfades the 4 / 8 stage outputs
    double sr = 44100.0;
    double phase = 0.0;                                  // LFO phase 0..1

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PH2AudioProcessor)
};
