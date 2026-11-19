// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/VintagePedalEditor.h"

// BS-1 in the default (Vintage Hardware) look: TYPE (a phosphor display and the 12 reverb types as flat buttons) and CONTROLS (eight knobs), drawn in code.
class BigSkyAudioProcessorEditor : public vibr::ui::vintage::PedalEditor,
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
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
    };

    BigSkyAudioProcessor& proc;
    Knob knobs[8];

    int lastType = 0;
    bool lastOn = true;
    float lastVals[8] {};
    juce::String shown;            // temporary display text (empty = the decay and pre-delay summary)
    juce::uint32 shownUntil = 0;
    juce::ParameterAttachment typeAtt;      // declared last: its callback touches the members above

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BigSkyAudioProcessorEditor)
};
