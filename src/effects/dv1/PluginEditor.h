// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "MidiTarget.h"
#include "ui/VibrUI.h"

// DV-1 window (1200 x 690), on the shared UI of src/common/ui: header (name, preset chip, A / B, undo), the CONTROLS panel with the mode tab pills on its top edge
// (the knobs of the mode, and the flat buttons: the types, or the popup of the 12 OD-2 types), the OUTPUT panel (BOOST, MIX, LEVEL, BOOST and AUTO LEVEL switches)
// and the SHAPE display (the transfer curve with the live input as a dot, and a sine before and after the drive).
class DV1AudioProcessorEditor : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit DV1AudioProcessorEditor (DV1AudioProcessor&);
    ~DV1AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    float scale() const { return (float) getWidth() / 1200.0f; }
    void layoutKnobs();
    bool isOn (const char* id) const { return proc.apvts.getRawParameterValue (id)->load() > 0.5f; }
    float value (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }
    int choice (const char* id) const { return (int) std::lround (value (id)); }
    int mode() const { return juce::jlimit (0, dv1::kNumModes - 1, choice ("mode")); }
    int variant() const { return dv1::variantCount (mode()) == 0 ? 0 : juce::jlimit (0, 1, choice ("variant")); }
    void toggle (const char* id);
    void setChoice (const char* id, int index);
    void drawShape (juce::Graphics&);
    void showOdMenu();
    bool knobShown (int i) const;

    struct Knob
    {
        const char* id;
        int panel, index, count;      // panel (0 controls, 1 output), position and number of knobs in the group
        const char* name;
        int kind;                     // see the table in the editor
        int onlyMode;                 // -1 = every mode
        int typeMask;                 // the types the knob belongs to (bit 0 = type 0, ...); 7 = all
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> att;
    };

    DV1AudioProcessor& proc;
    vibr::ui::KnobLookAndFeel laf;
    juce::ComponentBoundsConstrainer constrainer;
    juce::Image grain;
    std::array<Knob, 31> knobs;
    MidiTarget boostTarget;                    // over the BOOST button (MIDI learn target)
    std::unique_ptr<MidiLearnUI> midiUI;       // right-click a knob: MIDI learn (rebuilt when the shown knobs change)
    juce::uint64 midiShown = 0;
    vibr::ui::PresetBarState barState;
    juce::ParameterAttachment powerAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DV1AudioProcessorEditor)
};
