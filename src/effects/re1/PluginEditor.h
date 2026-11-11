// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"

//==============================================================================
// Black knobs drawn over the scales printed in the background.
class SpaceEchoLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;

    // the power switch is painted by the editor; the button itself is just a click target
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override {}
};

//==============================================================================
class SpaceEchoAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit SpaceEchoAudioProcessorEditor (SpaceEchoAudioProcessor&);
    ~SpaceEchoAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct Knob
    {
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
    };

    void addKnob (Knob&, const juce::String& paramId);

    SpaceEchoAudioProcessor& proc;
    SpaceEchoLookAndFeel laf;

    juce::Image background;   // rendered panel (make_background.py); knobs, VU needle, lamps and the power switch are drawn live on top

    Knob wowFlutter, tapeAge, inputVol, mode, bass, treble, reverbVol, repeatRate, intensity, echoVol;

    juce::ToggleButton powerButton;
    std::unique_ptr<juce::ButtonParameterAttachment> powerAttachment;

    float vuValue = 0.0f;
    bool peakLit = false;
    int peakHold = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpaceEchoAudioProcessorEditor)
};
