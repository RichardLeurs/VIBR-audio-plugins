// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "BypassLink.h"

#include <JuceHeader.h>
#include "DriveEngine.h"
#include "ui/Presets.h"

// DV-1 Drive Lab: OVERDRIVE (OD-2, 12 types), DISTORTION (DS-1), TUBE, TAPE, FOLD and CRUSH in one plugin (see DriveEngine.h and .claude/lab_specs.md).
// Host parameters: power, mode, variant, odType, drive, tone, boostOn, boost, autoLevel, mix, level, odParam, odLow, odMid, odHigh, gateThresh, gateDecay (OVERDRIVE),
// tight (DISTORTION), tubeBias (TUBE), warmth (TAPE), fold, symmetry (FOLD), bits, rate, crushMix (CRUSH). The latency (the oversampler) is reported to the host.
class DV1AudioProcessor : public juce::AudioProcessor
{
public:
    DV1AudioProcessor();
    ~DV1AudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "DV-1"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    juce::AudioProcessorParameter* getBypassParameter() const override { return apvts.getParameter ("bypass"); }

    // the factory presets are the host's programs (the preset menu in the editor stays in step through the preset name)
    int getNumPrograms() override { return juce::jmax (1, (int) presets.factory().size()); }
    int getCurrentProgram() override
    {
        const auto& list = presets.factory();
        for (int i = 0; i < (int) list.size(); ++i)
            if (list[(size_t) i].name == presets.name()) return currentProgram = i;
        return currentProgram;
    }
    void setCurrentProgram (int index) override
    {
        if (index < 0 || index >= (int) presets.factory().size()) return;
        currentProgram = index;
        if (juce::MessageManager::getInstance()->isThisTheMessageThread()) presets.applyFactory (index);
        else juce::MessageManager::callAsync ([weak = juce::WeakReference<DV1AudioProcessor> (this), index] { if (weak != nullptr) weak->presets.applyFactory (index); });
    }
    const juce::String getProgramName (int index) override
    {
        const auto& list = presets.factory();
        return index >= 0 && index < (int) list.size() ? list[(size_t) index].name : juce::String();
    }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::UndoManager undoManager { 30000, 100 };
    juce::AudioProcessorValueTreeState apvts;
    vibr::BypassLink bypassLink { apvts };                // host bypass <-> OFF / ON switch
    vibr::PresetManager presets;
    int currentProgram = 0;
    JUCE_DECLARE_WEAK_REFERENCEABLE (DV1AudioProcessor)
    const dv1::Engine& getEngine() const { return engine; }
    dv1::Params readParams() const;                                             // the current settings (the editor draws the curves from them)

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    dv1::Engine engine;
    std::atomic<float> *pPower {}, *pMode {}, *pVariant {}, *pOdType {}, *pDrive {}, *pTone {}, *pBoostOn {}, *pBoost {}, *pAutoLevel {}, *pMix {}, *pLevel {},
                       *pOdParam {}, *pOdLow {}, *pOdMid {}, *pOdHigh {}, *pGateThresh {}, *pGateDecay {}, *pTight {}, *pTubeBias {}, *pWarmth {}, *pFold {}, *pSymmetry {},
                       *pBits {}, *pRate {}, *pCrushMix {};
    juce::SmoothedValue<float> powerSmooth;
    juce::AudioBuffer<float> dryBuf;
    int delayPos = 0;
    std::vector<float> powerDelay[2];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DV1AudioProcessor)
};
