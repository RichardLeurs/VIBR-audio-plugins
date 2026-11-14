// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "BypassLink.h"

#include <JuceHeader.h>
#include "ModEngine.h"

// VC-1 Voltage Controller: a modulation matrix (envelope, LFO 1, LFO 2, random, MIDI) on a filter, amp, vibrato and pan (see ModEngine.h).
// Host parameters: power, type, shape1, shape2, cutoff, res, envsens, envrel, rate1, rate2, rand, xmod, mix, level and the 20 matrix depths m<source><destination>.
// MIDI: mod wheel (CC1), expression (CC11) and channel pressure drive the MIDI source.
class VC1AudioProcessor : public juce::AudioProcessor
{
public:
    VC1AudioProcessor();
    ~VC1AudioProcessor() override = default;

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

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    vibr::BypassLink bypassLink { apvts };                // host bypass <-> OFF / ON switch
    const vc1::Engine& getEngine() const { return engine; }       // the editor reads the source lamps from it

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    vc1::Engine engine;
    std::atomic<float> *pPower {}, *pSync {}, *pType {}, *pShape1 {}, *pShape2 {}, *pCutoff {}, *pRes {}, *pEnvSens {}, *pEnvRel {}, *pRate1 {}, *pRate2 {}, *pRand {}, *pXmod {}, *pMix {}, *pLevel {};
    std::atomic<float>* pMatrix[vc1::kNumSources][vc1::kNumDests] {};
    float midiValue = 0.0f;
    juce::SmoothedValue<float> powerSmooth;
    juce::AudioBuffer<float> dryBuf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VC1AudioProcessor)
};
