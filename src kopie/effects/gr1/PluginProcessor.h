// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "BypassLink.h"

#include "MidiMap.h"

#include <JuceHeader.h>
#include "GrainEngine.h"

// GR-1 Grain: granular processing: grains cut from the playing (see GrainEngine.h).
// Host parameters: power, shape, freeze, size, density, position, spray, pitch, random, reverse, spread, mix, level.
class GR1AudioProcessor : public juce::AudioProcessor
{
public:
    GR1AudioProcessor();
    ~GR1AudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "GR-1"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }
    juce::AudioProcessorParameter* getBypassParameter() const override { return apvts.getParameter ("bypass"); }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    MidiMap midiMap { apvts, { "size", "density", "position", "spray", "pitch", "random", "reverse", "spread", "mix", "level", "freeze" } };   // CC -> knob, MIDI learn

    vibr::BypassLink bypassLink { apvts };                // host bypass <-> OFF / ON switch
    const gr1::Engine& getEngine() const { return engine; }       // the editor reads the grain display from it

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    gr1::Engine engine;
    std::atomic<float> *pPower {}, *pShape {}, *pFreeze {}, *pSync {}, *pSize {}, *pDensity {}, *pPosition {}, *pSpray {}, *pPitch {}, *pRandom {}, *pReverse {}, *pSpread {}, *pMix {}, *pLevel {};
    juce::SmoothedValue<float> powerSmooth;
    juce::AudioBuffer<float> dryBuf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GR1AudioProcessor)
};
