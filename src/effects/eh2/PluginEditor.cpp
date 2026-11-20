// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"

namespace ui = vibr::ui::vintage;

namespace
{
const ui::Palette kPal { juce::Colour (32, 182, 208) };

// Design space: 1120 wide (margin 28, gap 18), one row of three equal panels, y 114 to 344, footer centre 15 above the bottom.
constexpr float kDesignW = 1120.0f, kDesignH = 374.0f;
constexpr float kPanelY = 114.0f, kPanelH = 230.0f, kKnobDy = 96.0f, kBigR = 44.0f, kLabelUp = 12.0f;
constexpr float kPanelW = (kDesignW - 2.0f * ui::kMargin - 2.0f * 18.0f) / 3.0f;

juce::Rectangle<float> panelRect (int i) { return { ui::kMargin + (float) i * (kPanelW + 18.0f), kPanelY, kPanelW, kPanelH }; }
juce::Point<float> knobCentre (int i) { return { panelRect (i).getCentreX(), kPanelY + kKnobDy }; }
juce::Point<float> lampCentre() { return { panelRect (2).getCentreX() + 120.0f, kPanelY + kKnobDy }; }   // SENS lamp: lit while the effect is open
}

EH2AudioProcessorEditor::EH2AudioProcessorEditor (EH2AudioProcessor& p)
    : PedalEditor (p, kDesignW, kDesignH), proc (p)
{
    for (auto* k : { &mix, &freq, &sens })
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
        k->attachment = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter (k->id), s, nullptr);
    }

    resized();
    startTimerHz (20);
}

EH2AudioProcessorEditor::~EH2AudioProcessorEditor() = default;

void EH2AudioProcessorEditor::resized()
{
    placeKnob (mix.slider, knobCentre (0), kBigR);
    placeKnob (freq.slider, knobCentre (1), kBigR);
    placeKnob (sens.slider, knobCentre (2), kBigR);
}

void EH2AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / scale();
    proc.undoManager.beginNewTransaction();
    if (ui::presetBarMouseDown (p, *this, proc.presets, proc.undoManager, scale(), [this] { repaint(); })) return;
    if (toggleBypassAt (e.position)) repaint();                       // the power switch is the bypass switch
}

void EH2AudioProcessorEditor::timerCallback()
{
    const bool on = powerOn();
    const bool lit = on && proc.getOpen() > 0.5f;
    if (on != lastOn || lit != lastSens) { lastOn = on; lastSens = lit; repaint(); }
}

void EH2AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    ui::paintBody (g, kPal, kDesignW, kDesignH);
    ui::paintHeader (g, kPal, "EH-2", "ENHANCER", "SONIC CLARITY");
    ui::paintPresetBar (g, kPal, proc.presets, proc.undoManager, powerOn());

    const char* titles[] = { "ENHANCE", "FREQUENCY", "SENSITIVITY" };
    const char* lo[] = { "INV", "LO", "MIN" };
    const char* hi[] = { "NOR", "HI", "MAX" };
    Knob* knobs[] = { &mix, &freq, &sens };
    for (int i = 0; i < 3; ++i)
    {
        ui::paintPanel (g, kPal, panelRect (i), titles[i]);
        auto& s = knobs[i]->slider;
        const auto c = knobCentre (i);
        const float prop = (float) s.valueToProportionOfLength (s.getValue());
        ui::paintKnob (g, kPal, c, knobs[i]->name, juce::String (s.getValue(), 1), lo[i], hi[i], i == 0 ? 0.0f : prop, kBigR, kLabelUp);
        if (i == 0) ui::paintBipolarArc (g, c, kBigR, prop * 2.0f - 1.0f, kPal.accent, kPal.track);
    }

    // SENS lamp (red, lit while the effect is open)
    const auto lc = lampCentre();
    const auto disc = [&] (float r) { return juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (lc); };
    if (lastSens)
    {
        g.setColour (juce::Colour (0xffff4a2a).withAlpha (0.18f)); g.fillEllipse (disc (14.0f));
        g.setColour (juce::Colour (0xffff4a2a)); g.fillEllipse (disc (7.0f));
    }
    else { g.setColour (juce::Colour (0xff5a2a20)); g.fillEllipse (disc (7.0f)); }
    g.setColour (kPal.ink); g.drawEllipse (disc (8.0f), 2.0f);
    ui::drawText (g, "OPEN", lc.x, lc.y + 28.0f, 13.0f, kPal.inkSoft, false, 0.1f);

    ui::paintFooter (g, kPal, kDesignH - 15.0f);
}
