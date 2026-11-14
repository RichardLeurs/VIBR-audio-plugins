// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"

// Black knob of the VIBR look (see style.md): matt body with a knurled edge, raised cap, white pointer bar.
// The knob is drawn 5 px inside its component so the shadow is not clipped.
class SU1LookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

// SU-1 window, drawn in code on a satin body: header (name, description, power), four panels in a row: LEVEL (gain), PAN (position + auto-pan),
// WIDTH (width, spread, bass mono) and CHANNELS (six buttons), and under them the FIELD display: level bars, a goniometer and the correlation.
class SU1AudioProcessorEditor : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit SU1AudioProcessorEditor (SU1AudioProcessor&);
    ~SU1AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    float scale() const { return (float) getWidth() / 1200.0f; }
    void layoutKnobs();
    bool isOn (const char* id) const { return proc.apvts.getRawParameterValue (id)->load() > 0.5f; }
    int solo() const { return juce::jlimit (0, 2, (int) std::lround (proc.apvts.getRawParameterValue ("solo")->load())); }
    void toggle (const char* id);
    void setChoice (const char* id, int index);
    void drawField (juce::Graphics&);

    struct Knob
    {
        const char* id;
        int panel, index, count;      // panel number, position in the panel, knobs in the panel
        const char* name;
        int kind;                     // 0 gain (dB), 1 pan, 2 rate (Hz), 3 percent, 4 width (%), 5 bass mono (Hz)
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> att;
    };

    SU1AudioProcessor& proc;
    SU1LookAndFeel laf;
    juce::ComponentBoundsConstrainer constrainer;
    juce::Image grain;
    std::array<Knob, 7> knobs;
    juce::ParameterAttachment powerAtt, invLAtt, invRAtt, swapAtt, monoAtt, soloAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SU1AudioProcessorEditor)
};
