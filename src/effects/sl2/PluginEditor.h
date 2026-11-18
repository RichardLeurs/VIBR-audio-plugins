// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/PedalLook.h"
#include "ui/PedalEditor.h"

// Black knurled knob with a raised cap and a white pointer (same as the DM-1), drawn over the scales printed in the background.
using SL2LookAndFeel = vibr::ui::PedalKnobLookAndFeel;

class SL2AudioProcessorEditor : public vibr::ui::PedalEditor,
                                private juce::Timer
{
public:
    explicit SL2AudioProcessorEditor (SL2AudioProcessor&);
    ~SL2AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    struct Knob
    {
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
    };

    SL2AudioProcessor& proc;
    SL2LookAndFeel laf;
    Knob balance, tempo, attack, duty, variation, mode;
    bool lastOn = true, lastHost = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SL2AudioProcessorEditor)
};
