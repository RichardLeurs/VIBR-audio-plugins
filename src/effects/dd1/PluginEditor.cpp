// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"

namespace ui = vibr::ui::vintage;

namespace
{
const ui::Palette kPal { juce::Colour (82, 118, 229) };

// Design space 1120 x 594 (margin 28, gap 18, row 2 another 14 lower so the tabs of EFFECTS keep a gap to row 1). Row 1 (202): DELAY A, DELAY B (equal, 523 wide; screen + PING-PONG on top, one row of 7 small knobs).
// Row 2 (216): TEMPO & STEREO (450) and EFFECTS (596, tabs BOOST / AMBIENCE / SHIMMER on its top edge; one tab's controls at a time). The content ends 30 above the bottom.
// Knob rows: delay knobs 106 below the panel top, effect and tempo knobs 96 (the same y in every tab).
constexpr float kDesignW = 1120.0f, kDesignH = 594.0f, kGap = 18.0f, kTabRoom = 14.0f;      // kTabRoom: extra space above row 2, the EFFECTS tabs stick out 15 above the panel edge
constexpr float kRow1Y = 114.0f, kRow1H = 202.0f, kRow2Y = kRow1Y + kRow1H + kGap + kTabRoom, kRow2H = 216.0f;
constexpr float kDelayW = 523.0f, kTempoW = 450.0f, kEffW = 1064.0f - kTempoW - kGap;
constexpr float kDelayKnobR = 22.0f, kBigKnobR = 27.0f, kFxKnobR = 28.0f, kDelayKnobDy = 106.0f, kRow2KnobDy = 96.0f;
const char* const kIntervalNames[5] = { "-OCT", "+5TH", "+OCT", "+OCT+5", "+2OCT" };
const std::vector<juce::String> kTabNames { "BOOST", "AMBIENCE", "SHIMMER" };

juce::Rectangle<float> delayPanel (int i) { return { ui::kMargin + (float) i * (kDelayW + kGap), kRow1Y, kDelayW, kRow1H }; }
juce::Rectangle<float> tempoPanel()  { return { ui::kMargin, kRow2Y, kTempoW, kRow2H }; }
juce::Rectangle<float> effPanel()    { return { ui::kMargin + kTempoW + kGap, kRow2Y, kEffW, kRow2H }; }

// DELAY: the time screen (left) and PING-PONG (right) on one line, the 7 knobs below (centres 70 apart, centred in the panel)
juce::Rectangle<float> timeScreen (int i)  { return { delayPanel (i).getX() + 20.0f, delayPanel (i).getY() + 22.0f, 150.0f, 40.0f }; }
juce::Rectangle<float> pingButton (int i)  { return juce::Rectangle<float> (120.0f, 32.0f).withRightX (delayPanel (i).getRight() - 20.0f).withY (timeScreen (i).getCentreY() - 16.0f); }
float delayKnobX (int i, int c)            { return delayPanel (i).getX() + (kDelayW - 6.0f * 70.0f) * 0.5f + 70.0f * (float) c; }

// TEMPO & STEREO: left column x +20 .. +250 (BPM screen, TAP / HOST SYNC, ROUTING), the TIME and SPREAD knobs right of it
juce::Rectangle<float> bpmScreen()    { return { tempoPanel().getX() + 20.0f, tempoPanel().getY() + 26.0f, 230.0f, 48.0f }; }
juce::Rectangle<float> tapButton()    { return { tempoPanel().getX() + 20.0f, tempoPanel().getY() + 96.0f, 101.0f, 36.0f }; }
juce::Rectangle<float> hostButton()   { return { tempoPanel().getX() + 129.0f, tempoPanel().getY() + 96.0f, 121.0f, 36.0f }; }
juce::Rectangle<float> routeButton (int i) { return { tempoPanel().getX() + 90.0f + 82.0f * (float) i, tempoPanel().getY() + 154.0f, 76.0f, 36.0f }; }     // SERIES | PARALLEL, 6 apart, close to the ROUTING label
juce::Point<float> tempoKnob (int i)  { return { tempoPanel().getX() + 308.0f + 84.0f * (float) i, tempoPanel().getY() + kRow2KnobDy }; }

// EFFECTS: the engage button of BOOST
juce::Rectangle<float> boostButton()  { return juce::Rectangle<float> (120.0f, 36.0f).withCentre ({ effPanel().getX() + 152.0f, effPanel().getY() + kRow2KnobDy }); }


// the three tab pills on the top edge of EFFECTS: the shared tab look (30 high, text 16, gap 6, same positions as ui::tabRects) but 18 wider, the room on the left of the text
// is for a small square that says whether that effect is on
constexpr float kMarkRoom = 18.0f;
std::vector<juce::Rectangle<float>> fxTabRects()
{
    std::vector<juce::Rectangle<float>> r;
    float x = effPanel().getX() + ui::titleEnd ("EFFECTS");
    for (const auto& n : kTabNames)
    {
        const float w = ui::textWidth (ui::font (16.0f, true).withExtraKerningFactor (0.08f), n) + 20.0f + kMarkRoom;
        r.push_back ({ x, effPanel().getY() - 15.0f + 1.0f, w, 30.0f });
        x += w + 6.0f;
    }
    return r;
}

const char* const kPingId[2] = { "pingA", "pingB" };

juce::String msText (float ms)
{
    return (ms < 100.0f ? juce::String (ms, 1) : juce::String ((int) std::lround (ms))) + " ms";
}

// a knob of the 7-knob delay rows: ring, end legends, name (16) and value (14) below
void paintCompactKnob (juce::Graphics& g, juce::Point<float> c, const juce::String& name, const juce::String& value, const juce::String& lo, const juce::String& hi, float prop)
{
    const float R = kDelayKnobR;
    ui::paintValueArc (g, c, R, prop, kPal.accent, kPal.track);
    const float yEnds = c.y + ui::ringRadius (R) * 0.707f + 15.0f;
    ui::drawText (g, lo, c.x - R * 1.35f, yEnds, 13.0f, kPal.inkSoft, false, 0.0f, juce::Justification::centredLeft, ui::Face::Mono);
    ui::drawText (g, hi, c.x + R * 1.35f, yEnds, 13.0f, kPal.inkSoft, false, 0.0f, juce::Justification::centredRight, ui::Face::Mono);
    ui::drawText (g, name, c.x, c.y + R * 1.71f + 14.0f, 16.0f, kPal.ink, true, 0.06f);
    ui::drawText (g, value, c.x, c.y + R * 1.71f + 32.0f, 14.0f, kPal.accent, true, 0.0f, juce::Justification::centred, ui::Face::Mono);
}

struct KnobSpec { const char* id; const char* name; juce::Point<float> c; int kind; float r; int tab; };      // tab -1 = always shown, 0 BOOST, 1 AMBIENCE, 2 SHIMMER
std::vector<KnobSpec> makeSpecs()
{
    std::vector<KnobSpec> v;
    v.push_back ({ "aTime", "TIME",   tempoKnob (0), 0, kBigKnobR, -1 });
    v.push_back ({ "spread", "SPREAD", tempoKnob (1), 0, kBigKnobR, -1 });
    for (int i = 0; i < 2; ++i)                                   // DELAY A (ids a...), DELAY B (b..., the filter ids lowCut / hiCut)
    {
        const bool a = i == 0;
        const float y = delayPanel (i).getY() + kDelayKnobDy;
        const char* ids[7]   = { a ? "aDiv" : "bDiv", a ? "aRep" : "bRep", a ? "aLowCut" : "lowCut", a ? "aHiCut" : "hiCut", a ? "aMod" : "bMod", a ? "aRate" : "bRate", a ? "aLevel" : "bLevel" };
        const char* names[7] = { "DIVISION", "REPEATS", "LOW CUT", "HI CUT", "MOD", "RATE", "LEVEL" };
        for (int c = 0; c < 7; ++c)
            v.push_back ({ ids[c], names[c], { delayKnobX (i, c), y }, c == 0 ? (a ? 1 : 2) : 0, kDelayKnobR, -1 });
    }
    const auto ep = effPanel();
    const float y = ep.getY() + kRow2KnobDy, r = kFxKnobR;
    const auto x = [&] (float dx) { return ep.getX() + dx; };
    // BOOST: the engage button, then DRIVE, TONE, LEVEL
    const char* bId[3] = { "drive", "boostTone", "boostLevel" }, *bName[3] = { "DRIVE", "TONE", "LEVEL" };
    for (int c = 0; c < 3; ++c) v.push_back ({ bId[c], bName[c], { x (286.0f + 90.0f * (float) c), y }, 0, r, 0 });
    // AMBIENCE: chorus (RATE, DEPTH, LEVEL) and reverb (DECAY, TONE, LEVEL) in one row
    const char* aId[6] = { "chRate", "chDepth", "chLevel", "rvDecay", "rvTone", "rvLevel" }, *aName[6] = { "RATE", "DEPTH", "LEVEL", "DECAY", "TONE", "LEVEL" };
    for (int c = 0; c < 6; ++c) v.push_back ({ aId[c], aName[c], { x (62.0f + 90.0f * (float) (c % 3) + (c / 3) * 292.0f), y }, 0, r, 1 });
    // SHIMMER: INTERVAL, DECAY | SHIMMER, TONE, LEVEL
    const char* sId[5] = { "rInt", "rDecay", "rShim", "rTone", "rLevel" }, *sName[5] = { "INTERVAL", "DECAY", "SHIMMER", "TONE", "LEVEL" };
    const float sx[5] = { 107.0f, 197.0f, 309.0f, 399.0f, 489.0f };
    for (int c = 0; c < 5; ++c) v.push_back ({ sId[c], sName[c], { x (sx[c]), y }, c == 0 ? 3 : 0, r, 2 });
    return v;
}
} // namespace

//==============================================================================
DD1AudioProcessorEditor::DD1AudioProcessorEditor (DD1AudioProcessor& p)
    : PedalEditor (p, kDesignW, kDesignH), proc (p)
{
    fxTab = juce::jlimit (0, 2, (int) proc.apvts.state.getProperty ("fxTab", 2));        // the tab shown survives closing the editor (kept in the state tree, no parameter)
    for (const auto& s : makeSpecs())
    {
        auto k = std::make_unique<Knob>();
        k->name = s.name;
        k->centre = s.c;
        k->kind = s.kind;
        k->r = s.r;
        k->tab = s.tab;
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
    {
        placeKnob (k->slider, k->centre, k->r);
        k->slider.setVisible (k->tab < 0 || k->tab == fxTab);        // only the selected tab's knobs of EFFECTS
    }
}

void DD1AudioProcessorEditor::setTab (int t)
{
    fxTab = juce::jlimit (0, 2, t);
    proc.apvts.state.setProperty ("fxTab", fxTab, nullptr);
    resized();
    repaint();
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
    if (! e.mods.isPopupMenu() && ui::presetBarMouseDown (e.position / scale(), *this, proc.presets, proc.undoManager, scale(), [this] { repaint(); })) return;     // preset screen, UNDO / REDO, A / B
    if (toggleBypassAt (e.position)) { repaint(); return; }           // the power switch is the bypass switch
    if (e.mods.isPopupMenu()) return;
    const auto pt = e.position / scale();
    const auto hit = [&pt] (juce::Rectangle<float> r) { return r.expanded (3.0f).contains (pt); };

    const auto tabs = fxTabRects();
    for (size_t i = 0; i < tabs.size(); ++i)
        if (tabs[i].contains (pt)) { setTab ((int) i); return; }

    if (hit (tapButton()))                tap();
    else if (hit (hostButton()))          toggleParam ("hostSync");
    else if (fxTab == 0 && hit (boostButton())) toggleParam ("boostOn");
    else if (hit (routeButton (0)))       setBool ("series", true);
    else if (hit (routeButton (1)))       setBool ("series", false);
    else
        for (int i = 0; i < 2; ++i)
            if (hit (pingButton (i))) { toggleParam (kPingId[i]); break; }
}

void DD1AudioProcessorEditor::timerCallback()
{
    {   // the preset bar follows host program changes, loads, undo / redo and A / B
        const int key = (int) proc.presets.name().hashCode() ^ (proc.undoManager.canUndo() ? 1 : 0) ^ (proc.undoManager.canRedo() ? 2 : 0) ^ (proc.presets.abSlot() << 2);
        if (key != presetBarKey) { presetBarKey = key; repaint(); }
    }
    if (midiUI.needsRepaint()) { midiUI.tick(); repaint(); }

    const bool on = powerOn();
    const bool series = proc.apvts.getRawParameterValue ("series")->load() > 0.5f;
    const auto flag = [this] (const char* id, int bit) { return proc.apvts.getRawParameterValue (id)->load() > 0.5f ? bit : 0; };
    const auto lvl = [this] (const char* id, int bit) { return proc.apvts.getRawParameterValue (id)->load() > 0.05f ? bit : 0; };
    const int flags = flag ("boostOn", 1) | flag ("pingA", 2) | flag ("pingB", 4) | flag ("hostSync", 8) | lvl ("chLevel", 16) | lvl ("rvLevel", 32) | lvl ("rLevel", 64);
    const float bpm = proc.bpmNow.load();
    const double nowMs = (double) juce::Time::getMillisecondCounterHiRes();
    const double beat = 60000.0 / juce::jmax (30.0f, bpm);
    const bool blink = std::fmod (nowMs, beat) < 90.0 || nowMs - proc.midiTap.lastMs() < 90.0;      // the TAP pad lights on the beat
    bool dirty = on != lastOn || series != lastSeries || flags != lastFlags || blink != lastBlink || std::fabs (bpm - lastBpm) > 0.04f;
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
    ui::paintHeader (g, kPal, "DD-1", "DUAL DELAY", "SHIMMERING ECHOES");
    ui::paintPresetBar (g, kPal, proc.presets, proc.undoManager, powerOn());

    const auto val = [this] (const char* id) { return proc.apvts.getRawParameterValue (id)->load(); };
    const auto flag = [&] (const char* id) { return val (id) > 0.5f; };

    // DELAY A / DELAY B: the time screen (ms and the division), PING-PONG
    for (int i = 0; i < 2; ++i)
    {
        ui::paintPanel (g, kPal, delayPanel (i), i == 0 ? "DELAY A" : "DELAY B");
        const auto sc = timeScreen (i);
        ui::paintScreen (g, kPal, sc);
        ui::drawText (g, msText (proc.timeMs[i].load()), sc.getX() + 12.0f, sc.getCentreY(), 20.0f, kPal.phosphor, true, 0.0f, juce::Justification::centredLeft, ui::Face::Mono);
        const int idx = (int) std::lround (val (i == 0 ? "aDiv" : "bDiv"));
        ui::drawText (g, i == 0 ? dd1::divisionName (idx) : dd1::ratioName (idx), sc.getRight() - 12.0f, sc.getCentreY(), 15.0f, kPal.phosphorDim, true, 0.0f,
                      juce::Justification::centredRight, ui::Face::Mono);
        ui::paintFlatButton (g, kPal, pingButton (i), "PING-PONG", flag (kPingId[i]));
    }

    // TEMPO & STEREO: BPM screen, TAP, HOST SYNC, ROUTING (SERIES, PARALLEL)
    ui::paintPanel (g, kPal, tempoPanel(), "TEMPO & STEREO");
    ui::paintScreen (g, kPal, bpmScreen());
    ui::drawText (g, juce::String (proc.bpmNow.load(), 1) + " BPM", bpmScreen().getCentreX(), bpmScreen().getCentreY(), 26.0f, kPal.phosphor, true, 0.02f,
                  juce::Justification::centred, ui::Face::Mono);
    {
        const double nowMs = (double) juce::Time::getMillisecondCounterHiRes();
        const bool lit = std::fmod (nowMs, 60000.0 / juce::jmax (30.0f, proc.bpmNow.load())) < 90.0 || nowMs - proc.midiTap.lastMs() < 90.0;
        ui::paintFlatButton (g, kPal, tapButton(), "TAP", lit);
        ui::paintFlatButton (g, kPal, hostButton(), "HOST SYNC", flag ("hostSync"));
        ui::drawText (g, "ROUTING", tempoPanel().getX() + 20.0f, routeButton (0).getCentreY(), 15.0f, kPal.ink, true, 0.1f, juce::Justification::centredLeft);
        const int lit2 = flag ("series") ? 0 : 1;
        for (const int i : { 1 - lit2, lit2 })                    // the lit one last: it keeps its whole border
            ui::paintFlatButton (g, kPal, routeButton (i), i == 0 ? "SERIES" : "PARALLEL", i == lit2, 15.0f);
    }

    // EFFECTS: the tabs on the top edge (the selected one is filled; a small square in each says whether that effect is on), then the selected tab's controls
    const auto ep = effPanel();
    ui::paintPanel (g, kPal, ep, "EFFECTS");
    {
        const auto rects = fxTabRects();
        const float x0 = rects.front().getX() - 6.0f, x1 = rects.back().getRight() + 6.0f;
        g.setColour (kPal.paper);
        g.fillRect (x0, ep.getY() - 1.0f, x1 - x0, 4.0f);                              // the panel's line stops 6 before the first tab and 6 after the last
        const bool on[3] = { flag ("boostOn"), val ("chLevel") > 0.05f || val ("rvLevel") > 0.05f, val ("rLevel") > 0.05f };      // BOOST: engaged; AMBIENCE: chorus or reverb level up; SHIMMER: its level up
        for (size_t i = 0; i < rects.size(); ++i)
        {
            const bool sel = (int) i == fxTab;
            ui::paintFlatButton (g, kPal, rects[i], "", sel);
            ui::drawText (g, kTabNames[i], rects[i].getX() + 10.0f + kMarkRoom, rects[i].getCentreY(), 16.0f, sel ? kPal.cream : kPal.ink, true, 0.08f, juce::Justification::centredLeft);
            const auto m = juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ rects[i].getX() + 10.0f + 4.5f, rects[i].getCentreY() });
            g.setColour (sel ? kPal.cream : kPal.ink);
            if (on[i]) g.fillRect (m); else g.drawRect (m, 1.5f);
        }
    }
    const auto groupCaption = [&] (const char* t, float dx) { ui::drawText (g, t, ep.getX() + dx, ep.getY() + 40.0f, 15.0f, kPal.inkSoft, true, 0.16f); };
    if (fxTab == 0)      ui::paintFlatButton (g, kPal, boostButton(), "ENGAGE", flag ("boostOn"));
    else if (fxTab == 1) { groupCaption ("CHORUS", 152.0f); groupCaption ("REVERB", 444.0f); }
    else                 { groupCaption ("PITCH", 152.0f); groupCaption ("MIX", 399.0f); }

    // the knobs
    for (auto& k : knobs)
    {
        if (k->tab >= 0 && k->tab != fxTab) continue;
        auto& s = k->slider;
        const float prop = (float) s.valueToProportionOfLength (s.getValue());
        const int idx = (int) std::lround (s.getValue());
        juce::String value, lo = "0", hi = "10";
        if (k->kind == 0)      value = juce::String (s.getValue(), 1);
        else if (k->kind == 1) { value = dd1::divisionName (idx); lo = dd1::divisionName (0); hi = dd1::divisionName (dd1::kNumDivisions - 1); }
        else if (k->kind == 2) { value = dd1::ratioName (idx);    lo = dd1::ratioName (0);    hi = dd1::ratioName (dd1::kNumDivisions - 1); }
        else                   { value = kIntervalNames[juce::jlimit (0, 4, idx)]; lo = kIntervalNames[0]; hi = kIntervalNames[4]; }
        if (k->r == kDelayKnobR) paintCompactKnob (g, k->centre, k->name, value, lo, hi, prop);
        else                     ui::paintKnob (g, kPal, k->centre, k->name, value, lo, hi, prop, k->r);
    }

    ui::paintFooter (g, kPal, kDesignH - 15.0f);
}
