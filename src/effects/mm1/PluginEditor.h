// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/VintagePedalEditor.h"

// MM-1 in the default (Vintage Hardware) look: MIX (BLEND, LEVEL), ECHO (FEEDBACK, DELAY, SYNC) and MODULATION (CHRS/VIBR, DEPTH), drawn in code (no background image).
class MemoryManAudioProcessorEditor : public vibr::ui::vintage::PedalEditor,
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
    void syncChanged (bool on);

    struct Knob
    {
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
    };

    MemoryManAudioProcessor& proc;
    MidiLearnUI midiUI { proc.midiMap };       // right-click a knob: MIDI learn
    Knob knobs[6];                             // blend, level, feedback, delay, chrsVibr, depth
    bool lastOn = true;
    bool syncOn = false;
    int division = 4;
    std::unique_ptr<juce::SliderParameterAttachment> divisionKnobAtt;      // the DELAY knob while sync is on
    // declared last so they are destroyed first (their callbacks touch the members above)
    juce::ParameterAttachment syncAtt, divisionAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MemoryManAudioProcessorEditor)
};
