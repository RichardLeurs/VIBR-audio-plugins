// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "ui/Presets.h"

namespace
{
namespace ui = vibr::ui::vintage;          // the default look (src/common/ui/VintageUI.h); the old grey look is vibr::ui (VibrUI.h)
using ui::drawText;

constexpr float kW = ui::kW, kH = ui::kH, kRow1Y = 114.0f, kRow1H = 244.0f, kRow2Y = 376.0f, kRow2H = 254.0f;
const ui::Palette kPal { juce::Colour (50, 196, 201) };            // the Modulation colour #32C4C9: value rings, full steps and the plugin code

// panels: 0 SOURCE (source tabs), 1 TARGET (destination tabs), 2 PATTERN (steps, scope, slice patterns), 3 OUTPUT
constexpr float kPanelX[4] = { 28.0f, 606.0f, 28.0f, 749.0f }, kPanelW[4] = { 560.0f, 486.0f, 703.0f, 343.0f };
juce::Rectangle<float> panelRect (int i) { return { kPanelX[i], i < 2 ? kRow1Y : kRow2Y, kPanelW[i], i < 2 ? kRow1H : kRow2H }; }
juce::Point<float> knobCentre (int panel, int k, int n)
{
    const auto r = panelRect (panel);
    if (panel == 3) return { r.getX() + 66.0f + 106.0f * (float) k, r.getY() + 112.0f };         // MIX and LEVEL left of the meters
    if (panel == 0)                                                                              // five cells of 92 spread over the panel
    {
        const float x0 = r.getX() + 20.0f + 48.0f, x1 = r.getRight() - 20.0f - 48.0f;
        return { n > 1 ? x0 + (x1 - x0) * (float) k / (float) (n - 1) : r.getCentreX(), r.getY() + 86.0f };
    }
    return { r.getCentreX() + 108.0f * ((float) k - 0.5f * (float) (n - 1)), r.getY() + 86.0f };
}
juce::Point<float> meterOrigin() { const auto r = panelRect (3); return { r.getRight() - 20.0f - ui::kMeterW, r.getY() + (r.getHeight() - ui::kMeterH) * 0.5f }; }
// the bottom row of SOURCE: SYNC, the note length, FOLLOW HOST
juce::Rectangle<float> controlButtonRect (int i)
{
    static const float x[3] = { 20.0f, 116.0f, 232.0f }, w[3] = { 90.0f, 106.0f, 180.0f };
    const auto r = panelRect (0);
    return { r.getX() + x[i], r.getY() + 200.0f, w[i], 30.0f };
}
// the bottom row of TARGET: the types of the destination, five of equal width
juce::Rectangle<float> typeRect (int i)
{
    const auto r = panelRect (1); const float w = (r.getWidth() - 40.0f - 24.0f) / 5.0f;
    return { r.getX() + 20.0f + (float) i * (w + 6.0f), r.getY() + 200.0f, w, 30.0f };
}
// the PATTERN panel: the step lane, the scope, the slice patterns (the label SLICE takes the first 56)
juce::Rectangle<float> laneRect() { const auto r = panelRect (2); return { r.getX() + 20.0f, r.getY() + 24.0f, r.getWidth() - 40.0f, 54.0f }; }
juce::Rectangle<float> scopeRect() { const auto r = panelRect (2); return { r.getX() + 20.0f, r.getY() + 98.0f, r.getWidth() - 40.0f, 100.0f }; }
juce::Rectangle<float> patternButtonRect (int i)
{
    const auto r = panelRect (2); const float pitch = ((r.getWidth() - 40.0f - 56.0f) + 6.0f) / (float) rh1::kNumPatterns;       // the eleven buttons share the rest of the panel
    return { r.getX() + 20.0f + 56.0f + (float) i * pitch, r.getY() + 210.0f, pitch - 6.0f, 30.0f };
}
const std::vector<juce::String> kSourceTabs { "LFO", "ENVELOPE", "STEPS" }, kModeTabs { "FILTER", "TREMOLO", "SLICER", "MATRIX" };

// the destination MATRIX (the VC-1 layout of the RH-1, in "matrix space"): the matrix with its source rows, FILTER (with the destination tabs) and CROSS / OUTPUT.
// matrixXform() scales that space into the content area of the window (from x 28 / y 114, the top edge of the SOURCE panel, 1064 wide); every rectangle below is in matrix space.
constexpr float kMatK = 560.0f / 700.0f;                                         // matrix space -> window: the matrix (700 wide) gets the width of the SOURCE panel (560)
constexpr float kMatX = 20.0f, kMatY = 134.0f, kMatW = 700.0f, kMatH = 516.0f / kMatK;       // the height of the other layout (y 114 .. 630) in matrix space
// the TARGET panel is as wide and in the same place as in the other layouts (486 at x 606), so the tabs do not move it; CROSS / OUTPUT has the same width and x
constexpr float kRightX = kMatX + (606.0f - 28.0f) / kMatK, kRightW = 486.0f / kMatK, kOutX = kRightX, kOutW = kRightW;
constexpr float kFilterH = kRow1H / kMatK, kOutY = kMatY + (kRow1H + 18.0f) / kMatK, kOutH = kRow2H / kMatK;      // the same heights and places as TARGET and OUTPUT of the other layouts
constexpr float kRowY0 = 216.0f, kRowPitch = 122.0f, kColX0 = 366.0f, kColPitch = 98.0f, kOwnA = 200.0f, kOwnB = 306.0f, kSmallR = 20.0f, kBigR = 30.0f / kMatK;      // the big knobs are the size of the knobs of the other layouts (radius 30 in the window)
juce::AffineTransform matrixXform()
{
    return juce::AffineTransform::translation (-kMatX, -kMatY).scaled (kMatK).translated (28.0f, kRow1Y);
}
juce::Rectangle<float> matrixRect() { return { kMatX, kMatY, kMatW, kMatH }; }
juce::Rectangle<float> filterRect() { return { kRightX, kMatY, kRightW, kFilterH }; }
juce::Rectangle<float> matOutRect() { return { kOutX, kOutY, kOutW, kOutH }; }
juce::Rectangle<float> mTypeRect (int m) { const auto r = filterRect(); return { r.getRight() - 144.0f, r.getY() + 63.0f + (float) m * 46.0f, 124.0f, 36.0f }; }
juce::Rectangle<float> mShapeRect (int lfo) { return juce::Rectangle<float> (76.0f, 28.0f).withCentre ({ kOwnB - 16.0f, kRowY0 + kRowPitch * (float) lfo }); }
juce::Rectangle<float> mSyncRect() { return juce::Rectangle<float> (88.0f, 26.0f).withCentre ({ 91.0f, 164.0f }); }
const char* const kMatrixIds[vc1::kNumSources][vc1::kNumDests] = { { "vm00", "vm01", "vm02", "vm03" }, { "vm10", "vm11", "vm12", "vm13" }, { "vm20", "vm21", "vm22", "vm23" },
                                                                  { "vm30", "vm31", "vm32", "vm33" }, { "vm40", "vm41", "vm42", "vm43" } };
// the tab buttons sit on the top edge of SOURCE and TARGET (in the MATRIX layout on its TARGET panel), right of the panel name, centred on the edge line
constexpr float kTabsTop = -15.0f, kTabsFont = 16.0f;
const float kSourceTabsLeft = ui::titleEnd ("SOURCE"), kTargetTabsLeft = ui::titleEnd ("TARGET");
std::vector<juce::Rectangle<float>> sourceTabRects() { return ui::tabRects (panelRect (0), kSourceTabs, kTabsTop, kTabsFont, kSourceTabsLeft); }
std::vector<juce::Rectangle<float>> modeTabRects (int) { return ui::tabRects (panelRect (1), kModeTabs, kTabsTop, kTabsFont, kTargetTabsLeft); }
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
    const auto fr = filterRect(), orr = matOutRect();                                                  // the big knobs, centred in their panel
    const float fcx = fr.getX() + (fr.getWidth() - 144.0f) * 0.5f, ocx = orr.getX() + (orr.getWidth() - (ui::kMeterW + 20.0f) / kMatK) * 0.5f;       // left of the type buttons / of the meter
    add ("vCutoff", 1, fcx - 54.0f / kMatK, fr.getY() + 86.0f / kMatK, kBigR, "CUTOFF");     add ("vRes", 0, fcx + 54.0f / kMatK, fr.getY() + 86.0f / kMatK, kBigR, "RESONANCE");
    add ("vXmod", 0, ocx - 106.0f / kMatK, orr.getY() + 112.0f / kMatK, kBigR, "X-MOD");       add ("mix", 0, ocx, orr.getY() + 112.0f / kMatK, kBigR, "MIX");
    add ("level", 6, ocx + 106.0f / kMatK, orr.getY() + 112.0f / kMatK, kBigR, "LEVEL");
    layoutKnobs();
    startTimerHz (30);
}

RH1AudioProcessorEditor::~RH1AudioProcessorEditor() { stopTimer(); }

void RH1AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    midiUI.paintMarks (g, kPal.accent);
}

void RH1AudioProcessorEditor::timerCallback()
{
    meter.update (proc.outMeter);
    if (const int shown = (mode() * 8 + variant()) * 4 + source(); shown != shownKey) { shownKey = shown; layoutKnobs(); repaint(); }       // a preset, undo, A / B or automation changed the tab
    if (midiUI.needsRepaint()) { midiUI.tick(); repaint(); }
    if (mode() == 3)                                                                                // the source lamps and the meters
    {
        const auto xf = matrixXform();
        repaintDesign (matrixRect().withWidth (140.0f).transformedBy (xf).expanded (2.0f));
        repaintDesign (juce::Rectangle<float> (meterOrigin().x, meterOrigin().y, ui::kMeterW, ui::kMeterH).expanded (2.0f));
    }
    else
    {
        repaintDesign (panelRect (2).expanded (4.0f));
        repaintDesign (juce::Rectangle<float> (meterOrigin().x, meterOrigin().y, ui::kMeterW, ui::kMeterH).expanded (2.0f));
    }
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
    const float sc = scale(), d = 2.0f * (ui::kKnobR + ui::kKnobPad) * sc;
    for (int i = 0; i < (int) knobs.size(); ++i)
    {
        auto& k = knobs[(size_t) i];
        // the knobs of the source panel are placed by their position among those shown
        int idx = k.index, cnt = k.count;
        if (k.panel == 0) { idx = 0; cnt = 0; for (int j = 0; j < (int) knobs.size(); ++j) if (knobs[(size_t) j].panel == 0 && knobShown (j)) { if (j < i) ++idx; ++cnt; } }
        k.slider.setBounds (juce::Rectangle<float> (d, d).withCentre (knobShown (i) ? knobCentre (k.panel, idx, cnt) * sc : juce::Point<float> (-2000.0f, -2000.0f)).toNearestInt());   // hidden: parked off screen so the MIDI mark is not drawn
        k.slider.setVisible (knobShown (i));
    }
    const auto xf = matrixXform();
    for (auto& mk : matrixKnobs)
    {
        const bool show = mode() == 3;
        const float dm = 2.0f * (mk->r + ui::kKnobPad) * kMatK * sc;
        mk->slider.setBounds (juce::Rectangle<float> (dm, dm).withCentre (show ? juce::Point<float> (mk->cx, mk->cy).transformedBy (xf) * sc : juce::Point<float> (-2000.0f, -2000.0f)).toNearestInt());
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
    const auto lane = laneRect(), scope = scopeRect();
    const auto& eng = proc.getEngine();
    const bool power = isOn ("power"), slicer = mode() == 2;
    const int cur = power ? eng.currentStep.load (std::memory_order_relaxed) : -1;
    float steps[rh1::kSteps];
    for (int s = 0; s < rh1::kSteps; ++s) steps[s] = value (("step" + juce::String (s + 1)).toRawUTF8()) * 0.01f;
    ui::paintStepLane (g, kPal, lane, steps, rh1::kSteps, cur, slicer);

    static const char* srcName[3] = { "LFO", "ENVELOPE", "STEPS" }, *destName[3] = { "CUTOFF", "LEVEL", "SLICE" };
    ui::paintScope (g, kPal, scope, juce::String (srcName[source()]) + " > " + destName[mode()], slicer ? "STEP >= 50 % IS ON, DIM = CONTROL" : "DIM = CONTROL, 5 SECONDS");
    const auto plot = ui::scopePlotRect (scope);
    juce::Path ctl, dest;
    const int pos = eng.histPos.load (std::memory_order_acquire), N = rh1::kHistory;
    for (int i = 0; i < N; ++i)
    {
        const int idx = (pos + i) % N;
        const float x = plot.getX() + plot.getWidth() * (float) i / (float) (N - 1);
        const float c = power ? eng.histCtl[idx].load (std::memory_order_relaxed) : 0.0f, d = power ? eng.histDest[idx].load (std::memory_order_relaxed) : 0.0f;
        const juce::Point<float> a (x, plot.getBottom() - plot.getHeight() * juce::jlimit (0.0f, 1.0f, c)), b (x, plot.getBottom() - plot.getHeight() * juce::jlimit (0.0f, 1.0f, d));
        if (i == 0) { ctl.startNewSubPath (a); dest.startNewSubPath (b); } else { ctl.lineTo (a); dest.lineTo (b); }
    }
    g.saveState();
    g.reduceClipRegion (scope.reduced (3.0f).toNearestInt());
    g.setColour (kPal.phosphorDim.withAlpha (0.8f)); g.strokePath (ctl, juce::PathStrokeType (1.3f));
    g.setColour (kPal.phosphor.withAlpha (0.18f)); g.strokePath (dest, juce::PathStrokeType (5.0f));      // glow
    g.setColour (kPal.phosphor); g.strokePath (dest, juce::PathStrokeType (2.0f));
    g.restoreState();
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

// MATRIX (the VC-1 layout): the modulation matrix (destination names on top, one row per source with its lamp, its own knobs and the depth knob of each destination),
// the FILTER panel (with the destination tabs on its top edge) and CROSS / OUTPUT. Drawn in matrix space (the caller has applied matrixXform()).
void RH1AudioProcessorEditor::drawMatrix (juce::Graphics& g)
{
    const auto body = kPal.accent, ink = kPal.ink;
    ui::paintFlatButton (g, kPal, mSyncRect(), "SYNC", isOn ("vSync"), 18.0f, 1.0f, 4.0f, 0.1f);                      // the LFO and RANDOM rates become note lengths of the host tempo
    for (int d = 0; d < vc1::kNumDests; ++d)
    {
        drawText (g, vc1::destName (d), kColX0 + kColPitch * (float) d, 164.0f, 19.0f, ink, true, 0.1f);
        g.setColour (ink.withAlpha (0.14f));
        g.drawLine (kColX0 + kColPitch * (float) d, kRowY0 - kSmallR - 5.0f, kColX0 + kColPitch * (float) d, kMatY + kMatH - 14.0f, 1.0f);
    }
    const auto& eng = proc.getEngine().matrixEngine();
    for (int sI = 0; sI < vc1::kNumSources; ++sI)
    {
        const float y = kRowY0 + kRowPitch * (float) sI;
        const float v = eng.sourceVis[sI].load (std::memory_order_relaxed);
        const float lamp = sI == vc1::Env || sI == vc1::Midi ? v : std::fabs (v);
        g.setColour (kPal.track);
        g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ kMatX + 32.0f, y }));
        g.setColour (body.withAlpha (juce::jlimit (0.0f, 1.0f, lamp)));
        g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ kMatX + 32.0f, y }));
        drawText (g, vc1::sourceName (sI), kMatX + 48.0f, y, 19.0f, ink, true, 0.06f, juce::Justification::centredLeft);
    }
    drawText (g, "MOD WHEEL / EXPRESSION", 0.5f * (kOwnA + kOwnB), kRowY0 + 4.0f * kRowPitch, 17.0f, kPal.inkSoft, false, 0.04f);
    for (int lfo = 1; lfo <= 2; ++lfo)
        ui::paintFlatButton (g, kPal, mShapeRect (lfo), vc1::shapeName (choice (lfo == 1 ? "vShape1" : "vShape2")), false, 15.0f, 1.0f, 4.0f, 0.06f);
    for (int t = 0; t < vc1::kNumTypes; ++t)
        ui::paintFlatButton (g, kPal, mTypeRect (t), vc1::typeName (t), t == choice ("vType"), 18.0f, 1.0f, 4.0f, 0.06f);

    for (auto& kp : matrixKnobs)
    {
        MKnob& k = *kp;
        const juce::Point<float> c (k.cx, k.cy);
        const double v = k.slider.getValue();
        const float prop = (float) k.slider.valueToProportionOfLength (v);
        if (k.r >= kBigR) continue;                                                                  // the big knobs are drawn in window units (drawBigMatrixKnobs)
        if (k.kind == 5)                                                                             // matrix depth: a mark at the top = 0, the ring runs from there
        {
            const bool zero = (int) std::lround (v) == 50;
            g.setColour (kPal.inkSoft);
            g.drawLine (c.x, c.y - ui::ringRadius (k.r) - 6.0f, c.x, c.y - ui::ringRadius (k.r) - 1.0f, 1.6f);
            ui::paintBipolarArc (g, c, k.r, (float) (v - 50.0) / 50.0f, body, kPal.track);
            drawText (g, matrixText (5, v), c.x, c.y + k.r + 27.0f, 15.0f, zero ? kPal.inkSoft : body, true, 0.04f, juce::Justification::centred, ui::Face::Mono);
            continue;
        }
        ui::paintSmallKnobTicks (g, kPal, c, k.r);
        ui::paintValueArc (g, c, k.r, prop, body, kPal.track);
        {                                                                                            // name and value on one line, 25 under the centre
            const juce::String name (k.name), val = matrixText (k.kind, v);
            const auto fn = ui::font (15.0f, true).withExtraKerningFactor (0.04f), fv = ui::font (15.0f, true, ui::Face::Mono);
            const float gap = 7.0f, wn = ui::textWidth (fn, name), wv = ui::textWidth (fv, val), x0 = c.x - (wn + gap + wv) * 0.5f, ty = c.y + k.r + 27.0f;
            drawText (g, name, x0 + wn * 0.5f, ty, 15.0f, ink, true, 0.04f);
            drawText (g, val, x0 + wn + gap + wv * 0.5f, ty, 15.0f, body, true, 0.0f, juce::Justification::centred, ui::Face::Mono);
        }
    }
}

void RH1AudioProcessorEditor::drawBigMatrixKnobs (juce::Graphics& g)           // CUTOFF, RESONANCE, X-MOD, MIX, LEVEL and the meter: as big as in the other layouts
{
    const auto xf = matrixXform();
    for (auto& kp : matrixKnobs)
    {
        MKnob& k = *kp;
        if (k.r < kBigR) continue;
        juce::String lo = "0", hi = "100";
        if (k.kind == 1) { lo = "60 Hz"; hi = "12 kHz"; }
        else if (k.kind == 6) { lo = "-24"; hi = "+12"; }
        const double v = k.slider.getValue();
        ui::paintKnob (g, kPal, juce::Point<float> (k.cx, k.cy).transformedBy (xf), k.name, matrixText (k.kind, v), lo, hi, (float) k.slider.valueToProportionOfLength (v), k.r * kMatK);
    }
    ui::paintMeter (g, kPal, meter, meterOrigin().x, meterOrigin().y, isOn ("power"));      // the same meter, in the same place as in OUTPUT
}

//==============================================================================
void RH1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    ui::paintBody (g, kPal, kW, kH);
    ui::paintHeader (g, kPal, "RH-1", "RHYTHM LAB", "RHYTHMIC MOTION");
    ui::paintPresetBar (g, kPal, proc.presets, proc.undoManager, isOn ("power"));
    ui::paintFooter (g, kPal, kH - 15.0f);

    const int m = mode();
    if (m == 3)
    {
        g.saveState();
        g.addTransform (matrixXform());
        drawMatrix (g);
        g.restoreState();
        const auto xf = matrixXform();                                                                  // the panels in window units: the same type size as the other layouts
        ui::paintPanel (g, kPal, matrixRect().transformedBy (xf), "MODULATION MATRIX");
        ui::paintPanel (g, kPal, panelRect (1), "TARGET");
        ui::paintTabs (g, kPal, panelRect (1), kModeTabs, 3, kTabsTop, kTabsFont, kTargetTabsLeft);
        ui::paintPanel (g, kPal, matOutRect().transformedBy (xf), "CROSS / OUTPUT");
        drawBigMatrixKnobs (g);
        return;
    }
    ui::paintPanel (g, kPal, panelRect (0), "SOURCE");
    ui::paintTabs (g, kPal, panelRect (0), kSourceTabs, source(), kTabsTop, kTabsFont, kSourceTabsLeft);
    ui::paintPanel (g, kPal, panelRect (1), "TARGET");
    ui::paintTabs (g, kPal, panelRect (1), kModeTabs, m, kTabsTop, kTabsFont, kTargetTabsLeft);
    ui::paintPanel (g, kPal, panelRect (2), "PATTERN");
    ui::paintPanel (g, kPal, panelRect (3), "OUTPUT");
    drawPattern (g);

    ui::paintFlatButton (g, kPal, controlButtonRect (0), "SYNC", isOn ("sync"), 17.0f);
    {                                                                                                   // the note length: a phosphor display that also switches SYNC on
        const auto r = controlButtonRect (1);
        ui::paintScreen (g, kPal, r, 4.0f);
        drawText (g, rh1::divisionName (choice ("division")), r.getCentreX(), r.getCentreY(), 20.0f, isOn ("sync") ? kPal.phosphor : kPal.phosphorDim.withAlpha (0.6f), true, 0.0f, juce::Justification::centred, ui::Face::Mono);
    }
    ui::paintFlatButton (g, kPal, controlButtonRect (2), "FOLLOW HOST", isOn ("followHost"), 17.0f);
    for (int v = 0; v < rh1::variantCount (m); ++v) ui::paintFlatButton (g, kPal, typeRect (v), rh1::variantName (m, v), v == variant(), 16.0f, 1.0f, 4.0f, 0.0f);
    drawText (g, "SLICE", panelRect (2).getX() + 20.0f, patternButtonRect (0).getCentreY(), 19.0f, kPal.ink, true, 0.12f, juce::Justification::centredLeft);
    for (int i = 0; i < rh1::kNumPatterns; ++i)
    {
        bool same = true;
        for (int s = 0; s < rh1::kSteps; ++s) if ((value (("step" + juce::String (s + 1)).toRawUTF8()) >= 50.0f) != (rh1::patternString (i)[s] == 'x')) same = false;
        ui::paintFlatButton (g, kPal, patternButtonRect (i), juce::String (i + 1), same, 19.0f, 1.0f, 4.0f, 0.0f);
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
        ui::paintKnob (g, kPal, knobCentre (k.panel, idx, cnt), k.name, txt, l, h, (float) k.slider.valueToProportionOfLength (k.slider.getValue()));
    }
    ui::paintMeter (g, kPal, meter, meterOrigin().x, meterOrigin().y, isOn ("power"));
}

void RH1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / scale();
    proc.undoManager.beginNewTransaction();
    if (ui::powerRect().contains (p)) { toggle ("power"); return; }
    if (ui::presetBarMouseDown (p, *this, proc.presets, proc.undoManager, scale(), [this] { repaint(); })) { layoutKnobs(); return; }
    if (mode() == 3)                                                                                // the MATRIX layout: the tabs, SYNC, the filter type, the shape of the two LFOs (in matrix space)
    {
        const auto pm = p.transformedBy (matrixXform().inverted());
        const auto mt = modeTabRects (3);
        for (int i = 0; i < (int) mt.size(); ++i)
            if (mt[(size_t) i].expanded (3.0f).contains (p)) { setChoice ("mode", i); setChoice ("variant", 0); layoutKnobs(); repaint(); return; }
        if (mSyncRect().expanded (3.0f).contains (pm)) { toggle ("vSync"); repaint(); return; }
        for (int t = 0; t < vc1::kNumTypes; ++t)
            if (mTypeRect (t).expanded (3.0f).contains (pm)) { setChoice ("vType", t); repaint(); return; }
        for (int lfo = 1; lfo <= 2; ++lfo)
            if (mShapeRect (lfo).expanded (3.0f).contains (pm)) { const char* id = lfo == 1 ? "vShape1" : "vShape2"; setChoice (id, (choice (id) + 1) % vc1::kNumShapes); repaint(); return; }
        return;
    }
    const auto st = sourceTabRects();
    for (int i = 0; i < (int) st.size(); ++i)
        if (st[(size_t) i].expanded (3.0f).contains (p)) { setChoice ("source", i); layoutKnobs(); repaint(); return; }
    const auto mt = modeTabRects (mode());
    for (int i = 0; i < (int) mt.size(); ++i)
        if (mt[(size_t) i].expanded (3.0f).contains (p)) { setChoice ("mode", i); setChoice ("variant", 0); layoutKnobs(); repaint(); return; }
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
