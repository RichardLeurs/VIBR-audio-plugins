// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ClickZone.h"
#include "ParamMemory.h"

//==============================================================================
// Black knobs drawn over the scales printed in the background.
class RE2LookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;

    // switches are click targets only; their LEDs are painted by the editor
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override {}
};


//==============================================================================
class RE2AudioProcessorEditor : public juce::AudioProcessorEditor,
                                  private juce::Timer
{
public:
    explicit RE2AudioProcessorEditor (RE2AudioProcessor&);
    ~RE2AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct Knob
    {
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
    };

    void addKnob (Knob&, const juce::String& paramId);
    void addSwitch (juce::ToggleButton&, std::unique_ptr<juce::ButtonParameterAttachment>&, const juce::String& paramId);

    RE2AudioProcessor& proc;
    RE2LookAndFeel laf;
    juce::Image background;

    Knob saturation, wowFlutter, mode, bass, treble, reverbVol, repeatRate, intensity, echoVol;

    juce::ToggleButton inputSwitch, tapeSwitch, powerSwitch;
    std::unique_ptr<juce::ButtonParameterAttachment> inputAttachment, tapeAttachment, powerAttachment;

    int lastHeadMask = -1;

    // MEMORY: short press steps MANUAL / 1-4, holding it writes the knobs into the selected slot
    ClickZone memoryZone;
    ParamMemory memory { proc.apvts, { "power" } };
    bool memHeld = false, memWritten = false;
    juce::uint32 memDownAt = 0, memFlashUntil = 0;
    int memShown = -1;
    bool memFlashShown = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RE2AudioProcessorEditor)
};
