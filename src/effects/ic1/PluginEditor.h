// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/LabEditor.h"

// IC-1 window (1200 x 890), on the shared UI of src/common/ui: header (name, preset chip, A / B, undo); row 1: NODES (TIME, SPREAD, the four feedback levels, CROSS,
// REGEN) and ROUTING (the routing, the input paths, SELF-OSC, HOLD, EXCITE); row 2: FILTER (the type as tabs, CUTOFF, RES, TILT), PHASE & MOD (PHASE, RATE, DEPTH)
// and OUTPUT (DRIVE, MIX, LEVEL, meters); row 3: the NETWORK display (the four nodes and the live feedback paths between them).
class IC1AudioProcessorEditor : public vibr::ui::LabEditor
{
public:
    explicit IC1AudioProcessorEditor (IC1AudioProcessor&);
    ~IC1AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void layoutKnobs();
    void setValue (const char* id, float v);
    void drawNetwork (juce::Graphics&);

    struct Knob
    {
        const char* id;
        int panel, index, count;      // panel (0 nodes, 2 filter, 3 phase and mod, 4 output), position and number of knobs in the group
        const char* name;
        int kind;                     // 0 percent, 1 ms, 2 Hz (cutoff), 3 Hz (rate), 4 dB
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> att;
    };

    IC1AudioProcessor& proc;
    MidiLearnUI midiUI { proc.midiMap };       // right-click a knob: MIDI learn
    std::array<Knob, 17> knobs;
    vibr::ui::OutputMeter meter;                                                                       // output dB meters
    juce::int64 exciteAt = 0;                                                                          // the EXCITE button lights for a moment
    float pulse[ic1::kNodes] {};                                                                       // the node levels, smoothed for the display

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IC1AudioProcessorEditor)
};
