// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ClickZone.h"
#include "ParamMemory.h"
#include "ui/VibrUI.h"

//==============================================================================
// Chrome knobs drawn over the knobs printed in the background photo.
class DM1LookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

//==============================================================================
class DM1AudioProcessorEditor : public juce::AudioProcessorEditor,
                                  private juce::Timer
{
public:
    explicit DM1AudioProcessorEditor (DM1AudioProcessor&);
    ~DM1AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void paintOverChildren (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void syncChanged();
    void timerCallback() override;
    void togglePower();
    void cycleDivision();

    struct Knob
    {
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
    };
    void addKnob (Knob&, const juce::String& paramId);

    DM1AudioProcessor& proc;
    DM1LookAndFeel laf;
    juce::Image background;
    juce::Colour panelColour { 0xff1c1d21 };      // sampled from the DELAY panel of the background; used to dim the printed legends

    MidiLearnUI midiUI { proc.midiMap };       // right-click a knob: MIDI learn
    Knob modRate, modDepth, variation, delayTime, intensity, delayVolume, mode;
    ClickZone powerZone, tapZone, divisionZone, memoryZone;
    ClickZone memLedZone[5], divLedZone[5];       // click a lamp to select it (memory slot / tap division)
    juce::ParameterAttachment syncAtt, divisionAtt;      // SYNC button (drawn in paint) and the division readout
    std::unique_ptr<juce::SliderParameterAttachment> divisionKnobAtt;      // the DELAY TIME knob while sync is on
    int divisionIdx = 0;

    // MEMORY: short press steps MANUAL / 1-4, holding it writes the knobs into the selected slot
    ParamMemory memory { proc.apvts, { "power", "bypass" } };
    bool memHeld = false, memWritten = false;
    juce::uint32 memDownAt = 0, memFlashUntil = 0;
    int memShown = -1;
    bool memFlashShown = false;

    bool powerOn = true, tapOn = false, syncShown = false;
    int division = 2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DM1AudioProcessorEditor)
};
