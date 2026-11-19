// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/VintagePedalEditor.h"

// EH-2 in the default (Vintage Hardware) look: three one-knob panels ENHANCE / FREQUENCY / SENSITIVITY, drawn in code (no background image).
class EH2AudioProcessorEditor : public vibr::ui::vintage::PedalEditor,
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
        const char* id;
        const char* name;
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
    };

    EH2AudioProcessor& proc;
    Knob mix { "mix", "MIX", {}, {} }, freq { "freq", "FREQ", {}, {} }, sens { "sens", "SENS", {}, {} };
    bool lastOn = true;
    bool lastSens = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EH2AudioProcessorEditor)
};
