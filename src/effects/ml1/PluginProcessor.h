// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "BypassLink.h"

#include <JuceHeader.h>
#include "ModEngine.h"
#include "ui/Presets.h"

// ML-1 Modulation Lab: chorus, vibrato, flanger and phaser (see ModEngine.h).
// Host parameters: power, mode, variant, sync, rate, depth, extra (width / feedback), mix, level.
class ML1AudioProcessor : public juce::AudioProcessor
{
public:
    ML1AudioProcessor();
    ~ML1AudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ML-1"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }
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
        else juce::MessageManager::callAsync ([weak = juce::WeakReference<ML1AudioProcessor> (this), index] { if (weak != nullptr) weak->presets.applyFactory (index); });
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
    JUCE_DECLARE_WEAK_REFERENCEABLE (ML1AudioProcessor)
    const ml1::Engine& getEngine() const { return engine; }       // for the MOTION display of the editor

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    ml1::Engine engine;
    std::atomic<float> *pPower {}, *pMode {}, *pVariant {}, *pSync {}, *pRate {}, *pDepth {}, *pExtra {}, *pMix {}, *pLevel {};
    juce::SmoothedValue<float> powerSmooth;
    juce::AudioBuffer<float> dryBuf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ML1AudioProcessor)
};
