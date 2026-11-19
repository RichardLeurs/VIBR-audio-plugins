// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"

namespace ui = vibr::ui::vintage;

namespace
{
const ui::Palette kPal { juce::Colour (233, 185, 63) };

// Design space: 1120 wide (margin 28, gap 18), one row of two equal panels, y 114 to 344, footer centre 15 above the bottom.
constexpr float kDesignW = 1120.0f, kDesignH = 374.0f;
constexpr float kPanelY = 114.0f, kPanelH = 230.0f, kKnobDy = 86.0f, kKnobDx = 110.0f;
constexpr float kPanelW = (kDesignW - 2.0f * ui::kMargin - 18.0f) / 2.0f;

juce::Rectangle<float> panelRect (int i) { return { ui::kMargin + (float) i * (kPanelW + 18.0f), kPanelY, kPanelW, kPanelH }; }
juce::Point<float> knobCentre (int i)       // knobs 0..3: two per panel, symmetric about the panel centre
{
    const auto p = panelRect (i / 2);
    return { p.getCentreX() + ((i % 2) == 0 ? -kKnobDx : kKnobDx), kPanelY + kKnobDy };
}
}

LM2AudioProcessorEditor::LM2AudioProcessorEditor (LM2AudioProcessor& p)
    : PedalEditor (p, kDesignW, kDesignH), proc (p)
{
    for (auto* k : { &level, &tone, &release, &threshold })
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

LM2AudioProcessorEditor::~LM2AudioProcessorEditor() = default;

void LM2AudioProcessorEditor::resized()
{
    placeKnob (level.slider, knobCentre (0));
    placeKnob (tone.slider, knobCentre (1));
    placeKnob (release.slider, knobCentre (2));
    placeKnob (threshold.slider, knobCentre (3));
}

void LM2AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    if (toggleBypassAt (e.position)) repaint();                       // the power switch is the bypass switch
}

void LM2AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    midiUI.paintMarks (g, kPal.accent);
}

void LM2AudioProcessorEditor::timerCallback()
{
    if (midiUI.needsRepaint()) { midiUI.tick(); repaint(); }
    const bool on = powerOn();
    if (on != lastOn) { lastOn = on; repaint(); }
}

void LM2AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    ui::paintBody (g, kPal, kDesignW, kDesignH);
    ui::paintHeaderNoPresets (g, kPal, "LM-2", "PEAK LIMITER", "DYNAMIC CONTROL", powerOn());

    ui::paintPanel (g, kPal, panelRect (0), "OUTPUT");
    ui::paintPanel (g, kPal, panelRect (1), "LIMITER");

    Knob* knobs[] = { &level, &tone, &release, &threshold };
    const char* lo[] = { "MIN", "LO", "MIN", "MIN" };
    const char* hi[] = { "MAX", "HI", "MAX", "MAX" };
    for (int i = 0; i < 4; ++i)
    {
        auto& s = knobs[i]->slider;
        ui::paintKnob (g, kPal, knobCentre (i), knobs[i]->name, juce::String (s.getValue(), 1), lo[i], hi[i],
                       (float) s.valueToProportionOfLength (s.getValue()));
    }
    ui::paintFooter (g, kPal, kDesignH - 15.0f);
}
