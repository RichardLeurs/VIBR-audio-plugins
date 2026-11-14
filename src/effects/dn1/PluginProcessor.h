// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

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
