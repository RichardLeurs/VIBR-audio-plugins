// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "BypassLink.h"

#include <JuceHeader.h>
#include "MidiMap.h"
#include "AX1Engine.h"
#include "ui/Presets.h"

// AX-1 Amp Lab: Raw Power (.claude/ampcabinet_spec.md): gate, tight boost, 14 amp types, tone stack, two impulse-response cabinets (or a filter model), EQ, ambience, limiter.
// Host parameters: power, bypass, inGain, gateOn, gateThresh, gateRelease, boostOn, boostDrive, boostLevel, boostTight, ampType, gain, bass, middle, treble, presence, resonance,
// sag, master, bright, deep, cabOn, irALevel, irBLevel, lowCut, highCut, mic, ambience, eqOn, eqLow, eqMid, eqFreq, eqHigh, out, limiter.
// The latency (the oversampler) is reported to the host and is the same in every type. The impulse responses are part of the session state, not of the presets.
class AX1AudioProcessor : public juce::AudioProcessor,
      private juce::AsyncUpdater
{
public:
    AX1AudioProcessor();
    ~AX1AudioProcessor() override { cancelPendingUpdate(); }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "AX-1"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0 + (double) juce::jmax (irSeconds[0].load(), irSeconds[1].load()); }
    juce::AudioProcessorParameter* getBypassParameter() const override { return apvts.getParameter ("bypass"); }

    // the factory presets are the host's programs (the preset menu in the editor stays in step through the preset name)
    int getNumPrograms() override { return juce::jmax (1, (int) presets.factory().size()); }
    int getCurrentProgram() override { return currentProgram.load(); }
    void setCurrentProgram (int index) override
    {
        index = juce::jlimit (0, getNumPrograms() - 1, index);
        if (index == currentProgram.load()) return;   // a host re-sending the current program must not reset the user's edits
        currentProgram.store (index);
        if (juce::MessageManager::getInstance()->isThisTheMessageThread()) presets.applyFactory (index);
        else { pendingProgram.store (index); triggerAsyncUpdate(); }
    }
    const juce::String getProgramName (int index) override
    {
        const auto& list = presets.factory();
        return index >= 0 && index < (int) list.size() ? list[(size_t) index].name : juce::String();
    }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- impulse responses (message thread): slot 0 = A, 1 = B ----
    static constexpr int kCurvePoints = 160;                                       // the display curve of an IR: 20 Hz .. 20 kHz, log spaced, dB
    static constexpr float kMaxIrSeconds = 3.0f;
    bool loadImpulseResponse (int slot, const juce::File& file, juce::String* error = nullptr);
    void clearImpulseResponse (int slot);
    juce::String getImpulseName (int slot) const { const juce::ScopedLock sl (irLock); return irName[slot]; }    // empty = none loaded (filter model)
    bool hasImpulse (int slot) const { return engine.hasIr (slot); }
    void copyIrCurve (int slot, float* dest) const { const juce::SpinLock::ScopedLockType sl (curveLock); std::copy (irCurve[slot].begin(), irCurve[slot].end(), dest); }
    static juce::File irFolder() { return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("VIBR").getChildFile ("AX-1").getChildFile ("IRs"); }

    juce::UndoManager undoManager { 30000, 100 };
    juce::AudioProcessorValueTreeState apvts;
    MidiMap midiMap { apvts, { "gain", "bass", "middle", "treble", "presence", "resonance", "sag", "master", "boostDrive", "gateThresh", "irALevel", "irBLevel", "eqMid", "ambience", "out", "inGain",
                               "gateOn", "boostOn", "cabOn", "eqOn", "limiter" } };   // CC -> knob / switch, MIDI learn
    vibr::BypassLink bypassLink { apvts };                // host bypass <-> OFF / ON switch
    vibr::PresetManager presets;
    std::atomic<int> currentProgram { 0 };
    std::atomic<int> pendingProgram { -1 };
    std::atomic<int> uiTab { 0 }, uiDisplayTab { 0 };    // the tabs the editor shows (kept while the window is closed)
    const ax1::Engine& getEngine() const { return engine; }
    ax1::Engine& getEngine() { return engine; }
    ax1::Params readParams() const;                      // the current settings (the editor draws the curves from them)
    double getSampleRate() const { return sr; }

private:
    void handleAsyncUpdate() override { const int i = pendingProgram.exchange (-1); if (i >= 0) presets.applyFactory (i); }
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    bool loadIrData (int slot, const juce::MemoryBlock& data, const juce::String& name, juce::String* error);

    ax1::Engine engine;
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AX1AudioProcessor)
};
