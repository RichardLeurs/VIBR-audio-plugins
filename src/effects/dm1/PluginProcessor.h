// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include <JuceHeader.h>
#include "DM1DSP.h"

class DM1AudioProcessor : public juce::AudioProcessor
{
public:
    DM1AudioProcessor();
    ~DM1AudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Tap tempo. Call from the message thread only (TAP button / footswitch).
    // Averages the last few tap intervals and sets delayTime = beat x tapDivision.
    void tapTempo();

    juce::AudioProcessorValueTreeState apvts;

    // read by the editor
    std::atomic<float> outputLevel { 0.0f };   // output peak 0..1, decaying
    std::atomic<bool>  tapLedOn { false };     // blinks once per delay period

    // tapDivision (1..5) -> multiplier of the tapped beat; matches the panel LEDs left to right
    static float divisionMultiplier (int division);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    dm1::Params readParams() const;

    std::atomic<float>* pModRate, *pModDepth, *pVariation, *pDelayTime, *pIntensity,
                       *pDelayVolume, *pMode, *pTapDivision, *pPower;

    dm1::Engine engine;
    float meterOut = 0.0f;

    // tap-tempo state, message thread only
    static constexpr int kMaxTaps = 4;
    double lastTapSec = -100.0;
    double tapIntervals[kMaxTaps] {};
    int numTapIntervals = 0, tapWriteIdx = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DM1AudioProcessor)
};
