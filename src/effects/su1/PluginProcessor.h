// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include <JuceHeader.h>
#include "UtilityEngine.h"

// SU-1 Stereo Utility: channels (invert, swap, mono, mid / side solo), width, pan and level (see UtilityEngine.h).
// Host parameters: power, gain, pan, autoRate, autoDepth, width, spread, bassMono, invL, invR, swap, mono, solo.
class SU1AudioProcessor : public juce::AudioProcessor
{
public:
    SU1AudioProcessor();
    ~SU1AudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SU-1"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.05; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    const su1::Engine& getEngine() const { return engine; }       // for the FIELD display of the editor

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    su1::Engine engine;
    std::atomic<float> *pPower {}, *pGain {}, *pPan {}, *pAutoRate {}, *pAutoDepth {}, *pWidth {}, *pSpread {}, *pBassMono {}, *pInvL {}, *pInvR {},
                       *pSwap {}, *pMono {}, *pSolo {};
    juce::SmoothedValue<float> powerSmooth;
    juce::AudioBuffer<float> dryBuf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SU1AudioProcessor)
};
