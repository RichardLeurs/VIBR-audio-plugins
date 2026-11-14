// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"

// Black knurled knob with a raised cap and a white pointer (same as the DM-1), drawn over the scales printed in the background.
class MA1LookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

// Invisible click target; the editor paints the hardware under it.
class MA1Zone : public juce::Component
{
public:
    std::function<void()> onPress;
    bool isDown() const { return down; }
    void mouseDown (const juce::MouseEvent& e) override { if (e.mods.isPopupMenu()) return; down = true; if (onPress) onPress(); if (auto* p = getParentComponent()) p->repaint(); }
    void mouseUp (const juce::MouseEvent&) override { down = false; if (auto* p = getParentComponent()) p->repaint(); }
private:
    bool down = false;
};

class MA1AudioProcessorEditor : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit MA1AudioProcessorEditor (MA1AudioProcessor&);
    ~MA1AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void irMenu (int slot);
    void toggle (const char* id);
    void setTab (int newTab);
    void updateTabVisibility();

    struct Knob
    {
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
        juce::Point<float> centre;
        float radius = 36.0f;
    };

    static constexpr int kNumKnobs = 24;
    static constexpr int kNumButtons = 6;

    MA1AudioProcessor& proc;
    MA1LookAndFeel laf;
    juce::ComponentBoundsConstrainer constrainer;
    juce::Image background[2];           // one background per tab
    int tab = 0;                         // 0 = AMP, 1 = CABINET
    Knob knobs[kNumKnobs];
    std::unique_ptr<MidiLearnUI> midiUI;       // right-click a knob: MIDI learn (AMP tab only, rebuilt on a tab change)
    MA1Zone buttons[kNumButtons], irZones[2];
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String lastState;
    bool lastOn = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MA1AudioProcessorEditor)
};
