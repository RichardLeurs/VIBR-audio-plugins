// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/VintagePedalEditor.h"

// TR-2 in the default (Vintage Hardware) look: SPEED (RATE + SYNC) / SHAPE (WAVE) / AMOUNT (DEPTH), drawn in code (no background image).

class TR2AudioProcessorEditor : public vibr::ui::vintage::PedalEditor,
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
