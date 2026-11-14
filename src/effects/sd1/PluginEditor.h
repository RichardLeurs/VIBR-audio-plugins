// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/VibrUI.h"

// Black knurled knob with a raised cap and a white pointer (same as the DM-1), drawn over the scales printed in the background.
class SDD1000LookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

class SDD1000AudioProcessorEditor : public juce::AudioProcessorEditor,
                                    private juce::Timer
{
public:
    explicit SDD1000AudioProcessorEditor (SDD1000AudioProcessor&);
    ~SDD1000AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void paintOverChildren (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void syncChanged();                                   // TIME knob <-> division, dims FACTOR

    struct Knob
    {
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
    };

    SDD1000AudioProcessor& proc;
    SDD1000LookAndFeel laf;
    juce::ComponentBoundsConstrainer constrainer;
    juce::Image background;
    MidiLearnUI midiUI { proc.midiMap };       // right-click a knob: MIDI learn
    Knob knobs[9];

    // tempo sync pills in the DELAY TIME panel (code-drawn over the background)
    bool syncOn = false;
    int division = 4;
    juce::ParameterAttachment syncAtt, divisionAtt;
    std::unique_ptr<juce::SliderParameterAttachment> divisionKnobAtt;      // the TIME knob while sync is on

    float meterDb = -60.0f;
    int lastSegs = -1;
    bool lastPower = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SDD1000AudioProcessorEditor)
};
