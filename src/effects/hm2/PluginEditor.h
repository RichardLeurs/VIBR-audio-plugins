// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/VibrUI.h"

// HM-2 window (1200 x 690), on the shared UI of src/common/ui: header (name, preset chip, undo), row 1 HARMONICS (drive, odd/even, octave, sub), FREQUENCY (focus,
// tilt, tone) and OUTPUT (mix, level + the L / R dB meters), row 2 the full-width CHARACTER panel (four saturator buttons and the live harmonic display H2 - H9).
class HM2AudioProcessorEditor : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit HM2AudioProcessorEditor (HM2AudioProcessor&);
    ~HM2AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    float scale() const { return (float) getWidth() / 1200.0f; }
    void layoutKnobs();
    int currentCharacter() const { return juce::jlimit (0, hm2::kNumCharacters - 1, (int) std::lround (proc.apvts.getRawParameterValue ("character")->load())); }
    bool isOn (const char* id) const { return proc.apvts.getRawParameterValue (id)->load() > 0.5f; }
    void toggle (const char* id);
    void setChoice (const char* id, int index);

    struct Knob
    {
        const char* id;
        int panel, index, count;      // panel (1 harmonics, 2 frequency, 3 output), position and number of knobs in the panel
        const char* name;
        int kind;                     // 0 percent, 1 focus (Hz), 2 dB, 3 odd / even, 4 tilt
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> att;
    };

    HM2AudioProcessor& proc;
    MidiLearnUI midiUI { proc.midiMap };       // right-click a knob: MIDI learn
    vibr::ui::KnobLookAndFeel laf;
    juce::ComponentBoundsConstrainer constrainer;
    juce::Image grain;
    std::array<Knob, 9> knobs;
    vibr::ui::PresetBarState barState;
    juce::ParameterAttachment powerAtt, charAtt;
    vibr::ui::OutputMeter meter;                                                                       // output dB meters

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HM2AudioProcessorEditor)
};
