// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "BypassLink.h"
#include "TempoSync.h"
#include "MidiMap.h"
#include "TapTempo.h"

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

    // Tap tempo. Call from the message thread only (TAP button / footswitch).
    // Averages the last few tap intervals and sets delayTime = beat x tapDivision.
    // Does nothing while "sync" is on (the delay time then follows the host tempo).
    void tapTempo (double nowSec = -1.0);

    bool syncOn() const { return pSync->load() > 0.5f; }

    juce::AudioProcessorValueTreeState apvts;

    vibr::BypassLink bypassLink { apvts };                // host bypass <-> OFF / ON switch
    MidiMap midiMap { apvts, { "modRate", "modDepth", "variation", "delayTime", "intensity", "delayVolume", "mode" } };   // CC -> knob, MIDI learn
    vibr::TapTempo midiTap { [this] (double ms) { tapTempo (ms * 0.001); } };   // MIDI tap (note-on or CC85)

    // read by the editor
    std::atomic<float> outputLevel { 0.0f };   // output peak 0..1, decaying
    std::atomic<bool>  tapLedOn { false };     // blinks once per delay period
    std::atomic<float> tappedBpm { 120.0f };   // last tapped beat (quarter note) in bpm; the sync tempo when the host gives none

    // tapDivision (1..5) -> multiplier of the tapped beat; matches the panel LEDs left to right
    static float divisionMultiplier (int division);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    dm1::Params readParams() const;
    dm1::Params readParamsSynced();            // readParams() + the host-tempo delay time when "sync" is on (call from the audio thread / prepare)

    std::atomic<float>* pModRate, *pModDepth, *pVariation, *pDelayTime, *pIntensity,
                       *pDelayVolume, *pMode, *pTapDivision, *pPower,
                       *pSync, *pDivision;

    dm1::Engine engine;
    float meterOut = 0.0f;

    // tap-tempo state, message thread only
    static constexpr int kMaxTaps = 4;
    double lastTapSec = -100.0;
    double tapIntervals[kMaxTaps] {};
    int numTapIntervals = 0, tapWriteIdx = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DM1AudioProcessor)
};
