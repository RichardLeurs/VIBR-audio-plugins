// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include <JuceHeader.h>
#include "MA1DSP.h"

class MA1AudioProcessor : public juce::AudioProcessor
{
public:
    MA1AudioProcessor();
    ~MA1AudioProcessor() override = default;

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
    double getTailLengthSeconds() const override { return 1.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    // ---- impulse responses (message thread): slot 0 = A, 1 = B ----
    bool loadImpulseResponse (int slot, const juce::File& file);
    void clearImpulseResponse (int slot);
    juce::String getImpulseName (int slot) const { return irName[slot]; }     // empty = none loaded (factory cab model)

    std::atomic<float> gateOpenLamp { 0.0f }, limiterLamp { 0.0f };           // read by the editor
    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void loadIrData (int slot, const juce::MemoryBlock& data);

    static constexpr int kMaxChannels = 2;
    static constexpr int kOsFactor = 3;                    // 8x oversampling in the boost and amp stages

    std::atomic<float>* p (const char* id) { return apvts.getRawParameterValue (id); }
    std::atomic<float>* pInGain, *pGateOn, *pGateThresh, *pGateRelease, *pBoostOn, *pBoostDrive, *pBoostLevel, *pBoostTight,
                        *pAmpType, *pGain, *pBass, *pMiddle, *pTreble, *pPresence, *pResonance, *pSag, *pMaster, *pBright, *pDeep,
                        *pCabOn, *pIrALevel, *pIrBLevel, *pLowCut, *pHighCut, *pEqLow, *pEqMid, *pEqFreq, *pEqHigh, *pOut, *pLimiter;
    juce::AudioParameterBool* bypassParam = nullptr;

    // dsp
    ma1::Gate gate;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    ma1::AmpPre amp[kMaxChannels];
    ma1::Biquad bass[kMaxChannels], middle[kMaxChannels], treble[kMaxChannels], presence[kMaxChannels], resonance[kMaxChannels], deep[kMaxChannels],
                lowCut1[kMaxChannels], lowCut2[kMaxChannels], highCut1[kMaxChannels], highCut2[kMaxChannels],
                eqLow[kMaxChannels], eqMid[kMaxChannels], eqHigh[kMaxChannels];
    ma1::FactoryCab factory[kMaxChannels];
    ma1::Limiter limiter;
    juce::dsp::Convolution conv[2];
    juce::AudioBuffer<float> irBuf[2];
    std::atomic<bool> irLoaded[2] { false, false };
    juce::MemoryBlock irData[2];
    juce::String irName[2];

    juce::SmoothedValue<float> inSmooth, gainSmooth, boostSmooth, masterSmooth, outSmooth, sagSmooth, wetSmooth;
    float gateGain = 0.0f;
    double sr = 44100.0;
    int maxBlock = 512, lastType = -1;
    int dipLeft = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MA1AudioProcessor)
};
