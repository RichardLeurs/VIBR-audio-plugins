// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"

// Black knob of the VIBR look (see style.md): matt body with a knurled edge, raised cap, white pointer bar.
// The knob is drawn 5 px inside its component so the shadow is not clipped.
class DY1LookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

// DY-1 window, drawn in code on a satin body: header (name, description, power), four panels in a row: GATE, EXPANDER, TRANSIENT (each with its
// own on / off switch: click the title) and OUTPUT (mix, level), and under them the ACTION display: the input level and the gain of the chain
// over the last five seconds, with the gate and expander thresholds.
class DY1AudioProcessorEditor : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit DY1AudioProcessorEditor (DY1AudioProcessor&);
    ~DY1AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    float scale() const { return (float) getWidth() / 1200.0f; }
    void layoutKnobs();
    bool isOn (const char* id) const { return proc.apvts.getRawParameterValue (id)->load() > 0.5f; }
    bool stageOn (int panel) const { return panel == 0 ? isOn ("gateOn") : panel == 1 ? isOn ("expOn") : panel == 2 ? isOn ("transOn") : true; }
    void toggle (const char* id);
    void drawAction (juce::Graphics&);

    struct Knob
    {
        const char* id;
        int panel, index, count;      // panel number, position in the panel, knobs in the panel
        const char* name;
        int kind;                     // 0 threshold (dB), 1 release (ms), 2 ratio, 3 bipolar percent, 4 percent, 5 level (dB)
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> att;
    };

    DY1AudioProcessor& proc;
    DY1LookAndFeel laf;
    juce::ComponentBoundsConstrainer constrainer;
    juce::Image grain;
    std::array<Knob, 9> knobs;
    juce::ParameterAttachment powerAtt, gateAtt, expAtt, transAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DY1AudioProcessorEditor)
};
