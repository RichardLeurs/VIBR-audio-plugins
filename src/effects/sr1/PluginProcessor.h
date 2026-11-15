// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "BypassLink.h"

#include "MidiMap.h"

#include <JuceHeader.h>
#include "ResonatorEngine.h"
#include "ui/Presets.h"
#include "ui/OutputMeter.h"

// SR-1 Sympathetic Resonator: virtual strings tuned to a scale that ring along with the playing (see ResonatorEngine.h).
// Host parameters: power, root, scale, texture, strings, decay, damping, tension, motion, fine, mix, level.
class SR1AudioProcessor : public juce::AudioProcessor,
                          private juce::AsyncUpdater
{
public:
    SR1AudioProcessor();
    ~SR1AudioProcessor() override { cancelPendingUpdate(); }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SR-1"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 18.0; }
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
    MidiMap midiMap { apvts, { "strings", "decay", "damping", "tension", "motion", "fine", "mix", "level" } };   // CC -> knob, MIDI learn

    vibr::BypassLink bypassLink { apvts };                // host bypass <-> OFF / ON switch
    vibr::PresetManager presets;
    std::atomic<int> currentProgram { 0 };
    std::atomic<int> pendingProgram { -1 };
    const sr1::Engine& getEngine() const { return engine; }       // the editor reads the string display from it
    vibr::ui::PeakTap outMeter;                                   // sample peak of the output, read by the editor

private:
    void handleAsyncUpdate() override { const int i = pendingProgram.exchange (-1); if (i >= 0) presets.applyFactory (i); }
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    sr1::Engine engine;
    std::atomic<float> *pPower {}, *pRoot {}, *pScale {}, *pTexture {}, *pStrings {}, *pDecay {}, *pDamping {}, *pTension {}, *pMotion {}, *pFine {}, *pMix {}, *pLevel {};
    juce::SmoothedValue<float> powerSmooth;
    juce::AudioBuffer<float> dryBuf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SR1AudioProcessor)
};
