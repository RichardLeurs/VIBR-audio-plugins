// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "ui/Presets.h"

namespace
{
namespace ui = vibr::ui;
using ui::drawText;

// layout units: 1200 x 690 (style.md: 20 side margin, 10 gap, panels 30 below the rule, 24 between the rows)
constexpr float kW = 1200.0f, kH = 690.0f, kMargin = 20.0f, kBarY = 39.0f;
constexpr float kPanelY = 134.0f, kPanelH = 236.0f, kKnobR = 24.0f, kPad = 5.0f, kSeqY = 394.0f, kSeqH = 227.0f;
constexpr float kPanelX[5] = { 20.0f, 184.0f, 348.0f, 660.0f, 880.0f }, kPanelW[5] = { 154.0f, 154.0f, 302.0f, 210.0f, 300.0f };      // 10 between the panels, 20 .. 1180
constexpr const char* kPanelName[5] = { "FILTER", "SOURCE", "TONE", "MOTION", "OUTPUT" };

const ui::Palette kPal { juce::Colour (199, 125, 74), juce::Colour (36, 20, 10) };                // the Motion Filter colour #C77D4A and its print

juce::Rectangle<float> panelRect (int i) { return { kPanelX[i], kPanelY, kPanelW[i], kPanelH }; }
juce::Rectangle<float> seqRect() { return { kMargin, kSeqY, kW - 2.0f * kMargin, kSeqH }; }
// buttons 15 inside the panel edges: the five filter types, the three sources and SYNC under a divider (24 clear below, 23 below the title pill)
juce::Rectangle<float> typeRect (int m) { const auto r = panelRect (0); return { r.getX() + 15.0f, r.getY() + 34.0f + (float) m * 37.0f, r.getWidth() - 30.0f, 30.0f }; }
juce::Rectangle<float> sourceRect (int m) { const auto r = panelRect (1); return { r.getX() + 15.0f, r.getY() + 34.0f + (float) m * 42.0f, r.getWidth() - 30.0f, 32.0f }; }
juce::Rectangle<float> syncRect() { const auto r = panelRect (1); return { r.getX() + 15.0f, r.getY() + 180.0f, r.getWidth() - 30.0f, 32.0f }; }
juce::Rectangle<float> barsArea() { const auto r = seqRect(); return { r.getX() + 30.0f, r.getY() + 42.0f, r.getWidth() - 60.0f, 130.0f }; }
juce::Rectangle<float> barRect (int i)
{
    const auto a = barsArea();
    const float cell = a.getWidth() / (float) mf1::kNumSteps;
    return { a.getX() + cell * (float) i + (cell - 84.0f) * 0.5f, a.getY(), 84.0f, a.getHeight() };
}
juce::Point<float> knobCentre (int panel, int k, int n)
{
    const auto r = panelRect (panel);
    // TONE: 96 apart, the first and last centre 55 inside the panel (ticks reach 42: 13 clear); MOTION: the pair 100 apart round the middle (13 clear each side);
    // OUTPUT: centres x + 55 and x + 155 (100 apart), the dB meters on the right: 13 padding left of the first ticks and right of the scale labels
    if (panel == 4) return { r.getX() + 55.0f + 100.0f * (float) k, r.getY() + 100.0f };
    if (panel == 3) return { r.getCentreX() + 100.0f * ((float) k - 0.5f), r.getY() + 100.0f };
    const float x = n > 1 ? r.getX() + 55.0f + (r.getWidth() - 110.0f) * (float) k / (float) (n - 1) : r.getCentreX();
    return { x, r.getY() + 100.0f };
}
juce::Point<float> meterOrigin() { const auto r = panelRect (4); return { r.getX() + 217.0f, r.getY() + 54.0f }; }       // bars 110 high: y + 54 .. y + 164 (the MIX / LEVEL name row), peak values on the value row (y + 192)
} // namespace

//==============================================================================
MF1AudioProcessorEditor::MF1AudioProcessorEditor (MF1AudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      knobs { { { "cutoff", 2, 0, 3, "CUTOFF", 1, {}, {} },    { "res", 2, 1, 3, "RESONANCE", 0, {}, {} },
                { "depth", 2, 2, 3, "DEPTH", 0, {}, {} },
                { "m1", 3, 0, 2, nullptr, 2, {}, {} },         { "m2", 3, 1, 2, nullptr, 3, {}, {} },
                { "mix", 4, 0, 2, "MIX", 0, {}, {} },          { "level", 4, 1, 2, "LEVEL", 4, {}, {} } } },
      powerAtt  (*p.apvts.getParameter ("power"),  [this] (float) { repaint(); }, nullptr),
      typeAtt   (*p.apvts.getParameter ("type"),   [this] (float) { repaint(); }, nullptr),
      sourceAtt (*p.apvts.getParameter ("source"), [this] (float) { repaint(); }, nullptr),
      syncAtt   (*p.apvts.getParameter ("sync"),   [this] (float) { repaint(); }, nullptr)
{
    setLookAndFeel (&laf);
    for (auto& k : knobs)
    {
        auto& s = k.slider;
        ui::setupKnob (s, *this);
        s.setTextValueSuffix (k.kind == 4 ? " dB" : " %");
        auto* par = proc.apvts.getParameter (k.id);
        s.setDoubleClickReturnValue (true, par->convertFrom0to1 (par->getDefaultValue()));
        s.onDragStart = [this] { proc.undoManager.beginNewTransaction(); };
        addAndMakeVisible (s);
        k.att = std::make_unique<juce::SliderParameterAttachment> (*dynamic_cast<juce::RangedAudioParameter*> (par), s, nullptr);
        midiUI.add (s, k.id);
        s.onValueChange = [this] { repaint(); };
    }
    powerAtt.sendInitialUpdate();
    typeAtt.sendInitialUpdate();
    sourceAtt.sendInitialUpdate();
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

MF1AudioProcessorEditor::~MF1AudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void MF1AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    midiUI.paintMarks (g, kPal.body);
}

void MF1AudioProcessorEditor::timerCallback()
{
    const auto toScreen = [this] (juce::Rectangle<float> r) { return r.transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer(); };
    meter.update (proc.outMeter);
    repaint (toScreen (panelRect (4).withTrimmedLeft (panelRect (4).getWidth() - 150.0f).expanded (2.0f)));
    if (midiUI.needsRepaint()) { midiUI.tick(); repaint(); }

    // a preset, undo, A / B or automation changed the source (the MOTION knobs are renamed) or the steps: redraw
    int stepsKey = 0;
    for (int i = 0; i < mf1::kNumSteps; ++i) stepsKey = stepsKey * 31 + (int) std::lround (proc.apvts.getRawParameterValue ("step" + juce::String (i + 1))->load());
    const int shown = choice ("source") * 8 + choice ("type");
    if (shown != shownKey || stepsKey != this->stepsKey) { shownKey = shown; this->stepsKey = stepsKey; layoutKnobs(); repaint(); }

    const int st = proc.currentStep.load();
    if (st != lastStep)
    {
        lastStep = st;
        repaint (toScreen (seqRect().expanded (4.0f)));
    }
    if (barState.copiedAt != 0) { repaint (toScreen (juce::Rectangle<float> (0.0f, 30.0f, 900.0f, 50.0f))); if (! barState.copiedRecently()) barState.copiedAt = 0; }
}

void MF1AudioProcessorEditor::toggle (const char* id)
{
    auto* par = proc.apvts.getParameter (id);
    par->beginChangeGesture();
    par->setValueNotifyingHost (par->getValue() > 0.5f ? 0.0f : 1.0f);
    par->endChangeGesture();
}

void MF1AudioProcessorEditor::layoutKnobs()
{
    const float sc = scale(), d = 2.0f * (kKnobR + kPad) * sc;
    for (auto& k : knobs)
        k.slider.setBounds (juce::Rectangle<float> (d, d).withCentre (knobCentre (k.panel, k.index, k.count) * sc).toNearestInt());
}

void MF1AudioProcessorEditor::resized() { layoutKnobs(); }

//==============================================================================
void MF1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    ui::paintBody (g, kPal, kW, kH, grain);
    ui::paintHeader (g, kPal, "MF-1", "MOTION FILTER", "RHYTHMIC MOVEMENT", isOn ("power"));
    ui::paintPresetBar (g, kPal, proc.presets, proc.undoManager, kBarY, barState, false, 6.0f);

    for (int i = 0; i < 5; ++i) ui::paintPanel (g, kPal, panelRect (i), kPanelName[i]);
    ui::paintPanel (g, kPal, seqRect(), "SEQUENCER");

    const int type = choice ("type"), source = choice ("source");
    for (int m = 0; m < mf1::kNumTypes; ++m) ui::paintFlatButton (g, kPal, typeRect (m), mf1::typeName (m), m == type, 14.0f, 1.0f, 10.0f);
    for (int m = 0; m < mf1::kNumSources; ++m) ui::paintFlatButton (g, kPal, sourceRect (m), mf1::sourceName (m), m == source, 14.0f, 1.0f, 10.0f);
    {
        const auto r = panelRect (1);
        g.setColour (kPal.ink.withAlpha (0.35f));
        g.fillRect (r.getX() + 15.0f, r.getY() + 163.0f, r.getWidth() - 30.0f, 1.0f);
        ui::paintFlatButton (g, kPal, syncRect(), "SYNC", isOn ("sync"), 14.0f, 1.0f, 10.0f, 0.16f);
    }

    // knobs: recessed ring, 11 ticks, legends, name, value and the value ring
    for (auto& k : knobs)
    {
        const auto c = knobCentre (k.panel, k.index, k.count);
        const char* lo = k.kind == 4 ? "-24" : k.kind == 1 ? "LOW" : "0";
        const char* hi = k.kind == 4 ? "+12" : k.kind == 1 ? "HIGH" : k.kind == 0 ? "100" : "10";
        const float y0 = panelRect (k.panel).getY();
        ui::paintKnob (g, kPal, c, k.name != nullptr ? k.name : mf1::motionName (source, k.index), knobText (k.kind, k.slider.getValue()), lo, hi, y0 + 164.0f, y0 + 192.0f);
        ui::paintValueArc (g, c, kKnobR, (float) k.slider.valueToProportionOfLength (k.slider.getValue()), kPal.body);
    }
    meter.paint (g, meterOrigin().x, meterOrigin().y, isOn ("power"), kPal.body, kPal.ink, 110.0f, 28.0f, 13.0f);

    // sequencer: eight bars (dim unless the source is STEPS), the playing step lit
    {
        const bool active = source == mf1::Steps;
        const int cur = proc.currentStep.load();
        for (int i = 0; i < mf1::kNumSteps; ++i)
        {
            const auto b = barRect (i);
            const float v = proc.apvts.getRawParameterValue ("step" + juce::String (i + 1))->load() * 0.01f;
            const bool now = active && i == cur;
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.fillRoundedRectangle (b, 8.0f);
            g.setColour (juce::Colours::white.withAlpha (0.1f));
            g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.0f);
            const float h = juce::jmax (8.0f, v * b.getHeight());
            const auto fill = juce::Rectangle<float> (b.getX() + 4.0f, b.getBottom() - h - 0.0f, b.getWidth() - 8.0f, h - 4.0f > 4.0f ? h - 4.0f : 4.0f).withBottomY (b.getBottom() - 4.0f);
            if (now) { g.setColour (kPal.body.withAlpha (0.25f)); g.fillRoundedRectangle (fill.expanded (4.0f), 8.0f); }
            g.setColour (active ? (now ? kPal.body.brighter (0.35f) : kPal.body) : kPal.body.withAlpha (0.35f));
            g.fillRoundedRectangle (fill, 5.0f);
            drawText (g, juce::String (i + 1), b.getCentreX(), b.getBottom() + 18.0f, 14.0f, now ? kPal.body : kPal.ink.withAlpha (0.75f), true, 0.1f);
        }
    }

    ui::paintFooter (g, kPal, kSeqY + kSeqH + 35.0f);
}

juce::String MF1AudioProcessorEditor::knobText (int kind, double v) const
{
    const float f = (float) v * 0.01f;
    switch (kind)
    {
        case 1: { const float hz = mf1::cutoffHz (f); return hz >= 1000.0f ? juce::String (hz * 0.001f, 1) + " kHz" : juce::String ((int) std::lround (hz)) + " Hz"; }
        case 2:
            if (choice ("source") == mf1::Envelope) return juce::String ((int) std::lround (v)) + " %";
            if (isOn ("sync")) return mf1::divisionName (mf1::divisionIndex (f));
            return juce::String (mf1::rateHz (f), 2) + " Hz";
        case 3:
            if (choice ("source") == mf1::Lfo) return mf1::shapeName (f);
            if (choice ("source") == mf1::Envelope) return juce::String ((int) std::lround (mf1::decayMs (f))) + " ms";
            return juce::String ((int) std::lround (v)) + " %";
        case 4: return juce::String::formatted ("%+d dB", (int) std::lround (v));
        default: return juce::String ((int) std::lround (v)) + " %";
    }
}

void MF1AudioProcessorEditor::setChoice (const char* id, int index)
{
    auto* par = proc.apvts.getParameter (id);
    par->beginChangeGesture();
    par->setValueNotifyingHost (par->convertTo0to1 ((float) index));
    par->endChangeGesture();
}

void MF1AudioProcessorEditor::setStep (int step, float value01, bool begin)
{
    auto* par = proc.apvts.getParameter ("step" + juce::String (step + 1));
    if (begin) par->beginChangeGesture();
    par->setValueNotifyingHost (std::round (juce::jlimit (0.0f, 1.0f, value01) * 100.0f) / 100.0f);
}

void MF1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / scale();
    proc.undoManager.beginNewTransaction();
    if (ui::powerRect().contains (p)) { toggle ("power"); return; }
    if (ui::presetBarMouseDown (p, *this, proc.presets, proc.undoManager, kBarY, scale(), barState, [this] { repaint(); }, false)) { layoutKnobs(); return; }
    if (syncRect().expanded (3.0f).contains (p)) { toggle ("sync"); return; }
    for (int m = 0; m < mf1::kNumTypes; ++m)
        if (typeRect (m).expanded (3.0f).contains (p)) { setChoice ("type", m); return; }
    for (int m = 0; m < mf1::kNumSources; ++m)
        if (sourceRect (m).expanded (3.0f).contains (p)) { setChoice ("source", m); return; }
    if (seqRect().contains (p))
        for (int i = 0; i < mf1::kNumSteps; ++i)
        {
            const auto b = barRect (i);
            if (p.x >= b.getX() - 6.0f && p.x <= b.getRight() + 6.0f && p.y >= b.getY() - 8.0f && p.y <= b.getBottom() + 8.0f)
            {
                dragStep = i;
                setStep (i, 1.0f - (p.y - b.getY()) / b.getHeight(), true);
                return;
            }
        }
}

void MF1AudioProcessorEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (dragStep < 0) return;
    const auto p = e.position / scale();
    const auto b = barRect (dragStep);
    setStep (dragStep, 1.0f - (p.y - b.getY()) / b.getHeight(), false);
}

void MF1AudioProcessorEditor::mouseUp (const juce::MouseEvent&)
{
    if (dragStep >= 0) proc.apvts.getParameter ("step" + juce::String (dragStep + 1))->endChangeGesture();
    dragStep = -1;
}
