// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include <JuceHeader.h>
#include "EH2DSP.h"

class EH2AudioProcessor : public juce::AudioProcessor
{
public:
    EH2AudioProcessor();
    ~EH2AudioProcessor() override = default;

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
    float getOpen() const { return openMeter.load(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    static constexpr int kMaxChannels = 2;

    std::atomic<float>* pMix = nullptr, *pFreq = nullptr, *pSens = nullptr;
    juce::AudioParameterBool* bypassParam = nullptr;

    eh2::HighPass hp[kMaxChannels], hp2[kMaxChannels];
    juce::SmoothedValue<float> mixSmooth { 0.0f }, freqSmooth { 5.0f }, wetSmooth { 1.0f };
    double sr = 44100.0;
    float envDb = -120.0f, open = 0.0f;                   // detector envelope (dB), smoothed gate (0..1)
    std::atomic<float> openMeter { 0.0f };                // read by the editor for the SENS lamp

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EH2AudioProcessor)
};
