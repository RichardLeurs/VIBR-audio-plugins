// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/VibrUI.h"

// RV-1 window (1200 x 690), on the shared UI of src/common/ui: header (name, preset chip, A / B, undo), the CONTROLS panel with the mode tab pills (REVERSE, SCRUB,
// STUTTER, GLITCH, REPEAT) on its top edge (LENGTH, SPEED, SMOOTH and the two mode knobs, SYNC and FREEZE), OUTPUT (MIX, LEVEL) and the wide SLICES display (the read
// position of each slice over three seconds, as the mode, length, speed and chaos set it).
class RV1AudioProcessorEditor : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit RV1AudioProcessorEditor (RV1AudioProcessor&);
    ~RV1AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    float scale() const { return (float) getWidth() / 1200.0f; }
    void layoutKnobs();
    int currentMode() const { return juce::jlimit (0, rv1::kNumModes - 1, (int) std::lround (proc.apvts.getRawParameterValue ("mode")->load())); }
    bool isOn (const char* id) const { return proc.apvts.getRawParameterValue (id)->load() > 0.5f; }
    float value (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }
    void toggle (const char* id);
    void setChoice (const char* id, int index);
    void drawSlices (juce::Graphics&);

    struct Knob
    {
        const char* id;
        int panel, index, count;      // panel (0 controls, 1 output), position and number of knobs in the group
        const char* name;             // nullptr: the CHAOS name of the current mode
        int kind;                     // 0 percent, 1 speed (x), 2 dB
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> att;
    };

    RV1AudioProcessor& proc;
    MidiLearnUI midiUI { proc.midiMap };       // right-click a knob: MIDI learn
    vibr::ui::KnobLookAndFeel laf;
    juce::ComponentBoundsConstrainer constrainer;
    juce::Image grain;
    std::array<Knob, 7> knobs;
    vibr::ui::PresetBarState barState;
    juce::ParameterAttachment powerAtt;
    float scroll = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RV1AudioProcessorEditor)
};
