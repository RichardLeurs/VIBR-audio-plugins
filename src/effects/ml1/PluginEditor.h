// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/VibrUI.h"

// ML-1 window (1200 x 690), on the shared UI of src/common/ui: header (name, preset chip, A / B, undo), the CONTROLS panel with the mode tab pills (CHORUS, VIBRATO,
// FLANGER, PHASER) on its top edge (RATE, DEPTH and the mode's third knob, the TYPE buttons and SYNC), OUTPUT (MIX, LEVEL) and the wide MOTION display (a live
// picture of the effect: the voices, the LFO wave, the comb or the notches).
class ML1AudioProcessorEditor : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit ML1AudioProcessorEditor (ML1AudioProcessor&);
    ~ML1AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    float scale() const { return (float) getWidth() / 1200.0f; }
    void layoutKnobs();
    int currentMode() const { return juce::jlimit (0, ml1::kNumModes - 1, (int) std::lround (proc.apvts.getRawParameterValue ("mode")->load())); }
    int currentVariant() const { return juce::jlimit (0, ml1::variantCount (currentMode()) - 1, (int) std::lround (proc.apvts.getRawParameterValue ("variant")->load())); }
    bool isOn (const char* id) const { return proc.apvts.getRawParameterValue (id)->load() > 0.5f; }
    void toggle (const char* id);
    void setChoice (const char* id, int index);
    void drawMotion (juce::Graphics&, int mode, int variant);

    struct Knob
    {
        const char* id;
        int panel, index, count;      // panel (0 controls, 1 output), position and number of knobs in the group
        const char* name;             // nullptr: the name follows the mode (WIDTH / FEEDBACK)
        int kind;                     // 0 rate, 1 percent, 2 dB
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> att;
    };

    ML1AudioProcessor& proc;
    vibr::ui::KnobLookAndFeel laf;
    juce::ComponentBoundsConstrainer constrainer;
    juce::Image grain;
    std::array<Knob, 5> knobs;
    MidiLearnUI midiUI { proc.midiMap };       // right-click a knob: MIDI learn
    vibr::ui::PresetBarState barState;
    juce::ParameterAttachment powerAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ML1AudioProcessorEditor)
};
