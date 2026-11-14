// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "ui/Presets.h"

namespace
{
using vibr::ui::drawText;
namespace ui = vibr::ui;

constexpr float kW = 1200.0f, kH = 690.0f, kRow1Y = 134.0f, kRow1H = 247.0f, kRow2Y = 405.0f, kRow2H = 216.0f, kKnobR = 24.0f, kPad = 5.0f, kBarY = 39.0f;
const ui::Palette kPal { juce::Colour (255, 152, 74), juce::Colour (46, 27, 13) };            // the Drive colour #FF984A and its print

// panels: 0 CONTROLS (with the mode tabs), 1 OUTPUT, 2 SHAPE (display)
constexpr float kPanelX[3] = { 20.0f, 730.0f, 20.0f }, kPanelW[3] = { 700.0f, 450.0f, 1160.0f };
juce::Rectangle<float> panelRect (int i) { return { kPanelX[i], i < 2 ? kRow1Y : kRow2Y, kPanelW[i], i < 2 ? kRow1H : kRow2H }; }
juce::Point<float> knobCentre (int panel, int k, int n)
{
    const auto r = panelRect (panel);
    const float pitch = panel == 0 && n >= 7 ? 94.0f : 108.0f;
    return { r.getCentreX() + pitch * ((float) k - 0.5f * (float) (n - 1)), r.getY() + 92.0f };
}
juce::Rectangle<float> curveRect() { const auto r = panelRect (2); const float s = r.getHeight() - 74.0f; return { r.getX() + 70.0f, r.getY() + 34.0f, s, s }; }
juce::Rectangle<float> waveRect() { const auto r = panelRect (2), c = curveRect(); return { c.getRight() + 60.0f, c.getY(), r.getRight() - c.getRight() - 90.0f, c.getHeight() }; }
juce::Rectangle<float> buttonRect (int i) { const auto r = panelRect (0); return { r.getX() + 30.0f + (float) i * 130.0f, r.getY() + 204.0f, 120.0f, 30.0f }; }
juce::Rectangle<float> odButtonRect() { return buttonRect (0).withWidth (380.0f); }
juce::Rectangle<float> outButtonRect (int i) { const auto r = panelRect (1); return { r.getX() + 30.0f + (float) i * 205.0f, r.getY() + 204.0f, 190.0f, 30.0f }; }
const std::vector<juce::String> kModeTabs { "OVERDRIVE", "DISTORTION", "TUBE", "TAPE", "FOLD", "CRUSH" };
}

// kinds: 0 percent, 2 boost dB, 3 value 0 .. 10, 4 bits, 5 rate, 6 level dB
//==============================================================================
DV1AudioProcessorEditor::DV1AudioProcessorEditor (DV1AudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
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
                { "boost", 1, 0, 3, "BOOST", 2, -1, 3, {}, {} },         { "mix", 1, 1, 3, "MIX", 0, -1, 3, {}, {} },            { "level", 1, 2, 3, "LEVEL", 6, -1, 3, {}, {} } } },
      powerAtt (*p.apvts.getParameter ("power"), [this] (float) { repaint(); }, nullptr)
{
    setLookAndFeel (&laf);
    for (auto& k : knobs)
    {
        ui::setupKnob (k.slider, *this);
        auto* par = proc.apvts.getParameter (k.id);
        k.slider.setDoubleClickReturnValue (true, par->convertFrom0to1 (par->getDefaultValue()));
        k.slider.onDragStart = [this] { proc.undoManager.beginNewTransaction(); };
        addAndMakeVisible (k.slider);
        k.att = std::make_unique<juce::SliderParameterAttachment> (*dynamic_cast<juce::RangedAudioParameter*> (par), k.slider, nullptr);
        k.slider.onValueChange = [this] { repaint(); };
    }
    powerAtt.sendInitialUpdate();
    grain = ui::makeGrain();
    constrainer.setSizeLimits (720, 414, 2400, 1380);
    constrainer.setFixedAspectRatio ((double) kW / (double) kH);
    setConstrainer (&constrainer);
    setResizable (true, true);
    setSize ((int) kW, (int) kH);
    layoutKnobs();
    startTimerHz (30);
}

DV1AudioProcessorEditor::~DV1AudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void DV1AudioProcessorEditor::timerCallback()
{
    repaint (panelRect (2).expanded (4.0f).transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer());
    if (barState.copiedAt != 0) { repaint (juce::Rectangle<float> (0.0f, 30.0f, 900.0f, 50.0f).transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer()); if (! barState.copiedRecently()) barState.copiedAt = 0; }
}

void DV1AudioProcessorEditor::toggle (const char* id)
{
    auto* par = proc.apvts.getParameter (id);
    par->beginChangeGesture(); par->setValueNotifyingHost (par->getValue() > 0.5f ? 0.0f : 1.0f); par->endChangeGesture();
}
void DV1AudioProcessorEditor::setChoice (const char* id, int index)
{
    auto* par = proc.apvts.getParameter (id);
    par->beginChangeGesture(); par->setValueNotifyingHost (par->convertTo0to1 ((float) index)); par->endChangeGesture();
}

bool DV1AudioProcessorEditor::knobShown (int i) const
{
    const auto& k = knobs[(size_t) i];
    return (k.onlyMode < 0 || k.onlyMode == mode()) && (k.typeMask & (1 << variant())) != 0;
}

void DV1AudioProcessorEditor::layoutKnobs()
{
    const float sc = scale(), d = 2.0f * (kKnobR + kPad) * sc;
    for (int i = 0; i < (int) knobs.size(); ++i)
    {
        auto& k = knobs[(size_t) i];
        k.slider.setBounds (juce::Rectangle<float> (d, d).withCentre (knobCentre (k.panel, k.index, k.count) * sc).toNearestInt());
        k.slider.setVisible (knobShown (i));
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
    drawText (g, "IN", c.getRight() - 8.0f, c.getCentreY() + 12.0f, 10.0f, kPal.ink.withAlpha (0.55f), true, 0.1f);
    drawText (g, "OUT", c.getCentreX() + 18.0f, c.getY() + 10.0f, 10.0f, kPal.ink.withAlpha (0.55f), true, 0.1f);

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
    drawText (g, "SINE IN (GREY) AND OUT, OUT SCALED TO THE PEAK", w.getX() + 6.0f, w.getBottom() + 12.0f, 10.0f, kPal.ink.withAlpha (0.55f), true, 0.1f, juce::Justification::centredLeft);

    const auto r = panelRect (2);
    juce::String desc = p.mode == dv1::Overdrive ? juce::String (dv1::odTypeName (p.odType)) : juce::String (dv1::modeName (p.mode)) + " " + dv1::variantName (p.mode, p.variant);
    drawText (g, desc + "     4X OVERSAMPLED     LATENCY " + juce::String (eng.latency()) + " SAMPLES", r.getX() + 12.0f, r.getBottom() - 12.0f, 11.0f, kPal.ink.withAlpha (0.6f), true, 0.1f, juce::Justification::centredLeft);
}

//==============================================================================
void DV1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    ui::paintBody (g, kPal, kW, kH, grain);
    ui::paintHeader (g, kPal, "DV-1", "DRIVE LAB", "HARMONIC GRIT", isOn ("power"));
    ui::paintPresetBar (g, kPal, proc.presets, proc.undoManager, kBarY, barState);

    const int m = mode();
    ui::paintPanel (g, kPal, panelRect (0), {});
    ui::paintTabs (g, kPal, panelRect (0), kModeTabs, m, 13.0f);
    ui::paintPanel (g, kPal, panelRect (1), "OUTPUT");
    ui::paintPanel (g, kPal, panelRect (2), "SHAPE");
    drawShape (g);

    if (m == 0) ui::paintFlatButton (g, kPal, odButtonRect(), juce::String (dv1::odTypeName (choice ("odType"))) + "  v   (12 OD-2 TYPES)", true, 11.0f);
    else for (int v = 0; v < dv1::variantCount (m); ++v) ui::paintFlatButton (g, kPal, buttonRect (v), dv1::variantName (m, v), v == variant(), 11.0f);
    ui::paintFlatButton (g, kPal, outButtonRect (0), "BOOST", isOn ("boostOn"), 11.0f);
    ui::paintFlatButton (g, kPal, outButtonRect (1), "AUTO LEVEL", isOn ("autoLevel"), 11.0f);

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
        ui::paintKnob (g, kPal, c, k.name, txt, lo[k.kind], hi[k.kind], panelRect (k.panel).getY() + 154.0f, panelRect (k.panel).getY() + 182.0f);
    }
    ui::paintFooter (g, kPal, kRow2Y + kRow2H + 35.0f);
}

void DV1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / scale();
    proc.undoManager.beginNewTransaction();
    if (ui::powerRect().contains (p)) { toggle ("power"); return; }
    if (ui::presetBarMouseDown (p, *this, proc.presets, proc.undoManager, kBarY, scale(), barState, [this] { repaint(); })) { layoutKnobs(); return; }
    const auto tabs = ui::tabRects (panelRect (0), kModeTabs, 13.0f);
    for (int i = 0; i < (int) tabs.size(); ++i)
        if (tabs[(size_t) i].expanded (3.0f).contains (p)) { setChoice ("mode", i); setChoice ("variant", 0); layoutKnobs(); repaint(); return; }
    if (mode() == 0) { if (odButtonRect().expanded (3.0f).contains (p)) { showOdMenu(); return; } }
    else
        for (int v = 0; v < dv1::variantCount (mode()); ++v)
            if (buttonRect (v).expanded (3.0f).contains (p)) { setChoice ("variant", v); layoutKnobs(); repaint(); return; }
    if (outButtonRect (0).expanded (3.0f).contains (p)) { toggle ("boostOn"); repaint(); return; }
    if (outButtonRect (1).expanded (3.0f).contains (p)) { toggle ("autoLevel"); repaint(); return; }
}
