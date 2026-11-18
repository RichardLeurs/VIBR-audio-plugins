// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/LabEditor.h"

// SF-1 window (1200 x 890), on the shared UI of src/common/ui: header (name, preset chip, A / B, undo); row 1: HOLD (FREEZE, CAPTURE, four band switches, DRIFT, MORPH),
// TEXTURE (SMEAR, BLUR, FEEDBACK), SHIFT (SHIFT, SPREAD, RING, RING FREQ); row 2: WINDOW (amount, centre, width, scan), OUTPUT (MIX, LEVEL, meters), STATE (value cards);
// row 3: the SPECTRUM display (input, output, the snapshot, the frozen bands and the moving window).
class SF1AudioProcessorEditor : public vibr::ui::LabEditor
{
public:
    explicit SF1AudioProcessorEditor (SF1AudioProcessor&);
    ~SF1AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void layoutKnobs();
    void drawSpectrum (juce::Graphics&);
    void drawKeyboard (juce::Graphics&);               // HARMONY: the scale, the note heard and the four voices on a keyboard
    juce::String harmonyText (int kind, double v) const;
    int modeNow() const { return juce::jlimit (0, 1, choice ("mode")); }
    bool knobShown (int i) const;

    struct Knob
    {
        const char* id;
        int panel, index, count;      // panel (0 hold, 1 texture, 2 shift, 3 window, 4 output), position and number of knobs in the group
        const char* name;
        int kind;                     // 0 percent, 1 shift Hz, 2 dB, 3 ring Hz, 4 window centre Hz, 5 octaves, 6 scan Hz; HARMONY: 10 interval, 11 detune, 12 key, 13 scale
        int modes;                    // the modes the knob belongs to (bit 0 FOLD, bit 1 HARMONY)
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> att;
    };

    SF1AudioProcessor& proc;
    std::unique_ptr<MidiLearnUI> midiUI;       // right-click a knob: MIDI learn (rebuilt when the shown knobs change)
    juce::uint64 midiShown = 0;
    int shownMode = -1;
    float shownNote[4] { -2.0f, -2.0f, -2.0f, -2.0f }, shownHeard = -2.0f;
    std::array<Knob, 32> knobs;
    vibr::ui::OutputMeter meter;                                                                       // output dB meters
    juce::int64 capturedAt = 0;                                                                        // the CAPTURE button lights for a moment

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SF1AudioProcessorEditor)
};
