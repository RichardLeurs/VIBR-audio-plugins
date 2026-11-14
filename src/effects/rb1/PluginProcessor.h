// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include <JuceHeader.h>
#include "ReverbLabEngine.h"
#include "ui/Presets.h"

// RB-1 Reverb Lab: CLASSIC (BS-1, 12 types), SPACE (SP-1, 5 modes) and RESONATE (SR-1, strings) in one plugin (see ReverbLabEngine.h and .claude/lab_specs.md).
// Host parameters: power, mode, variant, decay, size, tone, motion, preDelay, duck, mix, level, freeze, param2, value (CLASSIC), diffusion, texture (SPACE),
// root, scale, damping, tension, fine, blend (RESONATE). There is no latency; the tail rings after the input stops.
class RB1AudioProcessor : public juce::AudioProcessor
{
public:
    RB1AudioProcessor();
    ~RB1AudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "RB-1"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 40.0; }

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
    const rb1::Engine& getEngine() const { return engine; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    rb1::Engine engine;
    std::atomic<float> *pPower {}, *pMode {}, *pVariant {}, *pDecay {}, *pSize {}, *pTone {}, *pMotion {}, *pPreDelay {}, *pDuck {}, *pMix {}, *pLevel {}, *pFreeze {}, *pParam2 {}, *pValue {},
                       *pDiffusion {}, *pTexture {}, *pRoot {}, *pScale {}, *pDamping {}, *pTension {}, *pFine {}, *pBlend {};
    juce::SmoothedValue<float> powerSmooth;
    juce::AudioBuffer<float> dryBuf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RB1AudioProcessor)
};
