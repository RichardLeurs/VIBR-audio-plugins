// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include <JuceHeader.h>
#include "MidiMap.h"
#include "TapTempo.h"
#include "DD1DSP.h"

class DD1AudioProcessor : public juce::AudioProcessor
{
public:
    DD1AudioProcessor();
    ~DD1AudioProcessor() override = default;

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
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    juce::AudioProcessorValueTreeState apvts;
    MidiMap midiMap { apvts, { "aTime", "aRep", "aLevel", "bTime", "bRep", "bLevel", "drive", "boostLevel", "chDepth", "chLevel", "rvDecay", "rvLevel", "rLevel", "rShim", "spread", "tempo" } };   // CC -> knob, MIDI learn
    void tapAt (double ms);                      // one tap (ms: juce::Time::getMillisecondCounterHiRes); message thread
    vibr::TapAverager tapAvg;
    vibr::TapTempo midiTap { [this] (double ms) { tapAt (ms); } };   // MIDI tap (note-on or CC85) and the TAP button

    // factory presets (Edge-style); a preset sets every control except tempo, host/tap source and bypass
    static int getNumPresets();
    static const char* getPresetName (int index);
    void applyPreset (int index);
    void seedFactoryMemories();
    int getPresetIndex() const noexcept { return presetIndex.load(); }
    std::atomic<float> bpmNow { 120.0f };                 // tempo in use (tapped or host), for the display
    std::atomic<float> timeMs[2] { 500.0f, 375.0f };      // current delay times (after tempo sync), for the display

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    struct DelayParams { std::atomic<float>* div, *time, *rep, *level, *mod, *rate; };
    DelayParams pa {}, pb {};
    std::atomic<float>* pTempo, *pHostSync, *pChRate, *pChDepth, *pChLevel, *pRvDecay, *pRvTone, *pRvLevel, *pLowCut, *pHiCut, *pLowCutA, *pHiCutA, *pBoostOn, *pDrive, *pBoostTone, *pBoostLevel, *pDecay, *pTone, *pShim, *pInt, *pRevLevel, *pSeries, *pSpread, *pPingA, *pPingB;
    juce::AudioParameterBool* bypassParam = nullptr;
    std::atomic<int> presetIndex { 0 };

    dd1::Delay delayA, delayB;
    dd1::Delay cleanA, cleanB;     // same times / repeats / filters but NO modulation: these feed the shimmer reverb
    dd1::Boost boostL, boostR;
    float boostG = 0.0f;
    bigsky::Reverb reverb, hall;     // shimmer reverb, and the normal (hall) reverb
    dd1::Chorus chorus;
    std::vector<float> revL, revR, dlL, dlR, hallL, hallR;
    float levA = 0.0f, levB = 0.0f, levR = 0.0f, levCh = 0.0f, levH = 0.0f, engage = 1.0f, spreadS = 0.5f;     // smoothed gains
    double sampleRateHz = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DD1AudioProcessor)
};
