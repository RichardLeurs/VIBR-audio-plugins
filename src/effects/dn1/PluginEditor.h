// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/OutputMeter.h"
#include "ui/LabEditor.h"

// DN-1 window (1200 x 690), on the shared UI of src/common/ui: header (name, preset chip, A / B, undo), the CONTROLS panel with the mode tab pills on its top edge
// (the knobs of the mode, and the flat buttons: the stage switches of the shaper, or the types), the SIDECHAIN panel (high-pass, stereo link), the ACTION display
// (the input level and the gain of the chain over five seconds, with the thresholds) and OUTPUT (MIX, LEVEL, AUTO LEVEL).
class DN1AudioProcessorEditor : public vibr::ui::LabEditor
{
public:
    explicit DN1AudioProcessorEditor (DN1AudioProcessor&);
    ~DN1AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void layoutKnobs();
    int mode() const { return juce::jlimit (0, dn1::kNumModes - 1, choice ("mode")); }
    int variant() const { return juce::jlimit (0, juce::jmax (0, dn1::variantCount (mode()) - 1), choice ("variant")); }
    void drawAction (juce::Graphics&);
    bool knobShown (int i) const;

    struct Knob
    {
        const char* id;
        int panel, index, count;      // panel (0 controls, 1 sidechain, 3 output), position and number of knobs in the group
        const char* name;
        int kind;                     // see the table in the editor
        int onlyMode;                 // -1 = every mode
        int typeMask;                 // the types the knob belongs to (bit 0 = type 0, ...); 7 = all
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> att;
    };

    DN1AudioProcessor& proc;
    std::array<Knob, 25> knobs;
    std::unique_ptr<MidiLearnUI> midiUI;       // right-click a knob: MIDI learn (rebuilt when the shown knobs change)
    bool extShown = false;
    vibr::ui::OutputMeter meter;                                           // output dB meters
    juce::uint64 midiShown = 0;
    int shownKey = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DN1AudioProcessorEditor)
};
