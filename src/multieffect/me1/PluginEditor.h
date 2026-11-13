// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"

// Black knob of the VIBR look (see style.md): matt body with a knurled edge, raised cap, white pointer bar.
// The knob is drawn 5 px inside its component so the shadow is not clipped.
class ME1LookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

// ME-1 window, drawn in code on a satin body: header (name, description, input / output, power), a strip of module tiles
// (colour bar = module group; add, remove, bypass, drag to reorder, scroll when there are many) and the selected module's panel below.
class ME1AudioProcessorEditor : public juce::AudioProcessorEditor,
                                private juce::ChangeListener,
                                private juce::Timer
{
public:
    explicit ME1AudioProcessorEditor (ME1AudioProcessor&);
    ~ME1AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;                        // level meters
    float scale() const { return (float) getWidth() / 1200.0f; }
    float tileW() const;
    float maxScroll() const;
    juce::Rectangle<float> tileRect (int i) const;            // layout units; i == numSlots: the add tile
    int tileAt (juce::Point<float> p) const;                  // -1 outside
    juce::Rectangle<float> panelRect() const;
    juce::Rectangle<float> pillRect() const;
    juce::Rectangle<float> syncRect() const;                 // the SYNC button of tempo-synced modules
    juce::Point<float> knobCentre (int k, int n) const;
    void ensureVisible (int slot);
    void rebuildKnobs();
    void layoutKnobs();
    void addMenu();
    void slotMenu (int slot);
    void paintKnobScale (juce::Graphics&, juce::Point<float> c, float knobR, bool dim) const;

    ME1AudioProcessor& proc;
    ME1LookAndFeel laf;
    juce::ComponentBoundsConstrainer constrainer;
    juce::Slider inKnob, outKnob;
    std::unique_ptr<juce::SliderParameterAttachment> inAtt, outAtt;
    juce::ParameterAttachment powerAtt;
    std::array<std::unique_ptr<juce::Slider>, me1::kMaxParams> knobs;      // the selected module's knobs
    juce::Image grain;
    float level[4] {}, hold[4] {}, holdAge[4] {};         // meters: in L, in R, out L, out R (0..1 of the -60 .. 0 dB scale)
    float scroll = 0.0f;
    int dragFrom = -1;
    bool dragged = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ME1AudioProcessorEditor)
};
