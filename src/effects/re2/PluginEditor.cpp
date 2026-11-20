// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"

namespace ui = vibr::ui::vintage;

namespace
{
const ui::Palette kPal { juce::Colour (49, 85, 181) };            // RE-2 #3155B5

// Design space 1120 x 620 (margin 28, gap 18). Row 1 (y 112, 270 high): MODE (432 wide) and ECHO (614). Row 2 (y 400, 190 high): TAPE (under MODE) and MIXER (under ECHO).
// The content area ends at y 590; the footer centre is 15 above the bottom.
constexpr float kDesignW = 1120.0f, kDesignH = 620.0f, kGap = 18.0f;
constexpr float kRow1Y = 112.0f, kRow1H = 270.0f, kRow2Y = kRow1Y + kRow1H + kGap, kRow2H = 190.0f;
constexpr float kLeftW = 432.0f, kRightW = 1120.0f - 2.0f * ui::kMargin - kGap - kLeftW;                      // 614
constexpr float kBigR = 32.0f, kSmallR = 24.0f;                                                             // ECHO / MODE knobs, TAPE / MIXER knobs (body radius)
constexpr float kBtnH = 32.0f;
const juce::Colour kRed (0xffFF4A2A), kRedOff (0xff5A2A20);

juce::Rectangle<float> modeP()  { return { ui::kMargin, kRow1Y, kLeftW, kRow1H }; }
juce::Rectangle<float> echoP()  { return { modeP().getRight() + kGap, kRow1Y, kRightW, kRow1H }; }
juce::Rectangle<float> tapeP()  { return { ui::kMargin, kRow2Y, kLeftW, kRow2H }; }
juce::Rectangle<float> mixerP() { return { echoP().getX(), kRow2Y, kRightW, kRow2H }; }

// knob order: 0 saturation, 1 wow, 2 bass, 3 treble, 4 reverb, 5 rate, 6 intensity, 7 echo, 8 mode
juce::Point<float> knobCentre (int i)
{
    if (i == 8) return { modeP().getX() + 108.0f, modeP().getCentreY() };
    if (i < 2)  return { tapeP().getX() + (i == 0 ? 55.0f : 264.0f), tapeP().getY() + 68.0f };
    if (i < 5)  return { mixerP().getX() + 88.0f + 184.0f * (float) (i - 2), mixerP().getY() + 68.0f };
    return { echoP().getX() + 120.0f + 187.0f * (float) (i - 5), echoP().getY() + 78.0f };
}
float knobRadius (int i) { return i == 8 || i >= 5 ? kBigR : kSmallR; }

// ECHO: divider and the SYNC row (button + hint)
float echoDividerY() { return echoP().getY() + 210.0f; }
juce::Rectangle<float> syncRect() { return juce::Rectangle<float> (92.0f, kBtnH).withCentre ({ echoP().getX() + 25.0f + 46.0f, echoP().getY() + 238.0f }); }

// MODE panel: dial on the left, a phosphor readout and the four head lamps on the right
float modeRightCx() { return modeP().getX() + 313.0f; }
juce::Rectangle<float> modeScreen() { return juce::Rectangle<float> (198.0f, 48.0f).withCentre ({ modeRightCx(), knobCentre (8).y - 28.0f }); }
juce::Point<float> headLamp (int i) { return { modeRightCx() + 52.0f * ((float) i - 1.5f), knobCentre (8).y + 23.0f }; }

// two-button rows: TAPE TYPE (NEW / AGED) and INPUT (GUITAR / LINE)
juce::Rectangle<float> rowButton (juce::Rectangle<float> panel, int i, float x0, float w)
{
    return { panel.getX() + x0 + (w + 8.0f) * (float) i, panel.getY() + 146.0f - kBtnH * 0.5f, w, kBtnH };
}
juce::Rectangle<float> tapeBtn (int i)  { return rowButton (tapeP(), i, 106.0f, 80.0f); }
juce::Rectangle<float> inputBtn (int i) { return rowButton (mixerP(), i, 76.0f, 96.0f); }

juce::Rectangle<int> px (juce::Rectangle<float> r, float s) { return r.expanded (3.0f).transformedBy (juce::AffineTransform::scale (s)).toNearestInt(); }

// flat lamp: a 14 disc with a 2 ink ring, a soft glow when lit; `caption` under it
void drawLamp (juce::Graphics& g, juce::Point<float> c, bool on, const juce::String& caption)
{
    const auto d = juce::Rectangle<float> (14.0f, 14.0f).withCentre (c);
    if (on) { g.setColour (kRed.withAlpha (0.28f)); g.fillEllipse (d.expanded (6.0f)); }
    g.setColour (on ? kRed : kRedOff);
    g.fillEllipse (d);
    g.setColour (kPal.ink);
    g.drawEllipse (d.expanded (1.0f), 2.0f);
    if (caption.isNotEmpty()) ui::drawText (g, caption, c.x, c.y + 24.0f, 14.0f, kPal.ink, true, 0.08f);
}

// a button of a two-button row: lamp at the left, label right of it
void drawRowButton (juce::Graphics& g, juce::Rectangle<float> b, const juce::String& label, bool on, bool down)
{
    ui::paintFlatButton (g, kPal, b, "", down);
    drawLamp (g, { b.getX() + 20.0f, b.getCentreY() }, on, {});
    ui::drawText (g, label, (b.getX() + 32.0f + b.getRight() - 8.0f) * 0.5f, b.getCentreY(), 16.0f, kPal.ink, true, 0.1f);
}

// ring, scale marks, name and value of a big knob (ECHO): the texts sit under it
void drawBigKnob (juce::Graphics& g, juce::Point<float> c, const juce::String& name, const juce::String& value, const juce::String& lo, const juce::String& hi, float prop)
{
    ui::paintValueArc (g, c, kBigR, prop, kPal.accent, kPal.track);
    ui::drawText (g, lo, c.x - 52.0f, c.y + 62.0f, 13.0f, kPal.inkSoft, false, 0.0f, juce::Justification::centredLeft, ui::Face::Mono);
    ui::drawText (g, hi, c.x + 52.0f, c.y + 62.0f, 13.0f, kPal.inkSoft, false, 0.0f, juce::Justification::centredRight, ui::Face::Mono);
    ui::drawText (g, name, c.x, c.y + 86.0f, 22.0f, kPal.ink, true, 0.12f);
    ui::drawText (g, value, c.x, c.y + 110.0f, 17.0f, kPal.accent, true, 0.0f, juce::Justification::centred, ui::Face::Mono);
}

// ring and the name, value and "0 - 10" to the right of a small knob (TAPE, MIXER)
void drawSideKnob (juce::Graphics& g, juce::Point<float> c, const juce::String& name, const juce::String& value, float prop)
{
    ui::paintValueArc (g, c, kSmallR, prop, kPal.accent, kPal.track);
    const float x = c.x + 45.0f;
    ui::drawText (g, name, x, c.y - 18.0f, 18.0f, kPal.ink, true, 0.1f, juce::Justification::centredLeft);
    ui::drawText (g, value, x, c.y + 4.0f, 17.0f, kPal.accent, true, 0.0f, juce::Justification::centredLeft, ui::Face::Mono);
    ui::drawText (g, "0 - 10", x, c.y + 23.0f, 13.0f, kPal.inkSoft, false, 0.0f, juce::Justification::centredLeft, ui::Face::Mono);
}

juce::String headText (int mode)
{
    const auto& m = re2::kModes[juce::jlimit (1, 12, mode) - 1];
    juce::StringArray heads;
    for (int i = 0; i < 3; ++i) if (m.headMask & (1 << i)) heads.add ("H" + juce::String (i + 1));
    if (heads.isEmpty()) return "REVERB";
    return heads.joinIntoString ("+") + (m.reverb ? " REV" : "");
}
} // namespace

//==============================================================================
RE2AudioProcessorEditor::RE2AudioProcessorEditor (RE2AudioProcessor& p)
    : PedalEditor (p, kDesignW, kDesignH), proc (p),
      syncAtt (*p.apvts.getParameter ("sync"), [this] (float v) { syncOn = v > 0.5f; syncChanged(); }, nullptr),
      divisionAtt (*p.apvts.getParameter ("division"), [this] (float v) { division = (int) std::lround (v); repaint(); }, nullptr)
{
    for (auto* k : { &saturation, &wowFlutter, &mode, &bass, &treble, &reverbVol, &repeatRate, &intensity, &echoVol })
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

    // the 1-12 dial: pointer on "1" at the lower left, sweeping clockwise round to "12" at the bottom
    mode.slider.setRotaryParameters (juce::degreesToRadians (-150.0f), juce::degreesToRadians (180.0f), true);
    mode.slider.setRange (1.0, 12.0, 1.0);

    for (int i = 0; i < 2; ++i)      // two-button rows: clicking NEW / AGED or GUITAR / LINE sets that value
    {
        const auto set = [this] (const char* id, bool on) { auto* prm = proc.apvts.getParameter (id); proc.undoManager.beginNewTransaction(); prm->beginChangeGesture(); prm->setValueNotifyingHost (on ? 1.0f : 0.0f); prm->endChangeGesture(); repaint(); };
        inputLedZone[i].onClick = [set, i] { set ("inputLine", i == 1); };      // GUITAR = off, LINE = on
        tapeLedZone[i].onClick  = [set, i] { set ("tapeAged", i == 1); };       // NEW = off, AGED = on
        addAndMakeVisible (inputLedZone[i]);
        addAndMakeVisible (tapeLedZone[i]);
    }

    syncAtt.sendInitialUpdate();
    divisionAtt.sendInitialUpdate();
    resized();
    timerCallback();
    startTimerHz (20);
}

RE2AudioProcessorEditor::~RE2AudioProcessorEditor() = default;

void RE2AudioProcessorEditor::syncChanged()
{
    // RATE is the echo rate, or the note division while sync is on (the attachment sets the slider range from the parameter)
    auto& k = repeatRate;
    k.attachment.reset();
    divisionKnobAtt.reset();
    if (syncOn) divisionKnobAtt = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter ("division"), k.slider, nullptr);
    else        k.attachment    = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter ("repeatRate"), k.slider, nullptr);
    repaint();
}

void RE2AudioProcessorEditor::resized()
{
    Knob* ks[] = { &saturation, &wowFlutter, &bass, &treble, &reverbVol, &repeatRate, &intensity, &echoVol, &mode };
    for (int i = 0; i < 9; ++i) placeKnob (ks[i]->slider, knobCentre (i), knobRadius (i) * 35.0f / 30.0f - 5.0f);

    const float s = scale();
    for (int i = 0; i < 2; ++i)
    {
        inputLedZone[i].setBounds (px (inputBtn (i), s));
        tapeLedZone[i].setBounds  (px (tapeBtn (i), s));
    }
}

void RE2AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    proc.undoManager.beginNewTransaction();
    if (ui::presetBarMouseDown (e.position / scale(), *this, proc.presets, proc.undoManager, scale(), [this] { repaint(); })) { repaint(); return; }
    if (toggleBypassAt (e.position)) { repaint(); return; }          // the power switch is the bypass switch
    if (syncRect().expanded (3.0f).contains (e.position / scale()))
        syncAtt.setValueAsCompleteGesture (syncOn ? 0.0f : 1.0f);
}

void RE2AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    midiUI.paintMarks (g, kPal.accent);
}

void RE2AudioProcessorEditor::timerCallback()
{
    if (midiUI.needsRepaint()) { midiUI.tick(); repaint(); }
    const int m = juce::jlimit (1, 12, (int) std::lround (proc.apvts.getRawParameterValue ("mode")->load()));
    lastHeadMask = re2::kModes[m - 1].headMask;
    const int state = lastHeadMask | (paramOn ("inputLine") ? 16 : 0) | (paramOn ("tapeAged") ? 32 : 0) | (powerOn() ? 64 : 0) | (m << 12);
    if (state != shownState)
    {
        shownState = state;
        repaint();
    }
}

void RE2AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    ui::paintBody (g, kPal, kDesignW, kDesignH);
    ui::paintHeader (g, kPal, "RE-2", "TAPE ECHO", "MAGNETIC WARMTH");
    ui::paintPresetBar (g, kPal, proc.presets, proc.undoManager, powerOn());

    ui::paintPanel (g, kPal, modeP(), "MODE");
    ui::paintPanel (g, kPal, echoP(), "ECHO");
    ui::paintPanel (g, kPal, tapeP(), "TAPE");
    ui::paintPanel (g, kPal, mixerP(), "MIXER");

    // knobs (RATE shows the note division while sync is on)
    Knob* ks[] = { &saturation, &wowFlutter, &bass, &treble, &reverbVol, &repeatRate, &intensity, &echoVol };
    for (int i = 0; i < 8; ++i)
    {
        auto& s = ks[i]->slider;
        const float prop = (float) s.valueToProportionOfLength (s.getValue());
        if (i < 5)
            drawSideKnob (g, knobCentre (i), ks[i]->name, juce::String (s.getValue(), 1), prop);
        else if (ks[i] == &repeatRate)
            drawBigKnob (g, knobCentre (i), ks[i]->name, syncOn ? vibr::sync::divisionNames()[juce::jlimit (0, vibr::sync::kNumDivisions - 1, division)] : juce::String (s.getValue(), 1),
                         syncOn ? "" : "slow", syncOn ? "" : "fast", prop);
        else
            drawBigKnob (g, knobCentre (i), ks[i]->name, juce::String (s.getValue(), 1), "0", "10", prop);
    }

    // ECHO: divider, SYNC button and its hint
    {
        const auto e = echoP();
        g.setColour (kPal.ink.withAlpha (0.35f));
        g.drawLine (e.getX() + 25.0f, echoDividerY(), e.getRight() - 25.0f, echoDividerY(), 2.0f);
        const auto b = syncRect();
        ui::paintFlatButton (g, kPal, b, "SYNC", syncOn);
        ui::drawText (g, syncOn ? "RATE FOLLOWS TEMPO" : "RATE RUNS FREE", b.getRight() + 14.0f, b.getCentreY(), 13.0f, kPal.inkSoft, false, 0.08f,
                      juce::Justification::centredLeft, ui::Face::Mono);
    }

    // TAPE TYPE and INPUT rows
    const bool line = paramOn ("inputLine"), aged = paramOn ("tapeAged");
    ui::drawText (g, "TAPE TYPE", tapeP().getX() + 24.0f, tapeBtn (0).getCentreY(), 18.0f, kPal.ink, true, 0.12f, juce::Justification::centredLeft);
    drawRowButton (g, tapeBtn (0), "NEW", ! aged, tapeLedZone[0].isDown());
    drawRowButton (g, tapeBtn (1), "AGED", aged, tapeLedZone[1].isDown());
    ui::drawText (g, "INPUT", mixerP().getX() + 24.0f, inputBtn (0).getCentreY(), 18.0f, kPal.ink, true, 0.12f, juce::Justification::centredLeft);
    drawRowButton (g, inputBtn (0), "GUITAR", ! line, inputLedZone[0].isDown());
    drawRowButton (g, inputBtn (1), "LINE", line, inputLedZone[1].isDown());

    // MODE: the 12-position dial with its legends, the readout and the head lamps (heads 1-3 from the mode; this engine has three heads)
    {
        const int cur = juce::jlimit (1, 12, (int) std::lround (mode.slider.getValue()));
        const auto c = knobCentre (8);
        for (int i = 0; i < 12; ++i)
        {
            const float a = juce::degreesToRadians (-150.0f + 30.0f * (float) i), sn = std::sin (a), co = -std::cos (a);
            const bool sel = i + 1 == cur;
            g.setColour (sel ? kPal.accent : kPal.track);
            g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ c.x + sn * 52.0f, c.y + co * 52.0f }));
            ui::drawText (g, juce::String (i + 1), c.x + sn * 74.0f, c.y + co * 74.0f, 18.0f, sel ? kPal.accent : kPal.inkSoft, sel, 0.0f,
                          juce::Justification::centred, ui::Face::Mono);
        }
        const auto sc = modeScreen();
        ui::paintScreen (g, kPal, sc);
        ui::drawText (g, "MODE " + juce::String (cur), sc.getX() + 14.0f, sc.getCentreY(), 13.0f, kPal.phosphor.withAlpha (0.7f), false, 0.12f, juce::Justification::centredLeft, ui::Face::Mono);
        ui::drawText (g, headText (cur), sc.getRight() - 14.0f, sc.getCentreY(), 20.0f, kPal.phosphor, true, 0.04f, juce::Justification::centredRight, ui::Face::Mono);
        for (int i = 0; i < 4; ++i)
            drawLamp (g, headLamp (i), i < 3 && (lastHeadMask & (1 << i)) != 0, "HEAD " + juce::String (i + 1));
    }

    ui::paintFooter (g, kPal, kDesignH - 15.0f);
}
