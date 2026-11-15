// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "MidiTarget.h"
#include "ui/VibrUI.h"
#include "SkeuoUI.h"

// AX-1 window (1200 x 690), on the shared UI of src/common/ui: header (name, preset chip, undo, OFF / ON), the CONTROLS panel with four tab pills on its top edge
// (GATE / BOOST, AMP, CABINET, EQ / OUTPUT) and the DISPLAY panel with three (CURVE, CABINET, METERS). The display redraws at 30 Hz from the engine's atomics.
class AX1AudioProcessorEditor : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit AX1AudioProcessorEditor (AX1AudioProcessor&);
    ~AX1AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    float scale() const { return (float) getWidth() / 1200.0f; }
    void layoutKnobs();
    bool isOn (const char* id) const { return proc.apvts.getRawParameterValue (id)->load() > 0.5f; }
    float value (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }
    int ampType() const { return juce::jlimit (0, ax1::kNumTypes - 1, (int) std::lround (value ("ampType"))); }
    int tab() const { return juce::jlimit (0, 3, proc.uiTab.load()); }
    int displayTab() const { return juce::jlimit (0, 2, proc.uiDisplayTab.load()); }
    void toggle (const char* id);
    void setChoice (const char* id, int index);
    void showIrMenu (int slot);
    void loadIr (int slot, const juce::File& f);
    void updateDisplay();
    void drawCurve (juce::Graphics&);
    void drawCabinet (juce::Graphics&);
    void drawMeters (juce::Graphics&);
    void paintSwitches (juce::Graphics&);

    struct Knob
    {
        const char* id;
        int tab;
        float x, y;                   // centre in the CONTROLS panel (layout units, from the panel's top left)
        const char* name;
        const char* lo;
        const char* hi;
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> att;
    };
    struct Switch
    {
        const char* id;
        int tab;
        juce::Rectangle<float> r;     // in the CONTROLS panel
        const char* label;
        MidiTarget target;
    };

    AX1AudioProcessor& proc;
    skeuo::KnobLookAndFeel laf;
    juce::ComponentBoundsConstrainer constrainer;
    juce::Image grain;                              // the star grain of the faceplate
    std::array<Knob, 25> knobs;
    std::array<Switch, 5> switches;                 // gate, boost, cab, EQ, limiter: drawn here, MIDI learn through MidiTarget
    std::unique_ptr<MidiLearnUI> midiUI;            // right-click a knob: MIDI learn (rebuilt when the tab changes)
    int midiTab = -1;
    vibr::ui::PresetBarState barState;
    juce::ParameterAttachment powerAtt;
    std::unique_ptr<juce::FileChooser> chooser;

    // display data, rebuilt at 30 Hz
    juce::Path curvePath, sineInPath, sineOutPath, totalPath, modelPath, irPath[2];
    float dotX = 0.0f, dotY = 0.0f;
    float inDb = -90.0f, outDb = -90.0f, grDb = 0.0f, gateLamp = -1.0f;
    float inHold = -90.0f, outHold = -90.0f;       // peak-hold ticks of the meters
    int inHoldAge = 0, outHoldAge = 0;
    std::array<float, AX1AudioProcessor::kCurvePoints> irCurve[2] {};
    bool irPresent[2] { false, false };
    juce::String irNames[2];
    ax1::CabModel dispCab;
    int latency = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AX1AudioProcessorEditor)
};
