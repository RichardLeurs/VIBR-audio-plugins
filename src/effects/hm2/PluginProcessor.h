// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "BypassLink.h"

#include <JuceHeader.h>
#include "HarmonicEngine.h"

// HM-2 Harmonic Machine: saturation with odd / even harmonic control, octave up and down, frequency-dependent drive (see HarmonicEngine.h).
// Host parameters: power, character, drive, odd/even, octave, sub, focus, tilt, tone, mix, level.
class HM2AudioProcessor : public juce::AudioProcessor
{
public:
    HM2AudioProcessor();
    ~HM2AudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "HM-2"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.1; }
    juce::AudioProcessorParameter* getBypassParameter() const override { return apvts.getParameter ("bypass"); }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    vibr::BypassLink bypassLink { apvts };                // host bypass <-> OFF / ON switch

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    hm2::Engine engine;
    std::atomic<float> *pPower {}, *pCharacter {}, *pDrive {}, *pOddEven {}, *pOctave {}, *pSub {}, *pFocus {}, *pTilt {}, *pTone {}, *pMix {}, *pLevel {};
    juce::SmoothedValue<float> powerSmooth;
    juce::AudioBuffer<float> dryBuf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HM2AudioProcessor)
};
