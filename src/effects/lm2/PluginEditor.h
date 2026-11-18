// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/PedalLook.h"
#include "ui/PedalEditor.h"

// Black knurled knob with a raised cap and a white pointer (same as the DM-1), drawn over the scales printed in the background.
using LM2LookAndFeel = vibr::ui::PedalKnobLookAndFeel;

class LM2AudioProcessorEditor : public vibr::ui::PedalEditor,
                                private juce::Timer
{
public:
    explicit LM2AudioProcessorEditor (LM2AudioProcessor&);
    ~LM2AudioProcessorEditor() override;

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

    LM2AudioProcessor& proc;
    LM2LookAndFeel laf;
    MidiLearnUI midiUI { proc.midiMap };       // right-click a knob: MIDI learn
    Knob level, tone, release, threshold;
    bool lastOn = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LM2AudioProcessorEditor)
};
