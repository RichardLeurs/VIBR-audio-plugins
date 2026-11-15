// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/VibrUI.h"

// SP-1 window (1200 x 450), on the shared UI of src/common/ui: header (name, preset chip, undo), the CONTROLS panel with the mode tab pills (SHIMMER, INFINITE,
// SPECTRAL, MODULATED, ROOMS) on its top edge (SIZE, DECAY, DIFFUSION, TONE, MOTION, TEXTURE and FREEZE) and OUTPUT (MIX, LEVEL, L / R dB meters).
class SP1AudioProcessorEditor : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit SP1AudioProcessorEditor (SP1AudioProcessor&);
    ~SP1AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    float scale() const { return (float) getWidth() / 1200.0f; }
    void layoutKnobs();
    int currentMode() const { return juce::jlimit (0, sp1::kNumModes - 1, (int) std::lround (proc.apvts.getRawParameterValue ("mode")->load())); }
    bool isOn (const char* id) const { return proc.apvts.getRawParameterValue (id)->load() > 0.5f; }
    void toggle (const char* id);
    void setChoice (const char* id, int index);

    struct Knob
    {
        const char* id;
        int panel, index, count;      // panel number, position in the panel, knobs in the panel
        const char* name;             // nullptr: the TEXTURE name of the current mode
        bool dB;
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> att;
    };

    SP1AudioProcessor& proc;
    MidiLearnUI midiUI { proc.midiMap };       // right-click a knob: MIDI learn
    vibr::ui::KnobLookAndFeel laf;
    juce::ComponentBoundsConstrainer constrainer;
    juce::Image grain;
    std::array<Knob, 8> knobs;
    vibr::ui::PresetBarState barState;
    juce::ParameterAttachment powerAtt, modeAtt, freezeAtt;
    int lastMode = -1;
    vibr::ui::OutputMeter meter;                                                                       // output dB meters

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SP1AudioProcessorEditor)
};
