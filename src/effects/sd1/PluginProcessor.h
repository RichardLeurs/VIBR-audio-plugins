// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "BypassLink.h"
#include "MidiMap.h"
#include "TapTempo.h"

#include <JuceHeader.h>
#include "TempoSync.h"
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
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }
    juce::AudioProcessorParameter* getBypassParameter() const override { return apvts.getParameter ("bypass"); }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    MidiMap midiMap { apvts, { "level", "delayTime", "factor", "feedback", "hiCut", "modIntensity", "modFrequency", "direct", "effect" } };   // CC -> knob, MIDI learn
    void tapAt (double ms);                      // one tap (ms: juce::Time::getMillisecondCounterHiRes); message thread
    vibr::TapAverager tapAvg;
    vibr::TapTempo midiTap { [this] (double ms) { tapAt (ms); } };   // MIDI tap (note-on or CC85) and the TAP button

    std::atomic<float> meterLevel { 0.0f };   // headroom meter: input peak after the Level knob (linear, decaying)

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::atomic<float>* pLevel, *pTime, *pFactor, *pFeedback, *pHiCut, *pModInt, *pModFreq, *pDirect, *pEffect,
                       *pBypass, *pPower, *pSync, *pDivision;

    double sr = 44100.0;
    sdd1000::Channel channels[2];

    juce::SmoothedValue<float> sDelayMs, sFeedback, sInGain, sDirect, sEffect, sActive, sModInt, sModRate, sCutHz;
    float phase = 0.0f, meter = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SDD1000AudioProcessor)
};
