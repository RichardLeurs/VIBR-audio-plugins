// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "MidiMap.h"

#include <JuceHeader.h>
#include "plugin/PedalPresets.h"
#include "OC5DSP.h"

class OC5AudioProcessor : public juce::AudioProcessor
{
public:
    OC5AudioProcessor();
    ~OC5AudioProcessor() override = default;

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
    vibr::PedalPresets presets { *this, apvts, undoManager, "OC-5 Presets", factoryPresets(), { "bypass" } };
    MidiMap midiMap { apvts, { "direct", "up1", "down1", "down2" } };   // CC -> knob, MIDI learn

private:
    static std::vector<vibr::PresetManager::Factory> factoryPresets();
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    static constexpr int kMaxChannels = 2;
    std::atomic<float>* pDirect, *pUp1, *pDown1, *pDown2, *pPoly;
    juce::AudioParameterBool* bypassParam = nullptr;

    oc5::Octave engine[kMaxChannels];
    juce::SmoothedValue<float> wetSmooth { 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OC5AudioProcessor)
};
