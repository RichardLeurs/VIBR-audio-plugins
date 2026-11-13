// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include <JuceHeader.h>
#include "VibeEngine.h"

// VR-1 Vibe and Rotary: uni-vibe, rotary speaker and ring mod (see VibeEngine.h).
// Host parameters: power, mode, variant, sync, rate, depth, mix, level.
class VR1AudioProcessor : public juce::AudioProcessor
{
public:
    VR1AudioProcessor();
    ~VR1AudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "VR-1"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.1; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    const vr1::Engine& getEngine() const { return engine; }       // for the MOTION display of the editor

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    vr1::Engine engine;
    std::atomic<float> *pPower {}, *pMode {}, *pVariant {}, *pSync {}, *pRate {}, *pDepth {}, *pMix {}, *pLevel {};
    juce::SmoothedValue<float> powerSmooth;
    juce::AudioBuffer<float> dryBuf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VR1AudioProcessor)
};
