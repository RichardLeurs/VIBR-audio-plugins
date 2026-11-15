// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "BypassLink.h"
#include "MidiMap.h"
#include "ui/Presets.h"
#include "ui/OutputMeter.h"

#include <JuceHeader.h>
#include "MotionEngine.h"

// MF-1 Motion Filter: a filter (low / band / high pass, notch, vowel) moved by an LFO, an envelope follower or an 8-step sequencer
// (see MotionEngine.h).  Host parameters: power, type, source, sync, cutoff, res, depth, rate, shape, mix, level, step1 .. step8.
class MF1AudioProcessor : public juce::AudioProcessor,
      private juce::AsyncUpdater
{
public:
    MF1AudioProcessor();
    ~MF1AudioProcessor() override { cancelPendingUpdate(); }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MF-1"; }
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

    vibr::BypassLink bypassLink { apvts };                // host bypass <-> OFF / ON switch
    MidiMap midiMap { apvts, { "cutoff", "res", "depth", "m1", "m2", "mix", "level" } };   // CC -> knob, MIDI learn
    std::atomic<int> currentStep { 0 };            // the step the sequencer plays now (for the editor)
    std::atomic<float> modulation { 0.0f };        // the movement, -1 .. 1 (for the editor)
    vibr::PresetManager presets;
    std::atomic<int> currentProgram { 0 };
    std::atomic<int> pendingProgram { -1 };
    vibr::ui::PeakTap outMeter;                    // sample peak of the output, read by the editor

private:
    void handleAsyncUpdate() override { const int i = pendingProgram.exchange (-1); if (i >= 0) presets.applyFactory (i); }
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    mf1::Engine engine;
    std::atomic<float> *pPower {}, *pType {}, *pSource {}, *pSync {}, *pCutoff {}, *pRes {}, *pDepth {}, *pM1 {}, *pM2 {}, *pMix {}, *pLevel {};
    std::atomic<float>* pStep[mf1::kNumSteps] {};
    juce::SmoothedValue<float> powerSmooth;
    juce::AudioBuffer<float> dryBuf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MF1AudioProcessor)
};
