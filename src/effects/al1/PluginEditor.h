// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "MidiTarget.h"
#include "ui/VintageLabEditor.h"
#include "ui/OutputMeter.h"

// AL-1 window (1120 x 660) in the "vintage hardware" look of ui/VintageUI.h: header (name, preset screen, undo / redo, A / B, power), the CONTROLS panel with four tabs on its top
// edge (GATE / BOOST, AMP, CABINET, EQ), the DISPLAY panel with two tabs (CURVE: transfer curve and sine in / out, CABINET: the response of the cabinet or the impulse responses) and
// the OUTPUT panel (OUTPUT knob, LIMITER button, gate and limiter readouts, L / R meters). The display redraws at 30 Hz from the engine's atomics.
class AL1AudioProcessorEditor : public vibr::ui::vintage::LabEditor
{
public:
    explicit AL1AudioProcessorEditor (AL1AudioProcessor&);
    ~AL1AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void layoutKnobs();
    int ampType() const { return juce::jlimit (0, al1::kNumTypes - 1, (int) std::lround (value ("ampType"))); }
    int tab() const { return juce::jlimit (0, 3, proc.uiTab.load()); }
    int displayTab() const { return juce::jlimit (0, 1, proc.uiDisplayTab.load()); }
    void showIrMenu (int slot);
    void loadIr (int slot, const juce::File& f);
    void updateDisplay();
    void drawCurve (juce::Graphics&);
    void drawCabinet (juce::Graphics&);
    void drawOutput (juce::Graphics&);
    void paintSwitches (juce::Graphics&);

    struct Knob
    {
        const char* id;
        int tab;                      // 0 .. 3 = the CONTROLS tab, -1 = the OUTPUT panel (always shown)
        int index;                    // position in the knob row of the tab
        const char* name;
        const char* lo;
        const char* hi;
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> att;
    };
    struct Switch
    {
        const char* id;
        int tab;                      // as Knob::tab
        const char* label;
        MidiTarget target;
    };

    AL1AudioProcessor& proc;
    std::array<Knob, 25> knobs;
    std::array<Switch, 5> switches;                 // gate, boost, cab, EQ, limiter: drawn here, MIDI learn through MidiTarget
    std::unique_ptr<MidiLearnUI> midiUI;            // right-click a knob: MIDI learn (rebuilt when the tab changes)
    int midiTab = -1;
    std::unique_ptr<juce::FileChooser> chooser;

    // display data, rebuilt at 30 Hz
    juce::Path curvePath, sineInPath, sineOutPath, totalPath, modelPath, irPath[2];
    float dotX = 0.0f, dotY = 0.0f;
    vibr::ui::OutputMeter meter;                                                       // output meter: bar and peak hold (ballistics shared with the other Labs)
    float inDb = -90.0f, outDb = -90.0f, grDb = 0.0f, gateLamp = -1.0f;
    std::array<float, AL1AudioProcessor::kCurvePoints> irCurve[2] {};
    bool irPresent[2] { false, false };
    juce::String irNames[2];
    al1::CabModel dispCab;
    int latency = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AL1AudioProcessorEditor)
};
