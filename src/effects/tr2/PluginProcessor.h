// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "MidiMap.h"

#include <JuceHeader.h>
#include "TR2DSP.h"

class TR2AudioProcessor : public juce::AudioProcessor
{
public:
    TR2AudioProcessor();
    ~TR2AudioProcessor() override = default;

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

    MidiMap midiMap { apvts, { "rate", "depth", "wave" } };   // CC -> knob, MIDI learn

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    static constexpr int kMaxChannels = 2;

    std::atomic<float>* pRate = nullptr, *pDepth = nullptr, *pWave = nullptr;
    juce::AudioParameterBool* bypassParam = nullptr;
    std::atomic<float>* pSync = nullptr, *pDivision = nullptr;
    juce::SmoothedValue<float> syncMix { 0.0f };                                              // 0 = RATE knob, 1 = tempo synced
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> syncHzSmooth { 1.0f }; // synced LFO frequency (Hz)

    juce::SmoothedValue<float> rateSmooth { 3.0f }, depthSmooth { 5.0f }, waveSmooth { 0.0f }, wetSmooth { 1.0f };
    double sr = 44100.0;
    double phase = 0.0;                                   // LFO phase 0 .. 1
    float gainState = 1.0f;                               // the level, smoothed (a square wave would click)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TR2AudioProcessor)
};
