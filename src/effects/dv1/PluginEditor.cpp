// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "ui/Presets.h"

namespace
{
using vibr::ui::drawText;
namespace ui = vibr::ui;

constexpr float kW = 1200.0f, kH = 690.0f, kRow1Y = 134.0f, kRow1H = 247.0f, kRow2Y = 405.0f, kRow2H = 216.0f, kBarY = 39.0f;
const ui::Palette kPal { juce::Colour (255, 152, 74), juce::Colour (46, 27, 13) };            // the Drive colour #FF984A and its print

// panels: 0 CONTROLS (with the mode tabs), 1 OUTPUT, 2 SHAPE (display)
constexpr float kPanelX[3] = { 20.0f, 730.0f, 20.0f }, kPanelW[3] = { 700.0f, 450.0f, 1160.0f };
juce::Rectangle<float> panelRect (int i) { return { kPanelX[i], i < 2 ? kRow1Y : kRow2Y, kPanelW[i], i < 2 ? kRow1H : kRow2H }; }
juce::Point<float> knobCentre (int panel, int k, int n)
{
    const auto r = panelRect (panel);
    const float pitch = panel == 0 && n >= 8 ? 84.0f : panel == 0 && n >= 7 ? 94.0f : 108.0f;
    const float cx = panel == 1 ? r.getX() + 175.0f : r.getCentreX();          // the output knobs left of the dB meters, the pair centred in the panel
    return { cx + pitch * ((float) k - 0.5f * (float) (n - 1)), r.getY() + 92.0f };
}
juce::Point<float> meterOrigin() { const auto r = panelRect (1); return { r.getX() + 360.0f, r.getY() + 46.0f }; }       // 110 high: the bars end on the knob name row (y + 156), the peak values sit on the readout row (y + 182)
juce::Rectangle<float> curveRect() { const auto r = panelRect (2); const float s = r.getHeight() - 94.0f; return { r.getX() + 70.0f, r.getY() + 32.0f, s, s }; }
juce::Rectangle<float> waveRect() { const auto r = panelRect (2), c = curveRect(); return { c.getRight() + 60.0f, c.getY(), r.getRight() - c.getRight() - 90.0f, c.getHeight() }; }
juce::Rectangle<float> buttonRect (int i) { const auto r = panelRect (0); return { r.getX() + 30.0f + (float) i * 130.0f, r.getY() + 204.0f, 120.0f, 30.0f }; }
juce::Rectangle<float> odButtonRect() { return buttonRect (0).withWidth (380.0f); }
juce::Rectangle<float> outButtonRect (int i) { const auto r = panelRect (1); return { r.getX() + 25.0f + (float) i * 210.0f, r.getY() + 204.0f, 190.0f, 30.0f }; }
const std::vector<juce::String> kModeTabs { "OVERDRIVE", "DISTORTION", "TUBE", "TAPE", "FOLD", "CRUSH", "CHAIN", "HARMONICS" };
}

// kinds: 0 percent, 2 boost dB, 3 value 0 .. 10, 4 bits, 5 rate, 6 level dB
//==============================================================================
DV1AudioProcessorEditor::DV1AudioProcessorEditor (DV1AudioProcessor& p)
    : LabEditor (p, kW, kH), proc (p),
      knobs { { { "drive", 0, 0, 7, "DRIVE", 0, 0, 1, {}, {} },          { "odParam", 0, 1, 7, "PARAM", 3, 0, 1, {}, {} },       { "odLow", 0, 2, 7, "LOW", 3, 0, 1, {}, {} },
                { "odMid", 0, 3, 7, "MIDDLE", 3, 0, 1, {}, {} },         { "odHigh", 0, 4, 7, "HIGH", 3, 0, 1, {}, {} },         { "gateThresh", 0, 5, 7, "GATE", 3, 0, 1, {}, {} },
                { "gateDecay", 0, 6, 7, "DECAY", 3, 0, 1, {}, {} },
                { "drive", 0, 0, 2, "DIST", 0, 1, 1, {}, {} },           { "tone", 0, 1, 2, "TONE", 0, 1, 1, {}, {} },
                { "drive", 0, 0, 3, "DIST", 0, 1, 2, {}, {} },           { "tone", 0, 1, 3, "TONE", 0, 1, 2, {}, {} },           { "tight", 0, 2, 3, "TIGHT", 0, 1, 2, {}, {} },
                { "drive", 0, 0, 3, "DRIVE", 0, 2, 1, {}, {} },          { "tone", 0, 1, 3, "TONE", 0, 2, 1, {}, {} },           { "tubeBias", 0, 2, 3, "BIAS", 0, 2, 1, {}, {} },
                { "drive", 0, 0, 2, "DRIVE", 0, 2, 2, {}, {} },          { "tone", 0, 1, 2, "TONE", 0, 2, 2, {}, {} },
                { "drive", 0, 0, 3, "DRIVE", 0, 3, 3, {}, {} },          { "tone", 0, 1, 3, "TONE", 0, 3, 3, {}, {} },           { "warmth", 0, 2, 3, "WARMTH", 0, 3, 3, {}, {} },
                { "drive", 0, 0, 4, "DRIVE", 0, 4, 3, {}, {} },          { "tone", 0, 1, 4, "TONE", 0, 4, 3, {}, {} },           { "fold", 0, 2, 4, "FOLD", 0, 4, 3, {}, {} },
                { "symmetry", 0, 3, 4, "SYMMETRY", 0, 4, 3, {}, {} },
                { "drive", 0, 0, 4, "GAIN", 0, 5, 3, {}, {} },           { "bits", 0, 1, 4, "BITS", 4, 5, 3, {}, {} },           { "rate", 0, 2, 4, "RATE", 5, 5, 3, {}, {} },
                { "crushMix", 0, 3, 4, "CRUSH MIX", 0, 5, 3, {}, {} },
                { "boostTone", 0, 0, 8, "TONE", 0, 6, 7, {}, {} },  { "tubeDrive", 0, 1, 8, "TUBE DRIVE", 0, 6, 7, {}, {} },    { "tubeBias", 0, 2, 8, "BIAS", 0, 6, 7, {}, {} },
                { "tapeDrive", 0, 3, 8, "TAPE DRIVE", 0, 6, 7, {}, {} }, { "warmth", 0, 4, 8, "WARMTH", 0, 6, 7, {}, {} },       { "bits", 0, 5, 8, "BITS", 4, 6, 7, {}, {} },
                { "rate", 0, 6, 8, "RATE", 5, 6, 7, {}, {} },             { "crushMix", 0, 7, 8, "CRUSH MIX", 0, 6, 7, {}, {} },
                { "drive", 0, 0, 7, "DRIVE", 0, 7, 7, {}, {} },            { "oddeven", 0, 1, 7, "ODD / EVEN", 0, 7, 7, {}, {} },  { "octave", 0, 2, 7, "OCTAVE UP", 0, 7, 7, {}, {} },
                { "sub", 0, 3, 7, "SUB OCTAVE", 0, 7, 7, {}, {} },        { "focus", 0, 4, 7, "FOCUS", 0, 7, 7, {}, {} },          { "tilt", 0, 5, 7, "TILT", 0, 7, 7, {}, {} },
                { "tone", 0, 6, 7, "TONE", 0, 7, 7, {}, {} },
                { "boost", 1, 0, 3, "BOOST", 2, -1, 3, {}, {} },         { "mix", 1, 1, 3, "MIX", 0, -1, 3, {}, {} },            { "level", 1, 2, 3, "LEVEL", 6, -1, 3, {}, {} } } }
{
    for (auto& k : knobs) attachKnob (k.slider, k.att, k.id);
    boostTarget.onPress = [this] { proc.undoManager.beginNewTransaction(); toggle ("boostOn"); };
    addAndMakeVisible (boostTarget);
    layoutKnobs();
    startTimerHz (30);
}

DV1AudioProcessorEditor::~DV1AudioProcessorEditor() = default;

void DV1AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    if (midiUI != nullptr) midiUI->paintMarks (g, kPal.body);
}

void DV1AudioProcessorEditor::timerCallback()
{
    meter.update (proc.outMeter);
    if (const int shown = mode() * 4 + variant(); shown != shownModeVariant) { shownModeVariant = shown; layoutKnobs(); repaint(); }       // a preset, undo, A / B or automation changed the mode
    repaintDesign (panelRect (1).withTrimmedLeft (panelRect (1).getWidth() - 150.0f).expanded (2.0f));
    if (midiUI != nullptr && midiUI->needsRepaint()) { midiUI->tick(); repaint(); }
    repaintDesign (panelRect (2).expanded (4.0f));
    tickPresetBar();
}

bool DV1AudioProcessorEditor::knobShown (int i) const
{
    const auto& k = knobs[(size_t) i];
    return (k.onlyMode < 0 || k.onlyMode == mode()) && (k.typeMask & (1 << variant())) != 0;
}

void DV1AudioProcessorEditor::layoutKnobs()
{
    const float sc = scale();
    for (int i = 0; i < (int) knobs.size(); ++i)
    {
        auto& k = knobs[(size_t) i];
        placeKnob (k.slider, knobCentre (k.panel, k.index, k.count));
        k.slider.setVisible (knobShown (i));
    }
    boostTarget.setBounds (outButtonRect (0).expanded (3.0f).transformedBy (juce::AffineTransform::scale (sc)).toNearestInt());

    juce::uint64 shown = 0;                                          // MIDI learn marks only for the knobs that are on screen
    for (int i = 0; i < (int) knobs.size(); ++i) if (knobShown (i)) shown |= (juce::uint64) 1 << i;
    if (midiUI == nullptr || shown != midiShown)
    {
        midiShown = shown;
        midiUI = std::make_unique<MidiLearnUI> (proc.midiMap);
        for (int i = 0; i < (int) knobs.size(); ++i) if (knobShown (i)) midiUI->add (knobs[(size_t) i].slider, knobs[(size_t) i].id);
        midiUI->add (boostTarget, "boostOn");
    }
}
void DV1AudioProcessorEditor::resized() { layoutKnobs(); }

void DV1AudioProcessorEditor::showOdMenu()
{
    juce::PopupMenu menu;
    for (int i = 0; i < dv1::kNumOdTypes; ++i) menu.addItem (i + 1, dv1::odTypeName (i), true, i == choice ("odType"));
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (odButtonRect().transformedBy (juce::AffineTransform::scale (scale())).toNearestInt())),
                        [this] (int r) { if (r > 0) { proc.undoManager.beginNewTransaction(); setChoice ("odType", r - 1); repaint(); } });
}

//==============================================================================
// SHAPE: the transfer curve of the mode (input against output, with the live input as a dot) and a sine before (grey) and after (body colour) the drive.
void DV1AudioProcessorEditor::drawShape (juce::Graphics& g)
{
    auto p = proc.readParams();
    const float boost = p.boostOn ? dv1::dbToGain (p.boostDb) : 1.0f;
    const auto& eng = proc.getEngine();
    const bool power = isOn ("power");

    // the transfer curve
    const auto c = curveRect();
    g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (c.expanded (8.0f), 10.0f);
    g.setColour (kPal.ink.withAlpha (0.14f));
    g.drawLine (c.getX(), c.getCentreY(), c.getRight(), c.getCentreY(), 1.0f); g.drawLine (c.getCentreX(), c.getY(), c.getCentreX(), c.getBottom(), 1.0f);
    g.setColour (kPal.ink.withAlpha (0.3f)); g.drawLine (c.getX(), c.getBottom(), c.getRight(), c.getY(), 1.0f);                // the straight line: no distortion
    const auto toXY = [&] (float x, float y) { return juce::Point<float> (c.getCentreX() + x * 0.5f * c.getWidth(), c.getCentreY() - juce::jlimit (-1.1f, 1.1f, y) * 0.5f * c.getHeight()); };
    juce::Path curve;
    for (int i = 0; i <= 240; ++i)
    {
        const float x = -1.0f + 2.0f * (float) i / 240.0f, y = dv1::staticCurve (p, x * boost);
        const auto pt = toXY (x, y); if (i == 0) curve.startNewSubPath (pt); else curve.lineTo (pt);
    }
    {
        juce::Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (c.expanded (4.0f).toNearestInt());
        g.setColour (kPal.body); g.strokePath (curve, juce::PathStrokeType (2.2f));
        if (power)
        {
            const float x = eng.dotIn.load (std::memory_order_relaxed), y = dv1::staticCurve (p, x * boost);
            const auto d = toXY (x, y);
            g.setColour (kPal.ink); g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (d));
        }
    }
    drawText (g, "IN", c.getRight() - 12.0f, c.getCentreY() + 12.0f, 13.0f, kPal.ink.withAlpha (0.55f), true, 0.1f);
    drawText (g, "OUT", c.getCentreX() + 20.0f, c.getY() + 10.0f, 13.0f, kPal.ink.withAlpha (0.55f), true, 0.1f);

    // a sine before and after
    const auto w = waveRect();
    g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (w.expanded (8.0f), 10.0f);
    g.setColour (kPal.ink.withAlpha (0.14f)); g.drawLine (w.getX(), w.getCentreY(), w.getRight(), w.getCentreY(), 1.0f);
    juce::Path in, out;
    float peak = 0.05f; std::vector<float> ys ((size_t) 400);
    for (int i = 0; i < 400; ++i) { const float x = 0.5f * std::sin (6.2831853f * 2.0f * (float) i / 400.0f); ys[(size_t) i] = dv1::staticCurve (p, x * boost * 2.0f); peak = std::max (peak, std::abs (ys[(size_t) i])); }
    for (int i = 0; i < 400; ++i)
    {
        const float xx = w.getX() + w.getWidth() * (float) i / 399.0f, x = std::sin (6.2831853f * 2.0f * (float) i / 400.0f);
        const juce::Point<float> a (xx, w.getCentreY() - x * 0.45f * w.getHeight()), b (xx, w.getCentreY() - ys[(size_t) i] / peak * 0.45f * w.getHeight());
        if (i == 0) { in.startNewSubPath (a); out.startNewSubPath (b); } else { in.lineTo (a); out.lineTo (b); }
    }
    g.setColour (kPal.ink.withAlpha (0.45f)); g.strokePath (in, juce::PathStrokeType (1.4f));
    g.setColour (kPal.body); g.strokePath (out, juce::PathStrokeType (2.2f));
    drawText (g, "SINE IN (GREY) AND OUT, OUT SCALED TO THE PEAK", w.getX() + 6.0f, w.getBottom() + 22.0f, 13.0f, kPal.ink.withAlpha (0.55f), true, 0.1f, juce::Justification::centredLeft);

    const auto r = panelRect (2);
    juce::String desc = p.mode == dv1::Overdrive ? juce::String (dv1::odTypeName (p.odType)) : juce::String (dv1::modeName (p.mode)) + " " + dv1::variantName (p.mode, p.variant);
    drawText (g, desc + (p.mode == dv1::Chain ? "     HOST RATE (THE SA-1 CHAIN)     LATENCY " : p.mode == dv1::Harmonics ? "     HOST RATE (THE HM-2, THE CURVE IS THE SATURATOR ONLY)     LATENCY " : "     4X OVERSAMPLED     LATENCY ") + juce::String (eng.latency()) + " SAMPLES", r.getX() + 18.0f, r.getBottom() - 22.0f, 13.0f, kPal.ink.withAlpha (0.6f), true, 0.1f, juce::Justification::centredLeft);
}

//==============================================================================
void DV1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    ui::paintBody (g, kPal, kW, kH, grain);
    ui::paintHeader (g, kPal, "DV-1", "DRIVE LAB", "HARMONIC GRIT", isOn ("power"));
    ui::paintPresetBar (g, kPal, proc.presets, proc.undoManager, kBarY, barState, false, 6.0f);

    const int m = mode();
    ui::paintPanel (g, kPal, panelRect (0), {});
    ui::paintTabs (g, kPal, panelRect (0), kModeTabs, m, 13.0f);
    ui::paintPanel (g, kPal, panelRect (1), "OUTPUT");
    ui::paintPanel (g, kPal, panelRect (2), "SHAPE");
    drawShape (g);

    if (m == 0) ui::paintFlatButton (g, kPal, odButtonRect(), juce::String (dv1::odTypeName (choice ("odType"))) + "  v   (12 OD-2 TYPES)", true, 13.0f);
    else if (m == dv1::Harmonics) { static const char* nm[4] = { "TUBE", "TAPE", "TRANSISTOR", "FOLD" }; for (int v = 0; v < 4; ++v) ui::paintFlatButton (g, kPal, buttonRect (v), nm[v], choice ("character") == v, 13.0f); }
    else if (m == dv1::Chain) { static const char* sw[3] = { "tubeOn", "tapeOn", "crushOn" }, *nm[3] = { "TUBE", "TAPE", "CRUSH" }; for (int v = 0; v < 3; ++v) ui::paintFlatButton (g, kPal, buttonRect (v), nm[v], isOn (sw[v]), 13.0f); }
    else for (int v = 0; v < dv1::variantCount (m); ++v) ui::paintFlatButton (g, kPal, buttonRect (v), dv1::variantName (m, v), v == variant(), 13.0f);
    ui::paintFlatButton (g, kPal, outButtonRect (0), "BOOST", isOn ("boostOn"), 13.0f);
    ui::paintFlatButton (g, kPal, outButtonRect (1), "AUTO LEVEL", isOn ("autoLevel"), 13.0f);

    static const char* lo[7] = { "0", "", "0", "0", "1", "500", "-24" }, *hi[7] = { "100", "", "24", "10", "16", "48k", "+12" };
    for (int i = 0; i < (int) knobs.size(); ++i)
    {
        auto& k = knobs[(size_t) i];
        if (! knobShown (i)) continue;
        const double v = k.slider.getValue();
        juce::String txt;
        switch (k.kind)
        {
            case 2: txt = juce::String::formatted ("+%.1f dB", v); break;
            case 3: txt = juce::String (v, 1); break;
            case 4: txt = juce::String ((int) std::lround (v)) + " BITS"; break;
            case 5: txt = v >= 1000.0 ? juce::String (v / 1000.0, 1) + " kHz" : juce::String ((int) std::lround (v)) + " Hz"; break;
            case 6: txt = juce::String::formatted ("%+.1f dB", v); break;
            default: txt = juce::String ((int) std::lround (v)) + " %"; break;
        }
        const auto c = knobCentre (k.panel, k.index, k.count);
        paintKnobCell (g, kPal, c, k.name, txt, lo[k.kind], hi[k.kind], panelRect (k.panel).getY() + 154.0f, panelRect (k.panel).getY() + 182.0f, k.slider);
    }
    meter.paint (g, meterOrigin().x, meterOrigin().y, isOn ("power"), kPal.body, kPal.ink, 110.0f, 26.0f, 13.0f);
    ui::paintFooter (g, kPal, kRow2Y + kRow2H + 35.0f);
}

void DV1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / scale();
    proc.undoManager.beginNewTransaction();
    if (ui::powerRect().contains (p)) { toggle ("power"); return; }
    if (ui::presetBarMouseDown (p, *this, proc.presets, proc.undoManager, kBarY, scale(), barState, [this] { repaint(); }, false)) { layoutKnobs(); return; }
    const auto tabs = ui::tabRects (panelRect (0), kModeTabs, 13.0f);
    for (int i = 0; i < (int) tabs.size(); ++i)
        if (tabs[(size_t) i].expanded (3.0f).contains (p)) { setChoice ("mode", i); setChoice ("variant", 0); layoutKnobs(); repaint(); return; }
    if (mode() == 0) { if (odButtonRect().expanded (3.0f).contains (p)) { showOdMenu(); return; } }
    else if (mode() == dv1::Harmonics)
    {
        for (int v = 0; v < 4; ++v) if (buttonRect (v).expanded (3.0f).contains (p)) { setChoice ("character", v); repaint(); return; }
    }
    else if (mode() == dv1::Chain)
    {
        static const char* sw[3] = { "tubeOn", "tapeOn", "crushOn" };
        for (int v = 0; v < 3; ++v) if (buttonRect (v).expanded (3.0f).contains (p)) { toggle (sw[v]); repaint(); return; }
    }
    else
        for (int v = 0; v < dv1::variantCount (mode()); ++v)
            if (buttonRect (v).expanded (3.0f).contains (p)) { setChoice ("variant", v); layoutKnobs(); repaint(); return; }
    if (outButtonRect (1).expanded (3.0f).contains (p)) { toggle ("autoLevel"); repaint(); return; }
}
