// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "BypassLink.h"

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
    juce::AudioProcessorParameter* getBypassParameter() const override { return apvts.getParameter ("bypass"); }

    // the factory presets are the host's programs (the preset menu in the editor stays in step through the preset name)
    int getNumPrograms() override { return juce::jmax (1, (int) presets.factory().size()); }
    int getCurrentProgram() override
    {
        const auto& list = presets.factory();
        for (int i = 0; i < (int) list.size(); ++i)
            if (list[(size_t) i].name == presets.name()) return currentProgram = i;
        return currentProgram;
    }
    void setCurrentProgram (int index) override
    {
        if (index < 0 || index >= (int) presets.factory().size()) return;
        currentProgram = index;
        if (juce::MessageManager::getInstance()->isThisTheMessageThread()) presets.applyFactory (index);
        else juce::MessageManager::callAsync ([weak = juce::WeakReference<RB1AudioProcessor> (this), index] { if (weak != nullptr) weak->presets.applyFactory (index); });
    }
    const juce::String getProgramName (int index) override
    {
        const auto& list = presets.factory();
        return index >= 0 && index < (int) list.size() ? list[(size_t) index].name : juce::String();
    }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::UndoManager undoManager { 30000, 100 };
    juce::AudioProcessorValueTreeState apvts;
    vibr::BypassLink bypassLink { apvts };                // host bypass <-> OFF / ON switch
    vibr::PresetManager presets;
    int currentProgram = 0;
    JUCE_DECLARE_WEAK_REFERENCEABLE (RB1AudioProcessor)
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
