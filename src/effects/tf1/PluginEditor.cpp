// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "ui/Presets.h"
#include "ui/OutputMeter.h"

namespace
{
using vibr::ui::drawText;
namespace ui = vibr::ui;

constexpr float kW = 1200.0f, kH = 890.0f, kRow1Y = 134.0f, kRow1H = 247.0f, kRow2Y = 405.0f, kRow2H = 216.0f, kRow3Y = 645.0f, kRow3H = 190.0f, kBarY = 39.0f;
const ui::Palette kPal { juce::Colour (224, 76, 197), juce::Colour (46, 10, 40) };             // the Experimental colour #E04CC5 and its print
const juce::Colour kIce (143, 211, 244);
const juce::Colour kOpColour[tf1::kOps] = { juce::Colour (90, 92, 98), juce::Colour (255, 196, 70), juce::Colour (120, 205, 255), juce::Colour (140, 230, 120),
                                            juce::Colour (255, 130, 90), juce::Colour (190, 150, 255), juce::Colour (150, 150, 156) };
const char* const kStepIds[tf1::kSteps] = { "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11", "s12", "s13", "s14", "s15", "s16" };

// STEPS: panels 0 SEQUENCER (row 1), 1 FRACTURE, 2 GLITCH, 3 OUTPUT (row 2), 4 BUFFER (row 3)
// SCRUB and GRAINS (the standard Lab layout, the RV-1 and the GR-1): 5 CONTROLS and 6 OUTPUT (row 1), 7 the display (rows 2 and 3, with the tabs of the sub-mode)
constexpr float kPanelX[8] = { 20.0f, 20.0f, 610.0f, 860.0f, 20.0f, 20.0f, 730.0f, 20.0f }, kPanelW[8] = { 1160.0f, 580.0f, 240.0f, 320.0f, 1160.0f, 700.0f, 450.0f, 1160.0f };
juce::Rectangle<float> panelRect (int i)
{
    if (i == 7) return { kPanelX[7], kRow2Y, kPanelW[7], kRow3Y + kRow3H - kRow2Y };
    const float y = i == 0 || i >= 5 ? kRow1Y : i < 4 ? kRow2Y : kRow3Y, h = i == 0 || i >= 5 ? kRow1H : i < 4 ? kRow2H : kRow3H;
    return { kPanelX[i], y, kPanelW[i], h };
}
const std::vector<juce::String> kModeTabs { "STEPS", "SCRUB", "GRAINS" }, kScrubTabs { "REVERSE", "SCRUB", "STUTTER", "GLITCH", "REPEAT" }, kGrainTabs { "SOFT", "SHARP", "PERCUSSIVE", "SWELL" };
juce::Rectangle<float> syncRect() { const auto r = panelRect (5); return { r.getX() + 30.0f, r.getY() + 204.0f, 120.0f, 30.0f }; }
juce::Rectangle<float> freezeRect() { const auto r = panelRect (5); return { r.getX() + 160.0f, r.getY() + 204.0f, 160.0f, 30.0f }; }
juce::Rectangle<float> slicesRect() { const auto r = panelRect (7); return { r.getX() + 30.0f, r.getY() + 64.0f, r.getWidth() - 60.0f, r.getHeight() - 124.0f }; }
juce::Rectangle<float> cloudPlot() { const auto r = panelRect (7); return { r.getX() + 64.0f, r.getY() + 64.0f, r.getWidth() - 94.0f, r.getHeight() - 124.0f }; }
juce::Point<float> knobCentre (int panel, int k, int n)
{
    const auto r = panelRect (panel);
    if (panel == 5) return { r.getCentreX() + (n >= 8 ? 84.0f : 112.0f) * ((float) k - 0.5f * (float) (n - 1)), r.getY() + 92.0f };           // CONTROLS: 5 knobs 112 apart, 8 knobs 84 apart
    if (panel == 6) return { r.getX() + 165.0f + 120.0f * ((float) k - 0.5f * (float) (n - 1)), r.getY() + 92.0f };                              // OUTPUT: the knobs left of the dB meters
    const float cx = panel == 3 ? r.getX() + 122.0f : r.getCentreX(), sp = panel == 1 ? 105.0f : panel == 2 ? 100.0f : 95.0f;
    return { cx + sp * ((float) k - 0.5f * (float) (n - 1)), r.getY() + 92.0f };
}
juce::Point<float> meterOrigin (bool hosted) { const auto r = panelRect (hosted ? 6 : 3); return { r.getX() + (hosted ? 317.0f : 235.0f), r.getY() + 46.0f }; }
juce::Rectangle<float> stepRect (int s) { const auto r = panelRect (0); return { r.getX() + 40.0f + 68.0f * (float) s, r.getY() + 32.0f, 60.0f, 112.0f }; }
juce::Rectangle<float> divisionRect (int d) { const auto r = panelRect (0); return { r.getX() + 40.0f + 62.0f * (float) d, r.getY() + 184.0f, 56.0f, 28.0f }; }
juce::Rectangle<float> lengthMinus() { const auto r = panelRect (0); return { r.getX() + 470.0f, r.getY() + 184.0f, 28.0f, 28.0f }; }
juce::Rectangle<float> lengthPlus() { const auto r = panelRect (0); return { r.getX() + 592.0f, r.getY() + 184.0f, 28.0f, 28.0f }; }
juce::Rectangle<float> holdRect() { const auto r = panelRect (0); return { r.getX() + 660.0f, r.getY() + 184.0f, 120.0f, 28.0f }; }
juce::Rectangle<float> clearRect() { const auto r = panelRect (0); return { r.getX() + 800.0f, r.getY() + 184.0f, 90.0f, 28.0f }; }
juce::Rectangle<float> randomRect() { const auto r = panelRect (0); return { r.getX() + 900.0f, r.getY() + 184.0f, 100.0f, 28.0f }; }
juce::Rectangle<float> plotRect() { const auto r = panelRect (4); return { r.getX() + 50.0f, r.getY() + 46.0f, r.getWidth() - 100.0f, 110.0f }; }
}

//==============================================================================
TF1AudioProcessorEditor::TF1AudioProcessorEditor (TF1AudioProcessor& p)
    : LabEditor (p, kW, kH), proc (p),
      knobs { { { "chance", 1, 0, 5, "CHANCE", 0, 1, {}, {} },     { "loop", 1, 1, 5, "LOOP", 0, 1, {}, {} },        { "scramble", 1, 2, 5, "SCRAMBLE", 0, 1, {}, {} },
                { "pitch", 1, 3, 5, "PITCH", 0, 1, {}, {} },       { "stretch", 1, 4, 5, "STRETCH", 0, 1, {}, {} },
                { "crush", 2, 0, 2, "CRUSH", 0, 1, {}, {} },       { "tempo", 2, 1, 2, "TEMPO", 2, 1, {}, {} },
                { "mix", 3, 0, 2, "MIX", 0, 1, {}, {} },           { "level", 3, 1, 2, "LEVEL", 1, 1, {}, {} },
                { "scrubLength", 5, 0, 5, "LENGTH", 3, 2, {}, {} }, { "scrubSpeed", 5, 1, 5, "SPEED", 4, 2, {}, {} },  { "scrubSmooth", 5, 2, 5, "SMOOTH", 0, 2, {}, {} },
                { "scrubRandom", 5, 3, 5, nullptr, 10, 2, {}, {} }, { "scrubRepeats", 5, 4, 5, nullptr, 10, 2, {}, {} },
                { "grainSize", 5, 0, 8, "SIZE", 5, 4, {}, {} },     { "grainDensity", 5, 1, 8, "DENSITY", 6, 4, {}, {} }, { "grainPosition", 5, 2, 8, "POSITION", 7, 4, {}, {} },
                { "grainSpray", 5, 3, 8, "SPRAY", 0, 4, {}, {} },   { "grainPitch", 5, 4, 8, "PITCH", 8, 4, {}, {} },     { "grainRandom", 5, 5, 8, "RANDOM", 9, 4, {}, {} },
                { "grainReverse", 5, 6, 8, "REVERSE", 0, 4, {}, {} }, { "grainSpread", 5, 7, 8, "SPREAD", 0, 4, {}, {} },
                { "mix", 6, 0, 2, "MIX", 0, 6, {}, {} },           { "level", 6, 1, 2, "LEVEL", 1, 6, {}, {} } } }
{
    for (auto& k : knobs)
    {
        attachKnob (k.slider, k.att, k.id);
    }
    layoutKnobs();
    startTimerHz (30);
}

TF1AudioProcessorEditor::~TF1AudioProcessorEditor() = default;

void TF1AudioProcessorEditor::timerCallback()
{
    meter.update (proc.outMeter);
    const auto region = [this] (juce::Rectangle<float> r) { return r.expanded (4.0f).transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer(); };
    const int mode = modeNow();
    if (mode != shownMode) { layoutKnobs(); repaint(); }                                                       // a preset, undo or automation changed the mode
    scroll += 0.033f;
    if (mode == 0)
    {
        repaint (region (panelRect (4)));                                                                      // the buffer
        repaint (region (panelRect (0)));                                                                      // the playing step
        repaint (region (panelRect (3).withTrimmedLeft (panelRect (3).getWidth() - 100.0f)));                   // the output meters
    }
    else
    {
        repaint (region (panelRect (7)));                                                                      // the slices or the grain cloud
        repaint (region (panelRect (6).withTrimmedLeft (panelRect (6).getWidth() - 150.0f)));                   // the output meters
    }
    if (midiUI != nullptr && midiUI->needsRepaint()) { midiUI->tick(); repaint(); }
    tickPresetBar();
}

void TF1AudioProcessorEditor::setValue (const char* id, float v)
{
    auto* par = proc.apvts.getParameter (id);
    par->beginChangeGesture(); par->setValueNotifyingHost (par->convertTo0to1 (v)); par->endChangeGesture();
}
int TF1AudioProcessorEditor::stepOp (int s) const { return juce::jlimit (0, tf1::kOps - 1, (int) std::lround (value (kStepIds[s]))); }

bool TF1AudioProcessorEditor::knobShown (int i) const { return (knobs[(size_t) i].modes & (1 << modeNow())) != 0; }

void TF1AudioProcessorEditor::layoutKnobs()
{
    shownMode = modeNow();
    juce::uint64 shown = 0;
    for (int i = 0; i < (int) knobs.size(); ++i)
    {
        auto& k = knobs[(size_t) i];
        placeKnob (k.slider, knobCentre (k.panel, k.index, k.count));
        k.slider.setVisible (knobShown (i));
        if (knobShown (i)) shown |= (juce::uint64) 1 << i;
    }
    if (midiUI == nullptr || shown != midiShown)                                      // MIDI learn marks only for the knobs that are on screen
    {
        midiShown = shown;
        midiUI = std::make_unique<MidiLearnUI> (proc.midiMap);
        for (int i = 0; i < (int) knobs.size(); ++i) if (knobShown (i)) midiUI->add (knobs[(size_t) i].slider, knobs[(size_t) i].id);
    }
}
void TF1AudioProcessorEditor::resized() { layoutKnobs(); }

//==============================================================================
// BUFFER: the rolling recording of the last PATTERN LENGTH steps (the right edge is now), the step grid, and the piece of the recording the current step plays
// (coloured by its operation: a REVERSE reads it backwards, a REPEAT loops it). A held buffer is tinted ice: it is not recorded any more.
void TF1AudioProcessorEditor::drawBuffer (juce::Graphics& g)
{
    const auto r = plotRect();
    g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (r.expanded (8.0f, 8.0f), 10.0f);
    const bool held = proc.engine.held(), on = isOn ("power");
    const float fs = proc.engine.sampleRate() > 0.0f ? proc.engine.sampleRate() : 48000.0f;
    const double stepLen = std::max (64.0, (double) proc.engine.stepLengthSamples());
    const int length = juce::jlimit (1, tf1::kSteps, (int) std::lround (value ("length")));
    const double windowSamples = std::min (stepLen * length, (double) proc.engine.capacitySeconds() * fs * 0.9);
    const double elapsed = held ? 0.0 : (double) proc.engine.stepElapsed();
    if (held) { g.setColour (kIce.withAlpha (0.07f)); g.fillRect (r); }

    const int count = proc.engine.peakCount();                                                                   // the recording as peaks (1024 samples each)
    const int cols = (int) r.getWidth();
    g.setColour (held ? kIce.withAlpha (0.85f) : kPal.ink.withAlpha (0.55f));
    for (int x = 0; x < cols; ++x)
    {
        const double a0 = (1.0 - (double) (x + 1) / cols) * windowSamples, a1 = (1.0 - (double) x / cols) * windowSamples;       // samples back from now
        const int i0 = count - 1 - (int) std::floor (a1 / tf1::kPeakBlock), i1 = count - 1 - (int) std::floor (a0 / tf1::kPeakBlock);
        if (i1 < 0) continue;
        float pk = 0.0f;
        for (int i = std::max (i0, count - tf1::kPeaks + 1); i <= i1; ++i) pk = std::max (pk, proc.engine.peak (i));
        const float h = std::min (1.0f, std::sqrt (pk) * 1.2f) * r.getHeight() * 0.5f;
        g.fillRect (r.getX() + (float) x, r.getCentreY() - h, 1.0f, std::max (1.0f, 2.0f * h));
    }
    g.setColour (kPal.ink.withAlpha (0.15f)); g.drawLine (r.getX(), r.getCentreY(), r.getRight(), r.getCentreY(), 1.0f);

    const auto xOfBack = [&] (double back) { return r.getRight() - r.getWidth() * (float) (back / windowSamples); };        // x of a point `back` samples before now
    for (int k = 0; k <= length; ++k)                                                                            // the step grid (counted back from the start of the current step)
    {
        const double back = elapsed + k * stepLen;
        if (back > windowSamples) break;
        const float x = xOfBack (back);
        g.setColour (kPal.ink.withAlpha (k == 0 ? 0.45f : 0.16f)); g.drawLine (x, r.getY(), x, r.getBottom(), k == 0 ? 1.6f : 1.0f);
    }

    const int op = proc.engine.currentOp();
    const double len = (double) proc.engine.sourceLength(), back0 = (double) proc.engine.sourceBack() + elapsed;       // where the playing piece comes from
    if (on && len > 0.0 && op != tf1::Play && op != tf1::Mute)
    {
        const float xa = xOfBack (std::min (back0, windowSamples)), xb = xOfBack (std::max (0.0, back0 - len));
        g.setColour (kOpColour[op].withAlpha (0.22f)); g.fillRect (juce::Rectangle<float> (xa, r.getY(), std::max (2.0f, xb - xa), r.getHeight()));
        g.setColour (kOpColour[op]); g.drawRect (juce::Rectangle<float> (xa, r.getY(), std::max (2.0f, xb - xa), r.getHeight()), 1.6f);
        drawText (g, op == tf1::Reverse ? "<<" : ">>", (xa + xb) * 0.5f, r.getBottom() - 14.0f, 15.0f, kOpColour[op], true, 0.0f);
    }
    drawText (g, juce::String ("-") + juce::String (windowSamples / fs, 1) + " S", r.getX(), r.getBottom() + 15.0f, 12.0f, kPal.ink.withAlpha (0.6f), false, 0.04f, juce::Justification::centredLeft);
    drawText (g, "NOW", r.getRight(), r.getBottom() + 15.0f, 12.0f, kPal.ink.withAlpha (0.75f), true, 0.1f, juce::Justification::centredRight);
    drawText (g, held ? "HELD: THE BUFFER IS NOT RECORDED" : "RECORDING", r.getX(), r.getY() - 14.0f, 12.0f, held ? kIce : kPal.ink.withAlpha (0.6f), true, 0.12f, juce::Justification::centredLeft);
    drawText (g, on ? juce::String ("STEP ") + juce::String (proc.engine.currentStep() + 1) + "   " + tf1::opName (op) : juce::String ("OFF"), r.getRight(), r.getY() - 14.0f, 12.0f,
              on && op != tf1::Play ? kOpColour[op] : kPal.ink.withAlpha (0.6f), true, 0.12f, juce::Justification::centredRight);
}

//==============================================================================
// SCRUB: the audio is cut into slices of LENGTH. Each slice is drawn as the path its read position takes through the captured audio (top = newest): REVERSE runs
// backwards from the newest sound at SPEED, SCRUB wanders by the scrub amount, STUTTER and REPEAT keep playing the same piece, GLITCH jumps to a random place per slice.
// (the RV-1 display)
void TF1AudioProcessorEditor::drawSlices (juce::Graphics& g)
{
    const auto r = slicesRect();
    g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (r.expanded (8.0f), 10.0f);
    const int mode = juce::jlimit (0, rv1::kNumModes - 1, choice ("scrubMode"));
    const bool freeze = isOn ("scrubFreeze");
    const float length = value ("scrubLength") * 0.01f, speed = rv1::speedRatio (value ("scrubSpeed") * 0.01f), wander = value ("scrubRandom") * 0.01f, repeats = value ("scrubRepeats") * 0.01f, smooth = value ("scrubSmooth") * 0.01f;
    const float sliceMs = isOn ("scrubSync") ? 60000.0f / 120.0f * rv1::divisionBeats (rv1::divisionIndex (length)) : rv1::lengthMs (length);
    const float spanMs = 3000.0f, x0 = r.getX() + 4.0f, w = r.getWidth() - 8.0f;
    const auto xOf = [&] (float ms) { return x0 + w * ms / spanMs; };
    g.setColour (kPal.ink.withAlpha (0.1f));
    for (float t = 0.0f; t <= 3.0f; t += 0.5f) { g.drawLine (xOf (t * 1000.0f), r.getY() + 4.0f, xOf (t * 1000.0f), r.getBottom() - 4.0f, 1.0f); drawText (g, juce::String (t, 1) + " s", xOf (t * 1000.0f), r.getBottom() + 20.0f, 13.0f, kPal.ink.withAlpha (0.7f), false, 0.0f); }
    juce::Random rnd (7 + (int) scroll);                                                                       // the random places change slowly
    const float top = r.getY() + 10.0f, bottom = r.getBottom() - 10.0f, h = bottom - top;
    float start = 0.0f; int held = 0;
    const int keep = 1 + (int) std::round (repeats * 5.0f);
    for (float t = 0.0f; t < spanMs - 1.0f; t += sliceMs)
    {
        const float end = std::min (spanMs, t + sliceMs), xa = xOf (t), xb = xOf (end);
        if (held <= 0) { start = (mode == rv1::Glitch ? rnd.nextFloat() : wander * 0.6f * (rnd.nextFloat() - 0.5f) + 0.5f * (mode == rv1::Reverse ? 0.0f : 1.0f)); held = keep; }
        --held;
        float y0, y1;
        const float span = std::min (1.0f, speed * 0.5f);
        if (mode == rv1::Reverse || mode == rv1::Scrub) { y0 = std::clamp (start, 0.0f, 1.0f) * 0.2f; y1 = y0 + span; }
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
              r.getX() - 8.0f, r.getY() - 26.0f, 13.0f, kPal.ink.withAlpha (0.85f), true, 0.12f, juce::Justification::centredLeft);
    if (freeze) drawText (g, "FROZEN: THE SLICES KEEP PLAYING WHAT WAS CAPTURED", r.getRight() + 8.0f, r.getY() - 26.0f, 13.0f, kIce, true, 0.12f, juce::Justification::centredRight);
}

// GRAINS: every grain is a dot; left = long ago, right = just now (the dots drift left); up = higher pitch; halo = grain length (the GR-1 display)
void TF1AudioProcessorEditor::drawCloud (juce::Graphics& g)
{
    const auto r = cloudPlot();
    g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (r.expanded (8.0f), 10.0f);
    for (int st : { -12, 0, 12 })
    {
        const float y = r.getY() + r.getHeight() * (1.0f - (0.5f + (float) st / 48.0f));
        g.setColour (kPal.ink.withAlpha (st == 0 ? 0.25f : 0.12f));
        g.drawLine (r.getX(), y, r.getRight(), y, 1.0f);
        drawText (g, st == 0 ? "0" : juce::String::formatted ("%+d", st), r.getX() - 24.0f, y, 13.0f, kPal.ink.withAlpha (0.7f), false, 0.0f);
    }
    drawText (g, "SEMITONES", panelRect (7).getX() + 22.0f, r.getY() - 26.0f, 13.0f, kPal.ink.withAlpha (0.7f), true, 0.1f, juce::Justification::centredLeft);
    drawText (g, "PAST", r.getX() + 24.0f, r.getBottom() + 20.0f, 13.0f, kPal.ink.withAlpha (0.7f), true, 0.14f);
    drawText (g, "NOW", r.getRight() - 16.0f, r.getBottom() + 20.0f, 13.0f, kPal.ink.withAlpha (0.7f), true, 0.14f);
    const auto& eng = proc.engine.grains();
    juce::Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (r.expanded (4.0f).toNearestInt());
    const float kWindow = 4.0f;                                                                                // grains of the last 4 s: they appear at the right and drift left
    const double now = (double) eng.clockNow.load (std::memory_order_relaxed), fs = (double) eng.sampleRate();
    for (int i = 0; i < gr1::kVisEvents; ++i)
    {
        const float age = (float) ((now - (double) eng.evT[i].load (std::memory_order_relaxed)) / fs);
        if (age < 0.0f || age > kWindow) continue;
        const float u = age / kWindow, fade = 1.0f - u;
        const float x = r.getRight() - r.getWidth() * u;
        const float y = r.getY() + r.getHeight() * (1.0f - eng.evY[i].load (std::memory_order_relaxed));
        const float rad = 9.0f + 17.0f * std::sqrt (eng.evS[i].load (std::memory_order_relaxed));              // the halo shows the grain length
        g.setColour (kPal.body.withAlpha (0.22f * fade));
        g.fillEllipse (juce::Rectangle<float> (2.0f * rad, 2.0f * rad).withCentre ({ x, y }));
        g.setColour (kPal.body.brighter (0.3f).withAlpha (0.25f + 0.75f * fade));
        g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ x, y }));
    }
}

juce::String TF1AudioProcessorEditor::knobText (int kind, double v) const
{
    const float f = (float) v * 0.01f;
    switch (kind)
    {
        case 1: return juce::String::formatted ("%+d dB", (int) std::lround (v));
        case 2: return juce::String ((int) std::lround (v)) + " bpm";
        case 3: { const float ms = rv1::lengthMs (f); return isOn ("scrubSync") ? juce::String (rv1::divisionName (rv1::divisionIndex (f))) : ms >= 1000.0f ? juce::String (ms * 0.001f, 2) + " s" : juce::String ((int) std::lround (ms)) + " ms"; }
        case 4: return juce::String (rv1::speedRatio (f), 2) + "x";
        case 5: { const float ms = gr1::sizeMs (f); return ms >= 100.0f ? juce::String ((int) std::lround (ms)) + " ms" : juce::String (ms, 1) + " ms"; }
        case 6: return isOn ("grainSync") ? juce::String (gr1::divisionName (gr1::divisionIndex (f))) : juce::String (gr1::densityHz (f), 1) + " /s";
        case 7: { const float ms = gr1::positionMs (f); return ms >= 1000.0f ? juce::String (ms * 0.001f, 2) + " s" : juce::String ((int) std::lround (ms)) + " ms"; }
        case 8: return juce::String::formatted ("%+d st", gr1::pitchSemis (f));
        case 9: return gr1::randomSemis (f) == 0 ? juce::String ("OFF") : juce::String::formatted ("+-%d st", gr1::randomSemis (f));
        default: return juce::String ((int) std::lround (v)) + " %";
    }
}

//==============================================================================
void TF1AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    if (midiUI != nullptr) midiUI->paintMarks (g, kPal.body);
}

void TF1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    ui::paintBody (g, kPal, kW, kH, grain);
    ui::paintHeader (g, kPal, "TF-1", "TIME FRACTURE", "BROKEN RHYTHMS", isOn ("power"));
    ui::paintPresetBar (g, kPal, proc.presets, proc.undoManager, kBarY, barState, false, 6.0f);

    const int mode = modeNow();
    const bool on = isOn ("power");
    if (mode != 0)
    {
        ui::paintPanel (g, kPal, panelRect (5), {});
        ui::paintTabs (g, kPal, panelRect (5), kModeTabs, mode, 13.0f);
        ui::paintPanel (g, kPal, panelRect (6), "OUTPUT");
        ui::paintPanel (g, kPal, panelRect (7), {});
        ui::paintTabs (g, kPal, panelRect (7), mode == 1 ? kScrubTabs : kGrainTabs, mode == 1 ? choice ("scrubMode") : choice ("grainShape"), 13.0f);
        ui::paintFlatButton (g, kPal, syncRect(), "SYNC", isOn (mode == 1 ? "scrubSync" : "grainSync"), 13.0f);
        {   // FREEZE has its own ice colour
            const auto b = freezeRect(); const bool fr = isOn (mode == 1 ? "scrubFreeze" : "grainFreeze");
            if (fr) { g.setColour (kIce); g.fillRoundedRectangle (b, 9.0f); }
            g.setColour (fr ? kIce : kPal.ink.withAlpha (0.4f)); g.drawRoundedRectangle (b.reduced (0.5f), 9.0f, 1.3f);
            drawText (g, "FREEZE", b.getCentreX(), b.getCentreY(), 13.0f, fr ? kPal.dark : kPal.ink.withAlpha (0.9f), true, 0.1f);
        }
        if (mode == 1) drawSlices (g); else drawCloud (g);
    }
    else
    {
    ui::paintPanel (g, kPal, panelRect (0), {});
    ui::paintTabs (g, kPal, panelRect (0), kModeTabs, 0, 13.0f);
    ui::paintPanel (g, kPal, panelRect (1), "FRACTURE");
    ui::paintPanel (g, kPal, panelRect (2), "GLITCH");
    ui::paintPanel (g, kPal, panelRect (3), "OUTPUT");
    ui::paintPanel (g, kPal, panelRect (4), "BUFFER");
    drawBuffer (g);

    const int length = juce::jlimit (1, tf1::kSteps, (int) std::lround (value ("length"))), playing = on ? proc.engine.currentStep() : -1;
    for (int s = 0; s < tf1::kSteps; ++s)                                                                      // the 16 steps
    {
        const auto b = stepRect (s);
        const int op = stepOp (s);
        const bool inside = s < length, now = s == playing && inside;
        const float a = inside ? 1.0f : 0.3f;
        if (op != tf1::Play) { g.setColour (kOpColour[op].withAlpha (a)); g.fillRoundedRectangle (b, 10.0f); }
        else { g.setColour (juce::Colours::black.withAlpha (0.35f * a)); g.fillRoundedRectangle (b, 10.0f); }
        g.setColour (now ? juce::Colours::white : kPal.ink.withAlpha (inside ? 0.4f : 0.15f)); g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, now ? 2.6f : 1.3f);
        drawText (g, tf1::opShort (op), b.getCentreX(), b.getCentreY() - 6.0f, 16.0f, op == tf1::Play ? kPal.ink.withAlpha (0.55f * a) : kPal.dark.withMultipliedAlpha (a), true, 0.06f);
        drawText (g, juce::String (s + 1), b.getCentreX(), b.getBottom() - 16.0f, 12.0f, op == tf1::Play ? kPal.ink.withAlpha (0.4f * a) : kPal.dark.withAlpha (0.6f * a), true, 0.04f);
        if (now) { juce::Path tri; tri.addTriangle (b.getCentreX() - 7.0f, b.getBottom() + 12.0f, b.getCentreX() + 7.0f, b.getBottom() + 12.0f, b.getCentreX(), b.getBottom() + 4.0f); g.setColour (juce::Colours::white); g.fillPath (tri); }
    }
    const int div = juce::jlimit (0, 5, (int) std::lround (value ("division")));
    for (int d = 0; d < 6; ++d) ui::paintFlatButton (g, kPal, divisionRect (d), tf1::divisionName (d), d == div, 12.0f);
    ui::paintFlatButton (g, kPal, lengthMinus(), "-", false, 15.0f);
    ui::paintFlatButton (g, kPal, lengthPlus(), "+", false, 15.0f);
    drawText (g, "LENGTH " + juce::String (length), (lengthMinus().getRight() + lengthPlus().getX()) * 0.5f, lengthMinus().getCentreY(), 14.0f, kPal.ink.withAlpha (0.9f), true, 0.08f);
    {   // HOLD has its own ice colour
        const auto b = holdRect(); const bool h = isOn ("hold");
        if (h) { g.setColour (kIce); g.fillRoundedRectangle (b, 9.0f); }
        g.setColour (h ? kIce : kPal.ink.withAlpha (0.4f)); g.drawRoundedRectangle (b.reduced (0.5f), 9.0f, 1.3f);
        drawText (g, "HOLD BUFFER", b.getCentreX(), b.getCentreY(), 12.0f, h ? kPal.dark : kPal.ink.withAlpha (0.9f), true, 0.08f);
    }
    ui::paintFlatButton (g, kPal, clearRect(), "CLEAR", false, 12.0f);
    ui::paintFlatButton (g, kPal, randomRect(), "RANDOM", false, 12.0f);
    {   // the legend of the operations
        const auto r = panelRect (0);
        float x = r.getX() + 40.0f;
        for (int o = 0; o < tf1::kOps; ++o)
        {
            g.setColour (kOpColour[o]); g.fillRoundedRectangle (x, r.getY() + 226.0f, 10.0f, 10.0f, 3.0f);
            drawText (g, tf1::opName (o), x + 16.0f, r.getY() + 231.0f, 11.0f, kPal.ink.withAlpha (0.7f), true, 0.08f, juce::Justification::centredLeft);
            x += 16.0f + ui::textWidth (ui::font (11.0f, true).withExtraKerningFactor (0.08f), tf1::opName (o)) + 28.0f;
        }
        drawText (g, "CLICK A STEP TO CHANGE ITS OPERATION", r.getRight() - 40.0f, r.getY() + 231.0f, 11.0f, kPal.ink.withAlpha (0.4f), true, 0.1f, juce::Justification::centredRight);
    }

    drawText (g, "NO HOST TEMPO", panelRect (2).getCentreX() + 50.0f, panelRect (2).getY() + 205.0f, 10.0f, kPal.ink.withAlpha (0.45f), true, 0.1f);
    }

    for (int i = 0; i < (int) knobs.size(); ++i)
    {
        if (! knobShown (i)) continue;
        auto& k = knobs[(size_t) i];
        const double v = k.slider.getValue();
        static const char* lo[11] = { "0", "-24", "40", "20 ms", ".25x", "10", "2", "0", "-12", "0", "0" }, *hi[11] = { "100", "+12", "240", "2 s", "4x", "500", "80", "2 s", "+12", "+-12", "100" };
        const auto c = knobCentre (k.panel, k.index, k.count);
        const char* name = k.name != nullptr ? k.name : rv1::chaosName (choice ("scrubMode"), k.index == 3 ? 0 : 1);
        ui::paintKnob (g, kPal, c, name, knobText (k.kind, v), lo[k.kind], hi[k.kind], panelRect (k.panel).getY() + 154.0f, panelRect (k.panel).getY() + 182.0f);
        ui::paintValueArc (g, c, ui::kKnobR, (float) k.slider.valueToProportionOfLength (k.slider.getValue()), kPal.body);
    }
    meter.paint (g, meterOrigin (mode != 0).x, meterOrigin (mode != 0).y, on, kPal.body, kPal.ink, 110.0f, 26.0f, 13.0f);
    ui::paintFooter (g, kPal, kRow3Y + kRow3H + 35.0f);
}

void TF1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / scale();
    proc.undoManager.beginNewTransaction();
    if (ui::powerRect().contains (p)) { toggle ("power"); return; }
    if (ui::presetBarMouseDown (p, *this, proc.presets, proc.undoManager, kBarY, scale(), barState, [this] { repaint(); }, false)) { layoutKnobs(); return; }
    const int mode = modeNow();
    {
        const auto tabs = ui::tabRects (panelRect (mode == 0 ? 0 : 5), kModeTabs, 13.0f);
        for (int i = 0; i < (int) tabs.size(); ++i)
            if (tabs[(size_t) i].expanded (3.0f).contains (p)) { setValue ("mode", (float) i); layoutKnobs(); repaint(); return; }
    }
    if (mode != 0)
    {
        const auto subs = ui::tabRects (panelRect (7), mode == 1 ? kScrubTabs : kGrainTabs, 13.0f);
        for (int i = 0; i < (int) subs.size(); ++i)
            if (subs[(size_t) i].expanded (3.0f).contains (p)) { setValue (mode == 1 ? "scrubMode" : "grainShape", (float) i); repaint(); return; }
        if (syncRect().expanded (3.0f).contains (p)) { toggle (mode == 1 ? "scrubSync" : "grainSync"); repaint(); return; }
        if (freezeRect().expanded (3.0f).contains (p)) { toggle (mode == 1 ? "scrubFreeze" : "grainFreeze"); repaint(); return; }
        return;
    }
    for (int s = 0; s < tf1::kSteps; ++s)
        if (stepRect (s).contains (p))
        {
            const int step = (e.mods.isRightButtonDown() || e.mods.isCtrlDown() || e.mods.isShiftDown()) ? -1 : 1;           // right click, ctrl or shift: back
            setValue (kStepIds[s], (float) ((stepOp (s) + step + tf1::kOps) % tf1::kOps));
            repaint(); return;
        }
    for (int d = 0; d < 6; ++d)
        if (divisionRect (d).expanded (3.0f).contains (p)) { setValue ("division", (float) d); repaint(); return; }
    if (lengthMinus().expanded (3.0f).contains (p)) { setValue ("length", (float) juce::jmax (1, (int) std::lround (value ("length")) - 1)); repaint(); return; }
    if (lengthPlus().expanded (3.0f).contains (p)) { setValue ("length", (float) juce::jmin (tf1::kSteps, (int) std::lround (value ("length")) + 1)); repaint(); return; }
    if (holdRect().expanded (3.0f).contains (p)) { toggle ("hold"); repaint(); return; }
    if (clearRect().expanded (3.0f).contains (p)) { for (int s = 0; s < tf1::kSteps; ++s) setValue (kStepIds[s], 0.0f); repaint(); return; }
    if (randomRect().expanded (3.0f).contains (p))
    {
        for (int s = 0; s < tf1::kSteps; ++s) setValue (kStepIds[s], rng.nextFloat() < 0.45f ? (float) (1 + rng.nextInt (tf1::kOps - 1)) : 0.0f);
        repaint(); return;
    }
}
