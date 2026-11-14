// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "BypassLink.h"

#include <JuceHeader.h>
#include "DelayEngine.h"
#include "ui/Presets.h"

// DL-1 Delay Lab: digital (SD-1) and analog (MM-1) delay voicings in one plugin (see DelayEngine.h and .claude/lab_specs.md).
// Host parameters: power, mode, variant, sync, division, time, feedback, low, high, modRate, modDepth, spread, hold, mix, level,
// digFactor, digDirect, digEffect, digInput (DIGITAL), anaBlend, anaDepth, anaGrit (ANALOG).
class DL1AudioProcessor : public juce::AudioProcessor
{
public:
    DL1AudioProcessor();
    ~DL1AudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "DL-1"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }
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
        else juce::MessageManager::callAsync ([weak = juce::WeakReference<DL1AudioProcessor> (this), index] { if (weak != nullptr) weak->presets.applyFactory (index); });
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
    JUCE_DECLARE_WEAK_REFERENCEABLE (DL1AudioProcessor)
    const dl1::Engine& getEngine() const { return engine; }
    float hostBpm() const { return bpmNow.load(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    dl1::Engine engine;
    std::atomic<float> bpmNow { 120.0f };
    std::atomic<float> *pPower {}, *pMode {}, *pVariant {}, *pSync {}, *pDivision {}, *pTime {}, *pFeedback {}, *pLow {}, *pHigh {}, *pModRate {}, *pModDepth {}, *pSpread {}, *pHold {},
                       *pMix {}, *pLevel {}, *pDigFactor {}, *pDigDirect {}, *pDigEffect {}, *pDigInput {}, *pAnaBlend {}, *pAnaDepth {}, *pAnaGrit {},
                       *pTapeWow {}, *pTapeSat {}, *pTapeAge {}, *pTapeSpacing {}, *pDualTimeB {}, *pDualFbB {}, *pDualLevelA {}, *pDualLevelB {}, *pDualHighB {}, *pDualPingA {}, *pDualPingB {}, *pModVoice {}, *pModVariation {}, *pModVolume {};
    juce::SmoothedValue<float> powerSmooth;
    juce::AudioBuffer<float> dryBuf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DL1AudioProcessor)
};
