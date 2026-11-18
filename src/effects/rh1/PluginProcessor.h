// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "ui/OutputMeter.h"
#include "BypassLink.h"
#include "MidiMap.h"
#include "plugin/LabProcessor.h"

#include <JuceHeader.h>
#include "RhythmEngine.h"
#include "ui/Presets.h"

// RH-1 Rhythm Lab: a control engine (LFO, envelope, 16 steps, sync, swing) that moves a FILTER (MF-1), a TREMOLO (TR-2) or a SLICER (SL-2) (see RhythmEngine.h and .claude/lab_specs.md).
// Host parameters: power, mode, variant, source, sync, division, followHost, tempo, rate, shape, depth, sens, decay, swing, glide, step1 .. step16, cutoff, res (FILTER),
// tremStereo, tremBias (TREMOLO), slAttack, slDuty, slBalance (SLICER), mix, level; and for the destination MATRIX (the VC-1 modulation matrix): vSync, vType, vShape1, vShape2,
// vCutoff, vRes, vEnvSens, vEnvRel, vRate1, vRate2, vRand, vXmod, vm00 .. vm43 (source x destination depth). There is no latency.
class RH1AudioProcessor : public vibr::LabProcessor
{
public:
    RH1AudioProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "RH-1"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    const rh1::Engine& getEngine() const { return engine; }
    float hostBpm() const { return bpmNow.load(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    rh1::Engine engine;
    std::atomic<float> bpmNow { 120.0f };
    std::atomic<float> *pPower {}, *pMode {}, *pVariant {}, *pSource {}, *pSync {}, *pDivision {}, *pFollow {}, *pTempo {}, *pRate {}, *pShape {}, *pDepth {}, *pSens {}, *pDecay {}, *pSwing {}, *pGlide {},
                       *pCutoff {}, *pRes {}, *pTremStereo {}, *pTremBias {}, *pSlAttack {}, *pSlDuty {}, *pSlBalance {}, *pMix {}, *pLevel {};
    std::atomic<float>* pSteps[rh1::kSteps] {};
    std::atomic<float> *pVSync {}, *pVType {}, *pVShape1 {}, *pVShape2 {}, *pVCutoff {}, *pVRes {}, *pVEnvSens {}, *pVEnvRel {}, *pVRate1 {}, *pVRate2 {}, *pVRand {}, *pVXmod {};       // MATRIX (the VC-1)
    std::atomic<float>* pVMatrix[vc1::kNumSources][vc1::kNumDests] {};
    float midiValue = 0.0f;                                           // the mod wheel / expression / pressure for the MATRIX source MIDI (0 .. 1)
    juce::SmoothedValue<float> powerSmooth;
    juce::AudioBuffer<float> dryBuf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RH1AudioProcessor)
};
