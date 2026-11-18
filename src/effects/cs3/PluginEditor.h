// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/PedalLook.h"
#include "ui/PedalEditor.h"

// Black knurled knob with a raised cap and a white pointer (same as the DM-1), drawn over the scales printed in the background.
using CS3LookAndFeel = vibr::ui::PedalKnobLookAndFeel;

class CS3AudioProcessorEditor : public vibr::ui::PedalEditor,
                                private juce::Timer
{
public:
    explicit CS3AudioProcessorEditor (CS3AudioProcessor&);
    ~CS3AudioProcessorEditor() override;

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

    CS3AudioProcessor& proc;
    CS3LookAndFeel laf;
    MidiLearnUI midiUI { proc.midiMap };       // right-click a knob: MIDI learn
    Knob level, tone, attack, sustain;
    bool lastOn = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CS3AudioProcessorEditor)
};
