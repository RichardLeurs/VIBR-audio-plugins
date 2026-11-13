// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"

// Black knob of the VIBR look (see style.md): matt body with a knurled edge, raised cap, white pointer bar.
// The knob is drawn 5 px inside its component so the shadow is not clipped.
class HM2LookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

// HM-2 window, drawn in code on a satin body: header (name, description, power), four panels:
// CHARACTER (four saturators + a harmonic display), HARMONICS (drive, odd/even, octave, sub), FREQUENCY (focus, tilt, tone) and OUTPUT (mix, level).
class HM2AudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit HM2AudioProcessorEditor (HM2AudioProcessor&);
    ~HM2AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    float scale() const { return (float) getWidth() / 1200.0f; }
    void layoutKnobs();
    int currentCharacter() const { return juce::jlimit (0, hm2::kNumCharacters - 1, (int) std::lround (proc.apvts.getRawParameterValue ("character")->load())); }
    bool isOn (const char* id) const { return proc.apvts.getRawParameterValue (id)->load() > 0.5f; }
    void toggle (const char* id);

    struct Knob
    {
        const char* id;
        int panel, index, count;      // panel number, position in the panel, knobs in the panel
        const char* name;             // nullptr: the TEXTURE name of the current mode
        int kind;                     // 0 percent, 1 speed (x), 2 dB
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> att;
    };

    HM2AudioProcessor& proc;
    HM2LookAndFeel laf;
    juce::ComponentBoundsConstrainer constrainer;
    juce::Image grain;
    std::array<Knob, 9> knobs;
    juce::ParameterAttachment powerAtt, charAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HM2AudioProcessorEditor)
};
