// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"

// Black knob of the VIBR look (see style.md): matt body with a knurled edge, raised cap, white pointer bar.
// The knob is drawn 5 px inside its component so the shadow is not clipped.
class MF1LookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

// MF-1 window, drawn in code on a satin body: header (name, description, power), five panels
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
    MF1LookAndFeel laf;
    juce::ComponentBoundsConstrainer constrainer;
    juce::Image grain;
    std::array<Knob, 7> knobs;
    MidiLearnUI midiUI { proc.midiMap };       // right-click a knob: MIDI learn
    juce::ParameterAttachment powerAtt, typeAtt, sourceAtt, syncAtt;
    int lastStep = -1, dragStep = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MF1AudioProcessorEditor)
};
