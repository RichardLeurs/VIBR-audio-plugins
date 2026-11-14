// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"

// Black knurled knob with a raised cap and a white pointer (same as the DM-1), drawn over the scales printed in the background.
class TR2LookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

class TR2AudioProcessorEditor : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit TR2AudioProcessorEditor (TR2AudioProcessor&);
    ~TR2AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void paintOverChildren (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void syncChanged (bool on);

    struct Knob
    {
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
    };

    TR2AudioProcessor& proc;
    TR2LookAndFeel laf;
    juce::ComponentBoundsConstrainer constrainer;
    juce::Image background;
    MidiLearnUI midiUI { proc.midiMap };       // right-click a knob: MIDI learn
    Knob rate, wave, depth;
    bool lastOn = true;
    bool syncOn = false;
    int division = 3;
    std::unique_ptr<juce::SliderParameterAttachment> divisionKnobAtt;      // the RATE knob while sync is on
    // declared last so they are destroyed first (their callbacks touch the members above)
    juce::ParameterAttachment syncAtt, divisionAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TR2AudioProcessorEditor)
};
