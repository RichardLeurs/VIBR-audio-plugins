// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/VintagePedalEditor.h"

// SD-1 in the default (Vintage Hardware) look, drawn in code (no background image): INPUT (headroom screen, LEVEL), DELAY TIME (TIME, FACTOR, SYNC),
// REGENERATION (FEEDBACK, HI-CUT), MODULATION (INTENSITY, FREQUENCY) and OUTPUT (DIRECT, EFFECT).
class SDD1000AudioProcessorEditor : public vibr::ui::vintage::PedalEditor,
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
    MidiLearnUI midiUI { proc.midiMap };       // right-click a knob: MIDI learn
    Knob knobs[9];

    bool syncOn = false;
    int division = 4;
    std::unique_ptr<juce::SliderParameterAttachment> divisionKnobAtt;      // the TIME knob while sync is on

    float meterDb = -60.0f;
    int lastSegs = -1;
    bool lastPower = true;
    // declared last so they are destroyed first (their callbacks touch the members above)
    juce::ParameterAttachment syncAtt, divisionAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SDD1000AudioProcessorEditor)
};
