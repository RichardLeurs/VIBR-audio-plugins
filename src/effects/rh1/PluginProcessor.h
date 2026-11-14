// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "BypassLink.h"
#include "MidiMap.h"

#include <JuceHeader.h>
#include "RhythmEngine.h"
#include "ui/Presets.h"

// RH-1 Rhythm Lab: a control engine (LFO, envelope, 16 steps, sync, swing) that moves a FILTER (MF-1), a TREMOLO (TR-2) or a SLICER (SL-2) (see RhythmEngine.h and .claude/lab_specs.md).
// Host parameters: power, mode, variant, source, sync, division, followHost, tempo, rate, shape, depth, sens, decay, swing, glide, step1 .. step16, cutoff, res (FILTER),
// tremStereo, tremBias (TREMOLO), slAttack, slDuty, slBalance (SLICER), mix, level. There is no latency.
class RH1AudioProcessor : public juce::AudioProcessor,
      private juce::AsyncUpdater
{
public:
    RH1AudioProcessor();
    ~RH1AudioProcessor() override { cancelPendingUpdate(); }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "RH-1"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    juce::AudioProcessorParameter* getBypassParameter() const override { return apvts.getParameter ("bypass"); }

    // the factory presets are the host's programs (the preset menu in the editor stays in step through the preset name)
    int getNumPrograms() override { return juce::jmax (1, (int) presets.factory().size()); }
    int getCurrentProgram() override { return currentProgram.load(); }
    void setCurrentProgram (int index) override
    {
        index = juce::jlimit (0, getNumPrograms() - 1, index);
        if (index == currentProgram.load()) return;   // a host re-sending the current program must not reset the user's edits
        currentProgram.store (index);
        if (juce::MessageManager::getInstance()->isThisTheMessageThread()) presets.applyFactory (index);
        else { pendingProgram.store (index); triggerAsyncUpdate(); }
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
    MidiMap midiMap { apvts, { "rate", "sens", "shape", "decay", "glide", "depth", "swing", "tempo", "cutoff", "res", "tremStereo", "slAttack", "slDuty", "slBalance", "mix", "level" } };   // CC -> knob, MIDI learn
    vibr::PresetManager presets;
    std::atomic<int> currentProgram { 0 };
    std::atomic<int> pendingProgram { -1 };
    const rh1::Engine& getEngine() const { return engine; }
    float hostBpm() const { return bpmNow.load(); }

private:
    void handleAsyncUpdate() override { const int i = pendingProgram.exchange (-1); if (i >= 0) presets.applyFactory (i); }
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
