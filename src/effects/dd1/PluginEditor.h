// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/VintagePedalEditor.h"

// DD-1 in the default (Vintage Hardware) look, drawn in code (no background image), two rows: DELAY A / DELAY B (time screen, PING-PONG, one row of 7 knobs) and
// TEMPO & STEREO (BPM, TAP, HOST SYNC, ROUTING, TIME, SPREAD) + EFFECTS (tabs BOOST / AMBIENCE / SHIMMER, one tab's knobs at a time).
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

private:
    int presetBarKey = 0;
    void timerCallback() override;
    void toggleParam (const char* id);
    void setBool (const char* id, bool v);
    void tap();
    void setTab (int);

    struct Knob
    {
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
        const char* name = "";
        juce::Point<float> centre;
        float r = 30.0f;            // body radius
        int tab = -1;               // -1 always shown, else the EFFECTS tab (0 BOOST, 1 AMBIENCE, 2 SHIMMER)
        int kind = 0;               // 0 = 0..10, 1 = division of delay A, 2 = ratio of delay B, 3 = shimmer interval
    };

    DD1AudioProcessor& proc;
    MidiLearnUI midiUI { proc.midiMap };       // right-click a knob: MIDI learn
    std::vector<std::unique_ptr<Knob>> knobs;
    bool lastOn = true, lastSeries = false;
    int lastFlags = 0;
    int fxTab = 2;
    bool lastBlink = false;
    float lastBpm = 0.0f;
    float lastMs[2] { -1.0f, -1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DD1AudioProcessorEditor)
};
