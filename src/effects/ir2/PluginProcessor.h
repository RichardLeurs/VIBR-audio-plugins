// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include <JuceHeader.h>
#include "IR2DSP.h"

class IR2AudioProcessor : public juce::AudioProcessor
{
public:
    IR2AudioProcessor();
    ~IR2AudioProcessor() override = default;

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
    double getTailLengthSeconds() const override { return 1.5; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    static constexpr int kMaxChannels = 2;

    std::atomic<float>* pType, *pGain, *pLevel, *pAmbience, *pBass, *pMiddle, *pTreble;
    juce::AudioParameterBool* bypassParam = nullptr;

    static constexpr int kOsFactor = 1;                   // 2x oversampling in the gain stages
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    ir2::AmpPre pre[kMaxChannels];
    ir2::PostChain post[kMaxChannels];
    ir2::Room room;
    juce::SmoothedValue<float> gainSmooth { 5.0f }, levelSmooth { 5.0f }, ambSmooth { 2.0f }, bassSmooth { 5.0f }, midSmooth { 5.0f }, trebSmooth { 5.0f }, wetSmooth { 1.0f };
    double sr = 44100.0;
    int maxBlock = 512, lastType = -1;
    int dipLeft = 0;                                      // samples left of the fade-in after an amp type change

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IR2AudioProcessor)
};
