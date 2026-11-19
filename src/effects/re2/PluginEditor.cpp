// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"

namespace ui = vibr::ui::vintage;

namespace
{
const ui::Palette kPal { juce::Colour (49, 85, 181) };            // RE-2 #3155B5

// Design space 1120 x 660 (margin 28, gap 18). Row 1 (y 114, 276 high): TAPE, MIXER, ECHO. Row 2 (y 408, 222 high): MODE (under TAPE and MIXER) and MEMORY (under ECHO).
// The content area ends at y 630; the footer centre is 15 above the bottom.
constexpr float kDesignW = 1120.0f, kDesignH = 660.0f, kGap = 18.0f;
constexpr float kRow1Y = 114.0f, kRow1H = 276.0f, kRow2Y = kRow1Y + kRow1H + kGap, kRow2H = 222.0f;
constexpr float kPanelW3 = 344.0f, kTapeW = kDesignW - 2.0f * ui::kMargin - 2.0f * kGap - 2.0f * kPanelW3;      // 340
constexpr float kKnobDy = 82.0f, kDividerDy = 198.0f, kStripDy = 232.0f, kBtnH = 30.0f, kLampPitch = 74.0f, kBtnW = 110.0f, kKnobPitch = 108.0f;
constexpr float kModeLegendR = 66.0f;
constexpr juce::uint32 kHoldMs = 800;
const juce::Colour kRed (0xffFF4A2A), kRedOff (0xff5A2A20), kGreen (0xff7FD86A), kGreenOff (0xff253020);

juce::Rectangle<float> tapeP()   { return { ui::kMargin, kRow1Y, kTapeW, kRow1H }; }
juce::Rectangle<float> mixerP()  { return { tapeP().getRight() + kGap, kRow1Y, kPanelW3, kRow1H }; }
juce::Rectangle<float> echoP()   { return { mixerP().getRight() + kGap, kRow1Y, kPanelW3, kRow1H }; }
juce::Rectangle<float> modeP()   { return { ui::kMargin, kRow2Y, mixerP().getRight() - ui::kMargin, kRow2H }; }
juce::Rectangle<float> memoryP() { return { echoP().getX(), kRow2Y, kPanelW3, kRow2H }; }

// knob order: 0 saturation, 1 wow, 2 bass, 3 treble, 4 reverb, 5 rate, 6 intensity, 7 echo, 8 mode
juce::Point<float> knobCentre (int i)
{
    if (i == 8) return { modeP().getX() + 186.0f, modeP().getCentreY() };
    if (i < 2)  return { tapeP().getCentreX() + (i == 0 ? -75.0f : 75.0f), tapeP().getY() + kKnobDy };
    const auto p = i < 5 ? mixerP() : echoP();
    return { p.getCentreX() + kKnobPitch * (float) ((i - 2) % 3 - 1), p.getY() + kKnobDy };
}

// lamps (two) and the push button of a strip in row 1
juce::Point<float> stripLamp (juce::Rectangle<float> p, int i) { return { p.getX() + 66.0f + kLampPitch * (float) i, p.getY() + kStripDy }; }
juce::Rectangle<float> stripButton (juce::Rectangle<float> p) { return { p.getX() + 188.0f, p.getY() + kStripDy - kBtnH * 0.5f, kBtnW, kBtnH }; }
juce::Rectangle<float> syncRect() { return juce::Rectangle<float> (88.0f, kBtnH).withCentre ({ knobCentre (5).x, echoP().getY() + kStripDy }); }

// MODE panel: dial on the left, a phosphor readout and the four head lamps on the right
float modeRightCx() { return modeP().getX() + 470.0f; }
juce::Rectangle<float> modeScreen() { return juce::Rectangle<float> (240.0f, 44.0f).withCentre ({ modeRightCx(), modeP().getY() + 80.0f }); }
juce::Point<float> headLamp (int i) { return { modeRightCx() + 66.0f * ((float) i - 1.5f), modeP().getY() + 133.0f }; }

// MEMORY panel: lamps row, divider, button row
juce::Point<float> memLamp (int i) { return { memoryP().getX() + 50.0f + 66.0f * (float) i, memoryP().getY() + 45.0f }; }
float memDividerY() { return memoryP().getY() + 115.0f; }
juce::Rectangle<float> memButton() { return { memoryP().getX() + 64.0f, memoryP().getY() + 169.0f - kBtnH * 0.5f, kBtnW, kBtnH }; }

juce::Rectangle<int> px (juce::Rectangle<float> r, float s) { return r.expanded (3.0f).transformedBy (juce::AffineTransform::scale (s)).toNearestInt(); }
juce::Rectangle<int> lampZone (juce::Point<float> c, float s) { return juce::Rectangle<float> (50.0f, 50.0f).withCentre (c).transformedBy (juce::AffineTransform::scale (s)).toNearestInt(); }

// flat lamp: a 14 disc with a 2 ink ring, a soft glow when lit; `caption` under it
void drawLamp (juce::Graphics& g, juce::Point<float> c, bool on, juce::Colour lit, juce::Colour dark, const juce::String& caption)
{
    const auto d = juce::Rectangle<float> (14.0f, 14.0f).withCentre (c);
    if (on) { g.setColour (lit.withAlpha (0.28f)); g.fillEllipse (d.expanded (6.0f)); }
    g.setColour (on ? lit : dark);
    g.fillEllipse (d);
    g.setColour (kPal.ink);
    g.drawEllipse (d.expanded (1.0f), 2.0f);
    ui::drawText (g, caption, c.x, c.y + 24.0f, 14.0f, kPal.ink, true, 0.08f);
}

void drawDivider (juce::Graphics& g, juce::Rectangle<float> panel, float y)
{
    g.setColour (kPal.ink.withAlpha (0.35f));
    g.drawLine (panel.getX() + 16.0f, y, panel.getRight() - 16.0f, y, 1.5f);
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

    memoryZone.onDown  = [this] { memHeld = true; memWritten = false; memDownAt = juce::Time::getMillisecondCounter(); };
    memoryZone.onUp    = [this] { memHeld = false; };
    memoryZone.onClick = [this] { if (! memWritten) memory.stepNext(); repaint(); };      // short press
    addAndMakeVisible (memoryZone);
    inputZone.onClick = [this] { toggleParam ("inputLine"); };
    tapeZone.onClick  = [this] { toggleParam ("tapeAged"); };
    addAndMakeVisible (inputZone);
    addAndMakeVisible (tapeZone);
    for (int i = 0; i < 5; ++i)      // clicking a lamp selects it: a memory slot (MANUAL, 1-4), GUITAR / LINE, NEW / AGED
    {
        memLedZone[i].onClick = [this, i] { memory.select (i); repaint(); };
        addAndMakeVisible (memLedZone[i]);
    }
    for (int i = 0; i < 2; ++i)
    {
        const auto set = [this] (const char* id, bool on) { auto* prm = proc.apvts.getParameter (id); prm->beginChangeGesture(); prm->setValueNotifyingHost (on ? 1.0f : 0.0f); prm->endChangeGesture(); repaint(); };
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

void RE2AudioProcessorEditor::toggleParam (const char* id)
{
    auto* prm = proc.apvts.getParameter (id);
    prm->beginChangeGesture();
    prm->setValueNotifyingHost (prm->getValue() > 0.5f ? 0.0f : 1.0f);
    prm->endChangeGesture();
    repaint();
}

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
    for (int i = 0; i < 9; ++i) placeKnob (ks[i]->slider, knobCentre (i));

    const float s = scale();
    inputZone.setBounds (px (stripButton (mixerP()), s));
    tapeZone.setBounds  (px (stripButton (tapeP()), s));
    memoryZone.setBounds (px (memButton(), s));
    for (int i = 0; i < 5; ++i) memLedZone[i].setBounds (lampZone (memLamp (i), s));
    for (int i = 0; i < 2; ++i)
    {
        inputLedZone[i].setBounds (lampZone (stripLamp (mixerP(), i), s));
        tapeLedZone[i].setBounds  (lampZone (stripLamp (tapeP(), i), s));
    }
}

void RE2AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
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
    // memory: holding the button writes the slot; moving a knob after a recall goes back to MANUAL
    const auto now = juce::Time::getMillisecondCounter();
    if (memHeld && ! memWritten && now - memDownAt >= kHoldMs)
    {
        memory.write();
        memWritten = true;
        memFlashUntil = now + 500;
    }
    memory.revertIfEdited();

    const int m = juce::jlimit (1, 12, (int) std::lround (proc.apvts.getRawParameterValue ("mode")->load()));
    lastHeadMask = re2::kModes[m - 1].headMask;
    const bool flashing = now < memFlashUntil;
    const int state = lastHeadMask | (paramOn ("inputLine") ? 16 : 0) | (paramOn ("tapeAged") ? 32 : 0) | (powerOn() ? 64 : 0) | (memory.current() << 8) | (m << 12);
    if (state != shownState || flashing || flashing != memFlashShown)
    {
        shownState = state;
        memFlashShown = flashing;
        repaint();
    }
}

void RE2AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    ui::paintBody (g, kPal, kDesignW, kDesignH);
    ui::paintHeaderNoPresets (g, kPal, "RE-2", "TAPE ECHO", "MAGNETIC WARMTH", powerOn());

    ui::paintPanel (g, kPal, tapeP(), "TAPE");
    ui::paintPanel (g, kPal, mixerP(), "MIXER");
    ui::paintPanel (g, kPal, echoP(), "ECHO");
    ui::paintPanel (g, kPal, modeP(), "MODE");
    ui::paintPanel (g, kPal, memoryP(), "MEMORY");

    // knobs (RATE shows the note division while sync is on)
    Knob* ks[] = { &saturation, &wowFlutter, &bass, &treble, &reverbVol, &repeatRate, &intensity, &echoVol };
    for (int i = 0; i < 8; ++i)
    {
        auto& s = ks[i]->slider;
        const float prop = (float) s.valueToProportionOfLength (s.getValue());
        if (ks[i] == &repeatRate)
            ui::paintKnob (g, kPal, knobCentre (i), ks[i]->name, syncOn ? vibr::sync::divisionNames()[juce::jlimit (0, vibr::sync::kNumDivisions - 1, division)] : juce::String (s.getValue(), 1),
                           syncOn ? "" : "SLOW", syncOn ? "" : "FAST", prop);
        else
            ui::paintKnob (g, kPal, knobCentre (i), ks[i]->name, juce::String (s.getValue(), 1), "0", "10", prop);
    }

    // row 1 strips under a divider: TAPE (NEW / AGED lamps + button), INPUT (GUITAR / LINE lamps + button), SYNC under RATE
    for (const auto& p : { tapeP(), mixerP(), echoP() }) drawDivider (g, p, p.getY() + kDividerDy);
    const bool line = paramOn ("inputLine"), aged = paramOn ("tapeAged");
    drawLamp (g, stripLamp (tapeP(), 0), ! aged, kRed, kRedOff, "NEW");
    drawLamp (g, stripLamp (tapeP(), 1),   aged, kRed, kRedOff, "AGED");
    ui::paintFlatButton (g, kPal, stripButton (tapeP()), "TAPE", tapeZone.isDown());
    drawLamp (g, stripLamp (mixerP(), 0), ! line, kRed, kRedOff, "GUITAR");
    drawLamp (g, stripLamp (mixerP(), 1),   line, kRed, kRedOff, "LINE");
    ui::paintFlatButton (g, kPal, stripButton (mixerP()), "INPUT", inputZone.isDown());
    ui::paintFlatButton (g, kPal, syncRect(), "SYNC", syncOn);

    // MODE: the 12-position dial with its legends, the readout and the head lamps (heads 1-3 from the mode; this engine has three heads)
    {
        const int cur = juce::jlimit (1, 12, (int) std::lround (mode.slider.getValue()));
        const auto c = knobCentre (8);
        for (int i = 0; i < 12; ++i)
        {
            const float a = juce::degreesToRadians (-150.0f + 30.0f * (float) i), sn = std::sin (a), co = -std::cos (a);
            const bool sel = i + 1 == cur;
            g.setColour (sel ? kPal.accent : kPal.track);
            g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ c.x + sn * 50.0f, c.y + co * 50.0f }));
            ui::drawText (g, juce::String (i + 1), c.x + sn * kModeLegendR, c.y + co * kModeLegendR, 17.0f, sel ? kPal.accent : kPal.inkSoft, sel, 0.0f,
                          juce::Justification::centred, ui::Face::Mono);
        }
        const auto sc = modeScreen();
        ui::paintScreen (g, kPal, sc);
        ui::drawText (g, headText (cur), sc.getCentreX(), sc.getCentreY(), 22.0f, kPal.phosphor, true, 0.04f, juce::Justification::centred, ui::Face::Mono);
        for (int i = 0; i < 4; ++i)
            drawLamp (g, headLamp (i), i < 3 && (lastHeadMask & (1 << i)) != 0, kRed, kRedOff, "HEAD " + juce::String (i + 1));
    }

    // MEMORY: MANUAL, 1-4 lamps (all five flash green after a write), divider, MEMORY button with its hint
    {
        const auto now = juce::Time::getMillisecondCounter();
        const bool flashing = now < memFlashUntil, blinkOff = flashing && ((now / 100) % 2) == 1;
        for (int i = 0; i < 5; ++i)
            drawLamp (g, memLamp (i), flashing ? ! blinkOff : memory.current() == i, kGreen, kGreenOff, i == 0 ? juce::String ("MANUAL") : juce::String (i));
        drawDivider (g, memoryP(), memDividerY());
        const auto b = memButton();
        ui::paintFlatButton (g, kPal, b, "MEMORY", memoryZone.isDown());
        ui::drawText (g, "HOLD = WRITE", b.getRight() + 14.0f, b.getCentreY(), 14.0f, kPal.inkSoft, true, 0.08f, juce::Justification::centredLeft);
    }

    ui::paintFooter (g, kPal, kDesignH - 15.0f);
}
