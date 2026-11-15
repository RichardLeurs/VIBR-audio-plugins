// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include <JuceHeader.h>
#include "SL2DSP.h"

class SL2AudioProcessor : public juce::AudioProcessor
{
public:
    SL2AudioProcessor();
    ~SL2AudioProcessor() override = default;

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

    static constexpr int kMaxChannels = 2;

    std::atomic<float>* pBalance, *pTempo, *pAttack, *pDuty, *pVariation, *pMode, *pHostSync;
    juce::AudioParameterBool* bypassParam = nullptr;

    juce::SmoothedValue<float> balanceSmooth { 0.5f }, wetSmooth { 1.0f };
    sl2::Svf svf[kMaxChannels];
    float lowBand[kMaxChannels] {};                      // harmonic mode crossover state
    float envA = 0.0f, envB = 0.0f, envTa = 1.0f, envTb = 1.0f;     // slice envelopes (pattern, gaps; smooth versions)
    double stepPos = 0.0;                                // free-running position in 16th notes
    double sr = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SL2AudioProcessor)
};
