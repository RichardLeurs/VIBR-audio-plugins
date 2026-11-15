// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/VibrUI.h"

// GR-1 window (1200 x 690) on the shared UI of src/common/ui: header (name, preset chip, A / B, undo), row 1 the panels ENVELOPE (four grain shapes + FREEZE),
// GRAINS (size, density, position, spray) and PITCH (pitch, random, reverse); row 2 the CLOUD (every grain that sounds is a dot, older to newer, pitch up and
// down) and OUTPUT (spread, mix, level + the L / R dB meters).
class GR1AudioProcessorEditor : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit GR1AudioProcessorEditor (GR1AudioProcessor&);
    ~GR1AudioProcessorEditor() override;

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
        int kind;                     // 0 percent, 1 size, 2 density, 3 position, 4 pitch (st), 5 random (st), 6 dB
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> att;
    };

    GR1AudioProcessor& proc;
    MidiLearnUI midiUI { proc.midiMap };       // right-click a knob: MIDI learn
    vibr::ui::KnobLookAndFeel laf;
    juce::ComponentBoundsConstrainer constrainer;
    juce::Image grain;
    std::array<Knob, 10> knobs;
    vibr::ui::PresetBarState barState;
    juce::ParameterAttachment powerAtt, shapeAtt, freezeAtt, syncAtt;
    vibr::ui::OutputMeter meter;                                                                       // output dB meters

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GR1AudioProcessorEditor)
};
