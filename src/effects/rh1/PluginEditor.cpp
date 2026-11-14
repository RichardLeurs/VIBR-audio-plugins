// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "ui/Presets.h"

namespace
{
using vibr::ui::drawText;
namespace ui = vibr::ui;

constexpr float kW = 1200.0f, kH = 690.0f, kRow1Y = 134.0f, kRow1H = 247.0f, kRow2Y = 405.0f, kRow2H = 216.0f, kKnobR = 24.0f, kPad = 5.0f, kBarY = 39.0f;
const ui::Palette kPal { juce::Colour (50, 196, 201), juce::Colour (9, 35, 36) };            // the Modulation colour #32C4C9 and its print

// panels: 0 CONTROL (source tabs), 1 DESTINATION (destination tabs), 2 PATTERN (display), 3 OUTPUT
constexpr float kPanelX[4] = { 20.0f, 730.0f, 20.0f, 930.0f }, kPanelW[4] = { 700.0f, 450.0f, 900.0f, 250.0f };
juce::Rectangle<float> panelRect (int i) { return { kPanelX[i], i < 2 ? kRow1Y : kRow2Y, kPanelW[i], i < 2 ? kRow1H : kRow2H }; }
juce::Point<float> knobCentre (int panel, int k, int n)
{
    const auto r = panelRect (panel);
    return { r.getCentreX() + 108.0f * ((float) k - 0.5f * (float) (n - 1)), r.getY() + 92.0f };
}
// the buttons of CONTROL: SYNC, the note length, FOLLOW HOST
juce::Rectangle<float> controlButtonRect (int i)
{
    static const float x[3] = { 30.0f, 116.0f, 250.0f }, w[3] = { 80.0f, 128.0f, 150.0f };
    const auto r = panelRect (0);
    return { r.getX() + x[i], r.getY() + 204.0f, w[i], 30.0f };
}
juce::Rectangle<float> typeRect (int i) { const auto r = panelRect (1); return { r.getX() + 16.0f + (float) i * 85.0f, r.getY() + 204.0f, 80.0f, 30.0f }; }
// the PATTERN display: the lane, the traces, the pattern buttons
juce::Rectangle<float> laneRect() { const auto r = panelRect (2); return { r.getX() + 24.0f, r.getY() + 30.0f, r.getWidth() - 48.0f, 82.0f }; }
juce::Rectangle<float> traceRect() { const auto r = panelRect (2); return { r.getX() + 24.0f, r.getY() + 122.0f, r.getWidth() - 48.0f, 48.0f }; }
juce::Rectangle<float> patternButtonRect (int i) { const auto r = panelRect (2); return { r.getX() + 24.0f + (float) i * 77.5f, r.getY() + 180.0f, 72.0f, 24.0f }; }
const std::vector<juce::String> kSourceTabs { "LFO", "ENVELOPE", "STEPS" }, kModeTabs { "FILTER", "TREMOLO", "SLICER" };
}

// kinds: 0 rate, 1 percent, 2 shape, 3 tempo, 4 level dB, 5 value 0 .. 10
//==============================================================================
RH1AudioProcessorEditor::RH1AudioProcessorEditor (RH1AudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      knobs { { { "rate", 0, 0, 5, "RATE", 0, 1 | 4, -1, {}, {} },     { "sens", 0, 0, 5, "SENS", 1, 2, -1, {}, {} },
                { "shape", 0, 1, 5, "SHAPE", 2, 1, -1, {}, {} },       { "decay", 0, 1, 5, "DECAY", 1, 2, -1, {}, {} },       { "glide", 0, 1, 5, "GLIDE", 1, 4, -1, {}, {} },
                { "depth", 0, 2, 5, "DEPTH", 1, 7, -1, {}, {} },       { "swing", 0, 3, 5, "SWING", 1, 7, -1, {}, {} },       { "tempo", 0, 4, 5, "TEMPO", 3, 7, -1, {}, {} },
                { "cutoff", 1, 0, 2, "CUTOFF", 1, 7, 0, {}, {} },      { "res", 1, 1, 2, "RESONANCE", 1, 7, 0, {}, {} },
                { "tremStereo", 1, 0, 2, "STEREO", 1, 7, 1, {}, {} },  { "tremBias", 1, 1, 2, "BIAS", 1, 7, 1, {}, {} },
                { "slAttack", 1, 0, 3, "ATTACK", 5, 7, 2, {}, {} },    { "slDuty", 1, 1, 3, "DUTY", 5, 7, 2, {}, {} },        { "slBalance", 1, 2, 3, "BALANCE", 5, 7, 2, {}, {} },
                { "mix", 3, 0, 2, "MIX", 1, 7, -1, {}, {} },           { "level", 3, 1, 2, "LEVEL", 4, 7, -1, {}, {} } } },
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

RH1AudioProcessorEditor::~RH1AudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void RH1AudioProcessorEditor::timerCallback()
{
    repaint (panelRect (2).expanded (4.0f).transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer());
    if (barState.copiedAt != 0) { repaint (juce::Rectangle<float> (0.0f, 30.0f, 900.0f, 50.0f).transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer()); if (! barState.copiedRecently()) barState.copiedAt = 0; }
}

void RH1AudioProcessorEditor::toggle (const char* id)
{
    auto* par = proc.apvts.getParameter (id);
    par->beginChangeGesture(); par->setValueNotifyingHost (par->getValue() > 0.5f ? 0.0f : 1.0f); par->endChangeGesture();
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
    if (k.panel == 0) return (k.onlySource & (1 << source())) != 0;
    if (k.panel == 1) return k.onlyMode == mode();
    return true;
}

void RH1AudioProcessorEditor::layoutKnobs()
{
    const float sc = scale(), d = 2.0f * (kKnobR + kPad) * sc;
    for (int i = 0; i < (int) knobs.size(); ++i)
    {
        auto& k = knobs[(size_t) i];
        // the knobs of the source panel are placed by their position among those shown
        int idx = k.index, cnt = k.count;
        if (k.panel == 0) { idx = 0; cnt = 0; for (int j = 0; j < (int) knobs.size(); ++j) if (knobs[(size_t) j].panel == 0 && knobShown (j)) { if (j < i) ++idx; ++cnt; } }
        k.slider.setBounds (juce::Rectangle<float> (d, d).withCentre (knobCentre (k.panel, idx, cnt) * sc).toNearestInt());
        k.slider.setVisible (knobShown (i));
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
        if (s % 4 == 0) drawText (g, juce::String (s / 4 + 1), col.getCentreX(), lane.getBottom() + 4.0f, 9.0f, kPal.ink.withAlpha (0.4f), false, 0.0f);
    }
    drawText (g, slicer ? "A STEP AT OR ABOVE 50 % IS ON" : "THE HEIGHT OF A STEP IS ITS VALUE (THE STEPS SOURCE)", lane.getRight() - 4.0f, lane.getY() + 8.0f, 9.0f, kPal.ink.withAlpha (0.4f), true, 0.1f, juce::Justification::centredRight);

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
    drawText (g, "CONTROL (GREY) AND " + juce::String (destName[mode()]) + " OVER FIVE SECONDS", tr.getX() + 8.0f, tr.getY() + 8.0f, 9.0f, kPal.ink.withAlpha (0.5f), true, 0.1f, juce::Justification::centredLeft);
}

//==============================================================================
void RH1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    ui::paintBody (g, kPal, kW, kH, grain);
    ui::paintHeader (g, kPal, "RH-1", "RHYTHM LAB", "RHYTHMIC MOTION", isOn ("power"));
    ui::paintPresetBar (g, kPal, proc.presets, proc.undoManager, kBarY, barState);

    const int m = mode();
    ui::paintPanel (g, kPal, panelRect (0), {});
    ui::paintTabs (g, kPal, panelRect (0), kSourceTabs, source());
    ui::paintPanel (g, kPal, panelRect (1), {});
    ui::paintTabs (g, kPal, panelRect (1), kModeTabs, m);
    ui::paintPanel (g, kPal, panelRect (2), "PATTERN");
    ui::paintPanel (g, kPal, panelRect (3), "OUTPUT");
    drawPattern (g);

    ui::paintFlatButton (g, kPal, controlButtonRect (0), "SYNC", isOn ("sync"), 11.0f);
    ui::paintFlatButton (g, kPal, controlButtonRect (1), rh1::divisionName (choice ("division")), isOn ("sync"), 11.0f, isOn ("sync") ? 1.0f : 0.55f);
    ui::paintFlatButton (g, kPal, controlButtonRect (2), "FOLLOW HOST", isOn ("followHost"), 11.0f);
    for (int v = 0; v < rh1::variantCount (m); ++v) ui::paintFlatButton (g, kPal, typeRect (v), rh1::variantName (m, v), v == variant(), 9.0f);
    for (int i = 0; i < rh1::kNumPatterns; ++i)
    {
        bool same = true;
        for (int s = 0; s < rh1::kSteps; ++s) if ((value (("step" + juce::String (s + 1)).toRawUTF8()) >= 50.0f) != (rh1::patternString (i)[s] == 'x')) same = false;
        ui::paintFlatButton (g, kPal, patternButtonRect (i), juce::String (i + 1), same, 10.0f, 1.0f, 7.0f);
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
    }
    ui::paintFooter (g, kPal, kRow2Y + kRow2H + 35.0f);
}

void RH1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / scale();
    proc.undoManager.beginNewTransaction();
    if (ui::powerRect().contains (p)) { toggle ("power"); return; }
    if (ui::presetBarMouseDown (p, *this, proc.presets, proc.undoManager, kBarY, scale(), barState, [this] { repaint(); })) { layoutKnobs(); return; }
    const auto st = ui::tabRects (panelRect (0), kSourceTabs);
    for (int i = 0; i < (int) st.size(); ++i)
        if (st[(size_t) i].expanded (3.0f).contains (p)) { setChoice ("source", i); layoutKnobs(); repaint(); return; }
    const auto mt = ui::tabRects (panelRect (1), kModeTabs);
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
