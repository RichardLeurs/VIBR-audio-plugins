// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "ui/Presets.h"

namespace
{
using vibr::ui::drawText;
namespace ui = vibr::ui;

// layout units: the design is 1200 x 690 (style.md: 20 side margin, 10 gap, panels 30 below the rule, 24 between the rows, 69 above the bottom)
constexpr float kW = 1200.0f, kH = 690.0f, kRow1Y = 134.0f, kRow1H = 247.0f, kRow2Y = 405.0f, kRow2H = 216.0f, kKnobR = 24.0f, kPad = 5.0f, kBarY = 39.0f;
const ui::Palette kPal { juce::Colour (184, 162, 114), juce::Colour (33, 28, 14) };            // the khaki body of the GR-1 and its print
const juce::Colour kIce (143, 211, 244);

// panels: 0 ENVELOPE, 1 GRAINS, 2 PITCH (row 1), 3 OUTPUT (row 2, right of the CLOUD)
constexpr float kPanelX[4] = { 20.0f, 290.0f, 806.0f, 730.0f }, kPanelW[4] = { 260.0f, 506.0f, 374.0f, 450.0f };
constexpr const char* kPanelName[4] = { "ENVELOPE", "GRAINS", "PITCH", "OUTPUT" };

juce::Rectangle<float> panelRect (int i) { return { kPanelX[i], i < 3 ? kRow1Y : kRow2Y, kPanelW[i], i < 3 ? kRow1H : kRow2H }; }
juce::Rectangle<float> cloudRect() { return { ui::kMargin, kRow2Y, 700.0f, kRow2H }; }
juce::Rectangle<float> plotRect() { const auto r = cloudRect(); return { r.getX() + 64.0f, r.getY() + 52.0f, r.getWidth() - 94.0f, 120.0f }; }   // the box (8 bigger) runs y + 44 .. y + 180
juce::Rectangle<float> shapeRect (int m) { const auto r = panelRect (0); return { r.getX() + 14.0f, r.getY() + 44.0f + (float) m * 38.0f, r.getWidth() - 28.0f, 29.0f }; }
juce::Rectangle<float> freezeRect() { const auto r = panelRect (0); return { r.getX() + 14.0f, r.getY() + 204.0f, r.getWidth() - 28.0f, 30.0f }; }
juce::Point<float> knobCentre (int panel, int k, int n)
{
    const auto r = panelRect (panel);
    if (panel == 3) return { r.getX() + 79.0f + 100.0f * (float) k, r.getY() + 92.0f };      // OUTPUT: knobs x + 79 / 179 / 279 (ticks x + 37 .. x + 321), the dB meters on the right
    // the knobs (ticks and legends reach 42 each side) sit 100 apart, centred in the panel: equal padding left and right (GRAINS 61, PITCH 45 to the outer ticks)
    const float step = 100.0f;
    return { r.getCentreX() + step * ((float) k - 0.5f * (float) (n - 1)), r.getY() + 92.0f };
}
// SYNC chip: above the DENSITY knob (the second of four in the GRAINS panel)
juce::Rectangle<float> syncRect() { return juce::Rectangle<float> (72.0f, 24.0f).withCentre ({ knobCentre (1, 1, 4).x, kRow1Y + 33.0f }); }
juce::Point<float> meterOrigin() { const auto r = panelRect (3); return { r.getX() + 345.0f, r.getY() + 46.0f }; }       // bars x + 345 .. x + 387, scale labels to about x + 414: 37 padding left of the first knob's ticks and 36 right of the labels; the bars end on the name row (y + 156), the peak values sit on the readout row (y + 182)
} // namespace

//==============================================================================
GR1AudioProcessorEditor::GR1AudioProcessorEditor (GR1AudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      knobs { { { "size", 1, 0, 4, "SIZE", 1, {}, {} },        { "density", 1, 1, 4, "DENSITY", 2, {}, {} },
                { "position", 1, 2, 4, "POSITION", 3, {}, {} }, { "spray", 1, 3, 4, "SPRAY", 0, {}, {} },
                { "pitch", 2, 0, 3, "PITCH", 4, {}, {} },       { "random", 2, 1, 3, "RANDOM", 5, {}, {} },
                { "reverse", 2, 2, 3, "REVERSE", 0, {}, {} },
                { "spread", 3, 0, 3, "SPREAD", 0, {}, {} },     { "mix", 3, 1, 3, "MIX", 0, {}, {} },
                { "level", 3, 2, 3, "LEVEL", 6, {}, {} } } },
      powerAtt  (*p.apvts.getParameter ("power"),  [this] (float) { repaint(); }, nullptr),
      shapeAtt  (*p.apvts.getParameter ("shape"),  [this] (float) { repaint(); }, nullptr),
      freezeAtt (*p.apvts.getParameter ("freeze"), [this] (float) { repaint(); }, nullptr),
      syncAtt   (*p.apvts.getParameter ("sync"),   [this] (float) { repaint(); }, nullptr)
{
    setLookAndFeel (&laf);
    for (auto& k : knobs)
    {
        auto& s = k.slider;
        ui::setupKnob (s, *this);
        s.setTextValueSuffix (k.kind == 6 ? " dB" : " %");
        auto* par = proc.apvts.getParameter (k.id);
        s.setDoubleClickReturnValue (true, par->convertFrom0to1 (par->getDefaultValue()));
        s.onDragStart = [this] { proc.undoManager.beginNewTransaction(); };
        addAndMakeVisible (s);
        k.att = std::make_unique<juce::SliderParameterAttachment> (*dynamic_cast<juce::RangedAudioParameter*> (par), s, nullptr);
        s.onValueChange = [this] { repaint(); };
        midiUI.add (s, k.id);
    }
    powerAtt.sendInitialUpdate();
    shapeAtt.sendInitialUpdate();
    freezeAtt.sendInitialUpdate();
    syncAtt.sendInitialUpdate();

    grain = ui::makeGrain();
    constrainer.setSizeLimits (720, 414, 2400, 1380);
    constrainer.setFixedAspectRatio ((double) kW / (double) kH);
    setConstrainer (&constrainer);
    setResizable (true, true);
    setSize ((int) kW, (int) kH);
    layoutKnobs();
    startTimerHz (30);
}

GR1AudioProcessorEditor::~GR1AudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void GR1AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    midiUI.paintMarks (g, kPal.body);
}

void GR1AudioProcessorEditor::timerCallback()
{
    meter.update (proc.outMeter);
    const auto px = [this] (juce::Rectangle<float> r) { return r.expanded (4.0f).transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer(); };
    repaint (px (panelRect (3).withTrimmedLeft (panelRect (3).getWidth() - 150.0f)));
    if (midiUI.needsRepaint()) { midiUI.tick(); repaint(); }
    repaint (px (cloudRect()));                                                                         // the grain cloud moves
    if (barState.copiedAt != 0) { repaint (juce::Rectangle<float> (0.0f, 30.0f, 900.0f, 50.0f).transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer()); if (! barState.copiedRecently()) barState.copiedAt = 0; }
}

void GR1AudioProcessorEditor::toggle (const char* id)
{
    auto* par = proc.apvts.getParameter (id);
    par->beginChangeGesture();
    par->setValueNotifyingHost (par->getValue() > 0.5f ? 0.0f : 1.0f);
    par->endChangeGesture();
}

void GR1AudioProcessorEditor::layoutKnobs()
{
    const float sc = scale(), d = 2.0f * (kKnobR + kPad) * sc;
    for (auto& k : knobs)
        k.slider.setBounds (juce::Rectangle<float> (d, d).withCentre (knobCentre (k.panel, k.index, k.count) * sc).toNearestInt());
}

void GR1AudioProcessorEditor::resized() { layoutKnobs(); }

//==============================================================================
void GR1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    ui::paintBody (g, kPal, kW, kH, grain);
    ui::paintHeader (g, kPal, "GR-1", "GRANULAR TEXTURE", "CLOUDS OF SOUND", isOn ("power"));
    ui::paintPresetBar (g, kPal, proc.presets, proc.undoManager, kBarY, barState, false, 6.0f);

    for (int i = 0; i < 4; ++i) ui::paintPanel (g, kPal, panelRect (i), kPanelName[i]);
    ui::paintPanel (g, kPal, cloudRect(), "CLOUD");

    const int shape = choice ("shape");
    for (int m = 0; m < gr1::kNumShapes; ++m)
    {
        const auto b = shapeRect (m);
        const bool sel = m == shape;
        if (sel) { g.setColour (kPal.body); g.fillRoundedRectangle (b, 10.0f); }
        g.setColour (sel ? kPal.body : kPal.ink.withAlpha (0.35f));
        g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, 1.2f);
        drawText (g, gr1::shapeName (m), b.getCentreX(), b.getCentreY(), 14.0f, sel ? kPal.dark : kPal.ink.withAlpha (0.85f), true, 0.1f);
    }
    {   // FREEZE has its own ice colour
        const bool on = isOn ("freeze");
        const auto b = freezeRect();
        if (on) { g.setColour (kIce.withAlpha (0.22f)); g.fillRoundedRectangle (b.expanded (4.0f), 14.0f); g.setColour (kIce); g.fillRoundedRectangle (b, 10.0f); }
        g.setColour (on ? kIce : kPal.ink.withAlpha (0.5f));
        g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, 1.4f);
        drawText (g, "FREEZE", b.getCentreX(), b.getCentreY(), 14.0f, on ? kPal.dark : kPal.ink.withAlpha (0.9f), true, 0.18f);
    }

    // knobs: recessed ring, 11 ticks, legends, name, value and the value ring
    for (auto& k : knobs)
    {
        const auto c = knobCentre (k.panel, k.index, k.count);
        const char* lo = k.kind == 6 ? "-24" : k.kind == 4 ? "-12" : "0";
        const char* hi = k.kind == 6 ? "+12" : k.kind == 4 ? "+12" : "10";
        const float top = panelRect (k.panel).getY();
        ui::paintKnob (g, kPal, c, k.name, knobText (k.kind, k.slider.getValue()), lo, hi, top + 154.0f, top + 182.0f);
        ui::paintValueArc (g, c, kKnobR, (float) k.slider.valueToProportionOfLength (k.slider.getValue()), kPal.body);
    }
    ui::paintFlatButton (g, kPal, syncRect(), "SYNC", isOn ("sync"), 13.0f, 1.0f, 8.0f, 0.14f);
    meter.paint (g, meterOrigin().x, meterOrigin().y, isOn ("power"), kPal.body, kPal.ink, 110.0f, 26.0f, 13.0f);

    // cloud: every grain is a dot; left = long ago, right = just now (the dots drift left); up = higher pitch; halo = grain length
    {
        const auto r = plotRect();
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (r.expanded (8.0f), 10.0f);
        for (int st : { -12, 0, 12 })
        {
            const float y = r.getY() + r.getHeight() * (1.0f - (0.5f + (float) st / 48.0f));
            g.setColour (kPal.ink.withAlpha (st == 0 ? 0.25f : 0.12f));
            g.drawLine (r.getX(), y, r.getRight(), y, 1.0f);
            drawText (g, st == 0 ? "0" : juce::String::formatted ("%+d", st), r.getX() - 24.0f, y, 13.0f, kPal.ink.withAlpha (0.7f), false, 0.0f);
        }
        drawText (g, "SEMITONES", cloudRect().getX() + 22.0f, cloudRect().getY() + 28.0f, 13.0f, kPal.ink.withAlpha (0.7f), true, 0.1f, juce::Justification::centredLeft);
        drawText (g, "PAST", r.getX() + 24.0f, r.getBottom() + 20.0f, 13.0f, kPal.ink.withAlpha (0.7f), true, 0.14f);
        drawText (g, "NOW", r.getRight() - 16.0f, r.getBottom() + 20.0f, 13.0f, kPal.ink.withAlpha (0.7f), true, 0.14f);
        const auto& eng = proc.getEngine();
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (r.expanded (4.0f).toNearestInt());
        // every grain that started in the last kWindow seconds is a dot: it appears at the right (NOW) and drifts to the left as it gets older
        const float kWindow = 4.0f;
        const double now = (double) eng.clockNow.load (std::memory_order_relaxed), fs = (double) eng.sampleRate();
        for (int i = 0; i < gr1::kVisEvents; ++i)
        {
            const float age = (float) ((now - (double) eng.evT[i].load (std::memory_order_relaxed)) / fs);
            if (age < 0.0f || age > kWindow) continue;
            const float u = age / kWindow, fade = 1.0f - u;
            const float x = r.getRight() - r.getWidth() * u;
            const float y = r.getY() + r.getHeight() * (1.0f - eng.evY[i].load (std::memory_order_relaxed));
            // the halo shows the grain length: never smaller than 9, up to 26 for the longest grains
            const float rad = 9.0f + 17.0f * std::sqrt (eng.evS[i].load (std::memory_order_relaxed));
            g.setColour (kPal.body.withAlpha (0.22f * fade));
            g.fillEllipse (juce::Rectangle<float> (2.0f * rad, 2.0f * rad).withCentre ({ x, y }));
            g.setColour (kPal.body.brighter (0.3f).withAlpha (0.25f + 0.75f * fade));
            g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ x, y }));
        }
    }

    ui::paintFooter (g, kPal, kRow2Y + kRow2H + 35.0f);
}

juce::String GR1AudioProcessorEditor::knobText (int kind, double v) const
{
    const float f = (float) v * 0.01f;
    switch (kind)
    {
        case 1: { const float ms = gr1::sizeMs (f); return ms >= 100.0f ? juce::String ((int) std::lround (ms)) + " ms" : juce::String (ms, 1) + " ms"; }
        case 2: return isOn ("sync") ? juce::String (gr1::divisionName (gr1::divisionIndex (f))) : juce::String (gr1::densityHz (f), 1) + " /s";
        case 3: { const float ms = gr1::positionMs (f); return ms >= 1000.0f ? juce::String (ms * 0.001f, 2) + " s" : juce::String ((int) std::lround (ms)) + " ms"; }
        case 4: return juce::String::formatted ("%+d st", gr1::pitchSemis (f));
        case 5: return gr1::randomSemis (f) == 0 ? juce::String ("OFF") : juce::String::formatted ("+-%d st", gr1::randomSemis (f));
        case 6: return juce::String::formatted ("%+d dB", (int) std::lround (v));
        default: return juce::String ((int) std::lround (v)) + " %";
    }
}

void GR1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / scale();
    proc.undoManager.beginNewTransaction();
    if (ui::powerRect().contains (p)) { toggle ("power"); return; }
    if (ui::presetBarMouseDown (p, *this, proc.presets, proc.undoManager, kBarY, scale(), barState, [this] { repaint(); }, false)) { layoutKnobs(); return; }
    if (freezeRect().expanded (3.0f).contains (p)) { toggle ("freeze"); return; }
    if (syncRect().expanded (3.0f).contains (p)) { toggle ("sync"); return; }
    for (int m = 0; m < gr1::kNumShapes; ++m)
        if (shapeRect (m).expanded (3.0f).contains (p))
        {
            auto* par = proc.apvts.getParameter ("shape");
            par->beginChangeGesture();
            par->setValueNotifyingHost (par->convertTo0to1 ((float) m));
            par->endChangeGesture();
            return;
        }
}
