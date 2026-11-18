// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "ui/Presets.h"

namespace
{
using vibr::ui::drawText;
namespace ui = vibr::ui;

constexpr float kW = 1200.0f, kH = 690.0f, kRow1Y = 134.0f, kRow1H = 247.0f, kRow2Y = 405.0f, kRow2H = 216.0f, kBarY = 39.0f;
const ui::Palette kPal { juce::Colour (50, 196, 201), juce::Colour (9, 35, 36) };            // the Modulation colour #32C4C9 and its print

// panels: 0 CONTROL (source tabs), 1 DESTINATION (destination tabs), 2 PATTERN (display), 3 OUTPUT
constexpr float kPanelX[4] = { 20.0f, 730.0f, 20.0f, 840.0f }, kPanelW[4] = { 700.0f, 450.0f, 810.0f, 340.0f };
juce::Rectangle<float> panelRect (int i) { return { kPanelX[i], i < 2 ? kRow1Y : kRow2Y, kPanelW[i], i < 2 ? kRow1H : kRow2H }; }
juce::Point<float> knobCentre (int panel, int k, int n)
{
    const auto r = panelRect (panel);
    if (panel == 3) return { r.getX() + 68.0f + 96.0f * (float) k, r.getY() + 92.0f };      // the output knobs left of the dB meters, the pair centred in the panel
    return { r.getCentreX() + 108.0f * ((float) k - 0.5f * (float) (n - 1)), r.getY() + 92.0f };
}
juce::Point<float> meterOrigin() { const auto r = panelRect (3); return { r.getRight() - 96.0f, r.getY() + 46.0f }; }       // 110 high: the bars end on the knob name row (y + 156), the peak values sit on the readout row (y + 182)
// the buttons of CONTROL: SYNC, the note length, FOLLOW HOST
juce::Rectangle<float> controlButtonRect (int i)
{
    static const float x[3] = { 30.0f, 116.0f, 250.0f }, w[3] = { 80.0f, 128.0f, 150.0f };
    const auto r = panelRect (0);
    return { r.getX() + x[i], r.getY() + 204.0f, w[i], 30.0f };
}
juce::Rectangle<float> typeRect (int i) { const auto r = panelRect (1); return { r.getX() + 15.0f + (float) i * 85.0f, r.getY() + 204.0f, 80.0f, 30.0f }; }
// the PATTERN display: the lane, the traces, the pattern buttons
juce::Rectangle<float> laneRect() { const auto r = panelRect (2); return { r.getX() + 24.0f, r.getY() + 26.0f, r.getWidth() - 48.0f, 48.0f }; }
juce::Rectangle<float> traceRect() { const auto r = panelRect (2); return { r.getX() + 24.0f, r.getY() + 123.0f, r.getWidth() - 48.0f, 40.0f }; }
juce::Rectangle<float> patternButtonRect (int i)
{
    const auto r = panelRect (2); const float pitch = (r.getWidth() - 48.0f + 5.5f) / (float) rh1::kNumPatterns;       // the eleven buttons share the panel width
    return { r.getX() + 24.0f + (float) i * pitch, r.getY() + 177.0f, pitch - 5.5f, 24.0f }; }
const std::vector<juce::String> kSourceTabs { "LFO", "ENVELOPE", "STEPS" }, kModeTabs { "FILTER", "TREMOLO", "SLICER", "MATRIX" };

// the destination MATRIX (the VC-1 layout in the same window): the matrix with its source rows, FILTER (with the destination tabs) and CROSS / OUTPUT
constexpr float kMatX = 20.0f, kMatY = 134.0f, kMatW = 740.0f, kMatH = 486.0f, kRightX = 770.0f, kRightW = 410.0f, kFilterH = 246.0f, kOutY = 404.0f, kOutH = 216.0f;
constexpr float kRowY0 = 204.0f, kRowPitch = 88.0f, kColX0 = 384.0f, kColPitch = 108.0f, kOwnA = 200.0f, kOwnB = 306.0f, kSmallR = 20.0f;
juce::Rectangle<float> matrixRect() { return { kMatX, kMatY, kMatW, kMatH }; }
juce::Rectangle<float> filterRect() { return { kRightX, kMatY, kRightW, kFilterH }; }
juce::Rectangle<float> matOutRect() { return { kRightX, kOutY, kRightW, kOutH }; }
juce::Rectangle<float> mTypeRect (int m) { const auto r = filterRect(); return { r.getRight() - 144.0f, r.getY() + 63.0f + (float) m * 46.0f, 124.0f, 36.0f }; }
juce::Rectangle<float> mShapeRect (int lfo) { return juce::Rectangle<float> (80.0f, 28.0f).withCentre ({ kOwnB, kRowY0 + kRowPitch * (float) lfo }); }
juce::Rectangle<float> mSyncRect() { return juce::Rectangle<float> (88.0f, 26.0f).withCentre ({ 91.0f, 166.0f }); }
juce::Point<float> matMeterOrigin() { const auto r = matOutRect(); return { r.getX() + 318.0f, r.getY() + 46.0f }; }
const char* const kMatrixIds[vc1::kNumSources][vc1::kNumDests] = { { "vm00", "vm01", "vm02", "vm03" }, { "vm10", "vm11", "vm12", "vm13" }, { "vm20", "vm21", "vm22", "vm23" },
                                                                  { "vm30", "vm31", "vm32", "vm33" }, { "vm40", "vm41", "vm42", "vm43" } };
// the bipolar depth ring: from the top mark (0) to the value, in the knob's recess; prop -1 .. +1
void paintBipolarArc (juce::Graphics& g, juce::Point<float> c, float knobRadius, float prop, juce::Colour body)
{
    const float r = knobRadius + 2.5f, a0 = juce::degreesToRadians (-135.0f);
    juce::Path track, lit;
    track.addCentredArc (c.x, c.y, r, r, 0.0f, a0, -a0, true);
    g.setColour (juce::Colours::white.withAlpha (0.08f)); g.strokePath (track, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (std::abs (prop) < 0.01f) return;
    const float a1 = prop * -a0;
    lit.addCentredArc (c.x, c.y, r, r, 0.0f, std::min (0.0f, a1), std::max (0.0f, a1), true);
    for (const auto& [w, al] : { std::pair { 6.0f, 0.12f }, std::pair { 4.0f, 0.22f } })
    { g.setColour (body.withAlpha (al)); g.strokePath (lit, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)); }
    g.setColour (body); g.strokePath (lit, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
// the rectangle the destination tabs sit on: the DESTINATION panel, in the MATRIX layout the FILTER panel
juce::Rectangle<float> tabsRect (int mode) { return mode == 3 ? filterRect() : panelRect (1); }
}

// kinds: 0 rate, 1 percent, 2 shape, 3 tempo, 4 level dB, 5 value 0 .. 10
//==============================================================================
RH1AudioProcessorEditor::RH1AudioProcessorEditor (RH1AudioProcessor& p)
    : LabEditor (p, kW, kH), proc (p),
      knobs { { { "rate", 0, 0, 5, "RATE", 0, 1 | 4, -1, {}, {} },     { "sens", 0, 0, 5, "SENS", 1, 2, -1, {}, {} },
                { "shape", 0, 1, 5, "SHAPE", 2, 1, -1, {}, {} },       { "decay", 0, 1, 5, "DECAY", 1, 2, -1, {}, {} },       { "glide", 0, 1, 5, "GLIDE", 1, 4, -1, {}, {} },
                { "depth", 0, 2, 5, "DEPTH", 1, 7, -1, {}, {} },       { "swing", 0, 3, 5, "SWING", 1, 7, -1, {}, {} },       { "tempo", 0, 4, 5, "TEMPO", 3, 7, -1, {}, {} },
                { "cutoff", 1, 0, 2, "CUTOFF", 1, 7, 0, {}, {} },      { "res", 1, 1, 2, "RESONANCE", 1, 7, 0, {}, {} },
                { "tremStereo", 1, 0, 2, "STEREO", 1, 7, 1, {}, {} },  { "tremBias", 1, 1, 2, "BIAS", 1, 7, 1, {}, {} },
                { "slAttack", 1, 0, 3, "ATTACK", 5, 7, 2, {}, {} },    { "slDuty", 1, 1, 3, "DUTY", 5, 7, 2, {}, {} },        { "slBalance", 1, 2, 3, "BALANCE", 5, 7, 2, {}, {} },
                { "mix", 3, 0, 2, "MIX", 1, 7, -1, {}, {} },           { "level", 3, 1, 2, "LEVEL", 4, 7, -1, {}, {} } } }
{
    for (auto& k : knobs)
    {
        attachKnob (k.slider, k.att, k.id);
        midiUI.add (k.slider, k.id);
    }
    const auto add = [this] (const juce::String& id, int kind, float cx, float cy, float r, const char* name)       // the knobs of the MATRIX layout
    {
        auto k = std::make_unique<MKnob>();
        k->id = id; k->kind = kind; k->cx = cx; k->cy = cy; k->r = r; k->name = name;
        auto& s = k->slider;
        s.textFromValueFunction = [this, kind] (double v) { return matrixText (kind, v); };               // the drag popup shows what the knob shows
        attachKnob (s, k->att, id);
        midiUI.add (s, id);
        matrixKnobs.push_back (std::move (k));
    };
    add ("vEnvSens", 7, kOwnA, kRowY0, kSmallR, "SENS");       add ("vEnvRel", 4, kOwnB, kRowY0, kSmallR, "REL");
    add ("vRate1", 2, kOwnA, kRowY0 + kRowPitch, kSmallR, "RATE");
    add ("vRate2", 2, kOwnA, kRowY0 + 2.0f * kRowPitch, kSmallR, "RATE");
    add ("vRand", 3, kOwnA, kRowY0 + 3.0f * kRowPitch, kSmallR, "RATE");
    for (int sI = 0; sI < vc1::kNumSources; ++sI)
        for (int d = 0; d < vc1::kNumDests; ++d)
            add (kMatrixIds[sI][d], 5, kColX0 + kColPitch * (float) d, kRowY0 + kRowPitch * (float) sI, kSmallR, nullptr);
    const auto fr = filterRect(), orr = matOutRect();                                                  // the big knobs: row y + 92 in every panel
    add ("vCutoff", 1, fr.getX() + 76.0f, fr.getY() + 92.0f, ui::kKnobR, "CUTOFF");     add ("vRes", 0, fr.getX() + 186.0f, fr.getY() + 92.0f, ui::kKnobR, "RESONANCE");
    add ("vXmod", 0, orr.getX() + 63.0f, orr.getY() + 92.0f, ui::kKnobR, "X-MOD");      add ("mix", 0, orr.getX() + 155.0f, orr.getY() + 92.0f, ui::kKnobR, "MIX");
    add ("level", 6, orr.getX() + 247.0f, orr.getY() + 92.0f, ui::kKnobR, "LEVEL");
    layoutKnobs();
    startTimerHz (30);
}

RH1AudioProcessorEditor::~RH1AudioProcessorEditor() = default;

void RH1AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    midiUI.paintMarks (g, kPal.body);
}

void RH1AudioProcessorEditor::timerCallback()
{
    meter.update (proc.outMeter);
    if (const int shown = (mode() * 8 + variant()) * 4 + source(); shown != shownKey) { shownKey = shown; layoutKnobs(); repaint(); }       // a preset, undo, A / B or automation changed the tab
    if (mode() != 3) repaintDesign (panelRect (3).withTrimmedLeft (panelRect (3).getWidth() - 110.0f).expanded (2.0f));
    if (midiUI.needsRepaint()) { midiUI.tick(); repaint(); }
    if (mode() == 3) { repaintDesign (matrixRect().withWidth (140.0f)); repaintDesign (matOutRect().withTrimmedLeft (matOutRect().getWidth() - 150.0f).expanded (2.0f)); }      // the source lamps and the meters
    else repaintDesign (panelRect (2).expanded (4.0f));
    tickPresetBar();
}

void RH1AudioProcessorEditor::setChoice (const char* id, int index) { setValue (id, (float) index, true); }
void RH1AudioProcessorEditor::setValue (const juce::String& id, float v, bool gesture)
{
    auto* par = proc.apvts.getParameter (id);
    if (gesture) par->beginChangeGesture();
    par->setValueNotifyingHost (par->convertTo0to1 (v));
    if (gesture) par->endChangeGesture();
}

bool RH1AudioProcessorEditor::knobShown (int i) const
{
    const auto& k = knobs[(size_t) i];
    if (mode() == 3) return false;                                                                   // the MATRIX layout has its own knobs
    if (k.panel == 0) return (k.onlySource & (1 << source())) != 0;
    if (k.panel == 1) return k.onlyMode == mode();
    return true;
}

void RH1AudioProcessorEditor::layoutKnobs()
{
    const float sc = scale(), d = 2.0f * (ui::kKnobR + 5.0f) * sc;
    for (int i = 0; i < (int) knobs.size(); ++i)
    {
        auto& k = knobs[(size_t) i];
        // the knobs of the source panel are placed by their position among those shown
        int idx = k.index, cnt = k.count;
        if (k.panel == 0) { idx = 0; cnt = 0; for (int j = 0; j < (int) knobs.size(); ++j) if (knobs[(size_t) j].panel == 0 && knobShown (j)) { if (j < i) ++idx; ++cnt; } }
        k.slider.setBounds (juce::Rectangle<float> (d, d).withCentre (knobShown (i) ? knobCentre (k.panel, idx, cnt) * sc : juce::Point<float> (-2000.0f, -2000.0f)).toNearestInt());   // hidden: parked off screen so the MIDI mark is not drawn
        k.slider.setVisible (knobShown (i));
    }
    for (auto& mk : matrixKnobs)
    {
        const bool show = mode() == 3;
        const float d = 2.0f * (mk->r + 5.0f) * sc;
        mk->slider.setBounds (juce::Rectangle<float> (d, d).withCentre (show ? juce::Point<float> (mk->cx, mk->cy) * sc : juce::Point<float> (-2000.0f, -2000.0f)).toNearestInt());
        mk->slider.setVisible (show);
    }
}
void RH1AudioProcessorEditor::resized() { layoutKnobs(); }

//==============================================================================
int RH1AudioProcessorEditor::stepAt (juce::Point<float> p) const
{
    const auto r = laneRect();
    if (! r.expanded (0.0f, 4.0f).contains (p)) return -1;
    return juce::jlimit (0, rh1::kSteps - 1, (int) ((p.x - r.getX()) / (r.getWidth() / (float) rh1::kSteps)));
}

void RH1AudioProcessorEditor::paintStep (juce::Point<float> p)
{
    const int s = stepAt (p);
    if (s < 0) return;
    const auto r = laneRect();
    const float v = juce::jlimit (0.0f, 1.0f, 1.0f - (p.y - r.getY()) / r.getHeight());
    setValue ("step" + juce::String (s + 1), std::round (v * 20.0f) * 5.0f, false);                 // steps of 5 %
    lastPainted = s;
}

void RH1AudioProcessorEditor::drawPattern (juce::Graphics& g)
{
    const auto lane = laneRect(), tr = traceRect();
    const auto& eng = proc.getEngine();
    const bool power = isOn ("power"), slicer = mode() == 2;
    const int cur = power ? eng.currentStep.load (std::memory_order_relaxed) : -1;
    g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (lane.expanded (6.0f), 8.0f);
    const float cw = lane.getWidth() / (float) rh1::kSteps;
    for (int s = 0; s < rh1::kSteps; ++s)
    {
        const float v = value (("step" + juce::String (s + 1)).toRawUTF8()) * 0.01f;
        const auto col = juce::Rectangle<float> (cw - 6.0f, lane.getHeight()).withX (lane.getX() + (float) s * cw + 3.0f).withY (lane.getY());
        g.setColour (kPal.ink.withAlpha (0.1f)); g.fillRoundedRectangle (col, 3.0f);
        const bool on = slicer ? v >= 0.5f : v > 0.0f;
        const float h = std::max (3.0f, col.getHeight() * v);
        g.setColour ((s == cur ? kPal.ink : kPal.body).withAlpha (on ? 1.0f : 0.35f));
        g.fillRoundedRectangle (col.withTop (col.getBottom() - h), 3.0f);
        if (s % 4 == 0) drawText (g, juce::String (s / 4 + 1), col.getCentreX(), lane.getBottom() + 15.0f, 13.0f, kPal.ink.withAlpha (0.7f), false, 0.0f);
    }
    const float noteY = lane.getBottom() + 33.0f;                                                       // the notes sit in their own row between the lane and the trace, not over the plot
    drawText (g, slicer ? "A STEP AT OR ABOVE 50 % IS ON" : "STEP HEIGHT = VALUE (STEPS SOURCE)", lane.getX(), noteY, 13.0f, kPal.ink.withAlpha (0.7f), true, 0.03f, juce::Justification::centredLeft);

    g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (tr.expanded (6.0f), 8.0f);
    juce::Path ctl, dest;
    const int pos = eng.histPos.load (std::memory_order_acquire), N = rh1::kHistory;
    for (int i = 0; i < N; ++i)
    {
        const int idx = (pos + i) % N;
        const float x = tr.getX() + tr.getWidth() * (float) i / (float) (N - 1);
        const float c = power ? eng.histCtl[idx].load (std::memory_order_relaxed) : 0.0f, d = power ? eng.histDest[idx].load (std::memory_order_relaxed) : 0.0f;
        const juce::Point<float> a (x, tr.getBottom() - tr.getHeight() * juce::jlimit (0.0f, 1.0f, c)), b (x, tr.getBottom() - tr.getHeight() * juce::jlimit (0.0f, 1.0f, d));
        if (i == 0) { ctl.startNewSubPath (a); dest.startNewSubPath (b); } else { ctl.lineTo (a); dest.lineTo (b); }
    }
    g.setColour (kPal.ink.withAlpha (0.5f)); g.strokePath (ctl, juce::PathStrokeType (1.3f));
    g.setColour (kPal.body); g.strokePath (dest, juce::PathStrokeType (2.0f));
    static const char* destName[3] = { "CUTOFF", "LEVEL", "SLICE" };
    drawText (g, "CONTROL (GREY) AND " + juce::String (destName[mode()]) + ", 5 SECONDS", lane.getRight(), noteY, 13.0f, kPal.ink.withAlpha (0.7f), true, 0.03f, juce::Justification::centredRight);
}

//==============================================================================
juce::String RH1AudioProcessorEditor::matrixText (int kind, double v) const
{
    const float f = (float) v * 0.01f;
    switch (kind)
    {
        case 1: { const float hz = vc1::cutoffHz (f); return hz >= 1000.0f ? juce::String (hz * 0.001f, 1) + " kHz" : juce::String ((int) std::lround (hz)) + " Hz"; }
        case 2: return isOn ("vSync") ? juce::String (vc1::divisionName (vc1::divisionIndex (f))) : juce::String (vc1::lfoHz (f), 2) + " Hz";
        case 3: return isOn ("vSync") ? juce::String (vc1::divisionName (vc1::divisionIndex (f))) : juce::String (vc1::randomHz (f), 1) + " /s";
        case 4: return juce::String ((int) std::lround (vc1::releaseMs (f))) + " ms";
        case 5: { const int d = (int) std::lround ((v - 50.0) * 2.0); return d == 0 ? juce::String ("0") : juce::String::formatted ("%+d", d); }
        case 6: return juce::String::formatted ("%+d dB", (int) std::lround (v));
        case 7: return juce::String (vc1::sensGain (f), 1) + "x";
        default: return juce::String ((int) std::lround (v)) + " %";
    }
}

// MATRIX (the VC-1 window): the modulation matrix (destination names on top, one row per source with its lamp, its own knobs and the depth knob of each destination),
// the FILTER panel (with the destination tabs on its top edge) and CROSS / OUTPUT.
void RH1AudioProcessorEditor::drawMatrix (juce::Graphics& g)
{
    const auto body = kPal.body, ink = kPal.ink;
    ui::paintPanel (g, kPal, matrixRect(), "MODULATION MATRIX");
    ui::paintPanel (g, kPal, filterRect(), {});
    ui::paintTabs (g, kPal, filterRect(), kModeTabs, 3);
    ui::paintPanel (g, kPal, matOutRect(), "CROSS / OUTPUT");
    ui::paintFlatButton (g, kPal, mSyncRect(), "SYNC", isOn ("vSync"), 14.0f, 1.0f, 9.0f, 0.14f);                      // the LFO and RANDOM rates become note lengths of the host tempo
    for (int d = 0; d < vc1::kNumDests; ++d)
    {
        drawText (g, vc1::destName (d), kColX0 + kColPitch * (float) d, 166.0f, 15.0f, body, true, 0.14f);
        g.setColour (ink.withAlpha (0.12f));
        g.drawLine (kColX0 + kColPitch * (float) d, kRowY0 - kSmallR - 5.0f, kColX0 + kColPitch * (float) d, kMatY + kMatH - 14.0f, 1.0f);
    }
    const auto& eng = proc.getEngine().matrixEngine();
    for (int sI = 0; sI < vc1::kNumSources; ++sI)
    {
        const float y = kRowY0 + kRowPitch * (float) sI;
        const float v = eng.sourceVis[sI].load (std::memory_order_relaxed);
        const float lamp = sI == vc1::Env || sI == vc1::Midi ? v : std::fabs (v);
        g.setColour (body.withAlpha (0.12f + 0.88f * juce::jlimit (0.0f, 1.0f, lamp)));
        g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre ({ kMatX + 32.0f, y }));
        drawText (g, vc1::sourceName (sI), kMatX + 48.0f, y, 15.0f, ink.withAlpha (0.95f), true, 0.1f, juce::Justification::centredLeft);
    }
    drawText (g, "MOD WHEEL / EXPRESSION", 0.5f * (kOwnA + kOwnB), kRowY0 + 4.0f * kRowPitch, 14.0f, ink.withAlpha (0.75f), false, 0.04f);
    for (int lfo = 1; lfo <= 2; ++lfo)
        ui::paintFlatButton (g, kPal, mShapeRect (lfo), vc1::shapeName (choice (lfo == 1 ? "vShape1" : "vShape2")), false, 13.0f, 1.0f, 8.0f, 0.08f, 1.2f);
    for (int t = 0; t < vc1::kNumTypes; ++t)
        ui::paintFlatButton (g, kPal, mTypeRect (t), vc1::typeName (t), t == choice ("vType"), 14.0f, 1.0f, 10.0f, 0.1f, 1.2f);

    for (auto& kp : matrixKnobs)
    {
        MKnob& k = *kp;
        const juce::Point<float> c (k.cx, k.cy);
        const double v = k.slider.getValue();
        const float prop = (float) k.slider.valueToProportionOfLength (v);
        if (k.r >= ui::kKnobR)                                                                       // the big knobs: the shared decoration, name and value rows at y + 62 / y + 90
        {
            juce::String lo = "0", hi = "100";
            if (k.kind == 1) { lo = "60 Hz"; hi = "12 kHz"; }
            else if (k.kind == 6) { lo = "-24"; hi = "+12"; }
            ui::paintKnob (g, kPal, c, k.name, matrixText (k.kind, v), lo, hi, c.y + 62.0f, c.y + 90.0f);
            ui::paintValueArc (g, c, k.r, prop, body);
            continue;
        }
        const auto ring = juce::Rectangle<float> (2.0f * (k.r + 5.0f), 2.0f * (k.r + 5.0f)).withCentre (c);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillEllipse (ring);
        g.setColour (juce::Colours::white.withAlpha (0.1f));
        g.drawEllipse (ring, 1.0f);
        if (k.kind == 5)                                                                             // matrix depth: a mark at the top = 0, the ring runs from there
        {
            const bool zero = (int) std::lround (v) == 50;
            g.setColour (ink.withAlpha (0.55f));
            g.drawLine (c.x, c.y - k.r - 10.0f, c.x, c.y - k.r - 5.0f, 1.6f);
            paintBipolarArc (g, c, k.r, (float) (v - 50.0) / 50.0f, body);
            drawText (g, matrixText (5, v), c.x, c.y + k.r + 25.0f, 13.0f, zero ? ink.withAlpha (0.6f) : body, true, 0.04f);
            continue;
        }
        for (int t = 0; t <= 10; ++t)
        {
            const float a = juce::degreesToRadians (-135.0f + 27.0f * (float) t), sn = std::sin (a), co = std::cos (a);
            const bool major = t == 0 || t == 5 || t == 10;
            const float r0 = k.r + 10.0f, r1 = r0 + (major ? 4.0f : 2.5f);
            g.setColour (ink.withAlpha (0.85f));
            g.drawLine (c.x + sn * r0, c.y - co * r0, c.x + sn * r1, c.y - co * r1, major ? 1.8f : 1.1f);
        }
        ui::paintValueArc (g, c, k.r, prop, body);
        {                                                                                            // name and value on one line, 25 under the centre
            const auto name = juce::String (k.name) + " ";
            const auto val = matrixText (k.kind, v);
            const auto fn = ui::font (13.0f, true).withExtraKerningFactor (0.04f);
            const float wn = ui::textWidth (fn, name), wv = ui::textWidth (fn, val), x0 = c.x - (wn + wv) * 0.5f, ty = c.y + k.r + 25.0f;
            drawText (g, name, x0 + wn * 0.5f, ty, 13.0f, ink.withAlpha (0.85f), true, 0.04f);
            drawText (g, val, x0 + wn + wv * 0.5f, ty, 13.0f, body, false, 0.0f);
        }
    }
    meter.paint (g, matMeterOrigin().x, matMeterOrigin().y, isOn ("power"), body, ink, 110.0f, 26.0f, 13.0f);
    ui::paintFooter (g, kPal, kMatY + kMatH + 35.0f);
}

//==============================================================================
void RH1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    ui::paintBody (g, kPal, kW, kH, grain);
    ui::paintHeader (g, kPal, "RH-1", "RHYTHM LAB", "RHYTHMIC MOTION", isOn ("power"));
    ui::paintPresetBar (g, kPal, proc.presets, proc.undoManager, kBarY, barState, false, 6.0f);

    const int m = mode();
    if (m == 3) { drawMatrix (g); return; }
    ui::paintPanel (g, kPal, panelRect (0), {});
    ui::paintTabs (g, kPal, panelRect (0), kSourceTabs, source());
    ui::paintPanel (g, kPal, panelRect (1), {});
    ui::paintTabs (g, kPal, panelRect (1), kModeTabs, m);
    ui::paintPanel (g, kPal, panelRect (2), "PATTERN");
    ui::paintPanel (g, kPal, panelRect (3), "OUTPUT");
    drawPattern (g);

    ui::paintFlatButton (g, kPal, controlButtonRect (0), "SYNC", isOn ("sync"), 13.0f);
    ui::paintFlatButton (g, kPal, controlButtonRect (1), rh1::divisionName (choice ("division")), isOn ("sync"), 13.0f, isOn ("sync") ? 1.0f : 0.55f);
    ui::paintFlatButton (g, kPal, controlButtonRect (2), "FOLLOW HOST", isOn ("followHost"), 13.0f);
    for (int v = 0; v < rh1::variantCount (m); ++v) ui::paintFlatButton (g, kPal, typeRect (v), rh1::variantName (m, v), v == variant(), 13.0f, 1.0f, 9.0f, 0.02f);
    for (int i = 0; i < rh1::kNumPatterns; ++i)
    {
        bool same = true;
        for (int s = 0; s < rh1::kSteps; ++s) if ((value (("step" + juce::String (s + 1)).toRawUTF8()) >= 50.0f) != (rh1::patternString (i)[s] == 'x')) same = false;
        ui::paintFlatButton (g, kPal, patternButtonRect (i), juce::String (i + 1), same, 13.0f, 1.0f, 7.0f);
    }

    static const char* lo[6] = { "", "0", "sine", "30", "-24", "0" }, *hi[6] = { "", "100", "rnd", "300", "+12", "10" };
    for (int i = 0; i < (int) knobs.size(); ++i)
    {
        auto& k = knobs[(size_t) i];
        if (! knobShown (i)) continue;
        const double v = k.slider.getValue();
        juce::String txt, l = lo[k.kind], h = hi[k.kind];
        switch (k.kind)
        {
            case 0: txt = isOn ("sync") ? juce::String (rh1::divisionName (choice ("division"))) : juce::String (v, 2) + " Hz"; l = ".05"; h = "20"; break;
            case 2: txt = m == 1 ? juce::String (v < 50.0 ? "SINE" : "SQUARE") + " " + juce::String ((int) std::lround (v)) + " %" : juce::String ((int) std::lround (v)) + " %"; l = m == 1 ? "sine" : "sine"; h = m == 1 ? "square" : "random"; break;
            case 3: txt = juce::String (v, 1) + " bpm"; break;
            case 4: txt = juce::String::formatted ("%+.1f dB", v); break;
            case 5: txt = juce::String (v, 1); break;
            default: txt = juce::String ((int) std::lround (v)) + " %"; break;
        }
        int idx = k.index, cnt = k.count;
        if (k.panel == 0) { idx = 0; cnt = 0; for (int j = 0; j < (int) knobs.size(); ++j) if (knobs[(size_t) j].panel == 0 && knobShown (j)) { if (j < i) ++idx; ++cnt; } }
        ui::paintKnob (g, kPal, knobCentre (k.panel, idx, cnt), k.name, txt, l, h, panelRect (k.panel).getY() + 154.0f, panelRect (k.panel).getY() + 182.0f);
        ui::paintValueArc (g, knobCentre (k.panel, idx, cnt), ui::kKnobR, (float) k.slider.valueToProportionOfLength (k.slider.getValue()), kPal.body);
    }
    meter.paint (g, meterOrigin().x, meterOrigin().y, isOn ("power"), kPal.body, kPal.ink, 110.0f, 26.0f, 13.0f);
    ui::paintFooter (g, kPal, kRow2Y + kRow2H + 35.0f);
}

void RH1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / scale();
    proc.undoManager.beginNewTransaction();
    if (ui::powerRect().contains (p)) { toggle ("power"); return; }
    if (ui::presetBarMouseDown (p, *this, proc.presets, proc.undoManager, kBarY, scale(), barState, [this] { repaint(); }, false)) { layoutKnobs(); return; }
    if (mode() != 3)
    {
        const auto st = ui::tabRects (panelRect (0), kSourceTabs);
        for (int i = 0; i < (int) st.size(); ++i)
            if (st[(size_t) i].expanded (3.0f).contains (p)) { setChoice ("source", i); layoutKnobs(); repaint(); return; }
    }
    const auto mt = ui::tabRects (tabsRect (mode()), kModeTabs);
    for (int i = 0; i < (int) mt.size(); ++i)
        if (mt[(size_t) i].expanded (3.0f).contains (p)) { setChoice ("mode", i); setChoice ("variant", 0); layoutKnobs(); repaint(); return; }
    if (mode() == 3)                                                                                // the MATRIX layout: SYNC, the filter type, the shape of the two LFOs
    {
        if (mSyncRect().expanded (3.0f).contains (p)) { toggle ("vSync"); repaint(); return; }
        for (int t = 0; t < vc1::kNumTypes; ++t)
            if (mTypeRect (t).expanded (3.0f).contains (p)) { setChoice ("vType", t); repaint(); return; }
        for (int lfo = 1; lfo <= 2; ++lfo)
            if (mShapeRect (lfo).expanded (3.0f).contains (p)) { const char* id = lfo == 1 ? "vShape1" : "vShape2"; setChoice (id, (choice (id) + 1) % vc1::kNumShapes); repaint(); return; }
        return;
    }
    for (int v = 0; v < rh1::variantCount (mode()); ++v)
        if (typeRect (v).expanded (3.0f).contains (p)) { setChoice ("variant", v); repaint(); return; }
    if (controlButtonRect (0).expanded (3.0f).contains (p)) { toggle ("sync"); repaint(); return; }
    if (controlButtonRect (1).expanded (3.0f).contains (p)) { setChoice ("division", (choice ("division") + (e.mods.isShiftDown() ? rh1::kNumDivisions - 1 : 1)) % rh1::kNumDivisions); if (! isOn ("sync")) toggle ("sync"); repaint(); return; }
    if (controlButtonRect (2).expanded (3.0f).contains (p)) { toggle ("followHost"); repaint(); return; }
    for (int i = 0; i < rh1::kNumPatterns; ++i)
        if (patternButtonRect (i).expanded (3.0f).contains (p))
        {
            for (int s = 0; s < rh1::kSteps; ++s) setValue ("step" + juce::String (s + 1), rh1::patternString (i)[s] == 'x' ? 100.0f : 0.0f, true);
            repaint(); return;
        }
    if (stepAt (p) >= 0)                                                                            // click or drag paints the lane
    {
        painting = true;
        paintStep (p);
        repaint();
    }
}

void RH1AudioProcessorEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (! painting) return;
    paintStep (e.position / scale());
    repaint();
}

void RH1AudioProcessorEditor::mouseUp (const juce::MouseEvent&) { painting = false; }
