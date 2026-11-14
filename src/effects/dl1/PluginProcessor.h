// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include <JuceHeader.h>
#include "DelayEngine.h"
#include "ui/Presets.h"

// DL-1 Delay Lab: digital (SD-1) and analog (MM-1) delay voicings in one plugin (see DelayEngine.h and .claude/lab_specs.md).
// Host parameters: power, mode, variant, sync, division, time, feedback, low, high, modRate, modDepth, spread, hold, mix, level,
// digFactor, digDirect, digEffect, digInput (DIGITAL), anaBlend, anaDepth, anaGrit (ANALOG).
class DL1AudioProcessor : public juce::AudioProcessor
{
public:
    DL1AudioProcessor();
    ~DL1AudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "DL-1"; }
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

    juce::UndoManager undoManager { 30000, 100 };
    juce::AudioProcessorValueTreeState apvts;
    vibr::PresetManager presets;
    const dl1::Engine& getEngine() const { return engine; }
    float hostBpm() const { return bpmNow.load(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    dl1::Engine engine;
    std::atomic<float> bpmNow { 120.0f };
    std::atomic<float> *pPower {}, *pMode {}, *pVariant {}, *pSync {}, *pDivision {}, *pTime {}, *pFeedback {}, *pLow {}, *pHigh {}, *pModRate {}, *pModDepth {}, *pSpread {}, *pHold {},
                       *pMix {}, *pLevel {}, *pDigFactor {}, *pDigDirect {}, *pDigEffect {}, *pDigInput {}, *pAnaBlend {}, *pAnaDepth {}, *pAnaGrit {},
                       *pTapeWow {}, *pTapeSat {}, *pTapeAge {}, *pTapeSpacing {}, *pDualTimeB {}, *pDualFbB {}, *pDualLevelA {}, *pDualLevelB {}, *pDualHighB {}, *pDualPingA {}, *pDualPingB {}, *pModVoice {}, *pModVariation {}, *pModVolume {};
    juce::SmoothedValue<float> powerSmooth;
    juce::AudioBuffer<float> dryBuf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DL1AudioProcessor)
};
