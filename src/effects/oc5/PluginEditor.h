// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/PedalLook.h"
#include "ui/PedalEditor.h"

// Black knurled knob with a raised cap and a white pointer (same as the DM-1), drawn over the scales printed in the background.
using OC5LookAndFeel = vibr::ui::PedalKnobLookAndFeel;

class OC5AudioProcessorEditor : public vibr::ui::PedalEditor,
                                private juce::Timer
{
public:
    explicit OC5AudioProcessorEditor (OC5AudioProcessor&);
    ~OC5AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void paintOverChildren (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    struct Knob
    {
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
    };

    OC5AudioProcessor& proc;
    MidiLearnUI midiUI { proc.midiMap };       // right-click a knob: MIDI learn
    OC5LookAndFeel laf;
    Knob direct, up1, down1, down2;
    bool lastOn = true, lastPoly = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OC5AudioProcessorEditor)
};
