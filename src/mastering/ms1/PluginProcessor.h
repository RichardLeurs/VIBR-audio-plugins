// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include <JuceHeader.h>
#include "MasteringEngine.h"
#include "MasteringStages.h"
#include "ReferencePlayer.h"
#include "ui/Presets.h"

// MS-1 Mastering Suite, version 0.8: input trim (gain, polarity, rumble high-pass) -> corrective EQ (8 bands, minimum or linear phase) -> multiband compressor (minimum or linear phase crossovers) -> stereo imager -> colour -> bus compressor -> limiter (true-peak safe, optional loudness target) -> output (gain, dither), with loudness,
// true-peak and spectrum meters (see MasteringEngine.h and .claude/mastering_specs.md).
// Host parameters: power, inGain, limGain, limCeiling, limRelease, limLook, limChar, limLink, limTarget, limAuto, target, ditherBits, ditherShape,
// outGain, delta, eqOn and per band n = 1 .. 8: eq<n>On, eq<n>Freq, eq<n>Gain, eq<n>Q, eq<n>Mode, busOn, busThresh, busRatio, busAttack, busRelease, busMakeup, busMix, busSc,
// mbOn, mbMix, xover1..3, mb<n>On / Solo / Auto / Thresh / Ratio / Attack / Release / Make (n = 1 .. 4), imgOn, imgX1, imgX2, width1..3, monoBelow, imgBalance,
// eqPhase, mbPhase, trimPolarity, trimHpf, monoCheck, limRelAuto, limFreeze, colourOn, colourChar, colourDrive, colourTilt, colourMix, colourOut, colourMode, refAB, refMatch, refPlay, refFollow. The latency (lookahead + the true-peak filter) is reported to the host and follows `limLook`.
class MS1AudioProcessor : public juce::AudioProcessor
{
public:
    MS1AudioProcessor();
    ~MS1AudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MS-1"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.2; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::UndoManager undoManager { 30000, 100 };                                   // 100 undo steps of every parameter change (the editor starts a step per gesture)
    juce::AudioProcessorValueTreeState apvts;

    // for the editor (read by the message thread, written by the audio thread)
    ms1::Loudness loudness;
    ms1::Spectrum specPre, specPost;
    ms1::LevelMeter levels;                                                         // sample peak, RMS, PSR window and DR of the output
    ms1::StereoMeter stereoMeter;                                                   // correlation (short-term, integrated) and the goniometer of the output
    // the options of the spectrum display; mode and source work on the audio thread, all three are saved in the state
    enum SpecOption { SpecMode, SpecSource, SpecSlope };                            // mode 0 smooth / 1 FFT 8192; source 0 mid / 1 side / 2 left / 3 right; slope 0 / 1 / 2 = 0 / +3 / +4.5 dB per octave
    int specOption (SpecOption o) const { return o == SpecMode ? specModeOpt.load() : o == SpecSource ? specSourceOpt.load() : specSlopeOpt.load(); }
    void setSpecOption (SpecOption o, int v);
    std::atomic<float> grDb { 0.0f }, tpNowL { -120.0f }, tpNowR { -120.0f }, tpMaxDb { -120.0f }, autoGainDb { 0.0f };
    ms1::MultibandComp mband;
    ms1::Imager imager;
    ReferencePlayer reference;                                                      // the reference track (loaded from the editor)
    std::array<std::atomic<float>, 4> mbGr {};                                      // the gain reduction of the multiband bands (dB), for the display
    std::atomic<float> refMatchDb { 0.0f };                                         // the level match applied to the reference now
    void loadReference (const juce::File& f);

    // presets, A / B and undo (src/common/ui/Presets.h; the editor shows them in the header)
    vibr::PresetManager presets;
    float eqDynAmount (int band) const { return eq.dynAmount[band].load (std::memory_order_relaxed); }
    float busGrDb() const { return bus.grDb.load (std::memory_order_relaxed); }     // the bus compressor's gain reduction now (dB, <= 0)
    // match EQ: the input's long-term spectrum is learned while audio plays; the button fits the 8 EQ bands to the reference (see matchEqFit)
    void resetLearn() { learnReset.store (true); }
    float learnSeconds() const { return (float) learnFrames.load() / 20.0f; }
    bool matchEq();                                                                // false: no reference or not enough input yet
    void requestReset() { resetFlag.store (true); }                 // the RESET button: integrated loudness, range and the true-peak hold start again

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void processChunk (float* l, float* r, int n);          // a part of a block that fits the scratch buffers

    ms1::Eq eq;
    juce::dsp::Convolution linConv;                                                 // the linear-phase EQ (a symmetric FIR from the bands)
    std::atomic<juce::AudioBuffer<float>*> irPending { nullptr }, irTrash { nullptr };
    std::atomic<bool> linMs { false };
    bool wasLinear = false;
    // the linear-phase crossovers of the multiband: 4 band FIRs (zero phase, N / 2 samples of latency), designed on the message thread like the EQ's
    std::array<juce::dsp::Convolution, 4> mbConv;
    std::array<std::atomic<juce::AudioBuffer<float>*>, 4> mbPending, mbTrash;
    std::array<std::array<std::vector<float>, 2>, 4> mbBand;
    std::vector<float> mbKey;
    bool wasMbLinear = false;
    ms1::Butter2 trimHp[2];
    float trimHpHz = -1.0f;
    std::vector<float> irKey;
    std::array<double, ms1::kBands> learnPower {};
    std::atomic<int> learnFrames { 0 };
    std::atomic<bool> learnReset { false };
    struct IrTimer : juce::Timer { MS1AudioProcessor& owner; explicit IrTimer (MS1AudioProcessor& o) : owner (o) {} void timerCallback() override { owner.updateMessageThread(); } };
    IrTimer irTimer { *this };
    void updateMessageThread();                                                     // 20 Hz on the message thread: the linear-phase IR, the learning of the input spectrum
    ms1::Colour colour;
    ms1::BusCompressor bus;
    ms1::Limiter limiter;
    ms1::Dither dither;
    ms1::TruePeak meterTp[2];
    std::atomic<float> *pPower {}, *pInGain {}, *pLimGain {}, *pCeiling {}, *pRelease {}, *pLook {}, *pChar {}, *pLink {}, *pTarget {}, *pAuto {},
                       *pBits {}, *pShape {}, *pOutGain {}, *pDelta {}, *pEqOn {}, *pEqPhase {}, *pMbPhase {}, *pPolarity {}, *pHpf {}, *pMonoCheck {}, *pLimRelAuto {}, *pLimFreeze {}, *pBusOn {}, *pBusThresh {}, *pBusRatio {}, *pBusAttack {}, *pBusRelease {}, *pBusMakeup {}, *pBusMix {}, *pBusSc {};
    struct EqPtrs { std::atomic<float> *on {}, *freq {}, *gain {}, *q {}, *mode {}; };
    std::array<EqPtrs, ms1::kEqBands> pEq {};
    struct EqDynPtrs { std::atomic<float> *dyn {}, *thr {}, *ratio {}, *atk {}, *rel {}, *below {}; };
    std::array<EqDynPtrs, ms1::kEqBands> pEqDyn {};
    std::atomic<float> *pMbKnee {}, *pMbRms {}, *pMbSc {}, *pImgSafe {}, *pImgSafeThr {};
    struct MbPtrs { std::atomic<float> *on {}, *solo {}, *autoRel {}, *thresh {}, *ratio {}, *attack {}, *release {}, *make {}; };
    std::array<MbPtrs, 4> pMb {};
    std::atomic<float> *pMbOn {}, *pMbMix {}, *pXover[3] {}, *pImgOn {}, *pImgX[2] {}, *pWidth[3] {}, *pMonoBelow {}, *pImgBalance {},
                       *pColOn {}, *pColChar {}, *pColDrive {}, *pColTilt {}, *pColMix {}, *pColOut {}, *pColMode {}, *pRefAB {}, *pRefMatch {}, *pRefPlay {}, *pRefFollow {};
    std::array<std::vector<float>, 2> refBuf;
    float refAct = 0.0f, refGainDb = 0.0f;
    juce::SmoothedValue<float> powerSmooth, inSmooth, outSmooth;
    std::array<std::vector<float>, 2> dryDelay, dryOut;     // the delay line of the dry sound and the delayed dry sound of the block being processed
    int dryPos = 0, lastLatency = -1;
    float autoGain = 0.0f, sampleRateNow = 48000.0f;
    bool lastAuto = false;
    std::atomic<bool> resetFlag { false };
    std::atomic<int> specModeOpt { 0 }, specSourceOpt { 0 }, specSlopeOpt { 2 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MS1AudioProcessor)
};
