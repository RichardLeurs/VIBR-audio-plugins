// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "BypassLink.h"

#include <JuceHeader.h>
#include "MidiMap.h"
#include "SaturatorEngine.h"

// SA-1 Lo-Fi Saturator: boost, tube, tape and bit crusher in one chain (see SaturatorEngine.h).
// Host parameters: power, boostOn, boostGain, boostTone, tubeOn, tubeDrive, tubeBias, tapeOn, tapeDrive, tapeWarmth, crushOn, bits, rate, crushMix, mix, level.
class SA1AudioProcessor : public juce::AudioProcessor
{
public:
    SA1AudioProcessor();
    ~SA1AudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SA-1"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    juce::AudioProcessorParameter* getBypassParameter() const override { return apvts.getParameter ("bypass"); }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    MidiMap midiMap { apvts, { "tubeDrive", "boostGain", "boostTone", "tubeBias", "tapeDrive", "tapeWarmth", "bits", "rate", "crushMix", "mix", "level", "boostOn", "tubeOn", "tapeOn", "crushOn" } };   // CC -> knob, MIDI learn

    vibr::BypassLink bypassLink { apvts };                // host bypass <-> OFF / ON switch

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    sa1::Engine engine;
    std::atomic<float> *pPower {}, *pBoostOn {}, *pBoostGain {}, *pBoostTone {}, *pTubeOn {}, *pTubeDrive {}, *pTubeBias {}, *pTapeOn {}, *pTapeDrive {},
                       *pTapeWarmth {}, *pCrushOn {}, *pBits {}, *pRate {}, *pCrushMix {}, *pMix {}, *pLevel {};
    juce::SmoothedValue<float> powerSmooth;
    juce::AudioBuffer<float> dryBuf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SA1AudioProcessor)
};
