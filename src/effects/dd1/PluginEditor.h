// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ParamMemory.h"

// Knurled chrome knob with a dark pointer, drawn over the printed scale of the background.
class DD1LookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

class DD1AudioProcessorEditor : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit DD1AudioProcessorEditor (DD1AudioProcessor&);
    ~DD1AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void toggleParam (const char* id);
    void tap();
    // void stepPreset (int dir);        // presets panel replaced by the memory panel
    // void showPresetMenu();

    struct Knob
    {
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
        juce::Point<float> centre;
        float radius = 40.0f;
    };

    DD1AudioProcessor& proc;
    DD1LookAndFeel laf;
    juce::ComponentBoundsConstrainer constrainer;
    juce::Image background;
    std::vector<std::unique_ptr<Knob>> knobs;
    bool lastOn = true, lastSeries = false;
    int lastPing = 0;
    // MEMORY: short press steps MANUAL / 1-4, holding it writes the knobs into the selected slot
    ParamMemory memory { proc.apvts, { "bypass" } };
    bool memHeld = false, memWritten = false, memFlashShown = false;
    int memShown = -1;
    juce::uint32 memDownAt = 0, memFlashUntil = 0;
    bool lastHost = false, lastBlink = false;
    float lastBpm = 0.0f;
    std::vector<double> taps;
    double lastTap = -1000.0;
    float lastMs[2] { -1.0f, -1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DD1AudioProcessorEditor)
};
