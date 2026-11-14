// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "ui/Presets.h"

namespace
{
using vibr::ui::drawText;
namespace ui = vibr::ui;

constexpr float kW = 1200.0f, kH = 690.0f, kRow1Y = 134.0f, kRow1H = 247.0f, kRow2Y = 405.0f, kRow2H = 216.0f, kKnobR = 24.0f, kPad = 5.0f, kBarY = 39.0f;
const ui::Palette kPal { juce::Colour (122, 145, 176), juce::Colour (14, 20, 30) };            // the Reverse colour #7A91B0 and its print
const juce::Colour kIce (143, 211, 244);

// panels: 0 CONTROLS (mode tabs), 1 OUTPUT, 2 SLICES (display)
constexpr float kPanelX[3] = { 20.0f, 730.0f, 20.0f }, kPanelW[3] = { 700.0f, 450.0f, 1160.0f };
juce::Rectangle<float> panelRect (int i) { return { kPanelX[i], i < 2 ? kRow1Y : kRow2Y, kPanelW[i], i < 2 ? kRow1H : kRow2H }; }
juce::Point<float> knobCentre (int panel, int k, int n)
{
    const auto r = panelRect (panel);
    return { r.getCentreX() + (panel == 0 ? 112.0f : 108.0f) * ((float) k - 0.5f * (float) (n - 1)), r.getY() + 92.0f };
}
juce::Rectangle<float> syncRect() { const auto r = panelRect (0); return { r.getX() + 30.0f, r.getY() + 204.0f, 120.0f, 30.0f }; }
juce::Rectangle<float> freezeRect() { const auto r = panelRect (0); return { r.getX() + 160.0f, r.getY() + 204.0f, 160.0f, 30.0f }; }
juce::Rectangle<float> plotRect() { const auto r = panelRect (2); return { r.getX() + 24.0f, r.getY() + 34.0f, r.getWidth() - 48.0f, r.getHeight() - 56.0f }; }
const std::vector<juce::String> kModeTabs { "REVERSE", "SCRUB", "STUTTER", "GLITCH", "REPEAT" };
}

//==============================================================================
RV1AudioProcessorEditor::RV1AudioProcessorEditor (RV1AudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      knobs { { { "length", 0, 0, 5, "LENGTH", 0, {}, {} },   { "speed", 0, 1, 5, "SPEED", 1, {}, {} },     { "smooth", 0, 2, 5, "SMOOTH", 0, {}, {} },
                { "random", 0, 3, 5, nullptr, 0, {}, {} },    { "repeats", 0, 4, 5, nullptr, 0, {}, {} },
                { "mix", 1, 0, 2, "MIX", 0, {}, {} },         { "level", 1, 1, 2, "LEVEL", 2, {}, {} } } },
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
    startTimerHz (20);
}

RV1AudioProcessorEditor::~RV1AudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void RV1AudioProcessorEditor::timerCallback()
{
    scroll += 0.05f;
    repaint (panelRect (2).expanded (4.0f).transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer());
    if (barState.copiedAt != 0) { repaint (juce::Rectangle<float> (0.0f, 30.0f, 900.0f, 50.0f).transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer()); if (! barState.copiedRecently()) barState.copiedAt = 0; }
}

void RV1AudioProcessorEditor::toggle (const char* id)
{
    auto* par = proc.apvts.getParameter (id);
    par->beginChangeGesture(); par->setValueNotifyingHost (par->getValue() > 0.5f ? 0.0f : 1.0f); par->endChangeGesture();
}
void RV1AudioProcessorEditor::setChoice (const char* id, int index)
{
    auto* par = proc.apvts.getParameter (id);
    par->beginChangeGesture(); par->setValueNotifyingHost (par->convertTo0to1 ((float) index)); par->endChangeGesture();
}

void RV1AudioProcessorEditor::layoutKnobs()
{
    const float sc = scale(), d = 2.0f * (kKnobR + kPad) * sc;
    for (auto& k : knobs)
        k.slider.setBounds (juce::Rectangle<float> (d, d).withCentre (knobCentre (k.panel, k.index, k.count) * sc).toNearestInt());
}
void RV1AudioProcessorEditor::resized() { layoutKnobs(); }

//==============================================================================
// SLICES: the audio is cut into slices of LENGTH. Each slice is drawn as the path its read position takes through the captured audio (top = newest): REVERSE runs
// backwards from the newest sound at SPEED, SCRUB wanders by the scrub amount, STUTTER and REPEAT keep playing the same piece, GLITCH jumps to a random place per slice.
// The wander (RANDOM / SCRUB / CHAOS / JITTER) moves the starts around; REPEATS sets how many slices keep the same start.
void RV1AudioProcessorEditor::drawSlices (juce::Graphics& g)
{
    const auto r = plotRect();
    g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (r.expanded (8.0f), 10.0f);
    const int mode = currentMode();
    const bool power = isOn ("power"), freeze = isOn ("freeze");
    const float length = value ("length") * 0.01f, speed = rv1::speedRatio (value ("speed") * 0.01f), wander = value ("random") * 0.01f, repeats = value ("repeats") * 0.01f, smooth = value ("smooth") * 0.01f;
    const float sliceMs = isOn ("sync") ? 60000.0f / 120.0f * rv1::divisionBeats (rv1::divisionIndex (length)) : rv1::lengthMs (length);
    const float spanMs = 3000.0f, x0 = r.getX() + 4.0f, w = r.getWidth() - 8.0f;
    const auto xOf = [&] (float ms) { return x0 + w * ms / spanMs; };
    g.setColour (kPal.ink.withAlpha (0.1f));
    for (float s = 0.0f; s <= 3.0f; s += 0.5f) { g.drawLine (xOf (s * 1000.0f), r.getY() + 4.0f, xOf (s * 1000.0f), r.getBottom() - 4.0f, 1.0f); drawText (g, juce::String (s, 1) + " s", xOf (s * 1000.0f), r.getBottom() + 12.0f, 10.0f, kPal.ink.withAlpha (0.5f), false, 0.0f); }
    juce::Random rng (7 + (int) scroll);                                                                       // the random places change slowly
    const float top = r.getY() + 10.0f, bottom = r.getBottom() - 10.0f, h = bottom - top;
    float start = 0.0f; int held = 0;
    const int keep = 1 + (int) std::round (repeats * 5.0f);
    for (float t = 0.0f; t < spanMs - 1.0f; t += sliceMs)
    {
        const float end = std::min (spanMs, t + sliceMs), xa = xOf (t), xb = xOf (end);
        if (held <= 0) { start = (mode == rv1::Glitch ? rng.nextFloat() : wander * 0.6f * (rng.nextFloat() - 0.5f) + 0.5f * (mode == rv1::Reverse ? 0.0f : 1.0f)); held = keep; }
        --held;
        float y0, y1;
        const float span = std::min (1.0f, speed * 0.5f);
        if (mode == rv1::Reverse || mode == rv1::Scrub) { y0 = std::clamp (start, 0.0f, 1.0f) * 0.2f; y1 = y0 + span; }                    // backwards: from the newest sound down into the older
        else { y0 = 1.0f - std::clamp (start, 0.0f, 1.0f) * 0.7f; y1 = y0 - 0.25f * speed; }
        y1 = std::clamp (y1, 0.0f, 1.0f); y0 = std::clamp (y0, 0.0f, 1.0f);
        juce::Path slice;
        slice.startNewSubPath (xa + 1.0f, top + h * y0);
        slice.lineTo (xb - 1.0f, top + h * y1);
        g.setColour (freeze ? kIce : kPal.body);
        g.strokePath (slice, juce::PathStrokeType (2.0f + 3.0f * smooth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (kPal.ink.withAlpha (0.25f));
        g.drawLine (xb, r.getY() + 4.0f, xb, r.getBottom() - 4.0f, 1.0f);
    }
    drawText (g, juce::String (rv1::modeName (mode)) + "     SLICE " + (sliceMs >= 1000.0f ? juce::String (sliceMs * 0.001f, 2) + " S" : juce::String ((int) std::lround (sliceMs)) + " MS") + "     SPEED " + juce::String (speed, 2) + "X",
              r.getX() + 8.0f, r.getY() + 14.0f, 10.0f, kPal.ink.withAlpha (0.6f), true, 0.12f, juce::Justification::centredLeft);
    if (freeze) drawText (g, "FROZEN: THE SLICES KEEP PLAYING WHAT WAS CAPTURED", r.getRight() - 8.0f, r.getY() + 14.0f, 10.0f, kIce, true, 0.12f, juce::Justification::centredRight);
    (void) power;
}

//==============================================================================
void RV1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    ui::paintBody (g, kPal, kW, kH, grain);
    ui::paintHeader (g, kPal, "RV-1", "REVERSE LAB", "TIME MANIPULATION", isOn ("power"));
    ui::paintPresetBar (g, kPal, proc.presets, proc.undoManager, kBarY, barState, false);

    const int mode = currentMode();
    ui::paintPanel (g, kPal, panelRect (0), {});
    ui::paintTabs (g, kPal, panelRect (0), kModeTabs, mode);
    ui::paintPanel (g, kPal, panelRect (1), "OUTPUT");
    ui::paintPanel (g, kPal, panelRect (2), "SLICES");
    drawSlices (g);

    ui::paintFlatButton (g, kPal, syncRect(), "SYNC", isOn ("sync"), 11.0f);
    {   // FREEZE has its own ice colour
        const auto b = freezeRect(); const bool on = isOn ("freeze");
        if (on) { g.setColour (kIce); g.fillRoundedRectangle (b, 9.0f); }
        g.setColour (on ? kIce : kPal.ink.withAlpha (0.4f)); g.drawRoundedRectangle (b.reduced (0.5f), 9.0f, 1.3f);
        drawText (g, "FREEZE", b.getCentreX(), b.getCentreY(), 11.0f, on ? kPal.dark : kPal.ink.withAlpha (0.9f), true, 0.1f);
    }

    for (auto& k : knobs)
    {
        const double v = k.slider.getValue();
        juce::String txt, lo = "0", hi = "100";
        if (k.kind == 2) { txt = juce::String::formatted ("%+d dB", (int) std::lround (v)); lo = "-24"; hi = "+12"; }
        else if (k.kind == 1) { txt = juce::String (rv1::speedRatio ((float) v * 0.01f), 2) + "x"; lo = ".25x"; hi = "4x"; }
        else if (k.name != nullptr && juce::String (k.name) == "LENGTH")
        {
            const float ms = rv1::lengthMs ((float) v * 0.01f);
            txt = isOn ("sync") ? juce::String (rv1::divisionName (rv1::divisionIndex ((float) v * 0.01f))) : ms >= 1000.0f ? juce::String (ms * 0.001f, 2) + " s" : juce::String ((int) std::lround (ms)) + " ms";
            lo = "20 ms"; hi = "2 s";
        }
        else txt = juce::String ((int) std::lround (v)) + " %";
        ui::paintKnob (g, kPal, knobCentre (k.panel, k.index, k.count), k.name != nullptr ? k.name : rv1::chaosName (mode, k.index == 3 ? 0 : 1), txt, lo, hi, panelRect (k.panel).getY() + 154.0f, panelRect (k.panel).getY() + 182.0f);
    }
    ui::paintFooter (g, kPal, kRow2Y + kRow2H + 35.0f);
}

void RV1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / scale();
    proc.undoManager.beginNewTransaction();
    if (ui::powerRect().contains (p)) { toggle ("power"); return; }
    if (ui::presetBarMouseDown (p, *this, proc.presets, proc.undoManager, kBarY, scale(), barState, [this] { repaint(); }, false)) { layoutKnobs(); return; }
    const auto tabs = ui::tabRects (panelRect (0), kModeTabs);
    for (int i = 0; i < (int) tabs.size(); ++i)
        if (tabs[(size_t) i].expanded (3.0f).contains (p)) { setChoice ("mode", i); repaint(); return; }
    if (freezeRect().expanded (3.0f).contains (p)) { toggle ("freeze"); repaint(); return; }
    if (syncRect().expanded (3.0f).contains (p)) { toggle ("sync"); repaint(); return; }
}
