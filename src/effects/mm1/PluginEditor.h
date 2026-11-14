// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"

// Black knurled knob with a raised cap and a white pointer (same as the DM-1), drawn over the scales printed in the background.
class MemoryManLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

class MemoryManAudioProcessorEditor : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit MemoryManAudioProcessorEditor (MemoryManAudioProcessor&);
    ~MemoryManAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void paintOverChildren (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void syncChanged();

    struct Knob
    {
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
    };

    MemoryManAudioProcessor& proc;
    MemoryManLookAndFeel laf;
    juce::ComponentBoundsConstrainer constrainer;
    juce::Image background;
    MidiLearnUI midiUI { proc.midiMap };       // right-click a knob: MIDI learn
    Knob knobs[6];
    bool lastOn = true;
    // SYNC button under the DELAY knob (drawn in code over the background)
    juce::ParameterAttachment syncAtt, divisionAtt;
    bool syncOn = false;
    int division = 4;
    std::unique_ptr<juce::SliderParameterAttachment> divisionKnobAtt;      // the DELAY knob while sync is on

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MemoryManAudioProcessorEditor)
};
