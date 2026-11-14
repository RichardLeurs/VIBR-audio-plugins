// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"

// Black knob of the VIBR look (see style.md): matt body with a knurled edge, raised cap, white pointer bar.
// The knob is drawn 5 px inside its component so the shadow is not clipped.
class GR1LookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

// GR-1 window, drawn in code on a satin body: header (name, description, power), four panels ENVELOPE (four grain shapes + FREEZE),
// GRAINS (size, density, position, spray), PITCH (pitch, random, reverse) and OUTPUT (spread, mix, level), and below them the CLOUD:
// every grain that sounds is a dot (older to newer, pitch up and down).
class GR1AudioProcessorEditor : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit GR1AudioProcessorEditor (GR1AudioProcessor&);
    ~GR1AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void paintOverChildren (juce::Graphics&) override;

private:
    void timerCallback() override;
    float scale() const { return (float) getWidth() / 1200.0f; }
    void layoutKnobs();
    int choice (const char* id) const { return (int) std::lround (proc.apvts.getRawParameterValue (id)->load()); }
    bool isOn (const char* id) const { return proc.apvts.getRawParameterValue (id)->load() > 0.5f; }
    void toggle (const char* id);
    juce::String knobText (int kind, double v) const;

    struct Knob
    {
        const char* id;
        int panel, index, count;      // panel number, position in the panel, knobs in the panel
        const char* name;
        int kind;                     // 0 percent, 1 size, 2 density, 3 position, 4 pitch (st), 5 random (st), 6 dB
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> att;
    };

    GR1AudioProcessor& proc;
    MidiLearnUI midiUI { proc.midiMap };       // right-click a knob: MIDI learn
    GR1LookAndFeel laf;
    juce::ComponentBoundsConstrainer constrainer;
    juce::Image grain;
    std::array<Knob, 10> knobs;
    juce::ParameterAttachment powerAtt, shapeAtt, freezeAtt, syncAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GR1AudioProcessorEditor)
};
