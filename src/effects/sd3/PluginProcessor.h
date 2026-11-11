// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include <JuceHeader.h>
#include "SDD3000DSP.h"

class SDD3000AudioProcessor : public juce::AudioProcessor
{
public:
    static constexpr int kNumPrograms = 9;

    SDD3000AudioProcessor();
    ~SDD3000AudioProcessor() override = default;

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
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Programmer: 9 memories holding a delay time each. PROG selects + recalls, WRITE stores (message thread).
    void selectNextProgram();
    void writeProgram();
    int getProgramNumber() const { return (int) std::round (pProgram->load()); }

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> meterLevel { 0.0f };   // headroom meter: input peak after the Level knob (linear, decaying)

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void setIntParam (const char* id, int value);

    std::atomic<float>* pLevel, *pAttIn, *pAttOut, *pDelayMs, *pProgram, *pFeedback, *pInvFb, *pHold, *pFilter,
                       *pWaveform, *pIntensity, *pBalance, *pBypass, *pInvOut, *pPower;

    std::array<int, kNumPrograms> programTimes;

    double sr = 44100.0;
    sdd3000::Channel channels[2];

    juce::SmoothedValue<float> sDelayMs, sFeedback, sHoldIn, sInGain, sOutGain, sDry, sWet, sActive, sModDepth, sLpHz, sHpHz;
    float phase = 0.0f, meter = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SDD3000AudioProcessor)
};
