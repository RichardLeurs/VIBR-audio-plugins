// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"

namespace ui = vibr::ui::vintage;

namespace
{
const ui::Palette kPal { juce::Colour (230, 83, 60) };

// Design space: 1120 wide (margin 28, gap 18), one row of three equal panels, y 114 to 344, footer centre 15 above the bottom.
constexpr float kDesignW = 1120.0f, kDesignH = 374.0f;
constexpr float kPanelY = 114.0f, kPanelH = 230.0f, kKnobDy = 86.0f;
constexpr float kPanelW = (kDesignW - 2.0f * ui::kMargin - 2.0f * 18.0f) / 3.0f;

juce::Rectangle<float> panelRect (int i) { return { ui::kMargin + (float) i * (kPanelW + 18.0f), kPanelY, kPanelW, kPanelH }; }
juce::Point<float> knobCentre (int i) { return { panelRect (i).getCentreX(), kPanelY + kKnobDy }; }
}

DS1AudioProcessorEditor::DS1AudioProcessorEditor (DS1AudioProcessor& p)
    : PedalEditor (p, kDesignW, kDesignH), proc (p)
{
    for (auto* k : { &tone, &level, &dist })
    {
        auto& s = k->slider;
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        s.setRotaryParameters (juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
        s.setMouseDragSensitivity (180);
        s.setScrollWheelEnabled (false);
        s.setPopupDisplayEnabled (true, true, this);
        s.onValueChange = [this] { repaint(); };
        addAndMakeVisible (s);
        midiUI.add (s, k->id);
        k->attachment = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter (k->id), s, nullptr);
    }

    resized();
    startTimerHz (20);
}

DS1AudioProcessorEditor::~DS1AudioProcessorEditor() = default;

void DS1AudioProcessorEditor::resized()
{
    placeKnob (tone.slider, knobCentre (0));
    placeKnob (level.slider, knobCentre (1));
    placeKnob (dist.slider, knobCentre (2));
}

void DS1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    if (toggleBypassAt (e.position)) repaint();                       // the power switch is the bypass switch
}

void DS1AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    midiUI.paintMarks (g, kPal.accent);
}

void DS1AudioProcessorEditor::timerCallback()
{
    if (midiUI.needsRepaint()) { midiUI.tick(); repaint(); }
    const bool on = powerOn();
    if (on != lastOn) { lastOn = on; repaint(); }
}

void DS1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    ui::paintBody (g, kPal, kDesignW, kDesignH);
    ui::paintHeaderNoPresets (g, kPal, "DS-1", "DISTORTION", "SONIC GRIT", powerOn());

    const char* titles[] = { "FILTER", "OUTPUT", "DRIVE" };
    Knob* knobs[] = { &tone, &level, &dist };
    for (int i = 0; i < 3; ++i)
    {
        ui::paintPanel (g, kPal, panelRect (i), titles[i]);
        auto& s = knobs[i]->slider;
        ui::paintKnob (g, kPal, knobCentre (i), knobs[i]->name, juce::String (s.getValue(), 1), "0", "10",
                       (float) s.valueToProportionOfLength (s.getValue()));
    }
    ui::paintFooter (g, kPal, kDesignH - 15.0f);
}
