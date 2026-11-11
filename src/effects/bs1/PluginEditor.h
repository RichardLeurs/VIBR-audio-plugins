// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "WrapSlider.h"

// Black knurled knob with a raised cap and a white pointer (same as the DM-1), drawn over the scales printed in the background.
class BigSkyLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};


class BigSkyAudioProcessorEditor : public juce::AudioProcessorEditor,
                                   private juce::Timer
{
public:
    explicit BigSkyAudioProcessorEditor (BigSkyAudioProcessor&);
    ~BigSkyAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    juce::String formatValue (int index) const;

    struct Knob
    {
        WrapSlider slider;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
    };

    BigSkyAudioProcessor& proc;
    BigSkyLookAndFeel laf;
    juce::ComponentBoundsConstrainer constrainer;
    juce::Image background;
    Knob knobs[8];
    Knob typeKnob;

    int lastType = -1;
    bool lastOn = true;
    float lastVals[8] {};
    juce::String shown;            // temporary display text (empty = show the type name)
    juce::uint32 shownUntil = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BigSkyAudioProcessorEditor)
};
