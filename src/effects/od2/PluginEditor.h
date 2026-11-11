// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ClickZone.h"
#include "WrapSlider.h"
#include "ParamMemory.h"

// Black knurled knob with a raised cap and a white pointer (same as the DM-1), drawn over the scales printed in the background.
class OD2LookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};




class OD2AudioProcessorEditor : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit OD2AudioProcessorEditor (OD2AudioProcessor&);
    ~OD2AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void selectSlot (int s);

    struct Knob
    {
        WrapSlider slider;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
    };

    OD2AudioProcessor& proc;
    OD2LookAndFeel laf;
    juce::ComponentBoundsConstrainer constrainer;
    juce::Image background;
    Knob type, param, level, drive, low, middle, high;
    ClickZone paramZone, memoryZone;

    int slot = 0;                                  // what the PARAM knob edits: TYPE param, POST, PRE, DECAY, THRESH
    bool lastOn = true, lastBoost = false, valueShown = false, memHeld = false, memWritten = false, memFlashShown = false;
    int lastType = -1, memShown = -1;
    juce::uint32 shownUntil = 0, memDownAt = 0, memFlashUntil = 0;
    ParamMemory memory { proc.apvts, { "bypass" } };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OD2AudioProcessorEditor)
};
