// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"

//==============================================================================
// Black knurled knob with a raised cap and a white pointer (same as the DM-1), drawn over the scales printed in the background.
class SDD3000LookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

// Invisible click target; the editor paints the hardware under it.
class Zone : public juce::Component
{
public:
    std::function<void (juce::Point<float>)> onPress;   // local position of the mouse-down
    bool isDown() const { return down; }

    void mouseDown (const juce::MouseEvent& e) override
    {
        down = true;
        if (onPress) onPress (e.position);
        if (auto* p = getParentComponent()) p->repaint();
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        down = false;
        if (auto* p = getParentComponent()) p->repaint();
    }

private:
    bool down = false;
};

//==============================================================================
class SDD3000AudioProcessorEditor : public juce::AudioProcessorEditor,
                                    private juce::Timer
{
public:
    explicit SDD3000AudioProcessorEditor (SDD3000AudioProcessor&);
    ~SDD3000AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct Knob
    {
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
        float radius = 31.0f;       // image pixels
        juce::Point<float> centre;  // image pixels
    };

    void addKnob (Knob&, const juce::String& paramId, juce::Point<float> centre, float radius, bool cream = false);
    void addZone (Zone&, std::function<void (juce::Point<float>)> onPress);

    float get (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }
    void setInt (const char* id, int v);
    void toggle (const char* id);
    void stepDelay (int delta);

    SDD3000AudioProcessor& proc;
    SDD3000LookAndFeel laf;
    juce::ComponentBoundsConstrainer constrainer;
    juce::Image background;   // the image with the printed display, LEDs and switches patched out

    Knob level, feedback, waveform, intensity, balance;
    Zone attInZone, attOutZone, filterZone, writeZone, progZone, downZone, upZone,
         invZone, holdZone, bypassZone, invOutZone, powerZone;

    float meterDb = -60.0f;
    int heldFrames = 0;
    int writeFlash = 0;                  // frames left of the display blink after WRITE
    juce::String lastState;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SDD3000AudioProcessorEditor)
};
