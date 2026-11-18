// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/PedalLook.h"
#include "ui/PedalEditor.h"

// Black knurled knob with a raised cap and a white pointer (same as the DM-1), drawn over the scales printed in the background.
using EH2LookAndFeel = vibr::ui::PedalKnobLookAndFeel;

class EH2AudioProcessorEditor : public vibr::ui::PedalEditor,
                                private juce::Timer
{
public:
    explicit EH2AudioProcessorEditor (EH2AudioProcessor&);
    ~EH2AudioProcessorEditor() override;

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

    EH2AudioProcessor& proc;
    EH2LookAndFeel laf;
    Knob mix, freq, sens;
    bool lastOn = true;
    bool lastSens = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EH2AudioProcessorEditor)
};
