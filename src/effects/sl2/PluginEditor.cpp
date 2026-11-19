// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"

namespace ui = vibr::ui::vintage;

namespace
{
const ui::Palette kPal { juce::Colour (165, 201, 67) };

// Design space: 1120 wide (margin 28, gap 18). Row 1 (y 114, 230 high): OUTPUT 330 / SLICE 330 / MODE 368. Row 2 (y 362, 76 high): VARIATION 678 / TEMPO SOURCE 368.
constexpr float kDesignW = 1120.0f, kDesignH = 468.0f;
constexpr float kRow1Y = 114.0f, kRow1H = 230.0f, kRow2Y = 362.0f, kRow2H = 76.0f, kKnobDy = 86.0f, kGap = 18.0f;
constexpr float kWKnobs = 330.0f, kWMode = 368.0f, kWVar = 678.0f;
constexpr int kNumVariations = 11, kNumModes = 5;
const char* const kModeNames[kNumModes] = { "SINGLE", "DUAL", "TREMOLO", "SFX", "HARMONIC" };

juce::Rectangle<float> panelRect (int i)       // 0 OUTPUT, 1 SLICE, 2 MODE, 3 VARIATION, 4 TEMPO SOURCE
{
    const float x2 = ui::kMargin + 2.0f * (kWKnobs + kGap);
    switch (i)
    {
        case 0:  return { ui::kMargin, kRow1Y, kWKnobs, kRow1H };
        case 1:  return { ui::kMargin + kWKnobs + kGap, kRow1Y, kWKnobs, kRow1H };
        case 2:  return { x2, kRow1Y, kWMode, kRow1H };
        case 3:  return { ui::kMargin, kRow2Y, kWVar, kRow2H };
        default: return { x2, kRow2Y, kWMode, kRow2H };
    }
}
juce::Point<float> knobCentre (int k)          // 0 BALANCE, 1 TEMPO (OUTPUT), 2 ATTACK, 3 DUTY (SLICE)
{
    const auto p = panelRect (k / 2);
    return { p.getCentreX() + (k % 2 == 0 ? -75.0f : 75.0f), kRow1Y + kKnobDy };
}
juce::Rectangle<float> variationButton (int i)  // 11 buttons of 52 with a gap of 6, centred in the VARIATION panel
{
    const auto p = panelRect (3);
    const float total = 52.0f * kNumVariations + 6.0f * (kNumVariations - 1), x0 = p.getCentreX() - total * 0.5f;
    return { x0 + 58.0f * (float) i, p.getCentreY() - 14.0f, 52.0f, 30.0f };   // -14: the panel line is 2 thick, so the visual middle is 1 below the centre
}
juce::Rectangle<float> modeButton (int i)       // a column of 5 buttons (30 high, 8 apart), centred in the MODE panel
{
    const auto p = panelRect (2);
    const float total = 5.0f * 30.0f + 4.0f * 8.0f;
    return juce::Rectangle<float> (200.0f, 30.0f).withCentre ({ p.getCentreX(), p.getY() + 1.0f + (p.getHeight() - 1.0f) * 0.5f - total * 0.5f + 15.0f + 38.0f * (float) i });
}
juce::Rectangle<float> sourceButton (int i)     // KNOB / HOST: two buttons that share their middle border
{
    const auto p = panelRect (4);
    return { p.getCentreX() - 140.0f + 140.0f * (float) i - 1.0f * (float) i, p.getCentreY() - 14.0f, 141.0f, 30.0f };
}
} // namespace

SL2AudioProcessorEditor::SL2AudioProcessorEditor (SL2AudioProcessor& p)
    : PedalEditor (p, kDesignW, kDesignH), proc (p)
{
    for (auto* k : { &balance, &tempo, &attack, &duty })
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

SL2AudioProcessorEditor::~SL2AudioProcessorEditor() = default;

void SL2AudioProcessorEditor::resized()
{
    placeKnob (balance.slider, knobCentre (0));
    placeKnob (tempo.slider, knobCentre (1));
    placeKnob (attack.slider, knobCentre (2));
    placeKnob (duty.slider, knobCentre (3));
}

void SL2AudioProcessorEditor::setParam (const char* id, float value01)
{
    auto* prm = proc.apvts.getParameter (id);
    prm->beginChangeGesture();
    prm->setValueNotifyingHost (value01);
    prm->endChangeGesture();
    repaint();
}

void SL2AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    if (toggleBypassAt (e.position)) { repaint(); return; }       // the power switch is the bypass switch

    const auto p = e.position / scale();
    for (int i = 0; i < kNumVariations; ++i)
        if (variationButton (i).expanded (3.0f).contains (p))
        {
            setParam ("variation", proc.apvts.getParameter ("variation")->convertTo0to1 ((float) (i + 1)));
            return;
        }
    for (int i = 0; i < kNumModes; ++i)
        if (modeButton (i).expanded (3.0f).contains (p))
        {
            setParam ("mode", proc.apvts.getParameter ("mode")->convertTo0to1 ((float) i));
            return;
        }
    for (int i = 0; i < 2; ++i)                                    // 0 = KNOB, 1 = HOST
        if (sourceButton (i).expanded (3.0f).contains (p))
        {
            setParam ("hostSync", (float) i);
            return;
        }
}

void SL2AudioProcessorEditor::timerCallback()
{
    const bool on = powerOn();
    const bool host = proc.apvts.getRawParameterValue ("hostSync")->load() > 0.5f;
    const int variation = (int) std::lround (proc.apvts.getRawParameterValue ("variation")->load());
    const int mode = (int) std::lround (proc.apvts.getRawParameterValue ("mode")->load());
    if (on != lastOn || host != lastHost || variation != lastVariation || mode != lastMode)
    {
        lastOn = on; lastHost = host; lastVariation = variation; lastMode = mode;
        repaint();
    }
}

void SL2AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    ui::paintBody (g, kPal, kDesignW, kDesignH);
    ui::paintHeaderNoPresets (g, kPal, "SL-2", "RHYTHMIC SLICER", "RHYTHMIC PATTERNS", powerOn());

    ui::paintPanel (g, kPal, panelRect (0), "OUTPUT");
    ui::paintPanel (g, kPal, panelRect (1), "SLICE");
    ui::paintPanel (g, kPal, panelRect (2), "MODE");
    ui::paintPanel (g, kPal, panelRect (3), "VARIATION");
    ui::paintPanel (g, kPal, panelRect (4), "TEMPO SOURCE");

    Knob* knobs[] = { &balance, &tempo, &attack, &duty };
    for (int k = 0; k < 4; ++k)
    {
        auto& s = knobs[k]->slider;
        ui::paintKnob (g, kPal, knobCentre (k), knobs[k]->name, juce::String (s.getValue(), 1), "0", "10", (float) s.valueToProportionOfLength (s.getValue()));
    }

    const int variation = (int) std::lround (proc.apvts.getRawParameterValue ("variation")->load());
    for (int i = 0; i < kNumVariations; ++i)
        ui::paintFlatButton (g, kPal, variationButton (i), juce::String (i + 1), variation == i + 1, 20.0f);

    const int mode = (int) std::lround (proc.apvts.getRawParameterValue ("mode")->load());
    for (int i = 0; i < kNumModes; ++i)
        ui::paintFlatButton (g, kPal, modeButton (i), kModeNames[i], mode == i);

    const bool host = proc.apvts.getRawParameterValue ("hostSync")->load() > 0.5f;
    for (const int i : { host ? 0 : 1, host ? 1 : 0 })                  // the lit one last: it keeps its whole border
        ui::paintFlatButton (g, kPal, sourceButton (i), i == 0 ? "KNOB" : "HOST", (i == 1) == host);

    ui::paintFooter (g, kPal, kDesignH - 15.0f);
}
