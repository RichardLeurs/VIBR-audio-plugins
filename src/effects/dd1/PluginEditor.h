// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ParamMemory.h"
#include "ui/VintagePedalEditor.h"

// DD-1 in the default (Vintage Hardware) look, drawn in code (no background image): TEMPO (MEMORY, tap, host sync, TIME), STEREO, ROUTING,
// DELAY A / DELAY B (two knob rows, time screen, PING-PONG), BOOST, AMBIENCE (chorus / reverb) and SHIMMER.
class DD1AudioProcessorEditor : public vibr::ui::vintage::PedalEditor,
                                private juce::Timer
{
public:
    explicit DD1AudioProcessorEditor (DD1AudioProcessor&);
    ~DD1AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void toggleParam (const char* id);
    void setBool (const char* id, bool v);
    void tap();

    struct Knob
    {
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
        const char* name = "";
        juce::Point<float> centre;
        int kind = 0;               // 0 = 0..10, 1 = division of delay A, 2 = ratio of delay B, 3 = shimmer interval
    };

    DD1AudioProcessor& proc;
    MidiLearnUI midiUI { proc.midiMap };       // right-click a knob: MIDI learn
    std::vector<std::unique_ptr<Knob>> knobs;
    bool lastOn = true, lastSeries = false;
    int lastFlags = 0;
    // MEMORY: short press steps MANUAL / 1-4, holding it writes the knobs into the selected slot
    ParamMemory memory { proc.apvts, { "bypass" } };
    bool memHeld = false, memWritten = false, memFlashShown = false;
    int memShown = -1;
    juce::uint32 memDownAt = 0, memFlashUntil = 0;
    bool lastBlink = false;
    float lastBpm = 0.0f;
    float lastMs[2] { -1.0f, -1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DD1AudioProcessorEditor)
};
