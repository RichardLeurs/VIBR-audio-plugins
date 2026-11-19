// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/VintageLabEditor.h"

// TF-1 window (1120 x 910) in the "vintage hardware" look of ui/VintageUI.h (cream plate, bakelite knobs, phosphor screens). STEPS: row 1 the SEQUENCER (mode tabs on its edge, 16 steps,
// legend, division, LENGTH, HOLD BUFFER, CLEAR, RANDOM), row 2 FRACTURE (CHANCE, LOOP, SCRAMBLE, PITCH, STRETCH) and GLITCH (CRUSH, TEMPO), row 3 the BUFFER screen and OUTPUT (MIX, LEVEL, L / R meters).
// SCRUB and GRAINS: row 1 CONTROLS (the knobs of the mode, SYNC, FREEZE), rows 2 and 3 the slice display or the grain cloud, TYPE (the sub-mode) and OUTPUT in the same place.
class TF1AudioProcessorEditor : public vibr::ui::vintage::LabEditor
{
public:
    explicit TF1AudioProcessorEditor (TF1AudioProcessor&);
    ~TF1AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void layoutKnobs();
    void setValue (const char* id, float v);
    int stepOp (int s) const;
    void drawBuffer (juce::Graphics&);
    void drawSlices (juce::Graphics&);                 // SCRUB: the read positions of the slices (the RV-1 display)
    void drawCloud (juce::Graphics&);                  // GRAINS: the grain cloud (the GR-1 display)
    juce::String knobText (int kind, double v) const;
    int modeNow() const { return juce::jlimit (0, 2, choice ("mode")); }
    bool knobShown (int i) const;

    struct Knob
    {
        const char* id;
        int panel, index, count;      // panel (0 controls of SCRUB / GRAINS, 1 fracture, 2 glitch, 3 output), position and number of knobs in the group
        const char* name;
        int kind;                     // 0 percent, 1 dB, 2 bpm, 3 scrub length, 4 speed, 5 grain size, 6 density, 7 position, 8 pitch, 9 random pitch, 10 the CHAOS knobs of SCRUB
        int modes;                    // the modes the knob belongs to (bit 0 STEPS, bit 1 SCRUB, bit 2 GRAINS)
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> att;
    };

    TF1AudioProcessor& proc;
    std::unique_ptr<MidiLearnUI> midiUI;       // right-click a knob: MIDI learn (rebuilt when the shown knobs change)
    juce::uint64 midiShown = 0;
    int shownMode = -1;
    float scroll = 0.0f;
    std::array<Knob, 22> knobs;
    vibr::ui::OutputMeter meter;                                                                       // output dB meters
    juce::Random rng { 77 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TF1AudioProcessorEditor)
};
