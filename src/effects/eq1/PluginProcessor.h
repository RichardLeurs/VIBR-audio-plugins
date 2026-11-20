// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "BypassLink.h"
#include "MidiMap.h"
#include "plugin/LabProcessor.h"

#include <JuceHeader.h>
#include "../../mastering/ms1/MasteringEngine.h"          // ms1::Eq: the 8 band equalizer of the MS-1 (no JUCE dependency)
#include "ui/OutputMeter.h"
#include "ui/Presets.h"

// EQ-1 Graphic EQ (test plugin): the equalizer of the MS-1 as a plugin of its own. 8 bands in series, minimum phase (no latency): band 1 a low shelf, 2 .. 7 bells,
// 8 a high shelf; every band works on Stereo, Left, Right, Mid or Side and can be dynamic (its gain follows the level in the band, above or below a threshold).
// Host parameters: power, bypass, outGain and per band n = 1 .. 8: eq<n>On, eq<n>Freq, eq<n>Gain, eq<n>Q, eq<n>Mode, eq<n>Dyn, eq<n>DynThr, eq<n>DynRatio,
// eq<n>DynAtk, eq<n>DynRel, eq<n>DynBelow (the ids of the MS-1, so its settings are the same names).
class EQ1AudioProcessor : public vibr::LabProcessor
{
public:
    EQ1AudioProcessor();
    ~EQ1AudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "EQ-1"; }
    bool acceptsMidi() const override { return true; }          // MIDI learn: a CC moves the OUTPUT knob
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // for the display: how much of its gain a dynamic band applies now (0 .. 1), and the last samples of the output (mono) for the spectrum
    float dynAmount (int band) const { return eq.dynAmount[band].load (std::memory_order_relaxed); }
    static constexpr int kSpec = 4096;
    std::atomic<float> spec[kSpec];
    std::atomic<int> specPos { 0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    ms1::Eq eq;
    struct BandPtrs { std::atomic<float> *on {}, *freq {}, *gain {}, *q {}, *mode {}, *dyn {}, *thr {}, *ratio {}, *atk {}, *rel {}, *below {}; };
    std::array<BandPtrs, ms1::kEqBands> pBand {};
    std::atomic<float> *pPower {}, *pOutGain {};
    juce::SmoothedValue<float> powerSmooth, gainSmooth;
    juce::AudioBuffer<float> dryBuf;
    double sampleRateNow = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQ1AudioProcessor)
};
