// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/VintagePedalEditor.h"

// DS-1 in the default (Vintage Hardware) look: three one-knob panels FILTER / OUTPUT / DRIVE, drawn in code (no background image).
class DS1AudioProcessorEditor : public vibr::ui::vintage::PedalEditor,
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
        const char* id;
        const char* name;
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
    };

    DS1AudioProcessor& proc;
    MidiLearnUI midiUI { proc.midiMap };       // right-click a knob: MIDI learn
    Knob tone { "tone", "TONE", {}, {} }, level { "level", "LEVEL", {}, {} }, dist { "dist", "DIST", {}, {} };
    bool lastOn = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DS1AudioProcessorEditor)
};
