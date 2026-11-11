// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"

class DC1LookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

// DC-1 window: header (title, input / output gain), the chain strip (tiles + the add button) and the selected pedal's own editor.
class DC1AudioProcessorEditor : public juce::AudioProcessorEditor,
                                private juce::ChangeListener
{
public:
    explicit DC1AudioProcessorEditor (DC1AudioProcessor&);
    ~DC1AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void showInner();                                   // (re)creates the view of the selected pedal
    void dropInner();
    void fitHeight();
    juce::Rectangle<float> tileRect (int i) const;      // tile i in window pixels (i == numSlots: the add button)
    int tileAt (juce::Point<float>) const;
    void addMenu();

    struct Knob
    {
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
    };

    DC1AudioProcessor& proc;
    DC1LookAndFeel laf;
    juce::ComponentBoundsConstrainer constrainer;
    Knob inKnob, outKnob;
    std::unique_ptr<juce::AudioProcessorEditor> inner;
    juce::AudioProcessor* innerProc = nullptr;
    float innerRatio = 0.5f;                            // height / width of the pedal editor
    int dragFrom = -1;
    bool dragged = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DC1AudioProcessorEditor)
};
