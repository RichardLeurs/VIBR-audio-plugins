// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "MidiTarget.h"
#include "ui/VibrUI.h"

// SA-1 window, drawn in code on a satin body: header (name, description, power), four stage panels in a row: BOOST, TUBE, TAPE and CRUSH (each
// with its own on / off switch: click the title), and under them the SHAPE display (the transfer curve and the waveform of the chain) and OUTPUT (MIX, LEVEL and
// the L / R dB meters). Shared UI of src/common/ui: preset bar, knob value ring, output meter.
class SA1AudioProcessorEditor : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit SA1AudioProcessorEditor (SA1AudioProcessor&);
    ~SA1AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    float scale() const { return (float) getWidth() / 1200.0f; }
    void layoutKnobs();
    bool isOn (const char* id) const { return proc.apvts.getRawParameterValue (id)->load() > 0.5f; }
    bool stageOn (int panel) const { return panel == 0 ? isOn ("boostOn") : panel == 1 ? isOn ("tubeOn") : panel == 2 ? isOn ("tapeOn") : panel == 3 ? isOn ("crushOn") : true; }
    float value (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }
    void toggle (const char* id);
    void drawShape (juce::Graphics&);

    struct Knob
    {
        const char* id;
        int panel, index, count;      // panel number, position in the panel, knobs in the panel
        const char* name;
        int kind;                     // 0 gain (dB), 1 percent, 2 bits, 3 rate (Hz), 4 level (dB)
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> att;
    };

    SA1AudioProcessor& proc;
    vibr::ui::KnobLookAndFeel laf;
    juce::ComponentBoundsConstrainer constrainer;
    juce::Image grain;
    std::array<Knob, 11> knobs;
    MidiTarget stageTarget[4];                 // over the stage pills (BOOST, TUBE, TAPE, CRUSH switches): MIDI learn targets
    MidiLearnUI midiUI { proc.midiMap };       // right-click a knob: MIDI learn
    juce::ParameterAttachment powerAtt, boostAtt, tubeAtt, tapeAtt, crushAtt;
    vibr::ui::PresetBarState barState;
    vibr::ui::OutputMeter meter;                                                                       // output dB meters
    bool shownOn[4] { true, true, true, true };                                                        // the stage switches the knob dimming was last set for

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SA1AudioProcessorEditor)
};
