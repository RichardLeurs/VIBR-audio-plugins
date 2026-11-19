// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"

namespace ui = vibr::ui::vintage;

namespace
{
const ui::Palette kPal { juce::Colour (82, 118, 229) };

// Design space 1120 x 1182 (margin 28, gap 18). Row 1 (228): TEMPO, STEREO, ROUTING. Row 2 (387): DELAY A, DELAY B. Row 3 (387): BOOST, AMBIENCE, SHIMMER.
// Knob rows sit 86 and 236 below a panel top; the content area ends 30 above the bottom.
constexpr float kDesignW = 1120.0f, kDesignH = 1182.0f, kGap = 18.0f;
constexpr float kRow1Y = 114.0f, kRow1H = 228.0f, kRow2Y = kRow1Y + kRow1H + kGap, kRow23H = 387.0f, kRow3Y = kRow2Y + kRow23H + kGap;
constexpr float kKnobDy = 86.0f, kRowStep = 159.0f;
constexpr float kDelayW = 523.0f, kTempoW = 628.0f, kOneW = 200.0f;
constexpr juce::uint32 kHoldMs = 800;
const char* const kIntervalNames[5] = { "-OCT", "+5TH", "+OCT", "+OCT+5", "+2OCT" };

juce::Rectangle<float> tempoPanel()  { return { ui::kMargin, kRow1Y, kTempoW, kRow1H }; }
juce::Rectangle<float> stereoPanel() { return { ui::kMargin + kTempoW + kGap, kRow1Y, kOneW, kRow1H }; }
juce::Rectangle<float> routePanel()  { return { ui::kMargin + kTempoW + kOneW + 2.0f * kGap, kRow1Y, kOneW, kRow1H }; }
juce::Rectangle<float> delayPanel (int i) { return { ui::kMargin + (float) i * (kDelayW + kGap), kRow2Y, kDelayW, kRow23H }; }
juce::Rectangle<float> boostPanel()   { return { ui::kMargin, kRow3Y, 252.0f, kRow23H }; }
juce::Rectangle<float> ambiencePanel() { return { ui::kMargin + 252.0f + kGap, kRow3Y, 396.0f, kRow23H }; }
juce::Rectangle<float> shimmerPanel() { return { ui::kMargin + 252.0f + 396.0f + 2.0f * kGap, kRow3Y, 380.0f, kRow23H }; }

// TEMPO: MEMORY block (lamps, button, hint), the tempo block (display, TAP, HOST SYNC) and the TIME knob of delay A
float memLampX (int i)                  { return tempoPanel().getX() + 64.0f + 44.0f * (float) i; }
float memLampY()                        { return tempoPanel().getY() + 66.0f; }
juce::Rectangle<float> memoryButton()   { return juce::Rectangle<float> (150.0f, 30.0f).withCentre ({ tempoPanel().getX() + 152.0f, tempoPanel().getY() + 125.0f }); }
juce::Rectangle<float> bpmScreen()      { return { tempoPanel().getX() + 276.0f, tempoPanel().getY() + 52.0f, 170.0f, 44.0f }; }
juce::Rectangle<float> tapButton()      { return { tempoPanel().getX() + 276.0f, tempoPanel().getY() + 108.0f, 170.0f, 30.0f }; }
juce::Rectangle<float> hostButton()     { return { tempoPanel().getX() + 276.0f, tempoPanel().getY() + 150.0f, 170.0f, 30.0f }; }
juce::Point<float> timeCentre()         { return { tempoPanel().getX() + 541.0f, tempoPanel().getY() + kKnobDy }; }

// the 4th column of a delay panel: the time screen and the PING-PONG button
float delayCol (int i, int c)           { return delayPanel (i).getCentreX() + (-168.0f + 112.0f * (float) c); }
juce::Rectangle<float> timeScreen (int i) { return juce::Rectangle<float> (104.0f, 44.0f).withCentre ({ delayCol (i, 3), delayPanel (i).getY() + 94.0f }); }
juce::Rectangle<float> pingButton (int i) { return juce::Rectangle<float> (104.0f, 30.0f).withCentre ({ delayCol (i, 3), delayPanel (i).getY() + 145.0f }); }

juce::Rectangle<float> boostButton()    { return juce::Rectangle<float> (100.0f, 30.0f).withCentre ({ boostPanel().getX() + 70.0f, boostPanel().getY() + kKnobDy }); }
// SERIES over PARALLEL, sharing their middle border
juce::Rectangle<float> routeButton (int i) { return { routePanel().getX() + 20.0f, routePanel().getY() + 87.0f + 28.0f * (float) i, 160.0f, 30.0f }; }

const char* const kPingId[2] = { "pingA", "pingB" };

juce::String msText (float ms)
{
    return (ms < 100.0f ? juce::String (ms, 1) : juce::String ((int) std::lround (ms))) + " ms";
}

// the 14 disc lamp: lit = colour with a soft glow, off = its dark tone, 2 ink ring
void paintLamp (juce::Graphics& g, juce::Point<float> c, bool on)
{
    const auto r = juce::Rectangle<float> (14.0f, 14.0f).withCentre (c);
    const auto lit = kPal.segGreen, dark = kPal.segOff;
    if (on) { g.setColour (lit.withAlpha (0.28f)); g.fillEllipse (r.expanded (5.0f)); }
    g.setColour (on ? lit : dark);
    g.fillEllipse (r);
    g.setColour (kPal.ink);
    g.drawEllipse (r.expanded (1.0f), 2.0f);
}

void paintPair (juce::Graphics& g, juce::Rectangle<float> first, juce::Rectangle<float> second, const char* l0, const char* l1, int lit)
{
    for (const int i : { 1 - lit, lit })                          // the lit one last: it keeps its whole border
        ui::paintFlatButton (g, kPal, i == 0 ? first : second, i == 0 ? l0 : l1, i == lit);
}

struct KnobSpec { const char* id; const char* name; juce::Point<float> c; int kind; };
std::vector<KnobSpec> makeSpecs()
{
    std::vector<KnobSpec> v;
    v.push_back ({ "aTime", "TIME", timeCentre(), 0 });
    v.push_back ({ "spread", "SPREAD", { stereoPanel().getCentreX(), stereoPanel().getY() + kKnobDy }, 0 });
    for (int i = 0; i < 2; ++i)                                   // DELAY A (ids a...), DELAY B (b..., the filter ids lowCut / hiCut)
    {
        const auto p = delayPanel (i);
        const float y1 = p.getY() + kKnobDy, y2 = y1 + kRowStep;
        const bool a = i == 0;
        v.push_back ({ a ? "aDiv" : "bDiv",        "DIVISION", { delayCol (i, 0), y1 }, a ? 1 : 2 });
        v.push_back ({ a ? "aLowCut" : "lowCut",   "LOW CUT",  { delayCol (i, 1), y1 }, 0 });
        v.push_back ({ a ? "aHiCut" : "hiCut",     "HI CUT",   { delayCol (i, 2), y1 }, 0 });
        v.push_back ({ a ? "aRep" : "bRep",        "REPEATS",  { delayCol (i, 0), y2 }, 0 });
        v.push_back ({ a ? "aLevel" : "bLevel",    "LEVEL",    { delayCol (i, 1), y2 }, 0 });
        v.push_back ({ a ? "aMod" : "bMod",        "MOD",      { delayCol (i, 2), y2 }, 0 });
        v.push_back ({ a ? "aRate" : "bRate",      "RATE",     { delayCol (i, 3), y2 }, 0 });
    }
    const auto bp = boostPanel();
    v.push_back ({ "drive",      "DRIVE", { bp.getX() + 182.0f, bp.getY() + kKnobDy }, 0 });
    v.push_back ({ "boostTone",  "TONE",  { bp.getX() + 70.0f,  bp.getY() + kKnobDy + kRowStep }, 0 });
    v.push_back ({ "boostLevel", "LEVEL", { bp.getX() + 182.0f, bp.getY() + kKnobDy + kRowStep }, 0 });
    const auto ap = ambiencePanel();
    const char* chId[3] = { "chRate", "chDepth", "chLevel" }, *chName[3] = { "RATE", "DEPTH", "LEVEL" };
    const char* rvId[3] = { "rvDecay", "rvTone", "rvLevel" }, *rvName[3] = { "DECAY", "TONE", "LEVEL" };
    for (int c = 0; c < 3; ++c)
    {
        v.push_back ({ chId[c], chName[c], { ap.getX() + 118.0f + 100.0f * (float) c, ap.getY() + kKnobDy }, 0 });
        v.push_back ({ rvId[c], rvName[c], { ap.getX() + 118.0f + 100.0f * (float) c, ap.getY() + kKnobDy + kRowStep }, 0 });
    }
    const auto sp = shimmerPanel();
    v.push_back ({ "rInt",   "INTERVAL", { sp.getCentreX() - 56.0f, sp.getY() + kKnobDy }, 3 });
    v.push_back ({ "rDecay", "DECAY",    { sp.getCentreX() + 56.0f, sp.getY() + kKnobDy }, 0 });
    v.push_back ({ "rShim",  "SHIMMER",  { sp.getCentreX() - 112.0f, sp.getY() + kKnobDy + kRowStep }, 0 });
    v.push_back ({ "rTone",  "TONE",     { sp.getCentreX(),          sp.getY() + kKnobDy + kRowStep }, 0 });
    v.push_back ({ "rLevel", "LEVEL",    { sp.getCentreX() + 112.0f, sp.getY() + kKnobDy + kRowStep }, 0 });
    return v;
}
} // namespace

//==============================================================================
DD1AudioProcessorEditor::DD1AudioProcessorEditor (DD1AudioProcessor& p)
    : PedalEditor (p, kDesignW, kDesignH), proc (p)
{
    for (const auto& s : makeSpecs())
    {
        auto k = std::make_unique<Knob>();
        k->name = s.name;
        k->centre = s.c;
        k->kind = s.kind;
        k->slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        k->slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        k->slider.setRotaryParameters (juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
        k->slider.setMouseDragSensitivity (200);
        k->slider.setScrollWheelEnabled (false);
        k->slider.setPopupDisplayEnabled (true, true, this);
        k->slider.onValueChange = [this] { repaint(); };
        addAndMakeVisible (k->slider);
        k->attachment = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter (s.id), k->slider, nullptr);
        midiUI.add (k->slider, s.id);
        knobs.push_back (std::move (k));
    }
    resized();                                                       // place the knobs (the base class sized the window before they existed)
    startTimerHz (20);
}

DD1AudioProcessorEditor::~DD1AudioProcessorEditor() = default;

void DD1AudioProcessorEditor::resized()
{
    for (auto& k : knobs)
        placeKnob (k->slider, k->centre);
}

void DD1AudioProcessorEditor::tap()
{
    proc.midiTap.tap();                                     // the same tap logic as the MIDI tap (in the processor)
    repaint();
}

void DD1AudioProcessorEditor::toggleParam (const char* id)
{
    auto* prm = proc.apvts.getParameter (id);
    prm->beginChangeGesture();
    prm->setValueNotifyingHost (prm->getValue() > 0.5f ? 0.0f : 1.0f);
    prm->endChangeGesture();
}

void DD1AudioProcessorEditor::setBool (const char* id, bool v)
{
    auto* prm = proc.apvts.getParameter (id);
    if ((prm->getValue() > 0.5f) == v) return;
    prm->beginChangeGesture();
    prm->setValueNotifyingHost (v ? 1.0f : 0.0f);
    prm->endChangeGesture();
}

void DD1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    if (toggleBypassAt (e.position)) { repaint(); return; }           // the power switch is the bypass switch
    if (e.mods.isPopupMenu()) return;
    const auto pt = e.position / scale();
    const auto hit = [&pt] (juce::Rectangle<float> r) { return r.expanded (3.0f).contains (pt); };

    for (int i = 0; i < 5; ++i)           // a memory lamp (MANUAL, 1-4) selects that slot and recalls it
        if (juce::Rectangle<float> (44.0f, 50.0f).withCentre ({ memLampX (i), memLampY() + 8.0f }).contains (pt))
        {
            memory.select (i);
            repaint();
            return;
        }
    if (hit (memoryButton()))             // MEMORY: short press steps, hold writes
    {
        memHeld = true; memWritten = false; memDownAt = juce::Time::getMillisecondCounter();
    }
    else if (hit (tapButton()))           tap();
    else if (hit (hostButton()))          toggleParam ("hostSync");
    else if (hit (boostButton()))         toggleParam ("boostOn");
    else if (hit (routeButton (0)))       setBool ("series", true);
    else if (hit (routeButton (1)))       setBool ("series", false);
    else
        for (int i = 0; i < 2; ++i)
            if (hit (pingButton (i))) { toggleParam (kPingId[i]); break; }
}

void DD1AudioProcessorEditor::mouseUp (const juce::MouseEvent&)
{
    if (memHeld && ! memWritten)
        memory.stepNext();            // short press: MANUAL -> 1..4 and recall
    memHeld = false;
    repaint();
}

void DD1AudioProcessorEditor::timerCallback()
{
    if (midiUI.needsRepaint()) { midiUI.tick(); repaint(); }
    // memory: holding the button writes the slot; moving a knob after a recall goes back to MANUAL
    const auto nowMem = juce::Time::getMillisecondCounter();
    if (memHeld && ! memWritten && nowMem - memDownAt >= kHoldMs)
    {
        memory.write();
        memWritten = true;
        memFlashUntil = nowMem + 500;
    }
    memory.revertIfEdited();
    const bool flashing = nowMem < memFlashUntil;
    const bool memDirty = memory.current() != memShown || flashing || flashing != memFlashShown;
    memShown = memory.current();
    memFlashShown = flashing;

    const bool on = powerOn();
    const bool series = proc.apvts.getRawParameterValue ("series")->load() > 0.5f;
    const auto flag = [this] (const char* id, int bit) { return proc.apvts.getRawParameterValue (id)->load() > 0.5f ? bit : 0; };
    const int flags = flag ("boostOn", 1) | flag ("pingA", 2) | flag ("pingB", 4) | flag ("hostSync", 8);
    const float bpm = proc.bpmNow.load();
    const double nowMs = (double) juce::Time::getMillisecondCounterHiRes();
    const double beat = 60000.0 / juce::jmax (30.0f, bpm);
    const bool blink = std::fmod (nowMs, beat) < 90.0 || nowMs - proc.midiTap.lastMs() < 90.0;      // the TAP pad lights on the beat
    bool dirty = memDirty || on != lastOn || series != lastSeries || flags != lastFlags || blink != lastBlink || std::fabs (bpm - lastBpm) > 0.04f;
    lastBlink = blink; lastBpm = bpm; lastOn = on; lastSeries = series; lastFlags = flags;
    for (int i = 0; i < 2; ++i)
    {
        const float ms = proc.timeMs[i].load();
        if (std::fabs (ms - lastMs[i]) > 0.04f) { lastMs[i] = ms; dirty = true; }
    }
    if (dirty) repaint();
}

void DD1AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    midiUI.paintMarks (g, kPal.accent);
}

void DD1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    ui::paintBody (g, kPal, kDesignW, kDesignH);
    ui::paintHeaderNoPresets (g, kPal, "DD-1", "DUAL DELAY", "SHIMMERING ECHOES", powerOn());

    const auto flag = [this] (const char* id) { return proc.apvts.getRawParameterValue (id)->load() > 0.5f; };

    // TEMPO: MEMORY lamps and button, the tempo screen, TAP, HOST SYNC and the TIME knob
    ui::paintPanel (g, kPal, tempoPanel(), "TEMPO");
    {
        const auto t = juce::Time::getMillisecondCounter();
        const bool flashing = t < memFlashUntil, blinkOff = flashing && ((t / 100) % 2) == 1;      // all five flash after a write
        const char* lampNames[5] = { "MANUAL", "1", "2", "3", "4" };
        for (int i = 0; i < 5; ++i)
        {
            paintLamp (g, { memLampX (i), memLampY() }, flashing ? ! blinkOff : memory.current() == i);
            ui::drawText (g, lampNames[i], memLampX (i), memLampY() + 24.0f, 13.0f, kPal.ink, true, 0.06f);
        }
        ui::paintFlatButton (g, kPal, memoryButton(), "MEMORY", memHeld);
        ui::drawText (g, "HOLD 0.8 S TO WRITE", memoryButton().getCentreX(), memoryButton().getBottom() + 25.0f, 13.0f, kPal.inkSoft, false, 0.06f);

        ui::paintScreen (g, kPal, bpmScreen());
        ui::drawText (g, juce::String (proc.bpmNow.load(), 1) + " BPM", bpmScreen().getCentreX(), bpmScreen().getCentreY(), 24.0f, kPal.phosphor, true, 0.02f,
                      juce::Justification::centred, ui::Face::Mono);
        const double nowMs = (double) juce::Time::getMillisecondCounterHiRes();
        const bool lit = std::fmod (nowMs, 60000.0 / juce::jmax (30.0f, proc.bpmNow.load())) < 90.0 || nowMs - proc.midiTap.lastMs() < 90.0;
        ui::paintFlatButton (g, kPal, tapButton(), "TAP", lit);
        ui::paintFlatButton (g, kPal, hostButton(), "HOST SYNC", flag ("hostSync"));
    }

    ui::paintPanel (g, kPal, stereoPanel(), "STEREO");

    // ROUTING: SERIES / PARALLEL (the pair shares its middle border)
    ui::paintPanel (g, kPal, routePanel(), "ROUTING");
    paintPair (g, routeButton (0), routeButton (1), "SERIES", "PARALLEL", flag ("series") ? 0 : 1);

    // DELAY A / DELAY B
    for (int i = 0; i < 2; ++i)
    {
        ui::paintPanel (g, kPal, delayPanel (i), i == 0 ? "DELAY A" : "DELAY B");
        ui::paintScreen (g, kPal, timeScreen (i));
        ui::drawText (g, msText (proc.timeMs[i].load()), timeScreen (i).getCentreX(), timeScreen (i).getCentreY(), 20.0f, kPal.phosphor, true, 0.0f,
                      juce::Justification::centred, ui::Face::Mono);
        ui::paintFlatButton (g, kPal, pingButton (i), "PING-PONG", flag (kPingId[i]));
    }

    // BOOST, AMBIENCE (the rows are named on the left edge), SHIMMER
    ui::paintPanel (g, kPal, boostPanel(), "BOOST");
    ui::paintFlatButton (g, kPal, boostButton(), "ENGAGE", flag ("boostOn"));
    ui::paintPanel (g, kPal, ambiencePanel(), "AMBIENCE");
    ui::paintPanel (g, kPal, shimmerPanel(), "SHIMMER");
    for (int r = 0; r < 2; ++r)
    {
        const juce::Point<float> c { ambiencePanel().getX() + 30.0f, ambiencePanel().getY() + kKnobDy + kRowStep * (float) r };
        g.saveState();
        g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, c.x, c.y));
        ui::drawText (g, r == 0 ? "CHORUS" : "REVERB", c.x, c.y, 16.0f, kPal.inkSoft, true, 0.16f);
        g.restoreState();
    }

    // the knobs
    for (auto& k : knobs)
    {
        auto& s = k->slider;
        const float prop = (float) s.valueToProportionOfLength (s.getValue());
        const int idx = (int) std::lround (s.getValue());
        if (k->kind == 0)
            ui::paintKnob (g, kPal, k->centre, k->name, juce::String (s.getValue(), 1), "0", "10", prop);
        else if (k->kind == 1)
            ui::paintKnob (g, kPal, k->centre, k->name, dd1::divisionName (idx), dd1::divisionName (0), dd1::divisionName (dd1::kNumDivisions - 1), prop);
        else if (k->kind == 2)
            ui::paintKnob (g, kPal, k->centre, k->name, dd1::ratioName (idx), dd1::ratioName (0), dd1::ratioName (dd1::kNumDivisions - 1), prop);
        else
            ui::paintKnob (g, kPal, k->centre, k->name, kIntervalNames[juce::jlimit (0, 4, idx)], kIntervalNames[0], kIntervalNames[4], prop);
    }

    ui::paintFooter (g, kPal, kDesignH - 15.0f);
}
