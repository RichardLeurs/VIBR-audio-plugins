// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/LabEditor.h"
#include "ui/OutputMeter.h"

// RB-1 window (1200 x 690), on the shared UI of src/common/ui: header (name, preset chip, A / B, undo), the CONTROLS panel with the mode tab pills on its top edge (the
// knobs of the mode and its types: a popup of the 12 classic types, the 5 space modes, or the texture, root and scale of the strings), the OUTPUT panel (PRE-DELAY, DUCK,
// MIX, LEVEL, FREEZE) and the TAIL display (the wet level over five seconds, or the strings with their notes and how strongly they ring).
class RB1AudioProcessorEditor : public vibr::ui::LabEditor
{
public:
    explicit RB1AudioProcessorEditor (RB1AudioProcessor&);
    ~RB1AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    vibr::ui::OutputMeter meter;                                                                       // output dB meters
    bool extShown = false;                    // the EXT lamp as last painted
    void timerCallback() override;
    void layoutKnobs();
    int mode() const { return juce::jlimit (0, rb1::kNumModes - 1, choice ("mode")); }
    int variantMax() const { return mode() == 0 ? bigsky::kNumTypes - 1 : mode() == 1 ? sp1::kNumModes - 1 : sr1::kNumTextures - 1; }
    int variant() const { return juce::jlimit (0, variantMax(), choice ("variant")); }
    void drawTail (juce::Graphics&);
    void showMenu (const char* id, const juce::StringArray& items, int current, juce::Rectangle<float> area);
    bool knobShown (int i) const { return knobs[(size_t) i].onlyMode < 0 || knobs[(size_t) i].onlyMode == mode(); }

    struct Knob
    {
        const char* id;
        int panel, index, count;      // panel (0 controls, 1 output), position and number of knobs in the group
        const char* name;
        int kind;                     // see the table in the editor
        int onlyMode;                 // -1 = every mode
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> att;
    };

    RB1AudioProcessor& proc;
    MidiLearnUI midiUI { proc.midiMap };       // right-click a knob: MIDI learn
    std::array<Knob, 23> knobs;
    int shownKey = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RB1AudioProcessorEditor)
};
