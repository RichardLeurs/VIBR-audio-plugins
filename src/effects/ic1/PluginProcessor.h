// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "BypassLink.h"

#include <JuceHeader.h>
#include "MidiMap.h"
#include "plugin/LabProcessor.h"
#include "CircuitEngine.h"
#include "ui/Presets.h"
#include "ui/OutputMeter.h"

// IC-1 Infinite Circuit: a four-node feedback network (see CircuitEngine.h).
// Host parameters: power, bypass, route, inject, filter, time, spread, f1 .. f4, cross, regen, selfosc, hold, cutoff, res, tilt, phase, rate, depth, drive, mix, level.
class IC1AudioProcessor : public vibr::LabProcessor
{
public:
    IC1AudioProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "IC-1"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 20.0; }


    ic1::Engine engine;                                           // the editor reads the node levels and sends EXCITE

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::atomic<float> *pPower {}, *pRoute {}, *pInject {}, *pFilter {}, *pTime {}, *pSpread {}, *pF[ic1::kNodes] {}, *pCross {}, *pRegen {}, *pSelfOsc {}, *pHold {},
                       *pCutoff {}, *pRes {}, *pTilt {}, *pPhase {}, *pRate {}, *pDepth {}, *pDrive {}, *pMix {}, *pLevel {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IC1AudioProcessor)
};
