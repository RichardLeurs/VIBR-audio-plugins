// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/PedalLook.h"
#include "ui/PedalEditor.h"

// Black knurled knob with a raised cap and a white pointer (same as the DM-1), drawn over the scales printed in the background.
using DS1LookAndFeel = vibr::ui::PedalKnobLookAndFeel;

class DS1AudioProcessorEditor : public vibr::ui::PedalEditor,
                                private juce::Timer
{
public:
    explicit DS1AudioProcessorEditor (DS1AudioProcessor&);
    ~DS1AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    struct Knob
    {
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
    };

    DS1AudioProcessor& proc;
    DS1LookAndFeel laf;
    MidiLearnUI midiUI { proc.midiMap };       // right-click a knob: MIDI learn
    Knob tone, level, dist;
    bool lastOn = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DS1AudioProcessorEditor)
};
