// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "ui/Presets.h"
#include <complex>

namespace
{
using vibr::ui::drawText;
namespace ui = vibr::ui;

constexpr float kW = 1200.0f, kH = 690.0f, kRow1Y = 134.0f, kRow1H = 247.0f, kRow2Y = 405.0f, kRow2H = 216.0f, kKnobR = 24.0f, kPad = 5.0f, kBarY = 39.0f;
const ui::Palette kPal { juce::Colour (19, 191, 160), juce::Colour (6, 35, 29) };            // the Modulation colour #13BFA0 and its print
const juce::Colour kBody = kPal.body, kInk = kPal.ink;

// panels: 0 CONTROLS (mode tabs), 1 OUTPUT, 2 MOTION (display)
constexpr float kPanelX[3] = { 20.0f, 730.0f, 20.0f }, kPanelW[3] = { 700.0f, 450.0f, 1160.0f };
juce::Rectangle<float> panelRect (int i) { return { kPanelX[i], i < 2 ? kRow1Y : kRow2Y, kPanelW[i], i < 2 ? kRow1H : kRow2H }; }
juce::Point<float> knobCentre (int panel, int k, int n)
{
    const auto r = panelRect (panel);
    return { r.getCentreX() + 108.0f * ((float) k - 0.5f * (float) (n - 1)), r.getY() + 92.0f };
}
juce::Rectangle<float> variantRect (int v) { const auto r = panelRect (0); return { r.getX() + 30.0f + (float) v * 130.0f, r.getY() + 204.0f, 120.0f, 30.0f }; }
juce::Rectangle<float> syncRect() { const auto r = panelRect (0); return { r.getRight() - 150.0f, r.getY() + 204.0f, 120.0f, 30.0f }; }
juce::Rectangle<float> motionRect() { const auto r = panelRect (2); return { r.getX() + 24.0f, r.getY() + 34.0f, r.getWidth() - 48.0f, r.getHeight() - 56.0f }; }
const std::vector<juce::String> kModeTabs { "CHORUS", "VIBRATO", "FLANGER", "PHASER" };
}

//==============================================================================
ML1AudioProcessorEditor::ML1AudioProcessorEditor (ML1AudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      knobs { { { "rate", 0, 0, 3, "RATE", 0, {}, {} },     { "depth", 0, 1, 3, "DEPTH", 1, {}, {} },    { "extra", 0, 2, 3, nullptr, 1, {}, {} },
                { "mix", 1, 0, 2, "MIX", 1, {}, {} },       { "level", 1, 1, 2, "LEVEL", 2, {}, {} } } },
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
        midiUI.add (k.slider, k.id);
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

ML1AudioProcessorEditor::~ML1AudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void ML1AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    midiUI.paintMarks (g, kBody);
}

void ML1AudioProcessorEditor::timerCallback()
{
    if (midiUI.needsRepaint()) { midiUI.tick(); repaint(); }
    repaint (panelRect (2).expanded (4.0f).transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer());     // the picture moves
    if (barState.copiedAt != 0) { repaint (juce::Rectangle<float> (0.0f, 30.0f, 900.0f, 50.0f).transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer()); if (! barState.copiedRecently()) barState.copiedAt = 0; }
}

void ML1AudioProcessorEditor::toggle (const char* id)
{
    auto* par = proc.apvts.getParameter (id);
    par->beginChangeGesture(); par->setValueNotifyingHost (par->getValue() > 0.5f ? 0.0f : 1.0f); par->endChangeGesture();
}
void ML1AudioProcessorEditor::setChoice (const char* id, int index)
{
    auto* par = proc.apvts.getParameter (id);
    par->beginChangeGesture(); par->setValueNotifyingHost (par->convertTo0to1 ((float) index)); par->endChangeGesture();
}

void ML1AudioProcessorEditor::layoutKnobs()
{
    const float sc = scale(), d = 2.0f * (kKnobR + kPad) * sc;
    for (auto& k : knobs)
        k.slider.setBounds (juce::Rectangle<float> (d, d).withCentre (knobCentre (k.panel, k.index, k.count) * sc).toNearestInt());
}

void ML1AudioProcessorEditor::resized() { layoutKnobs(); }

//==============================================================================
// MOTION display: a live picture of the effect.
//   CHORUS   one lane per voice with the delay of each voice (8 .. 30 ms) sliding along it
//   VIBRATO  the LFO wave with the moving dot, and the pitch bend (cents) it makes
//   FLANGER  the comb filter (frequency response) for the delay and feedback right now
//   PHASER   the notches of the all-pass chain for the sweep position and feedback right now
void ML1AudioProcessorEditor::drawMotion (juce::Graphics& g, int mode, int variant)
{
    const auto r = motionRect();
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (r.expanded (8.0f), 10.0f);
    juce::Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (r.expanded (8.0f).toNearestInt());

    const auto& eng = proc.getEngine();
    const bool power = isOn ("power");
    const float phase = power ? eng.visPhase.load (std::memory_order_relaxed) : 0.0f, hz = power ? eng.visHz.load (std::memory_order_relaxed) : 0.0f;
    const auto param = [this] (const char* id) { return proc.apvts.getRawParameterValue (id)->load() * 0.01f; };
    const float depth = param ("depth"), extra = param ("extra");
    const float x0 = r.getX() + 20.0f, x1 = r.getRight() - 20.0f, w = x1 - x0;
    constexpr float twoPi = juce::MathConstants<float>::twoPi;

    drawText (g, juce::String (hz, 2) + " Hz", r.getX() + 14.0f, r.getY() + 16.0f, 15.0f, kBody, true, 0.06f, juce::Justification::centredLeft);

    if (mode == ml1::Chorus)
    {
        const int voices = ml1::voiceCount (variant);
        const float yTop = r.getY() + 34.0f, laneH = (r.getHeight() - 70.0f) / 4.0f;
        const auto xOf = [&] (float ms) { return x0 + w * (ms - 8.0f) / 22.0f; };
        for (int v = 0; v < voices; ++v)
        {
            const float yc = yTop + laneH * ((float) v + 0.5f) + (float) (4 - voices) * laneH * 0.5f;
            const float ms = ml1::chorusDelayMs (v, ml1::lfoShape (phase + (float) v / (float) voices, false), depth);
            g.setColour (kInk.withAlpha (0.25f));
            g.drawLine (x0, yc, x1, yc, 1.4f);
            g.setColour (kBody.withAlpha (0.2f));
            g.fillRoundedRectangle (juce::Rectangle<float> (xOf (ml1::chorusDelayMs (v, -1.0f, depth)), yc - 5.0f, xOf (ml1::chorusDelayMs (v, 1.0f, depth)) - xOf (ml1::chorusDelayMs (v, -1.0f, depth)), 10.0f), 5.0f);
            g.setColour (kBody);
            g.fillEllipse (juce::Rectangle<float> (16.0f, 16.0f).withCentre ({ xOf (ms), yc }));
        }
        g.setColour (kInk.withAlpha (0.3f));
        g.drawLine (x0, r.getBottom() - 30.0f, x1, r.getBottom() - 30.0f, 1.4f);
        for (float ms : { 8.0f, 19.0f, 30.0f })
        {
            g.drawLine (xOf (ms), r.getBottom() - 30.0f, xOf (ms), r.getBottom() - 24.0f, 1.4f);
            drawText (g, juce::String ((int) ms) + " ms", xOf (ms), r.getBottom() - 12.0f, 11.0f, kInk.withAlpha (0.55f), false, 0.04f);
        }
        drawText (g, juce::String (voices) + " VOICE CHORUS", r.getRight() - 14.0f, r.getY() + 16.0f, 11.0f, kInk.withAlpha (0.5f), true, 0.2f, juce::Justification::centredRight);
    }
    else if (mode == ml1::Vibrato)
    {
        const bool tri = variant == 1;
        const float yc = r.getCentreY() + 2.0f, amp = r.getHeight() * 0.3f;
        juce::Path wave;
        for (int i = 0; i <= 120; ++i)
        {
            const float u = (float) i / 120.0f, y = yc - ml1::lfoShape (u, tri) * amp;
            if (i == 0) wave.startNewSubPath (x0, y); else wave.lineTo (x0 + w * u, y);
        }
        g.setColour (kInk.withAlpha (0.25f));
        g.drawLine (x0, yc, x1, yc, 1.2f);
        g.setColour (kInk.withAlpha (0.6f));
        g.strokePath (wave, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const float lfo = ml1::lfoShape (phase, tri);
        const juce::Point<float> dot (x0 + w * phase, yc - lfo * amp);
        g.setColour (kBody.withAlpha (0.25f));
        g.drawLine (dot.x, yc, dot.x, dot.y, 2.0f);
        g.setColour (kBody);
        g.fillEllipse (juce::Rectangle<float> (16.0f, 16.0f).withCentre (dot));
        // the pitch follows the slope of the delay: -d(delay)/dt (a rising delay lowers the pitch)
        const float slope = tri ? (phase < 0.5f ? 4.0f : -4.0f) : twoPi * std::cos (twoPi * phase);
        const float ratio = 1.0f - 0.0036f * depth * slope * hz;
        const float cents = 1200.0f * std::log2 (std::max (0.2f, ratio));
        drawText (g, juce::String::formatted ("%+d cents", (int) std::lround (power ? cents : 0.0f)), x1 - 6.0f, r.getBottom() - 16.0f, 16.0f, kBody, true, 0.04f, juce::Justification::centredRight);
        drawText (g, tri ? "TRIANGLE PITCH WOBBLE" : "SINE PITCH WOBBLE", r.getRight() - 14.0f, r.getY() + 16.0f, 11.0f, kInk.withAlpha (0.5f), true, 0.2f, juce::Justification::centredRight);
    }
    else
    {
        // frequency response, 20 Hz .. 20 kHz on a log axis, -30 .. +12 dB
        const float yTop = r.getY() + 32.0f, yBot = r.getBottom() - 28.0f;
        const auto yOf = [&] (float db) { return yBot - (yBot - yTop) * (std::clamp (db, -30.0f, 12.0f) + 30.0f) / 42.0f; };
        const float lfo = ml1::lfoShape (phase, false), fs = 48000.0f;
        const float fb = ml1::feedbackAmount (extra) * (mode == ml1::Flanger && variant == 1 ? -1.0f : 1.0f);
        const auto magnitude = [&] (float f) -> float
        {
            const float om = twoPi * f / fs;
            const std::complex<float> z1 = std::polar (1.0f, -om);
            if (mode == ml1::Flanger)
            {
                const float d = ml1::flangerDelayMs (lfo, depth) * 0.001f * fs;
                const std::complex<float> zd = std::polar (1.0f, -om * d);
                return std::abs (0.5f * (1.0f + zd) / (1.0f - fb * zd)) * (1.0f - 0.6f * std::abs (fb));
            }
            const float tn = std::tan (ml1::kPi * std::min (ml1::phaserCentreHz (lfo, depth), 0.45f * fs) / fs), a = (tn - 1.0f) / (tn + 1.0f);
            const std::complex<float> ap = (a + z1) / (1.0f + a * z1);
            std::complex<float> gAll (1.0f, 0.0f);
            for (int k = 0; k < ml1::stageCount (variant); ++k) gAll *= ap;
            const float fbp = 0.95f * fb;                                   // the engine's feedback around the chain (one sample late)
            const std::complex<float> v = gAll / (1.0f - fbp * z1 * gAll);
            return std::abs (0.5f * (1.0f + v)) * (1.0f - 0.5f * fbp);
        };
        const auto xOf = [&] (float f) { return x0 + w * std::log (f / 20.0f) / std::log (1000.0f); };
        g.setColour (kInk.withAlpha (0.25f));
        g.drawLine (x0, yOf (0.0f), x1, yOf (0.0f), 1.0f);
        g.drawLine (x0, yBot, x1, yBot, 1.4f);
        for (float f : { 20.0f, 200.0f, 2000.0f, 20000.0f })
        {
            g.drawLine (xOf (f), yBot, xOf (f), yBot + 6.0f, 1.4f);
            drawText (g, f >= 1000.0f ? juce::String ((int) (f / 1000.0f)) + " kHz" : juce::String ((int) f) + " Hz", xOf (f), yBot + 16.0f, 11.0f, kInk.withAlpha (0.55f), false, 0.0f);
        }
        juce::Path curve, fill;
        for (int i = 0; i <= 160; ++i)
        {
            const float f = 20.0f * std::pow (1000.0f, (float) i / 160.0f), x = x0 + w * (float) i / 160.0f;
            const float db = 20.0f * std::log10 (std::max (1.0e-4f, magnitude (f))), y = yOf (db);
            if (i == 0) { curve.startNewSubPath (x, y); fill.startNewSubPath (x, yBot); fill.lineTo (x, y); }
            else { curve.lineTo (x, y); fill.lineTo (x, y); }
        }
        fill.lineTo (x1, yBot);
        fill.closeSubPath();
        g.setColour (kBody.withAlpha (0.16f));
        g.fillPath (fill);
        g.setColour (kBody);
        g.strokePath (curve, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        drawText (g, mode == ml1::Flanger ? (variant == 1 ? "COMB, NEGATIVE FEEDBACK" : "COMB, POSITIVE FEEDBACK")
                                          : juce::String (ml1::stageCount (variant)) + "-STAGE ALL-PASS NOTCHES",
                  r.getRight() - 14.0f, r.getY() + 16.0f, 11.0f, kInk.withAlpha (0.5f), true, 0.14f, juce::Justification::centredRight);
    }
}


//==============================================================================
void ML1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    ui::paintBody (g, kPal, kW, kH, grain);
    ui::paintHeader (g, kPal, "ML-1", "MODULATION LAB", "WAVE MOVEMENT", isOn ("power"));
    ui::paintPresetBar (g, kPal, proc.presets, proc.undoManager, kBarY, barState, false);

    const int mode = currentMode(), variant = currentVariant();
    ui::paintPanel (g, kPal, panelRect (0), {});
    ui::paintTabs (g, kPal, panelRect (0), kModeTabs, mode);
    ui::paintPanel (g, kPal, panelRect (1), "OUTPUT");
    ui::paintPanel (g, kPal, panelRect (2), "MOTION");
    drawMotion (g, mode, variant);

    // the TYPE buttons of the mode (voices, LFO wave, feedback polarity, stages) and SYNC (the rate becomes a note length of the host tempo)
    for (int v = 0; v < ml1::variantCount (mode); ++v) ui::paintFlatButton (g, kPal, variantRect (v), ml1::variantName (mode, v), v == variant, 11.0f);
    ui::paintFlatButton (g, kPal, syncRect(), "SYNC", isOn ("sync"), 11.0f);

    for (auto& k : knobs)
    {
        const double v = k.slider.getValue();
        juce::String txt, lo = "0", hi = "100";
        if (k.kind == 2) { txt = juce::String::formatted ("%+d dB", (int) std::lround (v)); lo = "-24"; hi = "+12"; }
        else if (k.kind == 0)
        {
            const float f = (float) v * 0.01f;
            txt = isOn ("sync") ? juce::String (ml1::divisionName (ml1::divisionIndex (f))) : juce::String (ml1::rateHz (mode, f), 2) + " Hz";
        }
        else txt = juce::String ((int) std::lround (v)) + " %";
        ui::paintKnob (g, kPal, knobCentre (k.panel, k.index, k.count), k.name != nullptr ? k.name : ml1::extraName (mode), txt, lo, hi, panelRect (k.panel).getY() + 154.0f, panelRect (k.panel).getY() + 182.0f);
    }
    ui::paintFooter (g, kPal, kRow2Y + kRow2H + 35.0f);
}

void ML1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / scale();
    proc.undoManager.beginNewTransaction();
    if (ui::powerRect().contains (p)) { toggle ("power"); return; }
    if (ui::presetBarMouseDown (p, *this, proc.presets, proc.undoManager, kBarY, scale(), barState, [this] { repaint(); }, false)) { layoutKnobs(); return; }
    const auto tabs = ui::tabRects (panelRect (0), kModeTabs);
    for (int i = 0; i < (int) tabs.size(); ++i)
        if (tabs[(size_t) i].expanded (3.0f).contains (p)) { setChoice ("mode", i); setChoice ("variant", 0); repaint(); return; }
    if (syncRect().expanded (3.0f).contains (p)) { toggle ("sync"); repaint(); return; }
    for (int v = 0; v < ml1::variantCount (currentMode()); ++v)
        if (variantRect (v).expanded (3.0f).contains (p)) { setChoice ("variant", v); repaint(); return; }
}
