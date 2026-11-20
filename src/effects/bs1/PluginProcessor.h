// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include <JuceHeader.h>
#include "plugin/PedalPresets.h"
#include "ReverbEngine.h"

class BigSkyAudioProcessor : public juce::AudioProcessor
{
public:
    BigSkyAudioProcessor();
    ~BigSkyAudioProcessor() override = default;

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
    vibr::PedalPresets presets { *this, apvts, undoManager, "BS-1 Presets", factoryPresets(), { "bypass" } };

private:
    static std::vector<vibr::PedalPresets::Factory> factoryPresets();
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::atomic<float>* pType, *pDecay, *pPre, *pMix, *pTone, *pP1, *pP2, *pMod, *pValue;
    juce::AudioParameterBool* bypassParam = nullptr;

    bigsky::Reverb engine;
    std::vector<float> dryL, dryR, wetR;
    juce::SmoothedValue<float> wetSmooth { 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BigSkyAudioProcessor)
};
