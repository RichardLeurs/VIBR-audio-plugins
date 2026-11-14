// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include <JuceHeader.h>
#include "MidiMap.h"
#include "CS3DSP.h"

class CS3AudioProcessor : public juce::AudioProcessor
{
public:
    CS3AudioProcessor();
    ~CS3AudioProcessor() override = default;

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
    MidiMap midiMap { apvts, { "sustain", "level", "tone", "attack" } };   // CC -> knob, MIDI learn

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    static constexpr int kMaxChannels = 2;

    std::atomic<float>* pLevel, *pTone, *pAttack, *pSustain;
    juce::AudioParameterBool* bypassParam = nullptr;

    cs3::Shelf low[kMaxChannels], high[kMaxChannels];
    juce::SmoothedValue<float> levelSmooth { 1.0f }, toneSmooth { 5.0f }, attackSmooth { 5.0f }, sustainSmooth { 5.0f }, wetSmooth { 1.0f };
    double sr = 44100.0;
    float envDb = -120.0f;                                // detector envelope (dB)
    float lastReduction = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CS3AudioProcessor)
};
