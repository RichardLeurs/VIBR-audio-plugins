// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"

namespace ui = vibr::ui::vintage;

namespace
{
const ui::Palette kPal { juce::Colour (232, 75, 145) };

// Design space: 1120 wide (margin 28, gap 18), one row of three panels LEVEL 450 / LOW 250 / MODE 328, y 114 to 344.
constexpr float kDesignW = 1120.0f, kDesignH = 374.0f;
constexpr float kPanelY = 114.0f, kPanelH = 230.0f, kKnobDy = 86.0f, kGap = 18.0f;
constexpr float kWLevel = 450.0f, kWLow = 250.0f, kWMode = 328.0f;

juce::Rectangle<float> panelRect (int i)
{
    const float x[] = { ui::kMargin, ui::kMargin + kWLevel + kGap, ui::kMargin + kWLevel + kWLow + 2.0f * kGap };
    const float w[] = { kWLevel, kWLow, kWMode };
    return { x[i], kPanelY, w[i], kPanelH };
}
juce::Point<float> knobCentre (int k)         // 0..2 = LEVEL, 3 = LOW
{
    if (k == 3) return { panelRect (1).getCentreX(), kPanelY + kKnobDy };
    return { panelRect (0).getCentreX() + (float) (k - 1) * 150.0f, kPanelY + kKnobDy };
}
// VINTAGE / POLY: two buttons that share their middle border, centred in the MODE panel
juce::Rectangle<float> modeButton (int i)
{
    const auto p = panelRect (2);
    return juce::Rectangle<float> (132.0f, 34.0f).withCentre ({ p.getCentreX() - 65.0f + 130.0f * (float) i, p.getCentreY() });
}
} // namespace

OC5AudioProcessorEditor::OC5AudioProcessorEditor (OC5AudioProcessor& p)
    : PedalEditor (p, kDesignW, kDesignH), proc (p)
{
    for (auto* k : { &direct, &up1, &down1, &down2 })
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

OC5AudioProcessorEditor::~OC5AudioProcessorEditor() = default;

void OC5AudioProcessorEditor::resized()
{
    placeKnob (direct.slider, knobCentre (0));
    placeKnob (up1.slider, knobCentre (1));
    placeKnob (down1.slider, knobCentre (2));
    placeKnob (down2.slider, knobCentre (3));
}

void OC5AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    if (ui::presetBarMouseDown (e.position / scale(), *this, proc.presets, proc.undoManager, scale(), [this] { repaint(); })) return;
    if (toggleBypassAt (e.position)) { repaint(); return; }       // the power switch is the bypass switch

    const auto p = e.position / scale();
    for (int i = 0; i < 2; ++i)                                    // 0 = VINTAGE, 1 = POLY
        if (modeButton (i).expanded (3.0f).contains (p))
        {
            auto* prm = proc.apvts.getParameter ("poly");
            prm->beginChangeGesture();
            prm->setValueNotifyingHost ((float) i);
            prm->endChangeGesture();
            repaint();
            return;
        }
}

void OC5AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    midiUI.paintMarks (g, kPal.accent);
}

void OC5AudioProcessorEditor::timerCallback()
{
    if (midiUI.needsRepaint()) { midiUI.tick(); repaint(); }
    const bool on = powerOn();
    const bool poly = proc.apvts.getRawParameterValue ("poly")->load() > 0.5f;
    if (on != lastOn || poly != lastPoly) { lastOn = on; lastPoly = poly; repaint(); }
}

void OC5AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    ui::paintBody (g, kPal, kDesignW, kDesignH);
    ui::paintHeader (g, kPal, "OC-5", "OCTAVE", "OCTAVE HARMONY");
    ui::paintPresetBar (g, kPal, proc.presets, proc.undoManager, powerOn());

    ui::paintPanel (g, kPal, panelRect (0), "LEVEL");
    ui::paintPanel (g, kPal, panelRect (1), "LOW");
    ui::paintPanel (g, kPal, panelRect (2), "MODE");

    Knob* knobs[] = { &direct, &up1, &down1, &down2 };
    for (int k = 0; k < 4; ++k)
    {
        auto& s = knobs[k]->slider;
        ui::paintKnob (g, kPal, knobCentre (k), knobs[k]->name, juce::String (s.getValue(), 1), k == 3 ? "LOWEST" : "0", k == 3 ? "MAX" : "10",
                       (float) s.valueToProportionOfLength (s.getValue()));
    }

    const bool poly = proc.apvts.getRawParameterValue ("poly")->load() > 0.5f;
    ui::paintFlatButton (g, kPal, modeButton (0), "VINTAGE", ! poly);
    ui::paintFlatButton (g, kPal, modeButton (1), "POLY", poly);
    ui::paintFooter (g, kPal, kDesignH - 15.0f);
}
