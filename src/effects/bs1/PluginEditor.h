// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/PedalLook.h"
#include "ui/PedalEditor.h"
#include "WrapSlider.h"

// Black knurled knob with a raised cap and a white pointer (same as the DM-1), drawn over the scales printed in the background.
using BigSkyLookAndFeel = vibr::ui::PedalKnobLookAndFeel;


class BigSkyAudioProcessorEditor : public vibr::ui::PedalEditor,
                                   private juce::Timer
{
public:
    explicit BigSkyAudioProcessorEditor (BigSkyAudioProcessor&);
    ~BigSkyAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    juce::String formatValue (int index) const;

    struct Knob
    {
        WrapSlider slider;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
    };

    BigSkyAudioProcessor& proc;
    BigSkyLookAndFeel laf;
    Knob knobs[8];
    Knob typeKnob;

    int lastType = -1;
    bool lastOn = true;
    float lastVals[8] {};
    juce::String shown;            // temporary display text (empty = show the type name)
    juce::uint32 shownUntil = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BigSkyAudioProcessorEditor)
};
