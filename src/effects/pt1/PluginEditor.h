// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/VibrUI.h"

// PT-1 window (1200 x 690), on the shared UI of src/common/ui: header (name, preset chip, undo, power), four VOICE panels (INTERVAL, DETUNE, LEVEL and the note the
// voice plays now), and below them KEY (key, scale, INTELLIGENT, the note heard), CHORDS (five presets for the four intervals) and OUTPUT (SPREAD, MIX, LEVEL, L / R dB meters).
class PT1AudioProcessorEditor : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit PT1AudioProcessorEditor (PT1AudioProcessor&);
    ~PT1AudioProcessorEditor() override;

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
        juce::String id;
        int kind = 0;                 // 0 interval, 1 detune, 2 level, 3 root, 4 scale, 5 percent, 6 dB
        float cx = 0, cy = 0, nameY = 0, valueY = 0;
        const char* name = nullptr;
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> att;
    };

    PT1AudioProcessor& proc;
    MidiLearnUI midiUI { proc.midiMap };       // right-click a knob: MIDI learn
    vibr::ui::KnobLookAndFeel laf;
    juce::ComponentBoundsConstrainer constrainer;
    juce::Image grain;
    std::vector<std::unique_ptr<Knob>> knobs;
    vibr::ui::PresetBarState barState;
    juce::ParameterAttachment powerAtt, intelAtt, rootAtt, scaleAtt;
    vibr::ui::OutputMeter meter;                                                                       // output dB meters
    float shownNote[pt1::kNumVoices] { -2.0f, -2.0f, -2.0f, -2.0f }, shownHeard = -2.0f;               // the notes last painted (repaint only on a change)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PT1AudioProcessorEditor)
};
