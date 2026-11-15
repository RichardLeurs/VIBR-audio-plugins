// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "ui/Presets.h"
#include "ui/OutputMeter.h"

namespace
{
using vibr::ui::drawText;
using vibr::ui::textWidth;
using vibr::ui::font;
namespace ui = vibr::ui;

// layout units: the design is 1200 x 690 (style.md: 20 side margin, 10 gap, name centre 55, rule 104, panels 30 below the rule, 24 between the rows)
constexpr float kW = 1200.0f, kH = 690.0f, kMargin = 20.0f, kBarY = 39.0f;
constexpr float kPanelY = 134.0f, kPanelH = 247.0f, kKnobR = 24.0f, kPad = 5.0f, kRow2Y = 405.0f, kRow2H = 216.0f, kKnobPitch = 116.0f;
// panels 0 .. 3: the four stages (row 1); panel 4: OUTPUT (row 2, right, as wide as CRUSH above it); the SHAPE display is the row 2 panel on the left
constexpr float kPanelX[5] = { 20.0f, 290.0f, 560.0f, 830.0f, 830.0f }, kPanelW[5] = { 260.0f, 260.0f, 260.0f, 350.0f, 350.0f };
constexpr const char* kPanelName[5] = { "BOOST", "TUBE", "TAPE", "CRUSH", "OUTPUT" };

const juce::Colour kBody (201, 54, 104), kDark (38, 6, 18), kLight (244, 246, 250);
const ui::Palette kPal { kBody, kLight };         // header, preset bar and footer: the light print on the raspberry body
const ui::Palette kPanelPal { kBody, kDark };     // the lit stage pills: dark print and border on the body colour

juce::Rectangle<float> panelRect (int i)
{
    if (i == 4) return { kPanelX[4], kRow2Y, kPanelW[4], kRow2H };
    return { kPanelX[i], kPanelY, kPanelW[i], kPanelH };
}
juce::Rectangle<float> shapePanelRect() { return { kMargin, kRow2Y, 800.0f, kRow2H }; }           // 20 .. 820: ends where TAPE ends, OUTPUT starts under CRUSH
// the vertical positions inside a panel: knob centre, name and value (row 1: panels 247 high; OUTPUT 216 high, as the other Labs' OUTPUT rows)
float knobOffset (int panel) { return panel == 4 ? 92.0f : 100.0f; }
float nameOffset (int panel) { return panel == 4 ? 154.0f : 178.0f; }
float valueOffset (int panel) { return panel == 4 ? 182.0f : 206.0f; }
juce::Rectangle<float> pillRect (int i)           // the title pill of a panel: for the four stages it is also the on / off switch
{
    const auto r = panelRect (i);
    const float w = textWidth (font (18.0f, true).withExtraKerningFactor (0.08f), kPanelName[i]) + 38.0f;
    return juce::Rectangle<float> (w, 22.0f).withCentre ({ r.getCentreX(), r.getY() });
}
juce::Point<float> knobCentre (int panel, int k, int n)
{
    const auto r = panelRect (panel);
    // the knobs sit 116 apart around the middle of the panel (ticks and legends reach 42 each side); OUTPUT: the knobs and the meters are one centred group
    // (padding 30 | knob | 116 | knob | gap 20 | bars | scale labels | padding 30)
    const float cx = panel == 4 ? r.getX() + 72.0f + 0.5f * kKnobPitch : r.getCentreX();
    return { cx + kKnobPitch * ((float) k - 0.5f * (float) (n - 1)), r.getY() + knobOffset (panel) };
}
juce::Point<float> meterOrigin() { const auto r = panelRect (4); return { r.getX() + 250.0f, r.getY() + 46.0f }; }   // bars x + 250 .. x + 292, labels to about x + 320; bars end on the name row (y + 156)
}

//==============================================================================
SA1AudioProcessorEditor::SA1AudioProcessorEditor (SA1AudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      knobs { { { "boostGain", 0, 0, 2, "GAIN", 0, {}, {} },    { "boostTone", 0, 1, 2, "TONE", 1, {}, {} },
                { "tubeDrive", 1, 0, 2, "DRIVE", 1, {}, {} },   { "tubeBias", 1, 1, 2, "BIAS", 1, {}, {} },
                { "tapeDrive", 2, 0, 2, "DRIVE", 1, {}, {} },   { "tapeWarmth", 2, 1, 2, "WARMTH", 1, {}, {} },
                { "bits", 3, 0, 3, "BITS", 2, {}, {} },         { "rate", 3, 1, 3, "RATE", 3, {}, {} },        { "crushMix", 3, 2, 3, "MIX", 1, {}, {} },
                { "mix", 4, 0, 2, "MIX", 1, {}, {} },           { "level", 4, 1, 2, "LEVEL", 4, {}, {} } } },
      powerAtt (*p.apvts.getParameter ("power"),   [this] (float) { repaint(); }, nullptr),
      boostAtt (*p.apvts.getParameter ("boostOn"), [this] (float) { repaint(); }, nullptr),
      tubeAtt  (*p.apvts.getParameter ("tubeOn"),  [this] (float) { repaint(); }, nullptr),
      tapeAtt  (*p.apvts.getParameter ("tapeOn"),  [this] (float) { repaint(); }, nullptr),
      crushAtt (*p.apvts.getParameter ("crushOn"), [this] (float) { repaint(); }, nullptr)
{
    setLookAndFeel (&laf);
    for (auto& k : knobs)
    {
        auto& s = k.slider;
        ui::setupKnob (s, *this);
        s.onDragStart = [this] { proc.undoManager.beginNewTransaction(); };
        auto* par = proc.apvts.getParameter (k.id);
        s.setDoubleClickReturnValue (true, par->convertFrom0to1 (par->getDefaultValue()));
        addAndMakeVisible (s);
        midiUI.add (s, k.id);
        k.att = std::make_unique<juce::SliderParameterAttachment> (*dynamic_cast<juce::RangedAudioParameter*> (par), s, nullptr);
        s.onValueChange = [this] { repaint(); };
    }
    {
        static const char* const stageIds[4] = { "boostOn", "tubeOn", "tapeOn", "crushOn" };
        for (int i = 0; i < 4; ++i)
        {
            stageTarget[i].onPress = [this, i] { toggle (stageIds[i]); };
            addAndMakeVisible (stageTarget[i]);
            midiUI.add (stageTarget[i], stageIds[i]);
        }
    }
    powerAtt.sendInitialUpdate(); boostAtt.sendInitialUpdate(); tubeAtt.sendInitialUpdate(); tapeAtt.sendInitialUpdate(); crushAtt.sendInitialUpdate();

    grain = ui::makeGrain();

    constrainer.setSizeLimits (720, 414, 2400, 1380);
    constrainer.setFixedAspectRatio ((double) kW / (double) kH);
    setConstrainer (&constrainer);
    setResizable (true, true);
    setSize ((int) kW, (int) kH);
    layoutKnobs();
    startTimerHz (30);
}

SA1AudioProcessorEditor::~SA1AudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void SA1AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    midiUI.paintMarks (g, kBody);
}

void SA1AudioProcessorEditor::timerCallback()
{
    meter.update (proc.outMeter);
    repaint (panelRect (4).withTrimmedLeft (panelRect (4).getWidth() - 150.0f).expanded (2.0f).transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer());
    if (midiUI.needsRepaint()) { midiUI.tick(); repaint(); }
    for (int i = 0; i < 4; ++i)                                                                       // a preset, automation or MIDI can switch a stage: redraw the pills and the dimmed knobs
        if (shownOn[i] != stageOn (i)) { shownOn[i] = stageOn (i); repaint(); }
    for (auto& k : knobs) k.slider.setAlpha (stageOn (k.panel) ? 1.0f : 0.4f);       // the knobs of a stage that is off are dimmed
    if (barState.copiedAt != 0) { repaint (juce::Rectangle<float> (0.0f, 30.0f, 900.0f, 50.0f).transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer()); if (! barState.copiedRecently()) barState.copiedAt = 0; }
}

void SA1AudioProcessorEditor::toggle (const char* id)
{
    auto* par = proc.apvts.getParameter (id);
    par->beginChangeGesture();
    par->setValueNotifyingHost (par->getValue() > 0.5f ? 0.0f : 1.0f);
    par->endChangeGesture();
}

void SA1AudioProcessorEditor::layoutKnobs()
{
    const float sc = scale(), d = 2.0f * (kKnobR + kPad) * sc;
    for (auto& k : knobs)
        k.slider.setBounds (juce::Rectangle<float> (d, d).withCentre (knobCentre (k.panel, k.index, k.count) * sc).toNearestInt());
    for (int i = 0; i < 4; ++i)
        stageTarget[i].setBounds (pillRect (i).expanded (6.0f, 8.0f).transformedBy (juce::AffineTransform::scale (sc)).toNearestInt());
}

void SA1AudioProcessorEditor::resized() { layoutKnobs(); }

//==============================================================================
// SHAPE display: on the left the transfer curve of the chain (input on the x axis, output on the y axis, the grey diagonal is "no effect"), on the
// right a 440 Hz sine (grey) and what the chain makes of it (colour): the saturation flattens and skews it, the crush turns it into steps.
void SA1AudioProcessorEditor::drawShape (juce::Graphics& g)
{
    sa1::Params p;
    p.boostOn = isOn ("boostOn");  p.boostGain = value ("boostGain");  p.tubeOn = isOn ("tubeOn");  p.tubeDrive = value ("tubeDrive") * 0.01f;
    p.tubeBias = value ("tubeBias") * 0.01f;  p.tapeOn = isOn ("tapeOn");  p.tapeDrive = value ("tapeDrive") * 0.01f;  p.crushOn = isOn ("crushOn");
    p.bits = value ("bits");  p.rate = value ("rate");  p.crushMix = value ("crushMix") * 0.01f;

    const auto panel = shapePanelRect();
    const float side = 130.0f;
    const juce::Rectangle<float> curve (panel.getX() + 48.0f, panel.getY() + 36.0f, side, side);       // the dark boxes (8 px bigger) are 40 from the panel edge on both sides
    const juce::Rectangle<float> wave (curve.getRight() + 70.0f, curve.getY(), panel.getRight() - curve.getRight() - 70.0f - 48.0f, side);

    for (const auto& r : { curve, wave })
    {
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (r.expanded (8.0f), 10.0f);
        g.setColour (kPal.ink.withAlpha (0.12f));
        g.drawLine (r.getX(), r.getCentreY(), r.getRight(), r.getCentreY(), 1.0f);
    }
    g.setColour (kPal.ink.withAlpha (0.12f));
    g.drawLine (curve.getCentreX(), curve.getY(), curve.getCentreX(), curve.getBottom(), 1.0f);
    g.setColour (kPal.ink.withAlpha (0.3f));
    g.drawLine (curve.getX(), curve.getBottom(), curve.getRight(), curve.getY(), 1.2f);            // the diagonal: no effect

    const auto clampY = [] (float v) { return std::clamp (v, -1.2f, 1.2f); };
    juce::Path pc;
    for (int i = 0; i <= 240; ++i)
    {
        const float x = -1.0f + 2.0f * (float) i / 240.0f, y = clampY (sa1::shape (x, p));
        const juce::Point<float> pt (curve.getCentreX() + x * side * 0.5f, curve.getCentreY() - y * side * 0.5f);
        if (i == 0) pc.startNewSubPath (pt); else pc.lineTo (pt);
    }
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (curve.expanded (4.0f).toNearestInt());
        g.setColour (kBody);
        g.strokePath (pc, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    drawText (g, "TRANSFER", curve.getCentreX(), curve.getBottom() + 26.0f, 13.0f, kPal.ink.withAlpha (0.75f), true, 0.18f);
    drawText (g, "IN", curve.getRight() - 10.0f, curve.getCentreY() + 14.0f, 13.0f, kPal.ink.withAlpha (0.75f), true, 0.1f);
    drawText (g, "OUT", curve.getCentreX() + 20.0f, curve.getY() + 10.0f, 13.0f, kPal.ink.withAlpha (0.75f), true, 0.1f);

    // the waveform: two cycles of a 440 Hz sine at 48 kHz, 0.7 peak; the crush holds each sample for 48000 / rate samples
    const int n = 218, hold = std::max (1, (int) std::lround (48000.0f / std::max (500.0f, p.rate)));
    juce::Path in, out;
    float held = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float x = 0.7f * std::sin (juce::MathConstants<float>::twoPi * 440.0f * (float) i / 48000.0f);
        float y = sa1::shape (x, p);
        if (p.crushOn)
        {
            if (i % hold == 0) held = sa1::crushCurve (sa1::shape (x, [&] { auto q = p; q.crushOn = false; return q; }()), p.bits);
            y = (sa1::shape (x, [&] { auto q = p; q.crushOn = false; return q; }())) + (held - sa1::shape (x, [&] { auto q = p; q.crushOn = false; return q; }())) * p.crushMix;
        }
        const float px = wave.getX() + wave.getWidth() * (float) i / (float) (n - 1);
        const juce::Point<float> a (px, wave.getCentreY() - x * wave.getHeight() * 0.5f), b (px, wave.getCentreY() - clampY (y) * wave.getHeight() * 0.5f);
        if (i == 0) { in.startNewSubPath (a); out.startNewSubPath (b); } else { in.lineTo (a); out.lineTo (b); }
    }
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (wave.expanded (4.0f).toNearestInt());
        g.setColour (kPal.ink.withAlpha (0.45f));
        g.strokePath (in, juce::PathStrokeType (1.6f));
        g.setColour (kBody);
        g.strokePath (out, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    drawText (g, "WAVEFORM  (440 Hz SINE)", wave.getCentreX(), wave.getBottom() + 26.0f, 13.0f, kPal.ink.withAlpha (0.75f), true, 0.18f);
}

//==============================================================================
void SA1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));

    ui::paintBody (g, kPal, kW, kH, grain);
    ui::paintHeader (g, kPal, "SA-1", "LO-FI SATURATOR", "WARMTH AND GRIT", isOn ("power"));
    ui::paintPresetBar (g, kPal, proc.presets, proc.undoManager, kBarY, barState, false, 6.0f);

    // panels: the pill of a stage is its on / off switch (lit = on, with a dark border)
    for (int i = 0; i < 4; ++i) ui::paintPanel (g, kPanelPal, panelRect (i), kPanelName[i], stageOn (i) ? ui::Pill::On : ui::Pill::Off);
    ui::paintPanel (g, kPanelPal, panelRect (4), "OUTPUT");
    ui::paintPanel (g, kPanelPal, shapePanelRect(), "SHAPE");
    drawShape (g);

    // knobs: recessed ring, 11 ticks, legends, name and value, and the value ring
    for (auto& k : knobs)
    {
        const float dim = stageOn (k.panel) ? 1.0f : 0.4f;
        const auto c = knobCentre (k.panel, k.index, k.count);
        static const char* lo[5] = { "0", "0", "1", "0.5k", "-24" }, * hi[5] = { "24", "100", "16", "48k", "+12" };
        const double v = k.slider.getValue();
        juce::String txt;
        switch (k.kind)
        {
            case 0: txt = juce::String::formatted ("+%.1f dB", v); break;
            case 1: txt = juce::String ((int) std::lround (v)) + " %"; break;
            case 2: txt = juce::String ((int) std::lround (v)) + " bit"; break;
            case 3: txt = v >= 1000.0 ? juce::String (v * 0.001, 1) + " kHz" : juce::String ((int) std::lround (v)) + " Hz"; break;
            default: txt = juce::String::formatted ("%+d dB", (int) std::lround (v)); break;
        }
        const float y0 = panelRect (k.panel).getY();
        ui::paintKnob (g, kPanelPal, c, k.name, txt, lo[k.kind], hi[k.kind], y0 + nameOffset (k.panel), y0 + valueOffset (k.panel), dim);
        ui::paintValueArc (g, c, kKnobR, (float) k.slider.valueToProportionOfLength (k.slider.getValue()), kBody.withMultipliedAlpha (dim));
    }
    meter.paint (g, meterOrigin().x, meterOrigin().y, isOn ("power"), kBody, kPal.ink, 110.0f, 26.0f, 13.0f);
    ui::paintFooter (g, kPal, kRow2Y + kRow2H + 35.0f);
}

void SA1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / scale();
    proc.undoManager.beginNewTransaction();
    if (ui::powerRect().contains (p)) { toggle ("power"); return; }
    if (ui::presetBarMouseDown (p, *this, proc.presets, proc.undoManager, kBarY, scale(), barState, [this] { repaint(); }, false)) { layoutKnobs(); return; }
}
