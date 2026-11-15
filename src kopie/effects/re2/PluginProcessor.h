// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "BypassLink.h"
#include "MidiMap.h"
#include "TapTempo.h"

#include <JuceHeader.h>
#include "RE2DSP.h"

class RE2AudioProcessor : public juce::AudioProcessor
{
public:
    RE2AudioProcessor();
    ~RE2AudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }
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
    MidiMap midiMap { apvts, { "saturation", "wowFlutter", "mode", "bass", "treble", "reverbVol", "repeatRate", "intensity", "echoVol" } };   // CC -> knob, MIDI learn
    void tapAt (double ms);                      // one tap (ms: juce::Time::getMillisecondCounterHiRes); message thread
    vibr::TapAverager tapAvg;
    vibr::TapTempo midiTap { [this] (double ms) { tapAt (ms); } };   // MIDI tap (note-on or CC85) and the TAP button

    // metering, read by the editor
    std::atomic<float> outputLevel { 0.0f };
    std::atomic<float> inputPeak { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void updateFilters (float bass, float treble, float age);

    std::atomic<float>* pMode, *pRate, *pIntensity, *pEchoVol, *pRevVol,
                       *pBass, *pTreble, *pSat, *pWowFlutter, *pTapeAged, *pInputLine, *pPower,
                       *pSync, *pDivision;

    // panel switches mapped onto the RE-201 engine's 0-10 scale
    float ageValue() const { return *pTapeAged > 0.5f ? 7.0f : 1.5f; }   // NEW / AGED tape
    float inputValue() const { return *pInputLine > 0.5f ? 3.5f : 5.0f; } // LINE comes in hotter, so pad it

    double sr = 44100.0;
    re2::ChannelState channels[2];

    // smoothed controls
    juce::SmoothedValue<float> inGain, echoGain, revGain, fbGain, powerMix;
    float headGain[3] = { 0.0f, 0.0f, 0.0f }, revSend = 0.0f;
    float tapeUnit = 0.09f;       // seconds between heads (smoothed => tape inertia)
    float tapeUnitCoef = 0.001f;
    double hostTempo = 120.0;     // last host tempo (bpm), read once per block; 120 until the host gives one
    float wowPhase = 0.0f, wowPhase2 = 0.0f, flutPhase = 0.0f, noiseLP = 0.0f;
    juce::Random rng;
    float lastBass = -1.0f, lastTreble = -1.0f, lastAge = -1.0f;
    float meterOut = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RE2AudioProcessor)
};
