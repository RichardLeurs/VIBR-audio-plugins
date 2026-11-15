// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/VibrUI.h"

//==============================================================================
// Black ribbed knobs on a silver skirt, drawn over the scale printed in the background.
class CE1LookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;

    // switches are painted by the editor; the buttons are only click targets
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override {}
};

//==============================================================================
class CE1AudioProcessorEditor : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit CE1AudioProcessorEditor (CE1AudioProcessor&);
    ~CE1AudioProcessorEditor() override;

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
    struct Switch
    {
        juce::ToggleButton button;
        std::unique_ptr<juce::ButtonParameterAttachment> attachment;
    };

    void addKnob (Knob&, const juce::String& paramId);
    void addSwitch (Switch&, const juce::String& paramId);

    CE1AudioProcessor& proc;
    CE1LookAndFeel laf;
    juce::Image background;   // rendered panel (make_background.py); switches, lamps and knobs are drawn live on top

    MidiLearnUI midiUI { proc.midiMap };       // right-click a knob: MIDI learn
    Knob level, chorus, depth, rate;
    Switch inputSwitch, effectSwitch, powerSwitch;

    bool peakLit = false;
    int peakHold = 0;

    // SYNC button in the VIBRATO panel (code-drawn); declared after the knobs because their callbacks dim the RATE knob
    bool syncOn = false;
    int division = 3;
    juce::ParameterAttachment syncAtt, divisionAtt;
    std::unique_ptr<juce::SliderParameterAttachment> divisionKnobAtt;      // the RATE knob while sync is on

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CE1AudioProcessorEditor)
};
