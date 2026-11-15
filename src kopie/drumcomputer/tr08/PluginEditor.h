// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"

//==============================================================================
// Black knurled knob with a raised cap and a white pointer, drawn over the scales printed in the background.
class TR08LookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

// Invisible click target; the editor paints the hardware under it.
class Zone : public juce::Component
{
public:
    std::function<void()> onPress;
    bool isDown() const { return down; }

    void mouseDown (const juce::MouseEvent&) override
    {
        down = true;
        if (onPress) onPress();
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
class TR08AudioProcessorEditor : public juce::AudioProcessorEditor,
                                 private juce::Timer
{
public:
    explicit TR08AudioProcessorEditor (TR08AudioProcessor&);
    ~TR08AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void timerCallback() override;

    struct Knob
    {
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
        float radius = 16.0f;       // image pixels
        juce::Point<float> centre;  // image pixels
    };

    void addKnob (Knob&, const char* paramId, juce::Point<float> centre, float radius, const char* style,
                  float startDeg = -135.0f, float endDeg = 135.0f);
    void addZone (Zone&, std::function<void()> onPress);

    float get (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }
    void setValue (const char* id, float v);
    void toggle (const char* id);
    int editVar() const;      // variation (A = 0, B = 1) the step buttons show and edit; AB shows A
    int editHalf() const;     // 1st / 2nd part shown (follows the Pattern Write knob)
    void pressStep (int step);
    void trackClear();
    void tap();
    void showKitMenu();
    void stepVariant (int inst, int dir);
    void setSwitchFrom (int which, juce::Point<float> p);
    int draggingSwitch = 0;        // 0 none, 1 pre-scale, 2 basic variation, 3 I/F variation

    TR08AudioProcessor& proc;
    TR08LookAndFeel laf;
    juce::ComponentBoundsConstrainer constrainer;
    juce::Image background;   // rendered by make_background.py (2400 x 1400)

    static constexpr int kNumKnobs = 27;
    Knob knobs[kNumKnobs];
    Zone altZones[5], stepZones[16], startZone, tapZone, bankZone, clearZone, kitZone;

    std::vector<double> taps;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String lastState;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TR08AudioProcessorEditor)
};
