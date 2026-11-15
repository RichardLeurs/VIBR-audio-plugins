// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/VibrUI.h"

// MF-1 window (1200 x 690) on the shared UI of src/common/ui: header (name, preset chip, undo), five panels
// FILTER (five filter types), SOURCE (LFO / ENVELOPE / STEPS + SYNC), TONE (cutoff, resonance, depth),
// MOTION (two knobs renamed per source) and OUTPUT (mix, level), and below them the SEQUENCER: eight bars to drag.
class MF1AudioProcessorEditor : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit MF1AudioProcessorEditor (MF1AudioProcessor&);
    ~MF1AudioProcessorEditor() override;

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
    int choice (const char* id) const { return (int) std::lround (proc.apvts.getRawParameterValue (id)->load()); }
    bool isOn (const char* id) const { return proc.apvts.getRawParameterValue (id)->load() > 0.5f; }
    void toggle (const char* id);
    void setChoice (const char* id, int index);
    void setStep (int step, float value01, bool begin);
    juce::String knobText (int kind, double v) const;

    struct Knob
    {
        const char* id;
        int panel, index, count;      // panel number, position in the panel, knobs in the panel
        const char* name;             // nullptr: the MOTION name of the current source
        int kind;                     // 0 percent, 1 cutoff, 2 rate / sens, 3 shape / decay / glide, 4 dB
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> att;
    };

    MF1AudioProcessor& proc;
    vibr::ui::KnobLookAndFeel laf;
    juce::ComponentBoundsConstrainer constrainer;
    juce::Image grain;
    std::array<Knob, 7> knobs;
    MidiLearnUI midiUI { proc.midiMap };       // right-click a knob: MIDI learn
    juce::ParameterAttachment powerAtt, typeAtt, sourceAtt, syncAtt;
    vibr::ui::PresetBarState barState;
    vibr::ui::OutputMeter meter;                                                                       // output dB meters
    int lastStep = -1, dragStep = -1, shownKey = -1, stepsKey = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MF1AudioProcessorEditor)
};
