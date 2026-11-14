// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "BypassLink.h"

#include <JuceHeader.h>
#include "MotionEngine.h"

// MF-1 Motion Filter: a filter (low / band / high pass, notch, vowel) moved by an LFO, an envelope follower or an 8-step sequencer
// (see MotionEngine.h).  Host parameters: power, type, source, sync, cutoff, res, depth, rate, shape, mix, level, step1 .. step8.
class MF1AudioProcessor : public juce::AudioProcessor
{
public:
    MF1AudioProcessor();
    ~MF1AudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MF-1"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }
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
    std::atomic<int> currentStep { 0 };            // the step the sequencer plays now (for the editor)
    std::atomic<float> modulation { 0.0f };        // the movement, -1 .. 1 (for the editor)

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    mf1::Engine engine;
    std::atomic<float> *pPower {}, *pType {}, *pSource {}, *pSync {}, *pCutoff {}, *pRes {}, *pDepth {}, *pM1 {}, *pM2 {}, *pMix {}, *pLevel {};
    std::atomic<float>* pStep[mf1::kNumSteps] {};
    juce::SmoothedValue<float> powerSmooth;
    juce::AudioBuffer<float> dryBuf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MF1AudioProcessor)
};
