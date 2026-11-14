// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "BypassLink.h"

#include <JuceHeader.h>
#include "DynamicEngine.h"

// DY-1 Dynamic Shaper: noise gate, expander and transient shaper in one chain (see DynamicEngine.h).
// Host parameters: power, gateOn, gateThresh, gateRelease, expOn, expThresh, expRatio, expRelease, transOn, attack, sustain, mix, level.
class DY1AudioProcessor : public juce::AudioProcessor
{
public:
    DY1AudioProcessor();
    ~DY1AudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "DY-1"; }
    bool acceptsMidi() const override { return false; }
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

    vibr::BypassLink bypassLink { apvts };                // host bypass <-> OFF / ON switch
    const dy1::Engine& getEngine() const { return engine; }       // for the ACTION display of the editor

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    dy1::Engine engine;
    std::atomic<float> *pPower {}, *pGateOn {}, *pGateThresh {}, *pGateRelease {}, *pExpOn {}, *pExpThresh {}, *pExpRatio {}, *pExpRelease {},
                       *pTransOn {}, *pAttack {}, *pSustain {}, *pMix {}, *pLevel {};
    juce::SmoothedValue<float> powerSmooth;
    juce::AudioBuffer<float> dryBuf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DY1AudioProcessor)
};
