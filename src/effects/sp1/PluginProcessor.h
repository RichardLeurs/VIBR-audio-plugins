// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "BypassLink.h"

#include <JuceHeader.h>
#include "SpaceEngine.h"

// SP-1 Space Engine: five space modes (shimmer, infinite, spectral, modulated, rooms) with freeze (see SpaceEngine.h).
// Host parameters: power, mode, freeze, size, decay, diffusion, tone, motion, texture, mix, level.
class SP1AudioProcessor : public juce::AudioProcessor
{
public:
    SP1AudioProcessor();
    ~SP1AudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SP-1"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 30.0; }
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

    sp1::Engine engine;
    std::atomic<float> *pPower {}, *pMode {}, *pFreeze {}, *pSize {}, *pDecay {}, *pDiff {}, *pTone {}, *pMotion {}, *pTexture {}, *pMix {}, *pLevel {};
    juce::SmoothedValue<float> powerSmooth;
    juce::AudioBuffer<float> dryBuf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SP1AudioProcessor)
};
