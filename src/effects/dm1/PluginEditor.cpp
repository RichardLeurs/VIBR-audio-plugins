// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"

namespace ui = vibr::ui::vintage;

namespace
{
const ui::Palette kPal { juce::Colour (37, 136, 201) };            // DM-1 #2588C9

// Design space 1120 x 502: header (0 to 100), row 1 (y 114, 244 high: MODULATION, MODE, DELAY), row 2 (y 376, 96 high: MEMORY, TAP, TAP DIVISION), footer.
constexpr float kDesignW = ui::kW, kDesignH = 502.0f, kGap = 18.0f;
constexpr float kRow1Y = 114.0f, kRow1H = 244.0f, kRow2Y = 376.0f, kRow2H = 96.0f;
constexpr float kKnobDy = 86.0f, kKnobPitch = 100.0f, kSyncDy = 215.0f, kBtnDy = 30.0f, kBtnH = 30.0f, kLampDy = 45.0f, kLabelDy = 68.0f, kLampPitch = 44.0f;
constexpr juce::uint32 kHoldMs = 800;

const juce::Rectangle<float> kPanels[6] {
    { 28.0f, kRow1Y, 330.0f, kRow1H },  { 376.0f, kRow1Y, 368.0f, kRow1H },  { 762.0f, kRow1Y, 330.0f, kRow1H },       // MODULATION, MODE, DELAY
    { 28.0f, kRow2Y, 330.0f, kRow2H },  { 376.0f, kRow2Y, 368.0f, kRow2H },  { 762.0f, kRow2Y, 330.0f, kRow2H } };     // MEMORY, TAP, TAP DIVISION
const char* const kTitles[6] = { "MODULATION", "MODE", "DELAY", "MEMORY", "TAP TEMPO", "TAP DIVISION" };
const char* const kModeNames[12] = { "CLASSIC", "VINTAGE", "MODERN", "MULTI-HEAD", "NON-LINEAR", "AMBIENCE", "REFLECT", "DOUBLING+DELAY", "WIDE", "DUAL MOD", "PAN", "PATTERN" };
const char* const kMemLabels[5] = { "MAN", "1", "2", "3", "4" };
const char* const kDivLabels[5] = { "1/2", "1/4", "1/8", "TRI", "DOT" };

juce::Point<float> knobCentre (int i)          // 0..2 MODULATION (RATE, DEPTH, VARIATION), 3..5 DELAY (TIME, INTENSITY, VOLUME), 6 MODE
{
    const auto& r = kPanels[i == 6 ? 1 : (i < 3 ? 0 : 2)];
    return { r.getCentreX() + (i == 6 ? 0.0f : kKnobPitch * (float) (i % 3 - 1)), r.getY() + kKnobDy };
}
juce::Rectangle<float> syncRect() { return juce::Rectangle<float> (110.0f, kBtnH).withCentre ({ knobCentre (3).x, kRow1Y + kSyncDy }); }
juce::Rectangle<float> modeScreen() { const auto& r = kPanels[1]; return juce::Rectangle<float> (220.0f, 30.0f).withCentre ({ r.getCentreX(), r.getY() + kSyncDy }); }

// row 2: a button at the left, five lamps (44 apart) at the right
juce::Rectangle<float> leftButton (int panel) { const auto& r = kPanels[panel]; return { r.getX() + 17.0f, r.getY() + kBtnDy, 90.0f, kBtnH }; }
juce::Point<float> lampCentre (int panel, int i) { const auto& r = kPanels[panel]; return { r.getRight() - 24.0f - kLampPitch * (float) (4 - i), r.getY() + kLampDy }; }
juce::Rectangle<float> tapRect() { const auto& r = kPanels[4]; return juce::Rectangle<float> (140.0f, kBtnH).withCentre ({ r.getCentreX(), r.getY() + kLampDy }); }

juce::Rectangle<int> px (juce::Rectangle<float> r, float s, float grow = 3.0f) { return r.expanded (grow).transformedBy (juce::AffineTransform::scale (s)).toNearestInt(); }

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
DM1AudioProcessorEditor::DM1AudioProcessorEditor (DM1AudioProcessor& p)
    : PedalEditor (p, kDesignW, kDesignH), proc (p),
      syncAtt (*p.apvts.getParameter ("sync"), [this] (float v) { syncShown = v > 0.5f; syncChanged(); }, nullptr),
      divisionAtt (*p.apvts.getParameter ("division"), [this] (float v) { divisionIdx = (int) std::lround (v); repaint(); }, nullptr)
{
    addKnob (modRate,     "modRate");
    addKnob (modDepth,    "modDepth");
    addKnob (variation,   "variation");
    addKnob (delayTime,   "delayTime");
    addKnob (intensity,   "intensity");
    addKnob (delayVolume, "delayVolume");
    addKnob (mode,        "mode");
    midiUI.add (modRate.slider, "modRate");          midiUI.add (modDepth.slider, "modDepth");          midiUI.add (variation.slider, "variation");
    midiUI.add (delayTime.slider, "delayTime");      midiUI.add (intensity.slider, "intensity");        midiUI.add (delayVolume.slider, "delayVolume");
    midiUI.add (mode.slider, "mode");

    // 12 positions at 30 degree steps, Classic at 12 o'clock, clockwise
    mode.slider.setRotaryParameters (0.0f, juce::degreesToRadians (330.0f), true);
    mode.slider.setRange (1.0, 12.0, 1.0);
    for (auto* k : { &modRate, &modDepth, &variation, &delayTime, &intensity, &delayVolume })
        k->slider.setRotaryParameters (juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);

    tapZone.onDown       = [this] { if (! proc.syncOn()) proc.tapTempo(); };   // TAP does nothing while sync is on
    divisionZone.onClick = [this] { cycleDivision(); };
    memoryZone.onDown    = [this] { memHeld = true; memWritten = false; memDownAt = juce::Time::getMillisecondCounter(); };
    memoryZone.onUp      = [this] { memHeld = false; };
    memoryZone.onClick   = [this] { if (! memWritten) memory.stepNext(); repaint(); };      // short press
    for (int i = 0; i < 5; ++i)      // clicking a lamp selects it: a memory slot (MANUAL, 1-4) or a tap division
    {
        memLedZone[i].onClick = [this, i] { memory.select (i); repaint(); };
        divLedZone[i].onClick = [this, i]
        {
            auto* prm = proc.apvts.getParameter ("tapDivision");
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 ((float) (i + 1)));
            prm->endChangeGesture();
        };
        addAndMakeVisible (memLedZone[i]);
        addAndMakeVisible (divLedZone[i]);
    }
    for (auto* z : { &tapZone, &divisionZone, &memoryZone })
        addAndMakeVisible (z);

    resized();
    syncAtt.sendInitialUpdate();
    divisionAtt.sendInitialUpdate();
    timerCallback();
    startTimerHz (30);
}

DM1AudioProcessorEditor::~DM1AudioProcessorEditor() { stopTimer(); }

void DM1AudioProcessorEditor::addKnob (Knob& k, const juce::String& paramId)
{
    auto& s = k.slider;
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    s.setMouseDragSensitivity (180);
    s.setScrollWheelEnabled (false);
    s.setPopupDisplayEnabled (true, true, this);
    s.onValueChange = [this] { repaint(); };
    addAndMakeVisible (s);
    k.attachment = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter (paramId), s, nullptr);
}

void DM1AudioProcessorEditor::syncChanged()
{
    // the DELAY TIME knob is the delay time, or the note division while sync is on (the attachment sets the slider range from the parameter)
    delayTime.attachment.reset();
    divisionKnobAtt.reset();
    if (syncShown) divisionKnobAtt = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter ("division"), delayTime.slider, nullptr);
    else           delayTime.attachment = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter ("delayTime"), delayTime.slider, nullptr);
    tapZone.setEnabled (! syncShown);      // TAP works only with sync off
    repaint();
}

void DM1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    if (toggleBypassAt (e.position)) { repaint(); return; }          // the power switch is the bypass switch
    if (syncRect().expanded (3.0f).contains (e.position / scale()))
        syncAtt.setValueAsCompleteGesture (syncShown ? 0.0f : 1.0f);
}

void DM1AudioProcessorEditor::cycleDivision()
{
    auto* prm = proc.apvts.getParameter ("tapDivision");
    const int next = division >= 5 ? 1 : division + 1;
    prm->beginChangeGesture();
    prm->setValueNotifyingHost (prm->convertTo0to1 ((float) next));
    prm->endChangeGesture();
}

void DM1AudioProcessorEditor::resized()
{
    Knob* ks[] = { &modRate, &modDepth, &variation, &delayTime, &intensity, &delayVolume, &mode };
    for (int i = 0; i < 7; ++i)
        placeKnob (ks[i]->slider, knobCentre (i));

    tapZone.setBounds (px (tapRect(), scale()));
    divisionZone.setBounds (px (leftButton (5), scale()));
    memoryZone.setBounds (px (leftButton (3), scale()));
    for (int i = 0; i < 5; ++i)
    {
        const auto z = [this] (juce::Point<float> c) { return juce::Rectangle<float> (50.0f, 50.0f).withCentre (c).transformedBy (juce::AffineTransform::scale (scale())).toNearestInt(); };
        memLedZone[i].setBounds (z (lampCentre (3, i)));
        divLedZone[i].setBounds (z (lampCentre (5, i)));
    }
}

void DM1AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    midiUI.paintMarks (g, kPal.accent);
}

void DM1AudioProcessorEditor::timerCallback()
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
    const bool flashing = now < memFlashUntil;
    if (memory.current() != memShown || flashing || flashing != memFlashShown)
    {
        memShown = memory.current();
        memFlashShown = flashing;
        repaint();
    }

    const bool p = powerOn();
    const bool t = proc.tapLedOn.load();
    const int d = juce::jlimit (1, 5, (int) std::round (proc.apvts.getRawParameterValue ("tapDivision")->load()));
    if (p != powerShown || t != tapOn || d != division)
    {
        powerShown = p;
        tapOn = t;
        division = d;
        repaint();
    }
}

void DM1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    ui::paintBody (g, kPal, kDesignW, kDesignH);
    ui::paintHeaderNoPresets (g, kPal, "DM-1", "DELAY MACHINE", "MODULATED DELAY", powerOn());
    ui::paintFooter (g, kPal, kDesignH - 15.0f);
    for (int i = 0; i < 6; ++i)
        ui::paintPanel (g, kPal, kPanels[i], kTitles[i]);

    // knobs: 0 to 2 MODULATION, 3 to 5 DELAY
    const char* const names[6] = { "RATE", "DEPTH", "VARIATION", "TIME", "INTENSITY", "VOLUME" };
    Knob* ks[] = { &modRate, &modDepth, &variation, &delayTime, &intensity, &delayVolume };
    for (int i = 0; i < 6; ++i)
    {
        auto& s = ks[i]->slider;
        juce::String value = juce::String (s.getValue(), 1), lo = "0", hi = "10";
        if (i == 3 && syncShown)
        {
            value = vibr::sync::divisionNames()[juce::jlimit (0, vibr::sync::kNumDivisions - 1, divisionIdx)];
            lo = "LONG"; hi = "SHORT";
        }
        ui::paintKnob (g, kPal, knobCentre (i), names[i], value, lo, hi, (float) s.valueToProportionOfLength (s.getValue()));
    }
    ui::paintFlatButton (g, kPal, syncRect(), "SYNC", syncShown, 17.0f);

    // MODE: 12 legends in two columns (clockwise from the top right, then up the left side), the position dots round the knob, the name in a screen
    {
        const auto c = knobCentre (6);
        const int m = juce::jlimit (1, 12, (int) std::lround (mode.slider.getValue())) - 1;
        for (int p = 0; p < 12; ++p)
        {
            const float a = juce::degreesToRadians ((float) p * 30.0f);
            const juce::Point<float> d { c.x + std::sin (a) * 47.0f, c.y - std::cos (a) * 47.0f };
            g.setColour (p == m ? kPal.accent : kPal.dashed);
            g.fillEllipse (juce::Rectangle<float> (p == m ? 6.0f : 4.0f, p == m ? 6.0f : 4.0f).withCentre (d));

            const bool right = p < 6;
            const int row = right ? p : 11 - p;
            ui::drawText (g, kModeNames[p], c.x + (right ? 62.0f : -62.0f), c.y + ((float) row - 2.5f) * 24.0f, 14.0f, p == m ? kPal.accent : kPal.inkSoft, p == m, 0.08f,
                          right ? juce::Justification::centredLeft : juce::Justification::centredRight);
        }
        ui::drawText (g, "MODE", c.x, c.y + 51.3f + 22.0f, 21.0f, kPal.ink, true, 0.1f);
        ui::drawText (g, juce::String (m + 1) + " / 12", c.x, c.y + 51.3f + 43.0f, 14.0f, kPal.accent, true, 0.0f, juce::Justification::centred, ui::Face::Mono);
        ui::paintScreen (g, kPal, modeScreen());
        ui::drawText (g, kModeNames[m], modeScreen().getCentreX(), modeScreen().getCentreY(), 19.0f, kPal.phosphor, true, 0.06f, juce::Justification::centred, ui::Face::Mono);
    }

    // MEMORY: the button and MANUAL / 1-4 (all five flash after a write)
    const auto now = juce::Time::getMillisecondCounter();
    const bool memFlashing = now < memFlashUntil, blinkOff = memFlashing && ((now / 100) % 2) == 1;
    ui::paintFlatButton (g, kPal, leftButton (3), "MEMORY", memoryZone.isDown(), 17.0f);
    for (int i = 0; i < 5; ++i)
    {
        drawLamp (g, lampCentre (3, i), memFlashing ? ! blinkOff : memory.current() == i, juce::Colour (0xff7FD86A), juce::Colour (0xff253020));
        ui::drawText (g, kMemLabels[i], lampCentre (3, i).x, kRow2Y + kLabelDy, 13.0f, kPal.inkSoft, false, 0.0f, juce::Justification::centred, ui::Face::Mono);
    }

    // TAP and TAP DIVISION
    ui::paintFlatButton (g, kPal, tapRect(), "TAP", tapOn || tapZone.isDown(), 17.0f, syncShown ? 0.4f : 1.0f);
    ui::drawText (g, syncShown ? "SYNC ON: HOST TEMPO" : "TAP SETS THE DELAY TIME", kPanels[4].getCentreX(), kRow2Y + kLabelDy, 14.0f, kPal.inkSoft, false, 0.04f,
                  juce::Justification::centred, ui::Face::Mono);
    ui::paintFlatButton (g, kPal, leftButton (5), "DIVISION", divisionZone.isDown(), 17.0f);
    for (int i = 0; i < 5; ++i)
    {
        drawLamp (g, lampCentre (5, i), division == i + 1, juce::Colour (0xffFF4A2A), juce::Colour (0xff5A2A20));
        ui::drawText (g, kDivLabels[i], lampCentre (5, i).x, kRow2Y + kLabelDy, 13.0f, kPal.inkSoft, false, 0.0f, juce::Justification::centred, ui::Face::Mono);
    }
}
