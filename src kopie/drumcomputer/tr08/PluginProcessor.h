// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include <JuceHeader.h>
#include "TR08DSP.h"
#include "KitMapper.h"

class TR08AudioProcessor : public juce::AudioProcessor
{
public:
    static constexpr int kSteps = 16;
    static constexpr int kTracks = 12;      // 0 = accent, 1..11 = BD SD LT MT HT RS CP CB CY OH CH
    static constexpr int kParts = 2;        // variations A / B

    TR08AudioProcessor();
    ~TR08AudioProcessor() override;

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
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Pattern editing (message thread). A rhythm has two variations (A, B); each has a 1st and a 2nd part that continue one
    // another (steps1 + steps2 <= 32 steps). A and B share the step counts and the pre-scale, as on the hardware.
    // Pattern bits are atomics, so the audio thread can read them lock-free.
    bool getStep (int var, int half, int track, int step) const { return (patterns[var][half][track].load() >> step) & 1; }
    void toggleStep (int var, int half, int track, int step) { patterns[var][half][track].fetch_xor ((uint16_t) (1u << step)); }
    void clearTrack (int var, int track) { for (int h = 0; h < 2; ++h) patterns[var][h][track].store (0); }
    void clearAll();                                  // both variations
    void clearVariation (int var);                    // [CLEAR] in PATTERN CLEAR: that variation, 16 + 0 steps
    int getSteps (int half) const { return half == 0 ? steps1.load() : steps2.load(); }
    void setSteps (int half, int steps);              // hold [CLEAR] + step button; 1st part 1..16, 2nd part 0..16
    int getAppliedPreScale() const { return scaleApplied.load(); }
    void applyPreScale() { scaleApplied.store (juce::jlimit (0, 3, juce::roundToInt (pPreScale->load()))); }   // [CLEAR] in 1st PART
    void triggerManual (int inst) { manualMask.fetch_or (1u << inst); }

    // Sample kits (message thread). Scanning and decoding run on a background thread; "" selects the built-in synth.
    juce::StringArray getKitNames() const;           // path-like names, e.g. "More Drumcomputer samples/Linn Drum LM2"
    juce::String getKitName() const;
    bool isLoading() const { return loading.load(); }
    juce::File getSamplesRoot() const;
    void setSamplesRoot (const juce::File&);
    void selectKit (const juce::String& name);
    void rescanKits();

    // how many sample variants the loaded kit has for each voice (0 = synth voice)
    int getVariantCount (int inst) const { return variantCounts[inst].load(); }

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<int> currentStep { -1 };      // index into the playing pattern (1st part then 2nd part), -1 when stopped
    std::atomic<float> displayBpm { 120.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void loadDefaultPattern();

    struct Event { int offset; int inst; float vel; bool accent; };
    static constexpr int kMaxEvents = 256;

    std::atomic<float>* pLevel[tr08::kNumInst] {};
    std::atomic<float>* pVar[tr08::kNumInst] {};
    std::atomic<int> variantCounts[tr08::kNumInst] {};
    std::atomic<float>* pBdTone, *pBdDecay, *pSdTone, *pSdSnappy, *pTune[3], *pAlt[5],
                       *pCyTone, *pCyDecay, *pOhDecay, *pAccent, *pTempo, *pFine, *pRun, *pVariation, *pShuffle, *pMeasures, *pFillPart, *pPreScale;

    std::atomic<uint16_t> patterns[kParts][2][kTracks];     // [variation A/B][1st/2nd part][track]
    std::atomic<uint32_t> manualMask { 0 };
    std::atomic<int> steps1 { 16 }, steps2 { 0 };           // step number of the 1st / 2nd part
    std::atomic<int> scaleApplied { 2 };                    // pre-scale in force (0..3); the switch only takes effect on [CLEAR]

    tr08::DrumKit kit;
    double sr = 44100.0;
    double stepPos = 0.0;          // position in 16th-note steps
    bool wasRunning = false;
    Event events[kMaxEvents];

    // ---- sample kits
    class KitLoader;
    void runJob (bool rescan);
    void install (std::unique_ptr<tr08::SampleSet>);

    std::unique_ptr<KitLoader> loader;
    mutable juce::CriticalSection kitLock;           // guards kits, kitName, samplesRoot (never taken on the audio thread)
    std::vector<tr08kit::Kit> kits;
    juce::String kitName, samplesRoot;
    std::atomic<bool> loading { false };
    std::atomic<const tr08::SampleSet*> currentSet { nullptr };
    std::unique_ptr<tr08::SampleSet> activeSet;
    std::vector<std::unique_ptr<tr08::SampleSet>> retiredSets;   // kept for a few swaps so a running block never reads freed memory

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TR08AudioProcessor)
};
