// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/VintagePedalEditor.h"

// CE-1 in the default Vintage Hardware look: cream plate, bakelite knobs, outlined panels INPUT / CHORUS / VIBRATO / EFFECT, all drawn in code.
class CE1AudioProcessorEditor : public vibr::ui::vintage::PedalEditor,
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
    int presetBarKey = 0;
    void timerCallback() override;
    void syncChanged();
    int stateKey() const;

    struct Knob
    {
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
    };

    CE1AudioProcessor& proc;
    MidiLearnUI midiUI { proc.midiMap };       // right-click a knob: MIDI learn
    Knob level, chorus, depth, rate;
    int lastKey = -1;

    bool inputHigh = true, vibrato = false;
    bool peakLit = false;
    int peakHold = 0;

    // SYNC button in the VIBRATO panel; declared after the knobs because their callbacks touch the RATE knob
    bool syncOn = false;
    int division = 3;
    juce::ParameterAttachment inputAtt, vibratoAtt, syncAtt, divisionAtt;
    std::unique_ptr<juce::SliderParameterAttachment> divisionKnobAtt;      // the RATE knob while sync is on

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CE1AudioProcessorEditor)
};
