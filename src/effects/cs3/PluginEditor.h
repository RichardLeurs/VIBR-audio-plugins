// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/VintagePedalEditor.h"

// CS-3 in the default (Vintage Hardware) look: OUTPUT (LEVEL, TONE) and COMPRESSION (ATTACK, SUSTAIN), drawn in code (no background image).
class CS3AudioProcessorEditor : public vibr::ui::vintage::PedalEditor,
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
        const char* id;
        const char* name;
        const char* lo;
        const char* hi;
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
    };

    CS3AudioProcessor& proc;
    MidiLearnUI midiUI { proc.midiMap };       // right-click a knob: MIDI learn
    Knob level   { "level",   "LEVEL",   "MIN", "MAX", {}, {} },
         tone    { "tone",    "TONE",    "LO",  "HI",  {}, {} },
         attack  { "attack",  "ATTACK",  "MIN", "MAX", {}, {} },
         sustain { "sustain", "SUSTAIN", "MIN", "MAX", {}, {} };
    bool lastOn = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CS3AudioProcessorEditor)
};
