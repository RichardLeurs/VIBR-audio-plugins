// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include <JuceHeader.h>
#include "ModEngine.h"
#include "ui/Presets.h"

// ML-1 Modulation Lab: chorus, vibrato, flanger and phaser (see ModEngine.h).
// Host parameters: power, mode, variant, sync, rate, depth, extra (width / feedback), mix, level.
class ML1AudioProcessor : public juce::AudioProcessor
{
public:
    ML1AudioProcessor();
    ~ML1AudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ML-1"; }
    bool acceptsMidi() const override { return false; }
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

    juce::UndoManager undoManager { 30000, 100 };
    juce::AudioProcessorValueTreeState apvts;
    vibr::PresetManager presets;
    const ml1::Engine& getEngine() const { return engine; }       // for the MOTION display of the editor

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    ml1::Engine engine;
    std::atomic<float> *pPower {}, *pMode {}, *pVariant {}, *pSync {}, *pRate {}, *pDepth {}, *pExtra {}, *pMix {}, *pLevel {};
    juce::SmoothedValue<float> powerSmooth;
    juce::AudioBuffer<float> dryBuf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ML1AudioProcessor)
};
