// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/VintageLabEditor.h"

// EQ-1 window (1120 x 660) in the "vintage hardware" look: the EQUALIZER display (a phosphor screen with the spectrum of the output, the summed curve and the eight band
// nodes: drag a node for frequency and gain, wheel for Q, double click for 0 dB), under it BAND (tabs EQ / DYNAMIC: the knobs of the selected band, its on / mode buttons
// and the band selector) and OUTPUT (GAIN and the L / R meters).
class EQ1AudioProcessorEditor : public vibr::ui::vintage::LabEditor
{
public:
    explicit EQ1AudioProcessorEditor (EQ1AudioProcessor&);
    ~EQ1AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void timerCallback() override;
    void selectBand (int band);                  // re-binds the band knobs to the parameters of that band
    void layoutKnobs();
    void updateSpectrum();
    int nodeAt (juce::Point<float> designPoint, float radius) const;
    juce::String bandId() const { return "eq" + juce::String (selBand + 1); }
    void endNodeDrag();

    struct Knob
    {
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> att;
    };

    EQ1AudioProcessor& proc;
    vibr::ui::OutputMeter meter;                                    // OUTPUT panel: L / R dB meters
    MidiLearnUI midiUI { proc.midiMap };                            // right-click the OUTPUT knob: MIDI learn
    Knob bandKnobs[7];                                              // FREQ, GAIN, Q (tab EQ), THRESHOLD, RATIO, ATTACK, RELEASE (tab DYNAMIC) of the selected band
    Knob outKnob;
    int selBand = 3, dragBand = -1, tab = 0;                        // selected band 0 .. 7, the band being dragged, 0 EQ / 1 DYNAMIC

    static constexpr int kFftOrder = 11, kFftSize = 1 << kFftOrder, kCols = 240;
    juce::dsp::FFT fft { kFftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) kFftSize, juce::dsp::WindowingFunction<float>::hann };
    std::vector<float> fftData = std::vector<float> ((size_t) kFftSize * 2, 0.0f);
    std::array<float, kCols> spectrumDb {};                         // the smoothed spectrum, one value per display column (log frequency)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQ1AudioProcessorEditor)
};
