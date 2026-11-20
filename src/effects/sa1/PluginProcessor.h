// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "BypassLink.h"
#include "MidiMap.h"
#include "plugin/LabProcessor.h"

#include <JuceHeader.h>
#include "ui/Presets.h"

// SA-1 Spectrum Analyzer: a pure analysis plugin. The audio passes through bit-exact (no filters, no gain); the last 32768 samples of each channel are kept in a ring
// for the editor, which does the FFT, the averaging, the peak finder and the fundamental / harmonic detection (see .claude/spectrum_analyser_specs.md).
// Host parameters: power, bypass, mode (spectrum, spectrogram, octave bands), fftSize (1K .. 16K), channel (left, right, sum), range (60, 90, 120 dB), smooth, avg, display, scale,
// bands (1/3 or 1 octave), speed (spectrogram), hold, freeze, harmonics, compare.
class SA1AudioProcessor : public vibr::LabProcessor
{
public:
    SA1AudioProcessor();
    ~SA1AudioProcessor() override = default;

    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SA-1"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    static constexpr int kRing = 32768;                 // a power of two
    std::atomic<float> ringL[kRing], ringR[kRing];      // the input, written by the audio thread, read by the editor
    std::atomic<int> ringPos { 0 };                     // the next write position

    // the snapshot for the A / B comparison (the averaged power per FFT bin and its bin width); only the editor on the message thread touches it. Not saved with the session.
    std::vector<float> snapPow;
    float snapBinHz = 0.0f;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SA1AudioProcessor)
};
