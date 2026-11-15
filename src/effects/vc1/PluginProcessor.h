// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "BypassLink.h"

#include <JuceHeader.h>
#include "ModEngine.h"
#include "MidiMap.h"
#include "ui/Presets.h"
#include "ui/OutputMeter.h"

// VC-1 Voltage Controller: a modulation matrix (envelope, LFO 1, LFO 2, random, MIDI) on a filter, amp, vibrato and pan (see ModEngine.h).
// Host parameters: power, type, shape1, shape2, cutoff, res, envsens, envrel, rate1, rate2, rand, xmod, mix, level and the 20 matrix depths m<source><destination>.
// MIDI: mod wheel (CC1), expression (CC11) and channel pressure drive the MIDI source.
class VC1AudioProcessor : public juce::AudioProcessor,
      private juce::AsyncUpdater
{
public:
    VC1AudioProcessor();
    ~VC1AudioProcessor() override { cancelPendingUpdate(); }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "VC-1"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }
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
    MidiMap midiMap { apvts, { "cutoff", "res", "xmod", "mix", "level", "envsens", "envrel", "rate1", "rate2", "rand",
                               "m00", "m01", "m02", "m03", "m10", "m11", "m12", "m13", "m20", "m21", "m22", "m23" } };   // CC -> knob, MIDI learn (24 slots at most)

    vibr::BypassLink bypassLink { apvts };                // host bypass <-> OFF / ON switch
    const vc1::Engine& getEngine() const { return engine; }       // the editor reads the source lamps from it
    vibr::PresetManager presets;
    std::atomic<int> currentProgram { 0 };
    std::atomic<int> pendingProgram { -1 };
    vibr::ui::PeakTap outMeter;                                   // sample peak of the output, read by the editor

private:
    void handleAsyncUpdate() override { const int i = pendingProgram.exchange (-1); if (i >= 0) presets.applyFactory (i); }
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    vc1::Engine engine;
    std::atomic<float> *pPower {}, *pSync {}, *pType {}, *pShape1 {}, *pShape2 {}, *pCutoff {}, *pRes {}, *pEnvSens {}, *pEnvRel {}, *pRate1 {}, *pRate2 {}, *pRand {}, *pXmod {}, *pMix {}, *pLevel {};
    std::atomic<float>* pMatrix[vc1::kNumSources][vc1::kNumDests] {};
    float midiValue = 0.0f;
    juce::SmoothedValue<float> powerSmooth;
    juce::AudioBuffer<float> dryBuf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VC1AudioProcessor)
};
