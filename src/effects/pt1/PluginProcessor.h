// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "BypassLink.h"

#include "MidiMap.h"

#include <JuceHeader.h>
#include "PhantomEngine.h"
#include "ui/Presets.h"
#include "ui/OutputMeter.h"

// PT-1 Phantom: a four-voice harmonizer; INTELLIGENT mode keeps the harmonies in the chosen key and scale (see PhantomEngine.h).
// Host parameters: power, intel, root, scale, int1..4 (interval), det1..4 (detune), lvl1..4 (level), spread, mix, level.
class PT1AudioProcessor : public juce::AudioProcessor,
                         private juce::AsyncUpdater
{
public:
    PT1AudioProcessor();
    ~PT1AudioProcessor() override { cancelPendingUpdate(); }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "PT-1"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.1; }
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
    MidiMap midiMap { apvts, { "int1", "int2", "int3", "int4", "det1", "det2", "det3", "det4", "lvl1", "lvl2", "lvl3", "lvl4", "spread", "mix", "level" } };   // CC -> knob, MIDI learn

    vibr::BypassLink bypassLink { apvts };                // host bypass <-> OFF / ON switch
    vibr::PresetManager presets;
    std::atomic<int> currentProgram { 0 };
    std::atomic<int> pendingProgram { -1 };
    const pt1::Engine& getEngine() const { return engine; }       // the editor reads the detected and played notes from it

    vibr::ui::PeakTap outMeter;                                   // sample peak of the output, read by the editor

private:
    void handleAsyncUpdate() override { const int i = pendingProgram.exchange (-1); if (i >= 0) presets.applyFactory (i); }
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    pt1::Engine engine;
    std::atomic<float> *pPower {}, *pIntel {}, *pRoot {}, *pScale {}, *pSpread {}, *pMix {}, *pLevel {};
    std::atomic<float> *pInt[pt1::kNumVoices] {}, *pDet[pt1::kNumVoices] {}, *pLvl[pt1::kNumVoices] {};
    juce::SmoothedValue<float> powerSmooth;
    juce::AudioBuffer<float> dryBuf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PT1AudioProcessor)
};
