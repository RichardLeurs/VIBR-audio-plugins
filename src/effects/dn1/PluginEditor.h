// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/VibrUI.h"

// DN-1 window (1200 x 690), on the shared UI of src/common/ui: header (name, preset chip, A / B, undo), the CONTROLS panel with the mode tab pills on its top edge
// (the knobs of the mode, and the flat buttons: the stage switches of the shaper, or the types), the SIDECHAIN panel (high-pass, stereo link), the ACTION display
// (the input level and the gain of the chain over five seconds, with the thresholds) and OUTPUT (MIX, LEVEL, AUTO LEVEL).
class DN1AudioProcessorEditor : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit DN1AudioProcessorEditor (DN1AudioProcessor&);
    ~DN1AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    float scale() const { return (float) getWidth() / 1200.0f; }
    void layoutKnobs();
    bool isOn (const char* id) const { return proc.apvts.getRawParameterValue (id)->load() > 0.5f; }
    float value (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }
    int choice (const char* id) const { return (int) std::lround (value (id)); }
    int mode() const { return juce::jlimit (0, dn1::kNumModes - 1, choice ("mode")); }
    int variant() const { return juce::jlimit (0, juce::jmax (0, dn1::variantCount (mode()) - 1), choice ("variant")); }
    void toggle (const char* id);
    void setChoice (const char* id, int index);
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
    vibr::ui::KnobLookAndFeel laf;
    juce::ComponentBoundsConstrainer constrainer;
    juce::Image grain;
    std::array<Knob, 25> knobs;
    vibr::ui::PresetBarState barState;
    juce::ParameterAttachment powerAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DN1AudioProcessorEditor)
};
