// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "BypassLink.h"
#include "TempoSync.h"
#include "MidiMap.h"

#include <JuceHeader.h>
#include "CE1DSP.h"

class CE1AudioProcessor : public juce::AudioProcessor
{
public:
    CE1AudioProcessor();
    ~CE1AudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.1; }
    juce::AudioProcessorParameter* getBypassParameter() const override { return apvts.getParameter ("bypass"); }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    vibr::BypassLink bypassLink { apvts };                // host bypass <-> OFF / ON switch
    MidiMap midiMap { apvts, { "level", "chorusIntensity", "vibratoDepth", "vibratoRate" } };   // CC -> knob, MIDI learn

    // read by the editor: input peak (cleared by the editor with exchange(0))
    std::atomic<float> inputPeak { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::atomic<float>* pLevel, *pChorus, *pDepth, *pRate, *pVibrato, *pInputHigh, *pPower, *pSync, *pDivision;

    double sr = 44100.0;
    ce1::Channel channels[2];

    juce::SmoothedValue<float> sDry, sWet, sCentreMs, sDepthMs, sRateHz, sInGain, sOutGain, sPower;
    float phase = 0.0f;
    float lpCoef = 0.5f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CE1AudioProcessor)
};
