// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/VintagePedalEditor.h"

// LM-2 in the default (Vintage Hardware) look: panels OUTPUT (LEVEL, TONE) and LIMITER (RELEASE, THRESHOLD), drawn in code.

class LM2AudioProcessorEditor : public vibr::ui::vintage::PedalEditor,
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
        const char* id;
        const char* name;
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
    };

    LM2AudioProcessor& proc;
    MidiLearnUI midiUI { proc.midiMap };       // right-click a knob: MIDI learn
    Knob level { "level", "LEVEL", {}, {} }, tone { "tone", "TONE", {}, {} },
         release { "release", "RELEASE", {}, {} }, threshold { "threshold", "THRESHOLD", {}, {} };
    bool lastOn = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LM2AudioProcessorEditor)
};
