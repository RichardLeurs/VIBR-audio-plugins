// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "BypassLink.h"

#include <JuceHeader.h>
#include "RhythmEngine.h"
#include "ui/Presets.h"

// RH-1 Rhythm Lab: a control engine (LFO, envelope, 16 steps, sync, swing) that moves a FILTER (MF-1), a TREMOLO (TR-2) or a SLICER (SL-2) (see RhythmEngine.h and .claude/lab_specs.md).
// Host parameters: power, mode, variant, source, sync, division, followHost, tempo, rate, shape, depth, sens, decay, swing, glide, step1 .. step16, cutoff, res (FILTER),
// tremStereo, tremBias (TREMOLO), slAttack, slDuty, slBalance (SLICER), mix, level. There is no latency.
class RH1AudioProcessor : public juce::AudioProcessor
{
public:
    RH1AudioProcessor();
    ~RH1AudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "RH-1"; }
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
        else juce::MessageManager::callAsync ([weak = juce::WeakReference<RH1AudioProcessor> (this), index] { if (weak != nullptr) weak->presets.applyFactory (index); });
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
    JUCE_DECLARE_WEAK_REFERENCEABLE (RH1AudioProcessor)
    const rh1::Engine& getEngine() const { return engine; }
    float hostBpm() const { return bpmNow.load(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    rh1::Engine engine;
    std::atomic<float> bpmNow { 120.0f };
    std::atomic<float> *pPower {}, *pMode {}, *pVariant {}, *pSource {}, *pSync {}, *pDivision {}, *pFollow {}, *pTempo {}, *pRate {}, *pShape {}, *pDepth {}, *pSens {}, *pDecay {}, *pSwing {}, *pGlide {},
                       *pCutoff {}, *pRes {}, *pTremStereo {}, *pTremBias {}, *pSlAttack {}, *pSlDuty {}, *pSlBalance {}, *pMix {}, *pLevel {};
    std::atomic<float>* pSteps[rh1::kSteps] {};
    juce::SmoothedValue<float> powerSmooth;
    juce::AudioBuffer<float> dryBuf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RH1AudioProcessor)
};
