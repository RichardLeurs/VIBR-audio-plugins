// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"

// Black knob of the VIBR look (see style.md): matt body with a knurled edge, raised cap, white pointer bar.
// The knob is drawn 5 px inside its component so the shadow is not clipped.
class PT1LookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

// PT-1 window, drawn in code on a satin body: header (name, description, power), four VOICE panels (INTERVAL, DETUNE, LEVEL and the note
// the voice plays now), and below them KEY (key, scale, INTELLIGENT, the note heard), CHORDS (five presets for the four intervals) and OUTPUT.
class PT1AudioProcessorEditor : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit PT1AudioProcessorEditor (PT1AudioProcessor&);
    ~PT1AudioProcessorEditor() override;

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
        const char* id = nullptr;
        int kind = 0;
        float cx = 0, cy = 0, r = 20;
        const char* name = nullptr;
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> att;
    };

    PT1AudioProcessor& proc;
    MidiLearnUI midiUI { proc.midiMap };       // right-click a knob: MIDI learn
    PT1LookAndFeel laf;
    juce::ComponentBoundsConstrainer constrainer;
    juce::Image grain;
    std::vector<std::unique_ptr<Knob>> knobs;
    juce::ParameterAttachment powerAtt, intelAtt, rootAtt, scaleAtt;
    juce::StringArray keepIds;                         // keeps the parameter ids alive for the knobs

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PT1AudioProcessorEditor)
};
