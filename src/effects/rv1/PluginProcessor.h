// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include <JuceHeader.h>
#include "ReverseEngine.h"
#include "ui/Presets.h"

// RV-1 Reverse Lab: reverse, scrub, stutter, glitch and repeat with freeze (see ReverseEngine.h).
// Host parameters: power, mode, freeze, length, speed, smooth, random, repeats, mix, level.
class RV1AudioProcessor : public juce::AudioProcessor
{
public:
    RV1AudioProcessor();
    ~RV1AudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "RV-1"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 30.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::UndoManager undoManager { 30000, 100 };
    juce::AudioProcessorValueTreeState apvts;
    vibr::PresetManager presets;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    rv1::Engine engine;
    std::atomic<float> *pPower {}, *pMode {}, *pFreeze {}, *pSync {}, *pLength {}, *pSpeed {}, *pSmooth {}, *pRandom {}, *pRepeats {}, *pMix {}, *pLevel {};
    juce::SmoothedValue<float> powerSmooth;
    juce::AudioBuffer<float> dryBuf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RV1AudioProcessor)
};
