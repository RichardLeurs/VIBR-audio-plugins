// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "BypassLink.h"

#include <JuceHeader.h>
#include "MidiMap.h"
#include "plugin/LabProcessor.h"
#include "TimeEngine.h"
#include "ui/Presets.h"
#include "ui/OutputMeter.h"

// TF-1 Time Fracture: a 16-step pattern of repeats, reversals, scrambles, pitch jumps and stretches over a rolling buffer (see TimeEngine.h).
// Host parameters: power, bypass, s1 .. s16 (the operation of each step), length, division, hold, tempo, chance, loop, scramble, pitch, stretch, crush, mix, level, and the
// Time Lab modes: mode (Steps / Scrub / Grains), scrubMode, scrubFreeze, scrubSync, scrubLength, scrubSpeed, scrubSmooth, scrubRandom, scrubRepeats (the RV-1) and
// grainShape, grainFreeze, grainSync, grainSize, grainDensity, grainPosition, grainSpray, grainPitch, grainRandom, grainReverse, grainSpread (the GR-1).
class TF1AudioProcessor : public vibr::LabProcessor
{
public:
    TF1AudioProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "TF-1"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 30.0; }               // the longest tail of the three modes (SCRUB, the RV-1 engine)


    tf1::Engine engine;                                           // the editor reads the display data

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::atomic<float> *pPower {}, *pStep[tf1::kSteps] {}, *pLength {}, *pDivision {}, *pHold {}, *pTempo {}, *pChance {}, *pLoop {}, *pScramble {}, *pPitch {}, *pStretch {}, *pCrush {}, *pMix {}, *pLevel {};
    std::atomic<float> *pMode {}, *pScrubMode {}, *pScrubFreeze {}, *pScrubSync {}, *pScrubLength {}, *pScrubSpeed {}, *pScrubSmooth {}, *pScrubRandom {}, *pScrubRepeats {};     // SCRUB (the RV-1)
    std::atomic<float> *pGrainShape {}, *pGrainFreeze {}, *pGrainSync {}, *pGrainSize {}, *pGrainDensity {}, *pGrainPosition {}, *pGrainSpray {}, *pGrainPitch {}, *pGrainRandom {}, *pGrainReverse {}, *pGrainSpread {};     // GRAINS (the GR-1)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TF1AudioProcessor)
};
