// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"

namespace ui = vibr::ui::vintage;

namespace
{
const ui::Palette kPal { juce::Colour (240, 120, 53) };            // OD-2 #F07835

// Design space: 1120 wide (margin 28, gap 18). TYPE across the top (y 114, 108 high), DRIVE and TONE below (y 240, 252 high), footer centre 15 above the bottom.
constexpr float kDesignW = 1120.0f, kDesignH = 522.0f, kGap = 18.0f;
constexpr float kTypeY = 114.0f, kTypeH = 108.0f, kRowY = 240.0f, kRowH = 252.0f;
constexpr float kKnobDy = 86.0f, kKnobPitch = 160.0f, kStripDy = 206.0f, kStripH = 30.0f, kLampPitch = 44.0f, kBtnW = 96.0f;
constexpr float kPanelW2 = (kDesignW - 2.0f * ui::kMargin - kGap) * 0.5f;
const char* const kSlotId[5] = { "param", "boostPost", "boostPre", "gateDecay", "gateThresh" };      // what the PARAM knob edits

juce::Rectangle<float> typePanel() { return { ui::kMargin, kTypeY, kDesignW - 2.0f * ui::kMargin, kTypeH }; }
juce::Rectangle<float> rowPanel (int i) { return { ui::kMargin + (float) i * (kPanelW2 + kGap), kRowY, kPanelW2, kRowH }; }     // 0 DRIVE, 1 TONE
juce::Rectangle<float> typeDisplay() { const auto r = typePanel(); return { r.getX() + 20.0f, r.getY() + 20.0f, 200.0f, 68.0f }; }
juce::Rectangle<float> typeButton (int i)                                                         // two rows of six, equal width
{
    const auto r = typePanel();
    const float x0 = typeDisplay().getRight() + 20.0f, w = (r.getRight() - 20.0f - x0 - 5.0f * 8.0f) / 6.0f;
    return { x0 + (float) (i % 6) * (w + 8.0f), r.getY() + 20.0f + (float) (i / 6) * 38.0f, w, 30.0f };
}
juce::Point<float> knobCentre (int i) { const auto r = rowPanel (i / 3); return { r.getCentreX() + kKnobPitch * (float) (i % 3 - 1), r.getY() + kKnobDy }; }
juce::Rectangle<float> leftButton (int panel) { const auto r = rowPanel (panel); return { r.getX() + 20.0f, r.getY() + kStripDy, kBtnW, kStripH }; }      // PARAM
juce::Rectangle<float> boostButton() { const auto r = rowPanel (0); return { r.getRight() - 20.0f - kBtnW, r.getY() + kStripDy, kBtnW, kStripH }; }
juce::Point<float> lampCentre (int panel, int i) { const auto r = rowPanel (panel); return { r.getCentreX() + kLampPitch * (float) (i - 2), r.getY() + kStripDy + kStripH * 0.5f }; }

juce::Rectangle<int> px (juce::Rectangle<float> r, float s) { return r.expanded (3.0f).transformedBy (juce::AffineTransform::scale (s)).toNearestInt(); }

// flat lamp: a 14 disc with a 2 ink ring, a soft glow when lit
void drawLamp (juce::Graphics& g, juce::Point<float> c, bool on, juce::Colour lit, juce::Colour dark)
{
    const auto d = juce::Rectangle<float> (14.0f, 14.0f).withCentre (c);
    if (on) { g.setColour (lit.withAlpha (0.28f)); g.fillEllipse (d.expanded (6.0f)); }
    g.setColour (on ? lit : dark);
    g.fillEllipse (d);
    g.setColour (kPal.ink);
    g.drawEllipse (d.expanded (1.0f), 2.0f);
}
} // namespace

//==============================================================================
OD2AudioProcessorEditor::OD2AudioProcessorEditor (OD2AudioProcessor& p)
    : PedalEditor (p, kDesignW, kDesignH), proc (p)
{
    for (auto* k : { &param, &level, &drive, &low, &middle, &high })
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
    param.slider.onValueChange = [this] { shownUntil = juce::Time::getMillisecondCounter() + 1200; repaint(); };   // show the value in the display for a moment

    paramZone.onClick = [this] { selectSlot ((slot + 1) % 5); };
    for (int i = 0; i < 5; ++i)      // clicking a lamp selects the parameter the PARAM knob edits
    {
        paramLedZone[i].onClick = [this, i] { selectSlot (i); };
        addAndMakeVisible (paramLedZone[i]);
    }
    typeTarget.onPress = [this] { setType ((lastType < 0 ? 0 : lastType) + 1); };
    addAndMakeVisible (typeTarget);
    midiUI.add (typeTarget, "type");
    boostTarget.onPress = [this] { toggleParam ("boostOn"); };
    addAndMakeVisible (boostTarget);
    midiUI.add (boostTarget, "boostOn");
    addAndMakeVisible (paramZone);

    resized();
    startTimerHz (30);
}

OD2AudioProcessorEditor::~OD2AudioProcessorEditor() = default;

void OD2AudioProcessorEditor::toggleParam (const char* id)
{
    auto* prm = proc.apvts.getParameter (id);
    prm->beginChangeGesture();
    prm->setValueNotifyingHost (prm->getValue() > 0.5f ? 0.0f : 1.0f);
    prm->endChangeGesture();
}

void OD2AudioProcessorEditor::setType (int t)
{
    t = (t % od2::kNumTypes + od2::kNumTypes) % od2::kNumTypes;
    auto* prm = proc.apvts.getParameter ("type");
    prm->beginChangeGesture();
    prm->setValueNotifyingHost (prm->convertTo0to1 ((float) t));
    prm->endChangeGesture();
    repaint();
}

void OD2AudioProcessorEditor::selectSlot (int s)
{
    slot = s;
    param.attachment.reset();                       // the PARAM knob now edits another parameter
    param.attachment = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter (kSlotId[slot]), param.slider, nullptr);
    repaint();
}

void OD2AudioProcessorEditor::resized()
{
    for (int i = 0; i < 6; ++i)
    {
        Knob* ks[] = { &param, &level, &drive, &low, &middle, &high };
        placeKnob (ks[i]->slider, knobCentre (i));
    }
    typeTarget.setBounds (typeDisplay().transformedBy (juce::AffineTransform::scale (scale())).toNearestInt());
    boostTarget.setBounds (px (boostButton(), scale()));
    paramZone.setBounds (px (leftButton (0), scale()));
    for (int i = 0; i < 5; ++i)
    {
        const auto z = [this] (juce::Point<float> c) { return juce::Rectangle<float> (50.0f, 50.0f).withCentre (c).transformedBy (juce::AffineTransform::scale (scale())).toNearestInt(); };
        paramLedZone[i].setBounds (z (lampCentre (0, i)));
    }
}

void OD2AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    if (ui::presetBarMouseDown (e.position / scale(), *this, proc.presets, proc.undoManager, scale(), [this] { repaint(); })) return;
    if (toggleBypassAt (e.position)) { repaint(); return; }          // the power switch is the bypass switch
    const auto p = e.position / scale();
    for (int i = 0; i < od2::kNumTypes; ++i)
        if (typeButton (i).expanded (3.0f).contains (p)) { setType (i); return; }
}

void OD2AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    midiUI.paintMarks (g, kPal.accent);
}

void OD2AudioProcessorEditor::timerCallback()
{
    if (midiUI.needsRepaint()) { midiUI.tick(); repaint(); }
    const bool on = powerOn();
    const bool boost = proc.apvts.getRawParameterValue ("boostOn")->load() > 0.5f;
    const int t = (int) std::lround (proc.apvts.getRawParameterValue ("type")->load());
    const bool showing = juce::Time::getMillisecondCounter() < shownUntil;
    if (on != lastOn || boost != lastBoost || t != lastType || showing || showing != valueShown)
    {
        lastOn = on; lastBoost = boost; lastType = t; valueShown = showing;
        repaint();
    }
}

void OD2AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    ui::paintBody (g, kPal, kDesignW, kDesignH);
    ui::paintHeader (g, kPal, "OD-2", "HYBRID DRIVE", "VINTAGE OVERDRIVE");
    ui::paintPresetBar (g, kPal, proc.presets, proc.undoManager, powerOn());

    // TYPE: the phosphor display (the drive type, or the PARAM value while the PARAM knob is turned) and the 12 buttons
    ui::paintPanel (g, kPal, typePanel(), "TYPE");
    const int t = juce::jlimit (0, od2::kNumTypes - 1, (int) std::lround (proc.apvts.getRawParameterValue ("type")->load()));
    {
        const bool showValue = juce::Time::getMillisecondCounter() < shownUntil;
        const juce::String text = showValue ? juce::String (param.slider.getValue(), 1) : juce::String (od2::typeDef (t).name);
        const auto d = typeDisplay();
        ui::paintScreen (g, kPal, d);
        float size = 25.0f;
        if (const float w = ui::textWidth (ui::font (size, true, ui::Face::Mono).withExtraKerningFactor (0.04f), text), room = d.getWidth() - 24.0f; w > room) size = juce::jmax (13.0f, size * room / w);
        ui::drawText (g, text, d.getCentreX(), d.getCentreY(), size, kPal.phosphor, true, 0.04f, juce::Justification::centred, ui::Face::Mono);
    }
    for (int i = 0; i < od2::kNumTypes; ++i)
        ui::paintFlatButton (g, kPal, typeButton (i), od2::typeDef (i).name, i == t, 17.0f);

    // DRIVE and TONE
    ui::paintPanel (g, kPal, rowPanel (0), "DRIVE");
    ui::paintPanel (g, kPal, rowPanel (1), "TONE");
    Knob* ks[] = { &param, &level, &drive, &low, &middle, &high };
    for (int i = 0; i < 6; ++i)
    {
        auto& s = ks[i]->slider;
        ui::paintKnob (g, kPal, knobCentre (i), ks[i]->name, juce::String (s.getValue(), 1), "0", "10", (float) s.valueToProportionOfLength (s.getValue()));
    }

    // lower strip: PARAM + 5 red lamps + BOOST
    ui::paintFlatButton (g, kPal, leftButton (0), "PARAM", paramZone.isDown(), 17.0f);
    ui::paintFlatButton (g, kPal, boostButton(), "BOOST", proc.apvts.getRawParameterValue ("boostOn")->load() > 0.5f, 17.0f);
    for (int i = 0; i < 5; ++i)
        drawLamp (g, lampCentre (0, i), slot == i, juce::Colour (0xffFF4A2A), juce::Colour (0xff5A2A20));

    ui::paintFooter (g, kPal, kDesignH - 15.0f);
}
