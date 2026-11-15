// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"

namespace
{
namespace ui = vibr::ui;
using ui::drawText;
using ui::textWidth;

// layout units: the design is 1200 x 690 (style.md: 20 side margin, 10 gap, name centre 55, rule 104, panels 30 below the rule, 24 between the rows)
constexpr float kW = 1200.0f, kH = 690.0f, kMargin = 20.0f, kBarY = 39.0f;
constexpr float kPanelY = 134.0f, kPanelH = 216.0f, kKnobR = 24.0f, kPad = 5.0f, kActY = 374.0f, kActH = 247.0f, kKnobPitch = 110.0f;
// GATE 20..250, EXPANDER 260..600, TRANSIENT 610..840, OUTPUT 850..1180: 10 apart, knobs 110 apart with 18 between the outer ticks and the border
constexpr float kPanelX[4] = { 20.0f, 260.0f, 610.0f, 850.0f }, kPanelW[4] = { 230.0f, 340.0f, 230.0f, 330.0f };
constexpr const char* kPanelName[4] = { "GATE", "EXPANDER", "TRANSIENT", "OUTPUT" };

const ui::Palette kPal { juce::Colour (242, 211, 78), juce::Colour (40, 33, 6) };       // the Dynamics yellow #F2D34E and its print
const juce::Colour kBody = kPal.body, kDark = kPal.dark, kPanel = kPal.panel, kInk = kPal.ink;

juce::Rectangle<float> panelRect (int i) { return { kPanelX[i], kPanelY, kPanelW[i], kPanelH }; }
juce::Rectangle<float> actionRect() { return { kMargin, kActY, kW - 2.0f * kMargin, kActH }; }
juce::Rectangle<float> plotRect() { const auto r = actionRect(); return { r.getX() + 70.0f, r.getY() + 36.0f, r.getWidth() - 140.0f, 160.0f }; }   // the box (8 px bigger) runs y + 28 .. y + 204; PAST / NOW below it
juce::Rectangle<float> pillRect (int i)           // the title pill of a panel: it is also the on / off switch of the stage
{
    const auto r = panelRect (i);
    const float w = textWidth (ui::font (18.0f, true).withExtraKerningFactor (0.08f), kPanelName[i]) + 38.0f;
    return juce::Rectangle<float> (w, 22.0f).withCentre ({ r.getCentreX(), r.getY() });
}
juce::Point<float> knobCentre (int panel, int k, int n)
{
    const auto r = panelRect (panel);
    const float cx = panel == 3 ? r.getX() + 121.0f : r.getCentreX();       // OUTPUT: knobs at x + 66 and x + 176, the dB meters on the right
    return { cx + kKnobPitch * ((float) k - 0.5f * (float) (n - 1)), r.getY() + 92.0f };
}
// bars x + 238 .. x + 280, scale labels to about x + 307: 24 px padding left of the first knob's ticks and 23 right of the labels; 110 high: the bars end on
// the MIX / LEVEL names (y + 154 ..), the peak values sit on the readout row (y + 182)
juce::Point<float> meterOrigin() { const auto r = panelRect (3); return { r.getX() + 238.0f, r.getY() + 46.0f }; }
} // namespace

//==============================================================================
//==============================================================================
DY1AudioProcessorEditor::DY1AudioProcessorEditor (DY1AudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      knobs { { { "gateThresh", 0, 0, 2, "THRESH", 0, {}, {} },  { "gateRelease", 0, 1, 2, "RELEASE", 1, {}, {} },
                { "expThresh", 1, 0, 3, "THRESH", 0, {}, {} },   { "expRatio", 1, 1, 3, "RATIO", 2, {}, {} },     { "expRelease", 1, 2, 3, "RELEASE", 1, {}, {} },
                { "attack", 2, 0, 2, "ATTACK", 3, {}, {} },      { "sustain", 2, 1, 2, "SUSTAIN", 3, {}, {} },
                { "mix", 3, 0, 2, "MIX", 4, {}, {} },            { "level", 3, 1, 2, "LEVEL", 5, {}, {} } } },
      powerAtt (*p.apvts.getParameter ("power"),  [this] (float) { repaint(); }, nullptr),
      gateAtt  (*p.apvts.getParameter ("gateOn"), [this] (float) { repaint(); }, nullptr),
      expAtt   (*p.apvts.getParameter ("expOn"),  [this] (float) { repaint(); }, nullptr),
      transAtt (*p.apvts.getParameter ("transOn"), [this] (float) { repaint(); }, nullptr)
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
    powerAtt.sendInitialUpdate(); gateAtt.sendInitialUpdate(); expAtt.sendInitialUpdate(); transAtt.sendInitialUpdate();

    grain = ui::makeGrain();

    constrainer.setSizeLimits (720, 414, 2400, 1380);
    constrainer.setFixedAspectRatio ((double) kW / (double) kH);
    setConstrainer (&constrainer);
    setResizable (true, true);
    setSize ((int) kW, (int) kH);
    layoutKnobs();
    startTimerHz (30);
}

DY1AudioProcessorEditor::~DY1AudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void DY1AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    midiUI.paintMarks (g, kBody);
}

void DY1AudioProcessorEditor::timerCallback()
{
    meter.update (proc.outMeter);
    repaint (panelRect (3).withTrimmedLeft (panelRect (3).getWidth() - 110.0f).expanded (2.0f).transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer());
    if (midiUI.needsRepaint()) { midiUI.tick(); repaint(); }
    if (barState.copiedAt != 0) { repaint (juce::Rectangle<float> (0.0f, 30.0f, 900.0f, 50.0f).transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer()); if (! barState.copiedRecently()) barState.copiedAt = 0; }
    for (auto& k : knobs) k.slider.setAlpha (stageOn (k.panel) ? 1.0f : 0.4f);       // the knobs of a stage that is off are dimmed
    repaint (actionRect().expanded (4.0f).transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer());     // the display moves
}

void DY1AudioProcessorEditor::toggle (const char* id)
{
    auto* par = proc.apvts.getParameter (id);
    par->beginChangeGesture();
    par->setValueNotifyingHost (par->getValue() > 0.5f ? 0.0f : 1.0f);
    par->endChangeGesture();
}

void DY1AudioProcessorEditor::layoutKnobs()
{
    const float sc = scale(), d = 2.0f * (kKnobR + kPad) * sc;
    for (auto& k : knobs)
        k.slider.setBounds (juce::Rectangle<float> (d, d).withCentre (knobCentre (k.panel, k.index, k.count) * sc).toNearestInt());
}

void DY1AudioProcessorEditor::resized() { layoutKnobs(); }

//==============================================================================
// ACTION display: the input level (grey area, dB scale on the left) and the gain of the chain (orange line, scale on the right) over the last
// five seconds, left = past, right = now; the dashed lines are the gate and expander thresholds on the input scale.
void DY1AudioProcessorEditor::drawAction (juce::Graphics& g)
{
    const auto r = plotRect();
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (r.expanded (8.0f), 10.0f);
    const auto yIn = [&] (float db) { return r.getBottom() - r.getHeight() * (std::clamp (db, -80.0f, 0.0f) + 80.0f) / 80.0f; };
    const auto yGain = [&] (float db) { return r.getBottom() - r.getHeight() * (std::clamp (db, -60.0f, 12.0f) + 60.0f) / 72.0f; };
    for (float db : { -60.0f, -40.0f, -20.0f, 0.0f })
    {
        g.setColour (kInk.withAlpha (db == 0.0f ? 0.25f : 0.1f));
        g.drawLine (r.getX(), yIn (db), r.getRight(), yIn (db), 1.0f);
        drawText (g, juce::String ((int) db), r.getX() - 24.0f, yIn (db), 13.0f, kInk.withAlpha (0.55f), false, 0.0f);
    }
    for (float db : { -60.0f, -40.0f, -20.0f, 0.0f })
        drawText (g, juce::String::formatted ("%+d", (int) db), r.getRight() + 26.0f, yGain (db), 13.0f, kBody.withAlpha (0.7f), false, 0.0f);
    drawText (g, "INPUT dB", r.getX() - 24.0f, r.getY() - 14.0f, 13.0f, kInk.withAlpha (0.5f), true, 0.1f);
    drawText (g, "GAIN dB", r.getRight() + 26.0f, r.getY() - 14.0f, 13.0f, kBody.withAlpha (0.8f), true, 0.1f);
    drawText (g, "PAST", r.getX() + 24.0f, r.getBottom() + 22.0f, 13.0f, kInk.withAlpha (0.55f), true, 0.14f);
    drawText (g, "NOW", r.getRight() - 16.0f, r.getBottom() + 22.0f, 13.0f, kInk.withAlpha (0.55f), true, 0.14f);

    const auto& eng = proc.getEngine();
    const int pos = eng.histPos.load (std::memory_order_acquire);
    juce::Path area, line;
    area.startNewSubPath (r.getX(), r.getBottom());
    for (int k = 0; k < dy1::kHistory; ++k)
    {
        const int idx = (pos + k) % dy1::kHistory;
        const float x = r.getX() + r.getWidth() * (float) k / (float) (dy1::kHistory - 1);
        area.lineTo (x, yIn (eng.histIn[idx].load (std::memory_order_relaxed)));
        const float y = yGain (eng.histGain[idx].load (std::memory_order_relaxed));
        if (k == 0) line.startNewSubPath (x, y); else line.lineTo (x, y);
    }
    area.lineTo (r.getRight(), r.getBottom());
    area.closeSubPath();
    g.setColour (kInk.withAlpha (0.2f));
    g.fillPath (area);
    g.setColour (kInk.withAlpha (0.45f));
    g.strokePath (area, juce::PathStrokeType (1.2f));
    g.setColour (kBody);
    g.strokePath (line, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float dash[2] = { 6.0f, 5.0f };
    const auto threshold = [&] (const char* id, const char* label, bool on)
    {
        if (! on) return;
        const float y = yIn (proc.apvts.getRawParameterValue (id)->load());
        g.setColour (kBody.withAlpha (0.8f));
        g.drawDashedLine (juce::Line<float> (r.getX(), y, r.getRight(), y), dash, 2, 1.4f);
        drawText (g, label, r.getX() + 8.0f, y - 10.0f, 13.0f, kBody, true, 0.12f, juce::Justification::centredLeft);
    };
    threshold ("gateThresh", "GATE", isOn ("gateOn") && isOn ("power"));
    threshold ("expThresh", "EXPANDER", isOn ("expOn") && isOn ("power"));
}

//==============================================================================
void DY1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));

    ui::paintBody (g, kPal, kW, kH, grain);
    ui::paintHeader (g, kPal, "DY-1", "DYNAMIC SHAPER", "TRANSIENT SCULPTING", isOn ("power"));
    ui::paintPresetBar (g, kPal, proc.presets, proc.undoManager, kBarY, barState, false, 6.0f);

    // panels: charcoal, light outline, title pill on the top edge (the pill of a stage is its on / off switch: lit = on)
    for (int i = 0; i < 4; ++i)
        ui::paintPanel (g, kPal, panelRect (i), kPanelName[i], i == 3 ? ui::Pill::Plain : stageOn (i) ? ui::Pill::On : ui::Pill::Off);
    {
        const auto r = actionRect();
        ui::paintPanel (g, kPal, r, "ACTION");
        const float w = textWidth (ui::font (18.0f, true).withExtraKerningFactor (0.08f), "ACTION") + 38.0f;
        const auto pill = juce::Rectangle<float> (w, 22.0f).withCentre ({ r.getCentreX(), r.getY() });
        // EXT lamp: lit while the host's sidechain input is the key of the gate, expander and transient detectors
        const bool ext = proc.extKey.load (std::memory_order_relaxed);
        const auto lamp = juce::Rectangle<float> (10.0f, 10.0f).withCentre ({ pill.getRight() + 30.0f, r.getY() + 1.0f });
        g.setColour (kPanel);
        g.fillRoundedRectangle (juce::Rectangle<float> (64.0f, 22.0f).withCentre ({ lamp.getCentreX() + 21.0f, lamp.getCentreY() }), 11.0f);
        g.setColour (ext ? kBody : kBody.withAlpha (0.25f));
        g.fillEllipse (lamp);
        drawText (g, "EXT", lamp.getRight() + 6.0f, lamp.getCentreY(), 13.0f, kBody.withAlpha (ext ? 0.95f : 0.55f), true, 0.1f, juce::Justification::centredLeft);
    }
    drawAction (g);

    // knobs: recessed ring, 11 ticks, legends, name and value, and the value ring
    for (auto& k : knobs)
    {
        const float dim = stageOn (k.panel) ? 1.0f : 0.4f;
        const auto c = knobCentre (k.panel, k.index, k.count);
        static const char* lo[6] = { "-80", "10", "1", "-100", "0", "-24" }, * hi[6] = { "-10", "800", "10", "+100", "100", "+12" };
        const double v = k.slider.getValue();
        juce::String txt;
        switch (k.kind)
        {
            case 0: txt = juce::String::formatted ("%d dB", (int) std::lround (v)); break;
            case 1: txt = juce::String ((int) std::lround (v)) + " ms"; break;
            case 2: txt = juce::String (v, 1) + ":1"; break;
            case 3: txt = juce::String::formatted ("%+d %%", (int) std::lround (v)); break;
            case 4: txt = juce::String ((int) std::lround (v)) + " %"; break;
            default: txt = juce::String::formatted ("%+d dB", (int) std::lround (v)); break;
        }
        ui::paintKnob (g, kPal, c, k.name, txt, lo[k.kind], hi[k.kind], kPanelY + 154.0f, kPanelY + 182.0f, dim);
        ui::paintValueArc (g, c, kKnobR, (float) k.slider.valueToProportionOfLength (k.slider.getValue()), kBody.withMultipliedAlpha (dim));
    }
    meter.paint (g, meterOrigin().x, meterOrigin().y, isOn ("power"), kBody, kInk, 110.0f, 26.0f, 13.0f);
    ui::paintFooter (g, kPal, kActY + kActH + 35.0f);
}

void DY1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / scale();
    proc.undoManager.beginNewTransaction();
    if (ui::powerRect().contains (p)) { toggle ("power"); return; }
    if (ui::presetBarMouseDown (p, *this, proc.presets, proc.undoManager, kBarY, scale(), barState, [this] { repaint(); }, false)) return;
    static const char* ids[3] = { "gateOn", "expOn", "transOn" };
    for (int i = 0; i < 3; ++i)
        if (pillRect (i).expanded (6.0f, 8.0f).contains (p)) { toggle (ids[i]); return; }
}
