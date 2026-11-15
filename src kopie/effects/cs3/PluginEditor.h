// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"

// Black knurled knob with a raised cap and a white pointer (same as the DM-1), drawn over the scales printed in the background.
class CS3LookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

class CS3AudioProcessorEditor : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit CS3AudioProcessorEditor (CS3AudioProcessor&);
    ~CS3AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    struct Knob
    {
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
    };

    CS3AudioProcessor& proc;
    CS3LookAndFeel laf;
    juce::ComponentBoundsConstrainer constrainer;
    juce::Image background;
    MidiLearnUI midiUI { proc.midiMap };       // right-click a knob: MIDI learn
    Knob level, tone, attack, sustain;
    bool lastOn = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CS3AudioProcessorEditor)
};
