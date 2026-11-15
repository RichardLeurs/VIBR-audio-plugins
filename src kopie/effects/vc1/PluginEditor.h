// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"

// Black knob of the VIBR look (see style.md): matt body with a knurled edge, raised cap, white pointer bar.
// The knob is drawn 5 px inside its component so the shadow is not clipped.
class VC1LookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

// VC-1 window, drawn in code on a satin body: header (name, description, power), the MODULATION MATRIX (five sources in rows, with their own
// knobs and a lamp that shows the source live, four destinations in columns, a depth knob -100 .. +100 at every crossing),
// the FILTER panel (cutoff, resonance, type) and CROSS / OUTPUT (X-MOD, mix, level).
class VC1AudioProcessorEditor : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit VC1AudioProcessorEditor (VC1AudioProcessor&);
    ~VC1AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    float scale() const { return (float) getWidth() / 1200.0f; }
    void layoutKnobs();
    int choice (const char* id) const { return (int) std::lround (proc.apvts.getRawParameterValue (id)->load()); }
    bool isOn (const char* id) const { return proc.apvts.getRawParameterValue (id)->load() > 0.5f; }
    void toggle (const char* id);
    void setChoice (const char* id, int index);
    juce::String knobText (int kind, double v) const;

    struct Knob
    {
        const char* id = nullptr;
        int kind = 0;
        float cx = 0, cy = 0, r = 20;
        const char* name = nullptr;
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> att;
    };

    static constexpr const char* matrixIds[vc1::kNumSources][vc1::kNumDests] = {
        { "m00", "m01", "m02", "m03" }, { "m10", "m11", "m12", "m13" }, { "m20", "m21", "m22", "m23" },
        { "m30", "m31", "m32", "m33" }, { "m40", "m41", "m42", "m43" } };

    VC1AudioProcessor& proc;
    VC1LookAndFeel laf;
    juce::ComponentBoundsConstrainer constrainer;
    juce::Image grain;
    std::vector<std::unique_ptr<Knob>> knobs;
    juce::ParameterAttachment powerAtt, syncAtt, typeAtt, shape1Att, shape2Att;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VC1AudioProcessorEditor)
};
