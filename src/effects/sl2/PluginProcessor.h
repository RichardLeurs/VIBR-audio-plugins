// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include <JuceHeader.h>
#include "plugin/PedalPresets.h"
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
    vibr::PedalPresets presets { *this, apvts, undoManager, "SL-2 Presets", factoryPresets(), { "bypass" } };

private:
    static std::vector<vibr::PresetManager::Factory> factoryPresets();
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
