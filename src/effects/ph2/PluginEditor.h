// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/PedalLook.h"
#include "ui/PedalEditor.h"

// Black knurled knob with a raised cap and a white pointer (same as the DM-1), drawn over the scales printed in the background.
using PH2LookAndFeel = vibr::ui::PedalKnobLookAndFeel;

class PH2AudioProcessorEditor : public vibr::ui::PedalEditor,
                                private juce::Timer
{
public:
    explicit PH2AudioProcessorEditor (PH2AudioProcessor&);
    ~PH2AudioProcessorEditor() override;

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

    PH2AudioProcessor& proc;
    PH2LookAndFeel laf;
    MidiLearnUI midiUI { proc.midiMap };       // right-click a knob: MIDI learn
    Knob res, depth, rate, mode;
    bool lastOn = true;
    bool syncOn = false;
    int division = 3;
    std::unique_ptr<juce::SliderParameterAttachment> divisionKnobAtt;      // the RATE knob while sync is on
    // declared last so they are destroyed first (their callbacks touch the members above)
    juce::ParameterAttachment syncAtt, divisionAtt;   // divisionAtt only tracks the value for the readout

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PH2AudioProcessorEditor)
};
