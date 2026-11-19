// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/VintageLabEditor.h"

// SU-1 window (1120 x 660) in the "vintage hardware" look of src/common/ui/VintageUI.h: header (name, description, power), three panels in a row: PAN (position + auto-pan),
// WIDTH (width, spread, bass mono) and CHANNELS (six flat buttons), and under them the FIELD display (a phosphor screen with the goniometer and the correlation) and the
// OUTPUT panel (the GAIN knob and the L / R meters).
class SU1AudioProcessorEditor : public vibr::ui::vintage::LabEditor
{
public:
    explicit SU1AudioProcessorEditor (SU1AudioProcessor&);
    ~SU1AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void layoutKnobs();
    int solo() const { return juce::jlimit (0, 2, choice ("solo")); }
    void drawField (juce::Graphics&);

    struct Knob
    {
        const char* id;
        int panel, index, count;      // panel number (3 = OUTPUT), position in the panel, knobs in the panel
        const char* name;
        int kind;                     // 0 gain (dB), 1 pan, 2 rate (Hz), 3 percent, 4 width (%), 5 bass mono (Hz)
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> att;
    };

    SU1AudioProcessor& proc;
    vibr::ui::OutputMeter meter;                                    // OUTPUT panel: L / R dB meters
    std::array<Knob, 7> knobs;
    MidiLearnUI midiUI { proc.midiMap };       // right-click a knob: MIDI learn
    juce::ParameterAttachment invLAtt, invRAtt, swapAtt, monoAtt, soloAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SU1AudioProcessorEditor)
};
