// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include <JuceHeader.h>
#include "MidiMap.h"
#include "plugin/PedalPresets.h"
#include "LM2DSP.h"

class LM2AudioProcessor : public juce::AudioProcessor
{
public:
    LM2AudioProcessor();
    ~LM2AudioProcessor() override = default;

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

    int getNumPrograms() override { return presets.numPrograms(); }
    int getCurrentProgram() override { return presets.currentProgram(); }
    void setCurrentProgram (int i) override { presets.setProgram (i); }
    const juce::String getProgramName (int i) override { return presets.programName (i); }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    juce::UndoManager undoManager { 30000, 100 };
    juce::AudioProcessorValueTreeState apvts;
    vibr::PedalPresets presets { *this, apvts, undoManager, "LM-2 Presets", factoryPresets(), { "bypass" } };
    MidiMap midiMap { apvts, { "threshold", "level", "tone", "release" } };   // CC -> knob, MIDI learn

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static std::vector<vibr::PresetManager::Factory> factoryPresets();

    static constexpr int kMaxChannels = 2;

    std::atomic<float>* pLevel, *pTone, *pRelease, *pThreshold;
    juce::AudioParameterBool* bypassParam = nullptr;

    lm2::Shelf low[kMaxChannels], high[kMaxChannels];
    juce::SmoothedValue<float> levelSmooth { 1.0f }, toneSmooth { 5.0f }, releaseSmooth { 3.0f }, thresholdSmooth { 5.0f }, wetSmooth { 1.0f };
    double sr = 44100.0;
    float envDb = -120.0f;                                // detector envelope (dB)
    float lastReduction = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LM2AudioProcessor)
};
