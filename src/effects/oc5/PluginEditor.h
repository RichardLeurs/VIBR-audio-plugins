// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/VintagePedalEditor.h"

// OC-5 in the default (Vintage Hardware) look: LEVEL (three knobs), LOW (one knob) and MODE (VINTAGE / POLY), drawn in code (no background image).
class OC5AudioProcessorEditor : public vibr::ui::vintage::PedalEditor,
                                private juce::Timer
{
public:
    explicit OC5AudioProcessorEditor (OC5AudioProcessor&);
    ~OC5AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
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

    OC5AudioProcessor& proc;
    MidiLearnUI midiUI { proc.midiMap };       // right-click a knob: MIDI learn
    Knob direct { "direct", "DIRECT", {}, {} }, up1 { "up1", "+1 OCT", {}, {} }, down1 { "down1", "-1 OCT", {}, {} }, down2 { "down2", "-2 OCT", {}, {} };
    bool lastOn = true, lastPoly = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OC5AudioProcessorEditor)
};
