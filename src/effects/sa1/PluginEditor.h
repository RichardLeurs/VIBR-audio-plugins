// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/VintageLabEditor.h"

// SA-1 window (1120 x 660) in the "vintage hardware" look: the ANALYZER display with three modes (tabs on its edge): SPECTRUM (live FFT, peak hold, harmonic markers, a frequency
// cursor), SPECTROGRAM (frequency over time, colour coded) and OCTAVE (1/3 or 1 octave bars); SNAPSHOT / COMPARE / CLEAR keep a spectrum and lay it over the live one.
// Under it ANALYSIS (FFT size, channel, range, hold / freeze / harmonics), TRACE (the options of the mode) and READOUT (cursor, fundamental, the strongest peaks).
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
    static constexpr int kCols = 600;                       // display columns across the plot
    static constexpr int kMaxBands = 31;                    // 1/3 octave bands 20 Hz .. 20 kHz (10 for whole octaves)
    static constexpr int kSpecW = 480, kSpecH = 300;        // the spectrogram image: time across (newest right), frequency up
    struct Peak { float hz = 0.0f, db = -200.0f; };

    void timerCallback() override;
    void analyse();                                         // one FFT frame: average, columns, bands, hold, peaks, fundamental
    void computeColumns (const std::vector<float>& pow, float bHz, std::array<float, kCols>& out) const;      // power per bin -> dB per display column (peak of the bins, smoothed)
    int computeBands (const std::vector<float>& pow, float bHz, std::array<float, kMaxBands>& out) const;     // power per bin -> dB per octave band; returns the number of bands
    void pushSpectrogramColumn();
    void paintDisplay (juce::Graphics&);
    void paintSpectrum (juce::Graphics&, juce::Rectangle<float> plot, float rangeDb);
    void paintSpectrogram (juce::Graphics&, juce::Rectangle<float> plot, float rangeDb);
    void paintBands (juce::Graphics&, juce::Rectangle<float> plot, float rangeDb);
    void paintBandLabels (juce::Graphics&, juce::Rectangle<float> plot);
    void paintReadout (juce::Graphics&);
    float colFreq (float col) const;                        // the frequency at a (fractional) display column
    float freqToX (float hz) const;
    float freqToY (float hz) const;                         // for the spectrogram: frequency runs up the plot
    bool hasSnapshot() const { return ! proc.snapPow.empty() && proc.snapBinHz > 0.0f; }

    SA1AudioProcessor& proc;
    int fftOrder = 0, specTick = 0, numBands = kMaxBands;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> window;
    std::vector<float> fftData, avgPow;                     // the FFT work buffer; the averaged power per bin
    std::array<float, kCols> colDb {}, holdDb {}, snapDb {};          // the displayed spectrum, its peak hold and the snapshot (dB)
    std::array<float, kMaxBands> bandDb {}, bandHold {}, snapBand {}; // the same per octave band
    juce::Image specImage { juce::Image::ARGB, kSpecW, kSpecH, true };
    std::array<Peak, 5> peaks {};
    int numPeaks = 0;
    float fundHz = 0.0f, fundDb = -200.0f, binHz = 1.0f;    // detected fundamental (0 = none)
    juce::Point<float> hoverP { -1.0f, -1.0f };             // the mouse over the plot (design units), x < 0 = not there

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SA1AudioProcessorEditor)
};
