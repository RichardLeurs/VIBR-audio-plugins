// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "BypassLink.h"

#include <JuceHeader.h>
#include "DynamicsEngine.h"
#include "ui/Presets.h"

// DN-1 Dynamics Lab: SHAPER (DY-1), COMPRESSOR (SUSTAIN = CS-3, VCA, OPTO) and LIMITER (PEAK = LM-2, TRUE PEAK) in one plugin (see DynamicsEngine.h and .claude/lab_specs.md).
// Host parameters: power, mode, variant, gateOn, gateThresh, gateRelease, expOn, expThresh, expRatio, expRelease, transOn, attack, sustain (SHAPER),
// compSustain, compAttack, compThresh, compRatio, compAttackMs, compRelease, compMakeup, compMix (COMPRESSOR), tone, limThresh, limRelease, limCeiling (LIMITER),
// scHp, link, mix, level, autoLevel. The latency is constant (the lookahead of the true-peak limiter) and reported to the host.
class DN1AudioProcessor : public juce::AudioProcessor
{
public:
    DN1AudioProcessor();
    ~DN1AudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "DN-1"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
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
        else juce::MessageManager::callAsync ([weak = juce::WeakReference<DN1AudioProcessor> (this), index] { if (weak != nullptr) weak->presets.applyFactory (index); });
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
    JUCE_DECLARE_WEAK_REFERENCEABLE (DN1AudioProcessor)
    const dn1::Engine& getEngine() const { return engine; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    dn1::Engine engine;
    std::atomic<float> *pPower {}, *pMode {}, *pVariant {}, *pGateOn {}, *pGateThresh {}, *pGateRelease {}, *pExpOn {}, *pExpThresh {}, *pExpRatio {}, *pExpRelease {}, *pTransOn {}, *pAttack {}, *pSustain {},
                       *pCompSustain {}, *pCompAttack {}, *pCompThresh {}, *pCompRatio {}, *pCompAttackMs {}, *pCompRelease {}, *pCompMakeup {}, *pCompMix {}, *pTone {},
                       *pLimThresh {}, *pLimRelease {}, *pLimCeiling {}, *pScHp {}, *pLink {}, *pMix {}, *pLevel {}, *pAutoLevel {};
    juce::SmoothedValue<float> powerSmooth;
    juce::AudioBuffer<float> dryBuf;
    int delayPos = 0;
    std::vector<float> powerDelay[2];                                              // the untouched signal, delayed by the latency, for the power switch

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DN1AudioProcessor)
};
