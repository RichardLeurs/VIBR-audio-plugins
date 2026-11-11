// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include <JuceHeader.h>
#include "SpaceEchoDSP.h"

class SpaceEchoAudioProcessor : public juce::AudioProcessor
{
public:
    SpaceEchoAudioProcessor();
    ~SpaceEchoAudioProcessor() override = default;

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
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // metering, read by the editor
    std::atomic<float> outputLevel { 0.0f };
    std::atomic<float> inputPeak { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void updateFilters (float bass, float treble, float age);

    std::atomic<float>* pMode, *pRate, *pIntensity, *pEchoVol, *pRevVol,
                       *pBass, *pTreble, *pInput, *pWowFlutter, *pAge, *pPower;

    double sr = 44100.0;
    spaceecho::ChannelState channels[2];

    // smoothed controls
    juce::SmoothedValue<float> inGain, echoGain, revGain, fbGain, powerMix;
    float headGain[3] = { 0.0f, 0.0f, 0.0f }, revSend = 0.0f;
    float tapeUnit = 0.09f;       // seconds between heads (smoothed => tape inertia)
    float tapeUnitCoef = 0.001f;
    float wowPhase = 0.0f, wowPhase2 = 0.0f, flutPhase = 0.0f, noiseLP = 0.0f;
    juce::Random rng;
    float lastBass = -1.0f, lastTreble = -1.0f, lastAge = -1.0f;
    float meterOut = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpaceEchoAudioProcessor)
};
