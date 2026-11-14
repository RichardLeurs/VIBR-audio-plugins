// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/VibrUI.h"

// Black knurled knob with a raised cap and a white pointer (same as the DM-1), drawn over the scales printed in the background.
class BF3LookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

class BF3AudioProcessorEditor : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit BF3AudioProcessorEditor (BF3AudioProcessor&);
    ~BF3AudioProcessorEditor() override;

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

    BF3AudioProcessor& proc;
    BF3LookAndFeel laf;
    juce::ComponentBoundsConstrainer constrainer;
    juce::Image background;
    MidiLearnUI midiUI { proc.midiMap };       // right-click a knob: MIDI learn
    Knob manual, res, depth, rate, mode;
    bool lastOn = true;

    // tempo sync pills under the SWEEP panel (code-drawn); declared after the knobs because their callbacks dim the RATE knob
    bool syncOn = false;
    int division = 3;
    juce::ParameterAttachment syncAtt, divisionAtt;
    std::unique_ptr<juce::SliderParameterAttachment> divisionKnobAtt;      // the RATE knob while sync is on

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BF3AudioProcessorEditor)
};
