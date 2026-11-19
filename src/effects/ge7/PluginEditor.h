// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/VintagePedalEditor.h"

// The fader cap of the GE-7 in the Vintage Hardware look: an ink cap with a cream centre line; the slot, scale and value fill are drawn by the editor's paint().
// `scale` = window pixels per design unit (set in resized()).
class GE7FaderLookAndFeel : public vibr::ui::vintage::KnobLookAndFeel
{
public:
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float sliderPos, float, float,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    int getSliderThumbRadius (juce::Slider&) override { return juce::roundToInt (13.0f * scale); }
    float scale = 1.0f;
};

// GE-7 in the default (Vintage Hardware) look: panels EQUALIZER (7 band faders) and LEVEL, drawn in code (no background image).
class GE7AudioProcessorEditor : public vibr::ui::vintage::PedalEditor,
                                private juce::Timer
{
public:
    explicit GE7AudioProcessorEditor (GE7AudioProcessor&);
    ~GE7AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    struct Fader
    {
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
    };

    GE7AudioProcessor& proc;
    GE7FaderLookAndFeel faderLaf;
    Fader band[ge7::kBands], level;
    bool lastOn = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GE7AudioProcessorEditor)
};
