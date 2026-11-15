// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "ui/Presets.h"

namespace
{
using vibr::ui::drawText;
using vibr::ui::textWidth;
using vibr::ui::font;
namespace ui = vibr::ui;

// layout units: the design is 1200 x 690 (style.md: 20 side margin, 10 gap, 24 between rows, panels 30 below the rule, 69 above the bottom)
constexpr float kW = 1200.0f, kH = 690.0f, kBarY = 39.0f, kPad = 5.0f;
constexpr float kMatX = 20.0f, kMatY = 134.0f, kMatW = 740.0f, kMatH = 486.0f;
constexpr float kRightX = 770.0f, kRightW = 410.0f, kFilterH = 246.0f, kOutY = 404.0f, kOutH = 216.0f;          // FILTER 134 .. 380, 24 gap, OUTPUT 404 .. 620 (the matrix ends on 620 too)
constexpr float kRowY0 = 204.0f, kRowPitch = 88.0f, kColX0 = 384.0f, kColPitch = 108.0f;                        // matrix rows (sources) and columns (destinations)
constexpr float kOwnA = 200.0f, kOwnB = 306.0f;                                                                  // the sources' own knobs: SENS / RATE, REL / shape button
constexpr float kKnobR = 24.0f, kSmallR = 20.0f;

const ui::Palette kPal { juce::Colour (82, 201, 107), juce::Colour (11, 35, 18) };                            // the Voltage colour #52C96B and its print
const juce::Colour kBody = kPal.body, kDark = kPal.dark, kInk = kPal.ink;

juce::Rectangle<float> matrixRect() { return { kMatX, kMatY, kMatW, kMatH }; }
juce::Rectangle<float> filterRect() { return { kRightX, kMatY, kRightW, kFilterH }; }
juce::Rectangle<float> outputRect() { return { kRightX, kOutY, kRightW, kOutH }; }
juce::Rectangle<float> typeRect (int m) { const auto r = filterRect(); return { r.getRight() - 144.0f, r.getY() + 63.0f + (float) m * 46.0f, 124.0f, 36.0f }; }       // 63 .. 191: from the knob rings to the value row
juce::Rectangle<float> shapeRect (int lfo) { return juce::Rectangle<float> (80.0f, 28.0f).withCentre ({ kOwnB, kRowY0 + kRowPitch * (float) lfo }); }
juce::Rectangle<float> syncRect() { return juce::Rectangle<float> (88.0f, 26.0f).withCentre ({ 91.0f, 166.0f }); }
juce::Point<float> meterOrigin() { const auto r = outputRect(); return { r.getX() + 318.0f, r.getY() + 46.0f }; }       // bars x + 318 .. x + 360, the scale labels to about x + 387: 21 px padding left of the first knob's ticks and 23 right of the labels; 110 high: the bars end on the knob names (y + 156)

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
} // namespace

//==============================================================================
VC1AudioProcessorEditor::VC1AudioProcessorEditor (VC1AudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      powerAtt (*p.apvts.getParameter ("power"), [this] (float) { repaint(); }, nullptr),
      syncAtt (*p.apvts.getParameter ("sync"), [this] (float) { repaint(); }, nullptr),
      typeAtt  (*p.apvts.getParameter ("type"),  [this] (float) { repaint(); }, nullptr),
      shape1Att (*p.apvts.getParameter ("shape1"), [this] (float) { repaint(); }, nullptr),
      shape2Att (*p.apvts.getParameter ("shape2"), [this] (float) { repaint(); }, nullptr)
{
    setLookAndFeel (&laf);
    // the sources' own knobs (kind: 0 percent, 1 cutoff, 2 LFO rate, 3 random rate, 4 release, 5 matrix depth, 6 dB, 7 sensitivity)
    const auto add = [this] (const char* id, int kind, float cx, float cy, float r, const char* name)
    {
        auto k = std::make_unique<Knob>();
        k->id = id; k->kind = kind; k->cx = cx; k->cy = cy; k->r = r; k->name = name;
        auto& s = k->slider;
        ui::setupKnob (s, *this);
        s.textFromValueFunction = [this, kind] (double v) { return knobText (kind, v); };               // the drag popup shows what the knob shows
        auto* par = proc.apvts.getParameter (id);
        s.setDoubleClickReturnValue (true, par->convertFrom0to1 (par->getDefaultValue()));
        s.onDragStart = [this] { proc.undoManager.beginNewTransaction(); };
        addAndMakeVisible (s);
        k->att = std::make_unique<juce::SliderParameterAttachment> (*dynamic_cast<juce::RangedAudioParameter*> (par), s, nullptr);
        midiUI.add (s, id);
        s.onValueChange = [this] { repaint(); };
        knobs.push_back (std::move (k));
    };
    add ("envsens", 7, kOwnA, kRowY0, kSmallR, "SENS");       add ("envrel", 4, kOwnB, kRowY0, kSmallR, "REL");
    add ("rate1", 2, kOwnA, kRowY0 + kRowPitch, kSmallR, "RATE");
    add ("rate2", 2, kOwnA, kRowY0 + 2.0f * kRowPitch, kSmallR, "RATE");
    add ("rand", 3, kOwnA, kRowY0 + 3.0f * kRowPitch, kSmallR, "RATE");
    for (int s = 0; s < vc1::kNumSources; ++s)
        for (int d = 0; d < vc1::kNumDests; ++d)
            add (matrixIds[s][d], 5, kColX0 + kColPitch * (float) d, kRowY0 + kRowPitch * (float) s, kSmallR, nullptr);
    const auto fr = filterRect(), orr = outputRect();                                                  // the big knobs: row y + 92 in every panel
    add ("cutoff", 1, fr.getX() + 76.0f, fr.getY() + 92.0f, kKnobR, "CUTOFF");     add ("res", 0, fr.getX() + 186.0f, fr.getY() + 92.0f, kKnobR, "RESONANCE");
    add ("xmod", 0, orr.getX() + 63.0f, orr.getY() + 92.0f, kKnobR, "X-MOD");      add ("mix", 0, orr.getX() + 155.0f, orr.getY() + 92.0f, kKnobR, "MIX");
    add ("level", 6, orr.getX() + 247.0f, orr.getY() + 92.0f, kKnobR, "LEVEL");
    powerAtt.sendInitialUpdate(); syncAtt.sendInitialUpdate(); typeAtt.sendInitialUpdate(); shape1Att.sendInitialUpdate(); shape2Att.sendInitialUpdate();

    grain = ui::makeGrain();
    constrainer.setSizeLimits (720, 414, 2400, 1380);
    constrainer.setFixedAspectRatio ((double) kW / (double) kH);
    setConstrainer (&constrainer);
    setResizable (true, true);
    setSize ((int) kW, (int) kH);
    layoutKnobs();
    startTimerHz (30);
}

VC1AudioProcessorEditor::~VC1AudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void VC1AudioProcessorEditor::timerCallback()
{
    const auto sc = juce::AffineTransform::scale (scale());
    meter.update (proc.outMeter);
    repaint (outputRect().withTrimmedLeft (outputRect().getWidth() - 150.0f).expanded (2.0f).transformedBy (sc).getSmallestIntegerContainer());      // the dB meters
    repaint (matrixRect().withWidth (140.0f).transformedBy (sc).getSmallestIntegerContainer());      // the source lamps
    if (midiUI.needsRepaint()) { midiUI.tick(); repaint(); }
    if (barState.copiedAt != 0) { repaint (juce::Rectangle<float> (0.0f, 30.0f, 900.0f, 50.0f).transformedBy (sc).getSmallestIntegerContainer()); if (! barState.copiedRecently()) barState.copiedAt = 0; }
}

void VC1AudioProcessorEditor::toggle (const char* id)
{
    auto* par = proc.apvts.getParameter (id);
    par->beginChangeGesture();
    par->setValueNotifyingHost (par->getValue() > 0.5f ? 0.0f : 1.0f);
    par->endChangeGesture();
}

void VC1AudioProcessorEditor::setChoice (const char* id, int index)
{
    auto* par = proc.apvts.getParameter (id);
    par->beginChangeGesture();
    par->setValueNotifyingHost (par->convertTo0to1 ((float) index));
    par->endChangeGesture();
}

void VC1AudioProcessorEditor::layoutKnobs()
{
    const float sc = scale();
    for (auto& k : knobs)
    {
        const float d = 2.0f * (k->r + kPad) * sc;
        k->slider.setBounds (juce::Rectangle<float> (d, d).withCentre (juce::Point<float> (k->cx, k->cy) * sc).toNearestInt());
    }
}

void VC1AudioProcessorEditor::resized() { layoutKnobs(); }

juce::String VC1AudioProcessorEditor::knobText (int kind, double v) const
{
    const float f = (float) v * 0.01f;
    switch (kind)
    {
        case 1: { const float hz = vc1::cutoffHz (f); return hz >= 1000.0f ? juce::String (hz * 0.001f, 1) + " kHz" : juce::String ((int) std::lround (hz)) + " Hz"; }
        case 2: return isOn ("sync") ? juce::String (vc1::divisionName (vc1::divisionIndex (f))) : juce::String (vc1::lfoHz (f), 2) + " Hz";
        case 3: return isOn ("sync") ? juce::String (vc1::divisionName (vc1::divisionIndex (f))) : juce::String (vc1::randomHz (f), 1) + " /s";
        case 4: return juce::String ((int) std::lround (vc1::releaseMs (f))) + " ms";
        case 5: { const int d = (int) std::lround ((v - 50.0) * 2.0); return d == 0 ? juce::String ("0") : juce::String::formatted ("%+d", d); }
        case 6: return juce::String::formatted ("%+d dB", (int) std::lround (v));
        case 7: return juce::String (vc1::sensGain (f), 1) + "x";
        default: return juce::String ((int) std::lround (v)) + " %";
    }
}

//==============================================================================
void VC1AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    midiUI.paintMarks (g, kBody);
}

void VC1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    ui::paintBody (g, kPal, kW, kH, grain);
    ui::paintHeader (g, kPal, "VC-1", "VOLTAGE CONTROLLER", "EXPRESSIVE MOTION", isOn ("power"));
    ui::paintPresetBar (g, kPal, proc.presets, proc.undoManager, kBarY, barState, false, 6.0f);

    ui::paintPanel (g, kPal, matrixRect(), "MODULATION MATRIX");
    ui::paintPanel (g, kPal, filterRect(), "FILTER");
    ui::paintPanel (g, kPal, outputRect(), "CROSS / OUTPUT");

    // SYNC: the LFO 1, LFO 2 and RANDOM rate knobs become note lengths of the host tempo
    ui::paintFlatButton (g, kPal, syncRect(), "SYNC", isOn ("sync"), 14.0f, 1.0f, 9.0f, 0.14f);

    // matrix: destination names on top, one row per source with its lamp and knobs
    for (int d = 0; d < vc1::kNumDests; ++d)
    {
        drawText (g, vc1::destName (d), kColX0 + kColPitch * (float) d, 166.0f, 15.0f, kBody, true, 0.14f);
        g.setColour (kInk.withAlpha (0.12f));
        g.drawLine (kColX0 + kColPitch * (float) d, kRowY0 - kSmallR - kPad, kColX0 + kColPitch * (float) d, kMatY + kMatH - 14.0f, 1.0f);
    }
    const auto& eng = proc.getEngine();
    for (int s = 0; s < vc1::kNumSources; ++s)
    {
        const float y = kRowY0 + kRowPitch * (float) s;
        const float v = eng.sourceVis[s].load (std::memory_order_relaxed);
        const float lamp = s == vc1::Env || s == vc1::Midi ? v : std::fabs (v);
        g.setColour (kBody.withAlpha (0.12f + 0.88f * juce::jlimit (0.0f, 1.0f, lamp)));
        g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre ({ kMatX + 32.0f, y }));
        drawText (g, vc1::sourceName (s), kMatX + 48.0f, y, 15.0f, kInk.withAlpha (0.95f), true, 0.1f, juce::Justification::centredLeft);
    }
    drawText (g, "MOD WHEEL / EXPRESSION", 0.5f * (kOwnA + kOwnB), kRowY0 + 4.0f * kRowPitch, 14.0f, kInk.withAlpha (0.75f), false, 0.04f);
    for (int lfo = 1; lfo <= 2; ++lfo)
        ui::paintFlatButton (g, kPal, shapeRect (lfo), vc1::shapeName (choice (lfo == 1 ? "shape1" : "shape2")), false, 13.0f, 1.0f, 8.0f, 0.08f, 1.2f);
    for (int m = 0; m < vc1::kNumTypes; ++m)
        ui::paintFlatButton (g, kPal, typeRect (m), vc1::typeName (m), m == choice ("type"), 14.0f, 1.0f, 10.0f, 0.1f, 1.2f);

    // knobs
    for (auto& kp : knobs)
    {
        Knob& k = *kp;
        const juce::Point<float> c (k.cx, k.cy);
        const double v = k.slider.getValue();
        const float prop = (float) k.slider.valueToProportionOfLength (v);
        if (k.r >= kKnobR)                                                                           // the big knobs: the shared decoration, name and value rows at y + 62 / y + 90
        {
            juce::String lo = "0", hi = "100";
            if (k.kind == 1) { lo = "60 Hz"; hi = "12 kHz"; }
            else if (k.kind == 6) { lo = "-24"; hi = "+12"; }
            ui::paintKnob (g, kPal, c, k.name, knobText (k.kind, v), lo, hi, c.y + 62.0f, c.y + 90.0f);
            ui::paintValueArc (g, c, k.r, prop, kBody);
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
            g.setColour (kInk.withAlpha (0.55f));
            g.drawLine (c.x, c.y - k.r - 10.0f, c.x, c.y - k.r - 5.0f, 1.6f);
            paintBipolarArc (g, c, k.r, (float) (v - 50.0) / 50.0f, kBody);
            drawText (g, knobText (5, v), c.x, c.y + k.r + 25.0f, 13.0f, zero ? kInk.withAlpha (0.6f) : kBody, true, 0.04f);
            continue;
        }
        for (int t = 0; t <= 10; ++t)
        {
            const float a = juce::degreesToRadians (-135.0f + 27.0f * (float) t), s = std::sin (a), co = std::cos (a);
            const bool major = t == 0 || t == 5 || t == 10;
            const float r0 = k.r + 10.0f, r1 = r0 + (major ? 4.0f : 2.5f);
            g.setColour (kInk.withAlpha (0.85f));
            g.drawLine (c.x + s * r0, c.y - co * r0, c.x + s * r1, c.y - co * r1, major ? 1.8f : 1.1f);
        }
        ui::paintValueArc (g, c, k.r, prop, kBody);
        {                                                                                            // name and value on one line, 25 under the centre: the MIDI dot sits between knob and text
            const auto name = juce::String (k.name) + " ";
            const auto val = knobText (k.kind, v);
            const auto fn = font (13.0f, true).withExtraKerningFactor (0.04f);
            const float wn = textWidth (fn, name), wv = textWidth (fn, val), x0 = c.x - (wn + wv) * 0.5f, ty = c.y + k.r + 25.0f;
            drawText (g, name, x0 + wn * 0.5f, ty, 13.0f, kInk.withAlpha (0.85f), true, 0.04f);
            drawText (g, val, x0 + wn + wv * 0.5f, ty, 13.0f, kBody, false, 0.0f);
        }
    }
    meter.paint (g, meterOrigin().x, meterOrigin().y, isOn ("power"), kBody, kInk, 110.0f, 26.0f, 13.0f);
    ui::paintFooter (g, kPal, kMatY + kMatH + 35.0f);
}

void VC1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / scale();
    proc.undoManager.beginNewTransaction();
    if (ui::powerRect().contains (p)) { toggle ("power"); return; }
    if (ui::presetBarMouseDown (p, *this, proc.presets, proc.undoManager, kBarY, scale(), barState, [this] { repaint(); }, false)) { layoutKnobs(); return; }
    if (syncRect().expanded (3.0f).contains (p)) { toggle ("sync"); return; }
    for (int m = 0; m < vc1::kNumTypes; ++m)
        if (typeRect (m).expanded (3.0f).contains (p)) { setChoice ("type", m); return; }
    for (int lfo = 1; lfo <= 2; ++lfo)
        if (shapeRect (lfo).expanded (3.0f).contains (p))
        {
            const char* id = lfo == 1 ? "shape1" : "shape2";
            setChoice (id, (choice (id) + 1) % vc1::kNumShapes);
            return;
        }
}
