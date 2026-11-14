// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "BypassLink.h"

#include "MidiMap.h"

#include <JuceHeader.h>
#include "ResonatorEngine.h"

// SR-1 Sympathetic Resonator: virtual strings tuned to a scale that ring along with the playing (see ResonatorEngine.h).
// Host parameters: power, root, scale, texture, strings, decay, damping, tension, motion, fine, mix, level.
class SR1AudioProcessor : public juce::AudioProcessor
{
public:
    SR1AudioProcessor();
    ~SR1AudioProcessor() override = default;

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

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    MidiMap midiMap { apvts, { "strings", "decay", "damping", "tension", "motion", "fine", "mix", "level" } };   // CC -> knob, MIDI learn

    vibr::BypassLink bypassLink { apvts };                // host bypass <-> OFF / ON switch
    const sr1::Engine& getEngine() const { return engine; }       // the editor reads the string display from it

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    sr1::Engine engine;
    std::atomic<float> *pPower {}, *pRoot {}, *pScale {}, *pTexture {}, *pStrings {}, *pDecay {}, *pDamping {}, *pTension {}, *pMotion {}, *pFine {}, *pMix {}, *pLevel {};
    juce::SmoothedValue<float> powerSmooth;
    juce::AudioBuffer<float> dryBuf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SR1AudioProcessor)
};
