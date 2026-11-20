// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/VintageLabEditor.h"

// SA-1 window (1120 x 660) in the "vintage hardware" look: the ANALYZER display (a phosphor screen with the live spectrum, peak hold, harmonic markers and a frequency cursor)
// over ANALYSIS (FFT size, channel, range, peak hold / freeze / harmonics), TRACE (smoothing, averaging, display, scale) and READOUT (cursor, fundamental, the strongest peaks).
// The FFT runs here, on the message thread, from the ring the processor fills; nothing is added to the audio.
class SA1AudioProcessorEditor : public vibr::ui::vintage::LabEditor
{
public:
    explicit SA1AudioProcessorEditor (SA1AudioProcessor&);
    ~SA1AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void analyse();                                         // one FFT frame: average, columns, hold, peaks, fundamental
    void paintDisplay (juce::Graphics&);
    void paintReadout (juce::Graphics&);
    float colFreq (float col) const;                        // the frequency at a (fractional) display column
    float freqToX (float hz) const;

    static constexpr int kCols = 600;                       // display columns across the plot
    struct Peak { float hz = 0.0f, db = -200.0f; };

    SA1AudioProcessor& proc;
    int fftOrder = 0;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> window;
    std::vector<float> fftData, avgPow;                     // the FFT work buffer; the averaged power per bin
    std::array<float, kCols> colDb {}, holdDb {};           // the displayed spectrum and its peak hold (dB)
    std::array<Peak, 5> peaks {};
    int numPeaks = 0;
    float fundHz = 0.0f, fundDb = -200.0f, binHz = 1.0f;    // detected fundamental (0 = none)
    float hoverX = -1.0f;                                   // the mouse in the plot (design units), < 0 = not there

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SA1AudioProcessorEditor)
};
