// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "BypassLink.h"

#include <JuceHeader.h>
#include "MidiMap.h"
#include "plugin/LabProcessor.h"
#include "AL1Engine.h"
#include "ui/Presets.h"

// AL-1 Amp Lab: Raw Power (.claude/ampcabinet_spec.md): gate, tight boost, 14 amp types, tone stack, two impulse-response cabinets (or a filter model), EQ, ambience, limiter.
// Host parameters: power, bypass, inGain, gateOn, gateThresh, gateRelease, boostOn, boostDrive, boostLevel, boostTight, ampType, gain, bass, middle, treble, presence, resonance,
// sag, master, bright, deep, cabOn, irALevel, irBLevel, lowCut, highCut, mic, ambience, eqOn, eqLow, eqMid, eqFreq, eqHigh, out, limiter.
// The latency (the oversampler) is reported to the host and is the same in every type. The impulse responses are part of the session state, not of the presets.
class AL1AudioProcessor : public vibr::LabProcessor
{
public:
    AL1AudioProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "AL-1"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0 + (double) juce::jmax (irSeconds[0].load(), irSeconds[1].load()); }

    // ---- impulse responses (message thread): slot 0 = A, 1 = B ----
    static constexpr int kCurvePoints = 160;                                       // the display curve of an IR: 20 Hz .. 20 kHz, log spaced, dB
    static constexpr float kMaxIrSeconds = 3.0f;
    bool loadImpulseResponse (int slot, const juce::File& file, juce::String* error = nullptr);
    void clearImpulseResponse (int slot);
    juce::String getImpulseName (int slot) const { const juce::ScopedLock sl (irLock); return irName[slot]; }    // empty = none loaded (filter model)
    bool hasImpulse (int slot) const { return engine.hasIr (slot); }
    void copyIrCurve (int slot, float* dest) const { const juce::SpinLock::ScopedLockType sl (curveLock); std::copy (irCurve[slot].begin(), irCurve[slot].end(), dest); }
    static juce::File irFolder() { return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("VIBR").getChildFile ("AL-1").getChildFile ("IRs"); }

    std::atomic<int> uiTab { 0 }, uiDisplayTab { 0 };    // the tabs the editor shows (kept while the window is closed)
    const al1::Engine& getEngine() const { return engine; }
    al1::Engine& getEngine() { return engine; }
    al1::Params readParams() const;                      // the current settings (the editor draws the curves from them)
    double getSampleRate() const { return sr; }

protected:
    void addExtraState (juce::ValueTree& state) override;
    void restoreExtraState (juce::ValueTree& state) override;
    void afterStateRestored() override;

private:
    juce::ValueTree irStash[2];                                        // the IR children of a state being restored (taken out by restoreExtraState)
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    bool loadIrData (int slot, const juce::MemoryBlock& data, const juce::String& name, juce::String* error);

    al1::Engine engine;
    std::atomic<float> *pPower {}, *pInGain {}, *pGateOn {}, *pGateThresh {}, *pGateRelease {}, *pBoostOn {}, *pBoostDrive {}, *pBoostLevel {}, *pBoostTight {}, *pAmpType {},
                       *pGain {}, *pBass {}, *pMiddle {}, *pTreble {}, *pPresence {}, *pResonance {}, *pSag {}, *pMaster {}, *pBright {}, *pDeep {}, *pCabOn {}, *pIrALevel {}, *pIrBLevel {},
                       *pLowCut {}, *pHighCut {}, *pMic {}, *pAmbience {}, *pEqOn {}, *pEqLow {}, *pEqMid {}, *pEqFreq {}, *pEqHigh {}, *pOut {}, *pLimiter {};
    juce::SmoothedValue<float> powerSmooth;
    juce::AudioBuffer<float> dryBuf;
    int delayPos = 0;
    std::vector<float> powerDelay[2];
    double sr = 44100.0;

    juce::CriticalSection irLock;
    juce::MemoryBlock irData[2];
    juce::String irName[2];
    std::atomic<float> irSeconds[2] { 0.0f, 0.0f };
    juce::SpinLock curveLock;
    std::array<float, kCurvePoints> irCurve[2] {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AL1AudioProcessor)
};
