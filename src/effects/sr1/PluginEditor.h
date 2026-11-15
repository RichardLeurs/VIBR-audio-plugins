// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/VibrUI.h"

// SR-1 window (1200 x 690) on the shared UI of src/common/ui: header (name, preset chip, A / B, undo, power), row 1 the panels TUNING (root, scale, fine),
// TEXTURE (wood / metal / glass) and RESONATORS (strings, decay, damping, tension, motion); row 2 the STRINGS harp and OUTPUT (blend, level, L / R dB meters). The harp: every string that rings is drawn, longer for lower notes, trembling and glowing with its level.
class SR1AudioProcessorEditor : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit SR1AudioProcessorEditor (SR1AudioProcessor&);
    ~SR1AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void paintOverChildren (juce::Graphics&) override;

private:
    void timerCallback() override;
    float scale() const { return (float) getWidth() / 1200.0f; }
    void layoutKnobs();
    int choice (const char* id) const { return (int) std::lround (proc.apvts.getRawParameterValue (id)->load()); }
    bool isOn (const char* id) const { return proc.apvts.getRawParameterValue (id)->load() > 0.5f; }
    void toggle (const char* id);
    juce::String knobText (int kind, double v) const;

    struct Knob
    {
        const char* id;
        int panel, index, count;      // panel number, position in the panel, knobs in the panel
        const char* name;
        int kind;                     // 0 percent, 1 root (note), 2 scale, 3 fine (cents), 4 strings, 5 decay (s), 6 dB
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> att;
    };

    SR1AudioProcessor& proc;
    MidiLearnUI midiUI { proc.midiMap };       // right-click a knob: MIDI learn
    vibr::ui::KnobLookAndFeel laf;
    juce::ComponentBoundsConstrainer constrainer;
    juce::Image grain;
    std::array<Knob, 10> knobs;
    vibr::ui::PresetBarState barState;
    juce::ParameterAttachment powerAtt, textureAtt;
    vibr::ui::OutputMeter meter;                                                                       // output dB meters

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SR1AudioProcessorEditor)
};
