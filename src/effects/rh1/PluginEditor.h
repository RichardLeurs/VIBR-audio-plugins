// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/VintageLabEditor.h"

// RH-1 window (1120 x 660): the RH-1 with the "vintage hardware" look of ui/VintageUI.h in src/common (cream plate, bakelite knobs, phosphor screens). Header (name, preset screen,
// undo / redo, A / B, power), the SOURCE panel with the source tabs (LFO, ENVELOPE, STEPS), the TARGET panel with the destination tabs (FILTER, TREMOLO, SLICER,
// MATRIX) and the types of the destination, the PATTERN panel (the 16-step lane to click and paint, the scope with the control signal and the destination over five
// seconds, the 11 slice patterns of the SL-2) and OUTPUT (MIX, LEVEL, L / R meters). The MATRIX tab draws the VC-1 layout of the RH-1, scaled into the same window.
// All drawing goes through `ui` = vibr::ui::vintage: the editor holds the layout and the behaviour only.
class RH1AudioProcessorEditor : public vibr::ui::vintage::LabEditor
{
public:
    explicit RH1AudioProcessorEditor (RH1AudioProcessor&);
    ~RH1AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void layoutKnobs();
    int mode() const { return juce::jlimit (0, rh1::kNumModes - 1, choice ("mode")); }
    int source() const { return juce::jlimit (0, 2, choice ("source")); }
    int variant() const { const int n = rh1::variantCount (mode()); return n > 0 ? juce::jlimit (0, n - 1, choice ("variant")) : 0; }
    void setChoice (const char* id, int index);
    void setValue (const juce::String& id, float v, bool gesture);
    void drawPattern (juce::Graphics&);
    void drawMatrix (juce::Graphics&);
    void drawBigMatrixKnobs (juce::Graphics&);                                 // the destination MATRIX (the VC-1 layout, in matrix space: see matrixXform)
    juce::String matrixText (int kind, double v) const;
    bool knobShown (int i) const;
    int stepAt (juce::Point<float> p) const;                          // the step under a point of the lane, or -1
    void paintStep (juce::Point<float> p);

    struct Knob
    {
        const char* id;
        int panel, index, count;      // panel (0 control, 1 destination, 3 output), position and number of knobs in the group
        const char* name;
        int kind;                     // see the table in the editor
        int onlySource;               // bit mask of the sources (7 = all), for panel 0
        int onlyMode;                 // the destination for panel 1, -1 = every
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> att;
    };

    struct MKnob                                                        // a knob of the MATRIX layout (free positions, small or big)
    {
        juce::String id;
        int kind;                     // 0 percent, 1 cutoff, 2 LFO rate, 3 random rate, 4 release, 5 matrix depth, 6 dB, 7 sensitivity
        float cx, cy, r;
        const char* name;
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> att;
    };

    RH1AudioProcessor& proc;
    std::vector<std::unique_ptr<MKnob>> matrixKnobs;
    std::array<Knob, 17> knobs;
    MidiLearnUI midiUI { proc.midiMap };       // right-click a knob: MIDI learn
    int shownKey = -1;
    bool painting = false;
    vibr::ui::OutputMeter meter;                                                                       // output dB meters
    float paintValue = 1.0f;
    int lastPainted = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RH1AudioProcessorEditor)
};
