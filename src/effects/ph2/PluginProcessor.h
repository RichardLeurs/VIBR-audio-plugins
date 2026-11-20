// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include <JuceHeader.h>
#include "PH2DSP.h"
#include "MidiMap.h"
#include "plugin/PedalPresets.h"

class PH2AudioProcessor : public juce::AudioProcessor
{
public:
    PH2AudioProcessor();
    ~PH2AudioProcessor() override = default;

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
    double getTailLengthSeconds() const override { return 0.2; }

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
    vibr::PedalPresets presets { *this, apvts, undoManager, "PH-2 Presets", factoryPresets(), { "bypass" } };
    MidiMap midiMap { apvts, { "res", "depth", "rate", "mode" } };      // MIDI CC + MIDI learn

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static std::vector<vibr::PresetManager::Factory> factoryPresets();

    static constexpr int kMaxChannels = 2;

    std::atomic<float>* pManual, *pRes, *pDepth, *pRate, *pMode;
    juce::AudioParameterBool* bypassParam = nullptr;
    std::atomic<float>* pSync = nullptr, *pDivision = nullptr;
    juce::SmoothedValue<float> syncMix { 0.0f };                                              // 0 = RATE knob, 1 = tempo synced
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> syncHzSmooth { 1.0f }; // synced LFO frequency (Hz)

    ph2::PhaserChannel phaser[kMaxChannels];
    juce::SmoothedValue<float> resSmooth { 0.5f }, depthSmooth { 0.5f }, rateSmooth { 0.5f }, wetSmooth { 1.0f };
    juce::SmoothedValue<float> modeSmooth { 0.0f };     // crossfades the 4 / 8 stage outputs
    double sr = 44100.0;
    double phase = 0.0;                                  // LFO phase 0..1

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PH2AudioProcessor)
};
