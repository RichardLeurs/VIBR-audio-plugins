// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/VibrUI.h"

// DL-1 window (1200 x 690), built on the shared UI of src/common/ui: header (name, preset chip, A / B, undo), the TIME panel with the mode tab pills on its top
// edge (seven shared knobs and the flat buttons: types, SYNC, note length, HOLD, TAP), the VOICING panel (the knobs of the mode), the ECHOES display (the repeats on
// a time axis, drag the first one to set the time) and OUTPUT (MIX, LEVEL).
class DL1AudioProcessorEditor : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit DL1AudioProcessorEditor (DL1AudioProcessor&);
    ~DL1AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    float scale() const { return (float) getWidth() / 1200.0f; }
    void layoutKnobs();
    bool isOn (const char* id) const { return proc.apvts.getRawParameterValue (id)->load() > 0.5f; }
    float value (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }
    int choice (const char* id) const { return (int) std::lround (value (id)); }
    int mode() const { return juce::jlimit (0, dl1::kNumModes - 1, choice ("mode")); }
    void toggle (const char* id);
    void setChoice (const char* id, int index);
    void setFloat (const char* id, float v);
    void drawEchoes (juce::Graphics&);
    void setTimeFromX (float x);
    void tap();
    void showVoiceMenu();

    struct Knob
    {
        const char* id;
        int panel, index, count;      // panel (0 shared, 1 voicing, 3 output), position and number of knobs in the group (the voicing knobs depend on the mode)
        const char* name;
        int kind;                     // 0 time, 1 percent, 2 low cut, 3 hi cut, 4 mod rate, 5 level
        int onlyMode;                 // -1 = every mode
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> att;
    };

    DL1AudioProcessor& proc;
    MidiLearnUI midiUI { proc.midiMap };       // right-click a knob: MIDI learn
    vibr::ui::KnobLookAndFeel laf;
    juce::ComponentBoundsConstrainer constrainer;
    juce::Image grain;
    std::array<Knob, 27> knobs;
    vibr::ui::PresetBarState barState;
    juce::ParameterAttachment powerAtt;
    bool draggingTime = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DL1AudioProcessorEditor)
};
