// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include <JuceHeader.h>
#include "TempoSync.h"
#include "MidiMap.h"
#include "BF3DSP.h"

class BF3AudioProcessor : public juce::AudioProcessor
{
public:
    BF3AudioProcessor();
    ~BF3AudioProcessor() override = default;

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
    double getTailLengthSeconds() const override { return 0.5; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    juce::AudioProcessorValueTreeState apvts;
    MidiMap midiMap { apvts, { "manual", "res", "depth", "rate", "mode" } };   // CC -> knob, MIDI learn

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    static constexpr int kMaxChannels = 2;

    // RATE knob position 0..1 (sync off), or the position that gives one LFO cycle per division at `tempo` (sync on)
    float rateTarget01 (double tempo) const;

    std::atomic<float>* pManual, *pRes, *pDepth, *pRate, *pMode, *pSync, *pDivision;
    juce::AudioParameterBool* bypassParam = nullptr;
    double bpm = 120.0;                                  // host tempo, read once per block

    bf3::FlangerChannel flanger[kMaxChannels];
    juce::SmoothedValue<float> manualSmooth { 0.5f }, resSmooth { 0.5f }, depthSmooth { 0.5f }, rateSmooth { 0.5f }, wetSmooth { 1.0f };
    juce::SmoothedValue<float> modeSmooth { 1.0f };     // smooths the mode-dependent depth / feedback scaling and the pan amount
    double sr = 44100.0;
    double phase = 0.0;                                  // LFO phase 0..1

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BF3AudioProcessor)
};
