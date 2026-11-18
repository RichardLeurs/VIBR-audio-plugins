// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "BypassLink.h"

#include <JuceHeader.h>
#include "MidiMap.h"
#include "plugin/LabProcessor.h"
#include "SpectralEngine.h"
#include "ui/Presets.h"
#include "ui/OutputMeter.h"

// SF-1 Spectral Fold: freeze, shift, smear, blur, morph and fold the spectrum (see SpectralEngine.h).
// Host parameters: power, bypass, freeze, fzLow, fzLowMid, fzHighMid, fzHigh, smear, blur, feedback, shift, spread, ring, ringFreq, drift, morph, win, winCentre,
// winWidth, scan, mix, level; and for the mode HARMONY (the PT-1 harmonizer): mode, hIntel, hRoot, hScale, hInt1 .. hInt4, hDet1 .. hDet4, hLvl1 .. hLvl4, hSpread.
// The latency is one frame (2048 samples), reported to the host and the same in every setting and mode.
class SF1AudioProcessor : public vibr::LabProcessor
{
public:
    SF1AudioProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SF-1"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }


    sf1::Engine engine;                                           // the editor reads the display data and sends CAPTURE

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::atomic<float> *pPower {}, *pFreeze {}, *pFz[sf1::kBands] {}, *pSmear {}, *pBlur {}, *pFeedback {}, *pShift {}, *pSpread {}, *pRing {}, *pRingFreq {}, *pDrift {}, *pMorph {},
                       *pWin {}, *pWinCentre {}, *pWinWidth {}, *pScan {}, *pMix {}, *pLevel {};
    std::atomic<float> *pMode {}, *pHIntel {}, *pHRoot {}, *pHScale {}, *pHSpread {}, *pHInt[pt1::kNumVoices] {}, *pHDet[pt1::kNumVoices] {}, *pHLvl[pt1::kNumVoices] {};       // HARMONY (the PT-1)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SF1AudioProcessor)
};
