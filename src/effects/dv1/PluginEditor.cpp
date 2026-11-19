// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "ui/Presets.h"

namespace
{
using vibr::ui::vintage::drawText;
namespace ui = vibr::ui::vintage;

constexpr float kW = ui::kW, kH = ui::kH, kRow1Y = 114.0f, kRow1H = 244.0f, kRow2Y = 376.0f, kRow2H = 254.0f;
const ui::Palette kPal { juce::Colour (255, 152, 74) };            // the Drive colour #FF984A: value rings, knob values and the plugin code

// panels: 0 CONTROLS (with the mode tabs), 1 OUTPUT, 2 SHAPE (two screens)
constexpr float kPanelX[3] = { 28.0f, 28.0f, 624.0f }, kPanelW[3] = { 1064.0f, 578.0f, 468.0f };
juce::Rectangle<float> panelRect (int i) { return { kPanelX[i], i == 0 ? kRow1Y : kRow2Y, kPanelW[i], i == 0 ? kRow1H : kRow2H }; }
juce::Point<float> knobCentre (int panel, int k, int n)
{
    const auto r = panelRect (panel);
    if (panel == 1) return { r.getX() + 61.0f + 100.0f * (float) k, r.getY() + 112.0f };                  // BOOST, MIX, LEVEL left of the buttons and the meter
    const float pitch = n == 2 ? 108.0f : juce::jmin (150.0f, (r.getWidth() - 136.0f) / (float) juce::jmax (1, n - 1));      // one or two knobs centred, more spread over the panel
    return { r.getCentreX() + pitch * ((float) k - 0.5f * (float) (n - 1)), r.getY() + 86.0f };
}
juce::Point<float> meterOrigin() { const auto r = panelRect (1); return { r.getRight() - 20.0f - ui::kMeterW, r.getY() + std::floor ((r.getHeight() - ui::kMeterH) * 0.5f) }; }
juce::Rectangle<float> curveScreen() { const auto r = panelRect (2); return { r.getX() + 20.0f, r.getY() + 24.0f, 150.0f, 176.0f }; }
juce::Rectangle<float> waveScreen() { const auto r = panelRect (2), c = curveScreen(); return { c.getRight() + 10.0f, c.getY(), r.getRight() - 20.0f - c.getRight() - 10.0f, c.getHeight() }; }
juce::Rectangle<float> buttonRect (int i) { const auto r = panelRect (0); return { r.getX() + 20.0f + (float) i * 136.0f, r.getY() + 200.0f, 130.0f, 30.0f }; }
juce::Rectangle<float> odButtonRect() { return buttonRect (0).withWidth (380.0f); }
juce::Rectangle<float> outButtonRect (int i) { const auto r = panelRect (1); return { r.getX() + 322.0f, r.getY() + 73.0f + (float) i * 48.0f, 120.0f, 30.0f }; }      // BOOST and AUTO LEVEL, centred on the knob row
const std::vector<juce::String> kModeTabs { "OVERDRIVE", "DISTORTION", "TUBE", "TAPE", "FOLD", "CRUSH", "CHAIN", "HARMONICS" };
constexpr float kTabsTop = -15.0f, kTabsFont = 16.0f;
const float kTabsLeft = ui::titleEnd ("CONTROLS");
std::vector<juce::Rectangle<float>> modeTabRects() { return ui::tabRects (panelRect (0), kModeTabs, kTabsTop, kTabsFont, kTabsLeft); }
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
    if (midiUI != nullptr) midiUI->paintMarks (g, kPal.accent);
}

void DV1AudioProcessorEditor::timerCallback()
{
    meter.update (proc.outMeter);
    if (const int shown = mode() * 4 + variant(); shown != shownModeVariant) { shownModeVariant = shown; layoutKnobs(); repaint(); }       // a preset, undo, A / B or automation changed the mode
    repaintDesign (juce::Rectangle<float> (meterOrigin().x, meterOrigin().y, ui::kMeterW, ui::kMeterH).expanded (2.0f));
    if (midiUI != nullptr && midiUI->needsRepaint()) { midiUI->tick(); repaint(); }
    repaintDesign (panelRect (2).expanded (4.0f));
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
// SHAPE: the transfer curve of the mode (input against output, with the live input as a dot) and a sine before (dim) and after (bright) the drive, on two phosphor screens.
void DV1AudioProcessorEditor::drawShape (juce::Graphics& g)
{
    auto p = proc.readParams();
    const float boost = p.boostOn ? dv1::dbToGain (p.boostDb) : 1.0f;
    const auto& eng = proc.getEngine();
    const bool power = isOn ("power");

    // the transfer curve
    const auto cs = curveScreen();
    ui::paintScope (g, kPal, cs, "TRANSFER", {});
    const auto c = juce::Rectangle<float> (138.0f, 138.0f).withCentre (ui::scopePlotRect (cs).getCentre());
    g.setColour (kPal.phosphor.withAlpha (0.22f));
    g.drawLine (c.getX(), c.getCentreY(), c.getRight(), c.getCentreY(), 1.0f); g.drawLine (c.getCentreX(), c.getY(), c.getCentreX(), c.getBottom(), 1.0f);
    g.setColour (kPal.phosphorDim.withAlpha (0.5f)); g.drawLine (c.getX(), c.getBottom(), c.getRight(), c.getY(), 1.0f);                // the straight line: no distortion
    const auto toXY = [&] (float x, float y) { return juce::Point<float> (c.getCentreX() + x * 0.5f * c.getWidth(), c.getCentreY() - juce::jlimit (-1.1f, 1.1f, y) * 0.5f * c.getHeight()); };
    juce::Path curve;
    for (int i = 0; i <= 240; ++i)
    {
        const float x = -1.0f + 2.0f * (float) i / 240.0f, y = dv1::staticCurve (p, x * boost);
        const auto pt = toXY (x, y); if (i == 0) curve.startNewSubPath (pt); else curve.lineTo (pt);
    }
    {
        juce::Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (c.getSmallestIntegerContainer().getIntersection (cs.reduced (3.0f).toNearestInt()));
        g.setColour (kPal.phosphor.withAlpha (0.18f)); g.strokePath (curve, juce::PathStrokeType (5.0f));
        g.setColour (kPal.phosphor); g.strokePath (curve, juce::PathStrokeType (2.0f));
        if (power)
        {
            const float x = eng.dotIn.load (std::memory_order_relaxed), y = dv1::staticCurve (p, x * boost);
            g.setColour (kPal.cream); g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (toXY (x, y)));
        }
    }
    drawText (g, "IN", c.getRight() - 12.0f, c.getCentreY() + 12.0f, 13.0f, kPal.phosphorDim, false, 0.08f, juce::Justification::centred, ui::Face::Mono);
    drawText (g, "OUT", c.getCentreX() - 20.0f, c.getY() + 10.0f, 13.0f, kPal.phosphorDim, false, 0.08f, juce::Justification::centred, ui::Face::Mono);

    // a sine before and after
    const auto ws = waveScreen();
    ui::paintScope (g, kPal, ws, "SINE IN / OUT", "SCALED");
    const auto w = ui::scopePlotRect (ws);
    g.setColour (kPal.phosphor.withAlpha (0.22f)); g.drawLine (w.getX(), w.getCentreY(), w.getRight(), w.getCentreY(), 1.0f);
    juce::Path in, out;
    float peak = 0.05f; std::vector<float> ys ((size_t) 400);
    for (int i = 0; i < 400; ++i) { const float x = 0.5f * std::sin (6.2831853f * 2.0f * (float) i / 400.0f); ys[(size_t) i] = dv1::staticCurve (p, x * boost * 2.0f); peak = std::max (peak, std::abs (ys[(size_t) i])); }
    for (int i = 0; i < 400; ++i)
    {
        const float xx = w.getX() + w.getWidth() * (float) i / 399.0f, x = std::sin (6.2831853f * 2.0f * (float) i / 400.0f);
        const juce::Point<float> a (xx, w.getCentreY() - x * 0.45f * w.getHeight()), b (xx, w.getCentreY() - ys[(size_t) i] / peak * 0.45f * w.getHeight());
        if (i == 0) { in.startNewSubPath (a); out.startNewSubPath (b); } else { in.lineTo (a); out.lineTo (b); }
    }
    {
        juce::Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (ws.reduced (3.0f).toNearestInt());
        g.setColour (kPal.phosphorDim.withAlpha (0.8f)); g.strokePath (in, juce::PathStrokeType (1.3f));
        g.setColour (kPal.phosphor.withAlpha (0.18f)); g.strokePath (out, juce::PathStrokeType (5.0f));
        g.setColour (kPal.phosphor); g.strokePath (out, juce::PathStrokeType (2.0f));
    }

    const auto r = panelRect (2);
    const juce::String desc = p.mode == dv1::Overdrive ? juce::String (dv1::odTypeName (p.odType)) : juce::String (dv1::modeName (p.mode)) + " " + dv1::variantName (p.mode, p.variant);
    const juce::String rate = p.mode == dv1::Chain || p.mode == dv1::Harmonics ? "HOST RATE" : "4X OVERSAMPLED";
    drawText (g, desc + "  |  " + rate + "  |  LATENCY " + juce::String (eng.latency()) + " SAMPLES", r.getX() + 20.0f, r.getY() + 226.0f, 14.0f, kPal.inkSoft, false, 0.05f, juce::Justification::centredLeft);
}

//==============================================================================
void DV1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    ui::paintBody (g, kPal, kW, kH);
    ui::paintHeader (g, kPal, "DV-1", "DRIVE LAB", "HARMONIC GRIT");
    ui::paintPresetBar (g, kPal, proc.presets, proc.undoManager, isOn ("power"));
    ui::paintFooter (g, kPal, kH - 15.0f);

    const int m = mode();
    ui::paintPanel (g, kPal, panelRect (0), "CONTROLS");
    ui::paintTabs (g, kPal, panelRect (0), kModeTabs, m, kTabsTop, kTabsFont, kTabsLeft);
    ui::paintPanel (g, kPal, panelRect (1), "OUTPUT");
    ui::paintPanel (g, kPal, panelRect (2), "SHAPE");
    drawShape (g);

    if (m == 0) ui::paintFlatButton (g, kPal, odButtonRect(), juce::String (dv1::odTypeName (choice ("odType"))) + "  v   (12 OD-2 TYPES)", true, 17.0f);
    else if (m == dv1::Harmonics) { static const char* nm[4] = { "TUBE", "TAPE", "TRANSISTOR", "FOLD" }; for (int v = 0; v < 4; ++v) ui::paintFlatButton (g, kPal, buttonRect (v), nm[v], choice ("character") == v, 17.0f); }
    else if (m == dv1::Chain) { static const char* sw[3] = { "tubeOn", "tapeOn", "crushOn" }, *nm[3] = { "TUBE", "TAPE", "CRUSH" }; for (int v = 0; v < 3; ++v) ui::paintFlatButton (g, kPal, buttonRect (v), nm[v], isOn (sw[v]), 17.0f); }
    else for (int v = 0; v < dv1::variantCount (m); ++v) ui::paintFlatButton (g, kPal, buttonRect (v), dv1::variantName (m, v), v == variant(), 17.0f);
    ui::paintFlatButton (g, kPal, outButtonRect (0), "BOOST", isOn ("boostOn"), 17.0f);
    ui::paintFlatButton (g, kPal, outButtonRect (1), "AUTO LEVEL", isOn ("autoLevel"), 17.0f);

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
        ui::paintKnob (g, kPal, knobCentre (k.panel, k.index, k.count), k.name, txt, lo[k.kind], hi[k.kind], (float) k.slider.valueToProportionOfLength (v));
    }
    ui::paintMeter (g, kPal, meter, meterOrigin().x, meterOrigin().y, isOn ("power"));
}

void DV1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / scale();
    proc.undoManager.beginNewTransaction();
    if (ui::powerRect().contains (p)) { toggle ("power"); return; }
    if (ui::presetBarMouseDown (p, *this, proc.presets, proc.undoManager, scale(), [this] { repaint(); })) { layoutKnobs(); return; }
    const auto tabs = modeTabRects();
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
