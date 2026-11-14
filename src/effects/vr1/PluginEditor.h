// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"

// Black knob of the VIBR look (see style.md): matt body with a knurled edge, raised cap, white pointer bar.
// The knob is drawn 5 px inside its component so the shadow is not clipped.
class VR1LookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

// VR-1 window, drawn in code on a satin body: header (name, description, power), four panels:
// MODE (three mode buttons + the TYPE buttons of the mode), CONTROLS (rate, depth + SYNC), MOTION (a live picture of the effect) and OUTPUT (mix, level).
class VR1AudioProcessorEditor : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit VR1AudioProcessorEditor (VR1AudioProcessor&);
    ~VR1AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    float scale() const { return (float) getWidth() / 1200.0f; }
    void layoutKnobs();
    int currentMode() const { return juce::jlimit (0, vr1::kNumModes - 1, (int) std::lround (proc.apvts.getRawParameterValue ("mode")->load())); }
    int currentVariant() const { return juce::jlimit (0, vr1::variantCount (currentMode()) - 1, (int) std::lround (proc.apvts.getRawParameterValue ("variant")->load())); }
    bool isOn (const char* id) const { return proc.apvts.getRawParameterValue (id)->load() > 0.5f; }
    void toggle (const char* id);
    void drawMotion (juce::Graphics&, int mode, int variant);

    struct Knob
    {
        const char* id;
        int panel, index, count;      // panel number, position in the panel, knobs in the panel
        const char* name;             // nullptr: the name follows the mode (RATE / SPEED / FREQ, INTENSITY / DEPTH)
        int kind;                     // 0 rate, 1 depth, 2 percent, 3 dB
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> att;
    };

    VR1AudioProcessor& proc;
    VR1LookAndFeel laf;
    juce::ComponentBoundsConstrainer constrainer;
    juce::Image grain;
    std::array<Knob, 4> knobs;
    MidiLearnUI midiUI { proc.midiMap };       // right-click a knob: MIDI learn
    juce::ParameterAttachment powerAtt, modeAtt, variantAtt, syncAtt;
    int lastMode = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VR1AudioProcessorEditor)
};
