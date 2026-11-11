// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include <JuceHeader.h>
#include "SDD1000DSP.h"

class SDD1000AudioProcessor : public juce::AudioProcessor
{
public:
    SDD1000AudioProcessor();
    ~SDD1000AudioProcessor() override = default;

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

    juce::AudioProcessorValueTreeState apvts;

    std::atomic<float> meterLevel { 0.0f };   // headroom meter: input peak after the Level knob (linear, decaying)

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::atomic<float>* pLevel, *pTime, *pFactor, *pFeedback, *pHiCut, *pModInt, *pModFreq, *pDirect, *pEffect,
                       *pHold, *pBypass, *pPower;

    double sr = 44100.0;
    sdd1000::Channel channels[2];

    juce::SmoothedValue<float> sDelayMs, sFeedback, sHoldIn, sInGain, sDirect, sEffect, sActive, sModInt, sModRate, sCutHz;
    float phase = 0.0f, meter = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SDD1000AudioProcessor)
};
