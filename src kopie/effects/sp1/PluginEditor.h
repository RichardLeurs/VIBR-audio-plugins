// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"

// Black knob of the VIBR look (see style.md): matt body with a knurled edge, raised cap, white pointer bar.
// The knob is drawn 5 px inside its component so the shadow is not clipped.
class SP1LookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

// SP-1 window, drawn in code on a satin body: header (name, description, power), four panels:
// MODE (five mode buttons + FREEZE), SPACE (size, decay, diffusion), CHARACTER (tone, motion, texture) and OUTPUT (mix, level).
class SP1AudioProcessorEditor : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit SP1AudioProcessorEditor (SP1AudioProcessor&);
    ~SP1AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    float scale() const { return (float) getWidth() / 1200.0f; }
    void layoutKnobs();
    int currentMode() const { return juce::jlimit (0, sp1::kNumModes - 1, (int) std::lround (proc.apvts.getRawParameterValue ("mode")->load())); }
    bool isOn (const char* id) const { return proc.apvts.getRawParameterValue (id)->load() > 0.5f; }
    void toggle (const char* id);

    struct Knob
    {
        const char* id;
        int panel, index, count;      // panel number, position in the panel, knobs in the panel
        const char* name;             // nullptr: the TEXTURE name of the current mode
        bool dB;
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> att;
    };

    SP1AudioProcessor& proc;
    MidiLearnUI midiUI { proc.midiMap };       // right-click a knob: MIDI learn
    SP1LookAndFeel laf;
    juce::ComponentBoundsConstrainer constrainer;
    juce::Image grain;
    std::array<Knob, 8> knobs;
    juce::ParameterAttachment powerAtt, modeAtt, freezeAtt;
    int lastMode = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SP1AudioProcessorEditor)
};
