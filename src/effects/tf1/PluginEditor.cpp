// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "ui/Presets.h"

namespace
{
namespace ui = vibr::ui::vintage;          // the default look (src/common/ui/VintageUI.h); the old look is vibr::ui (VibrUI.h)
using ui::drawText;

constexpr float kW = ui::kW, kH = 910.0f;                                                     // 660 + the extra row of the BUFFER (18 gap + 232)
constexpr float kRow1Y = 114.0f, kRow1H = 244.0f, kRow2Y = 376.0f, kRow2H = 232.0f, kRow3Y = 626.0f, kRow3H = 254.0f;
const ui::Palette kPal { juce::Colour (224, 76, 197) };                                        // the Experimental colour #E04CC5: value rings, the playing step, the values, the plugin code
const juce::Colour kIce (143, 211, 244);                                                       // a held buffer
const juce::Colour kOpColour[tf1::kOps] = { juce::Colour (239, 231, 213), juce::Colour (255, 196, 70), juce::Colour (120, 205, 255), juce::Colour (140, 230, 120),
                                            juce::Colour (255, 130, 90), juce::Colour (190, 150, 255), juce::Colour (150, 150, 156) };
const char* const kStepIds[tf1::kSteps] = { "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11", "s12", "s13", "s14", "s15", "s16" };

// panels: 0 SEQUENCER (STEPS) / CONTROLS (SCRUB, GRAINS), with the mode tabs; 1 FRACTURE; 2 GLITCH (STEPS) / TYPE (SCRUB, GRAINS); 3 OUTPUT; 4 BUFFER; 5 the slices or the grain cloud (rows 2 and 3)
// Two columns as in the RH-1: 703 and 343 (x 28 and 749); the OUTPUT and the meter are in the same place in every mode.
constexpr float kPanelX[6] = { 28.0f, 28.0f, 749.0f, 749.0f, 28.0f, 28.0f }, kPanelW[6] = { 1064.0f, 703.0f, 343.0f, 343.0f, 703.0f, 703.0f };
juce::Rectangle<float> panelRect (int i)
{
    if (i == 5) return { kPanelX[5], kRow2Y, kPanelW[5], kRow3Y + kRow3H - kRow2Y };
    const float y = i == 0 ? kRow1Y : i < 3 ? kRow2Y : kRow3Y, h = i == 0 ? kRow1H : i < 3 ? kRow2H : kRow3H;
    return { kPanelX[i], y, kPanelW[i], h };
}
const std::vector<juce::String> kModeTabs { "STEPS", "SCRUB", "GRAINS" }, kScrubTabs { "REVERSE", "SCRUB", "STUTTER", "GLITCH", "REPEAT" }, kGrainTabs { "SOFT", "SHARP", "PERCUSSIVE", "SWELL" };
constexpr float kTabsTop = -15.0f, kTabsFont = 16.0f;
const float kTabsLeft = juce::jmax (ui::titleEnd ("SEQUENCER"), ui::titleEnd ("CONTROLS"));     // the tabs stay in place in every mode
std::vector<juce::Rectangle<float>> modeTabRects() { return ui::tabRects (panelRect (0), kModeTabs, kTabsTop, kTabsFont, kTabsLeft); }
juce::Rectangle<float> syncRect() { const auto r = panelRect (0); return { r.getX() + 20.0f, r.getY() + 200.0f, 90.0f, 30.0f }; }
juce::Rectangle<float> freezeRect() { const auto r = panelRect (0); return { r.getX() + 116.0f, r.getY() + 200.0f, 120.0f, 30.0f }; }
juce::Rectangle<float> typeRect (int i) { const auto r = panelRect (2); return { r.getX() + 20.0f, r.getY() + 28.0f + 38.0f * (float) i, r.getWidth() - 40.0f, 30.0f }; }
juce::Rectangle<float> screenRect (int panel) { const auto r = panelRect (panel); return { r.getX() + 20.0f, r.getY() + 24.0f, r.getWidth() - 40.0f, r.getHeight() - 48.0f }; }     // the phosphor screen of BUFFER and of the display
juce::Rectangle<float> bufferPlot() { const auto s = screenRect (4); return { s.getX() + 16.0f, s.getY() + 36.0f, s.getWidth() - 32.0f, s.getHeight() - 70.0f }; }
juce::Rectangle<float> slicesPlot() { const auto s = screenRect (5); return { s.getX() + 16.0f, s.getY() + 40.0f, s.getWidth() - 32.0f, s.getHeight() - 74.0f }; }
juce::Rectangle<float> cloudPlot() { const auto s = screenRect (5); return { s.getX() + 60.0f, s.getY() + 40.0f, s.getWidth() - 80.0f, s.getHeight() - 74.0f }; }
juce::Point<float> knobCentre (int panel, int k, int n)
{
    const auto r = panelRect (panel);
    if (panel == 0) return { r.getCentreX() + 132.5f * ((float) k - 0.5f * (float) (n - 1)), r.getY() + 86.0f };                 // CONTROLS: 5 or 8 knobs, 132.5 apart
    if (panel == 1) return { r.getX() + 68.0f + (r.getWidth() - 136.0f) * (float) k / (float) (n - 1), r.getY() + 112.0f };       // FRACTURE: five cells spread over the panel
    if (panel == 3) return { r.getX() + 66.0f + 106.0f * (float) k, r.getY() + 112.0f };                                           // OUTPUT: the knobs left of the meters
    return { r.getCentreX() + 108.0f * ((float) k - 0.5f * (float) (n - 1)), r.getY() + 112.0f };                                  // GLITCH
}
juce::Point<float> meterOrigin() { const auto r = panelRect (3); return { r.getRight() - 20.0f - ui::kMeterW, r.getY() + (r.getHeight() - ui::kMeterH) * 0.5f }; }
// the SEQUENCER: the 16 steps (58 wide, spread over 1024), the legend, and the row of buttons 200 below the top
juce::Rectangle<float> stepRect (int s) { const auto r = panelRect (0); return { r.getX() + 20.0f + (1024.0f - 58.0f) / 15.0f * (float) s, r.getY() + 22.0f, 58.0f, 104.0f }; }
juce::Rectangle<float> divisionRect (int d) { const auto r = panelRect (0); return { r.getX() + 20.0f + 66.0f * (float) d, r.getY() + 200.0f, 60.0f, 30.0f }; }
juce::Rectangle<float> lengthMinus() { const auto r = panelRect (0); return { r.getX() + 434.0f, r.getY() + 200.0f, 30.0f, 30.0f }; }
juce::Rectangle<float> lengthPlus() { const auto r = panelRect (0); return { r.getX() + 616.0f, r.getY() + 200.0f, 30.0f, 30.0f }; }
juce::Rectangle<float> holdRect() { const auto r = panelRect (0); return { r.getX() + 668.0f, r.getY() + 200.0f, 160.0f, 30.0f }; }
juce::Rectangle<float> clearRect() { const auto r = panelRect (0); return { r.getX() + 834.0f, r.getY() + 200.0f, 90.0f, 30.0f }; }
juce::Rectangle<float> randomRect() { const auto r = panelRect (0); return { r.getX() + 930.0f, r.getY() + 200.0f, 114.0f, 30.0f }; }
}

//==============================================================================
TF1AudioProcessorEditor::TF1AudioProcessorEditor (TF1AudioProcessor& p)
    : LabEditor (p, kW, kH), proc (p),
      knobs { { { "chance", 1, 0, 5, "CHANCE", 0, 1, {}, {} },     { "loop", 1, 1, 5, "LOOP", 0, 1, {}, {} },        { "scramble", 1, 2, 5, "SCRAMBLE", 0, 1, {}, {} },
                { "pitch", 1, 3, 5, "PITCH", 0, 1, {}, {} },       { "stretch", 1, 4, 5, "STRETCH", 0, 1, {}, {} },
                { "crush", 2, 0, 2, "CRUSH", 0, 1, {}, {} },       { "tempo", 2, 1, 2, "TEMPO", 2, 1, {}, {} },
                { "mix", 3, 0, 2, "MIX", 0, 7, {}, {} },           { "level", 3, 1, 2, "LEVEL", 1, 7, {}, {} },
                { "scrubLength", 0, 0, 5, "LENGTH", 3, 2, {}, {} }, { "scrubSpeed", 0, 1, 5, "SPEED", 4, 2, {}, {} },  { "scrubSmooth", 0, 2, 5, "SMOOTH", 0, 2, {}, {} },
                { "scrubRandom", 0, 3, 5, nullptr, 10, 2, {}, {} }, { "scrubRepeats", 0, 4, 5, nullptr, 10, 2, {}, {} },
                { "grainSize", 0, 0, 8, "SIZE", 5, 4, {}, {} },     { "grainDensity", 0, 1, 8, "DENSITY", 6, 4, {}, {} }, { "grainPosition", 0, 2, 8, "POSITION", 7, 4, {}, {} },
                { "grainSpray", 0, 3, 8, "SPRAY", 0, 4, {}, {} },   { "grainPitch", 0, 4, 8, "PITCH", 8, 4, {}, {} },     { "grainRandom", 0, 5, 8, "RANDOM", 9, 4, {}, {} },
                { "grainReverse", 0, 6, 8, "REVERSE", 0, 4, {}, {} }, { "grainSpread", 0, 7, 8, "SPREAD", 0, 4, {}, {} } } }
{
    for (auto& k : knobs)
        attachKnob (k.slider, k.att, k.id);
    layoutKnobs();
    startTimerHz (30);
}

TF1AudioProcessorEditor::~TF1AudioProcessorEditor() = default;

void TF1AudioProcessorEditor::timerCallback()
{
    meter.update (proc.outMeter);
    const int mode = modeNow();
    if (mode != shownMode) { layoutKnobs(); repaint(); }                                                       // a preset, undo or automation changed the mode
    scroll += 0.033f;
    if (mode == 0)
    {
        repaintDesign (panelRect (4).expanded (4.0f));                                                         // the buffer
        repaintDesign (panelRect (0).expanded (4.0f));                                                         // the playing step
    }
    else repaintDesign (panelRect (5).expanded (4.0f));                                                        // the slices or the grain cloud
    repaintDesign (juce::Rectangle<float> (meterOrigin().x, meterOrigin().y, ui::kMeterW, ui::kMeterH).expanded (2.0f));
    if (midiUI != nullptr && midiUI->needsRepaint()) { midiUI->tick(); repaint(); }
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
        placeKnob (k.slider, knobShown (i) ? knobCentre (k.panel, k.index, k.count) : juce::Point<float> (-2000.0f, -2000.0f));      // hidden: parked off screen so the MIDI mark is not drawn
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
// (coloured by its operation: a REVERSE reads it backwards, a REPEAT loops it). A held buffer is tinted ice: it is not recorded any more. Drawn on a phosphor screen.
void TF1AudioProcessorEditor::drawBuffer (juce::Graphics& g)
{
    const bool held = proc.engine.held(), on = isOn ("power");
    const int op = proc.engine.currentOp();
    ui::paintScope (g, kPal, screenRect (4), held ? "HELD: NOT RECORDED" : "RECORDING",
                    on ? juce::String ("STEP ") + juce::String (proc.engine.currentStep() + 1) + "  " + tf1::opName (op) : juce::String ("OFF"));
    const auto r = bufferPlot();
    g.saveState();
    g.reduceClipRegion (screenRect (4).reduced (3.0f).toNearestInt());
    const float fs = proc.engine.sampleRate() > 0.0f ? proc.engine.sampleRate() : 48000.0f;
    const double stepLen = std::max (64.0, (double) proc.engine.stepLengthSamples());
    const int length = juce::jlimit (1, tf1::kSteps, (int) std::lround (value ("length")));
    const double windowSamples = std::min (stepLen * length, (double) proc.engine.capacitySeconds() * fs * 0.9);
    const double elapsed = held ? 0.0 : (double) proc.engine.stepElapsed();
    if (held) { g.setColour (kIce.withAlpha (0.10f)); g.fillRect (r); }

    const int count = proc.engine.peakCount();                                                                   // the recording as peaks (1024 samples each)
    const int cols = (int) r.getWidth();
    g.setColour (held ? kIce.withAlpha (0.85f) : kPal.phosphor.withAlpha (0.75f));
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
    g.setColour (kPal.phosphorDim.withAlpha (0.4f)); g.drawLine (r.getX(), r.getCentreY(), r.getRight(), r.getCentreY(), 1.0f);

    const auto xOfBack = [&] (double back) { return r.getRight() - r.getWidth() * (float) (back / windowSamples); };        // x of a point `back` samples before now
    for (int k = 0; k <= length; ++k)                                                                            // the step grid (counted back from the start of the current step)
    {
        const double back = elapsed + k * stepLen;
        if (back > windowSamples) break;
        const float x = xOfBack (back);
        g.setColour (kPal.phosphor.withAlpha (k == 0 ? 0.6f : 0.22f)); g.drawLine (x, r.getY(), x, r.getBottom(), k == 0 ? 1.6f : 1.0f);
    }

    const double len = (double) proc.engine.sourceLength(), back0 = (double) proc.engine.sourceBack() + elapsed;       // where the playing piece comes from
    if (on && len > 0.0 && op != tf1::Play && op != tf1::Mute)
    {
        const float xa = xOfBack (std::min (back0, windowSamples)), xb = xOfBack (std::max (0.0, back0 - len));
        g.setColour (kOpColour[op].withAlpha (0.22f)); g.fillRect (juce::Rectangle<float> (xa, r.getY(), std::max (2.0f, xb - xa), r.getHeight()));
        g.setColour (kOpColour[op]); g.drawRect (juce::Rectangle<float> (xa, r.getY(), std::max (2.0f, xb - xa), r.getHeight()), 1.6f);
        drawText (g, op == tf1::Reverse ? "<<" : ">>", (xa + xb) * 0.5f, r.getBottom() - 14.0f, 15.0f, kOpColour[op], true, 0.0f, juce::Justification::centred, ui::Face::Mono);
    }
    g.restoreState();
    const float ty = screenRect (4).getBottom() - 17.0f;
    drawText (g, juce::String ("-") + juce::String (windowSamples / fs, 1) + " S", r.getX(), ty, 13.0f, kPal.phosphorDim, false, 0.04f, juce::Justification::centredLeft, ui::Face::Mono);
    drawText (g, "NOW", r.getRight(), ty, 13.0f, kPal.phosphorDim, true, 0.1f, juce::Justification::centredRight, ui::Face::Mono);
}

//==============================================================================
// SCRUB: the audio is cut into slices of LENGTH. Each slice is drawn as the path its read position takes through the captured audio (top = newest): REVERSE runs
// backwards from the newest sound at SPEED, SCRUB wanders by the scrub amount, STUTTER and REPEAT keep playing the same piece, GLITCH jumps to a random place per slice.
// (the RV-1 display)
void TF1AudioProcessorEditor::drawSlices (juce::Graphics& g)
{
    const auto r = slicesPlot();
    const int mode = juce::jlimit (0, rv1::kNumModes - 1, choice ("scrubMode"));
    const bool freeze = isOn ("scrubFreeze");
    const float length = value ("scrubLength") * 0.01f, speed = rv1::speedRatio (value ("scrubSpeed") * 0.01f), wander = value ("scrubRandom") * 0.01f, repeats = value ("scrubRepeats") * 0.01f, smooth = value ("scrubSmooth") * 0.01f;
    const float sliceMs = isOn ("scrubSync") ? 60000.0f / 120.0f * rv1::divisionBeats (rv1::divisionIndex (length)) : rv1::lengthMs (length);
    ui::paintScope (g, kPal, screenRect (5), juce::String (rv1::modeName (mode)) + "  SLICE " + (sliceMs >= 1000.0f ? juce::String (sliceMs * 0.001f, 2) + " S" : juce::String ((int) std::lround (sliceMs)) + " MS") + "  SPEED " + juce::String (speed, 2) + "X",
                    freeze ? "FROZEN" : "");
    const float spanMs = 3000.0f, x0 = r.getX() + 4.0f, w = r.getWidth() - 8.0f;
    const auto xOf = [&] (float ms) { return x0 + w * ms / spanMs; };
    g.setColour (kPal.phosphor.withAlpha (0.12f));
    for (float t = 0.0f; t <= 3.0f; t += 0.5f)
    {
        g.drawLine (xOf (t * 1000.0f), r.getY() + 4.0f, xOf (t * 1000.0f), r.getBottom() - 4.0f, 1.0f);
        drawText (g, juce::String (t, 1) + " s", xOf (t * 1000.0f), screenRect (5).getBottom() - 17.0f, 13.0f, kPal.phosphorDim, false, 0.0f, juce::Justification::centred, ui::Face::Mono);
    }
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
        g.setColour ((freeze ? kIce : kPal.phosphor).withAlpha (0.18f));
        g.strokePath (slice, juce::PathStrokeType (5.0f + 3.0f * smooth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));             // glow
        g.setColour (freeze ? kIce : kPal.phosphor);
        g.strokePath (slice, juce::PathStrokeType (2.0f + 3.0f * smooth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (kPal.phosphorDim.withAlpha (0.35f));
        g.drawLine (xb, r.getY() + 4.0f, xb, r.getBottom() - 4.0f, 1.0f);
    }
}

// GRAINS: every grain is a dot; left = long ago, right = just now (the dots drift left); up = higher pitch; halo = grain length (the GR-1 display)
void TF1AudioProcessorEditor::drawCloud (juce::Graphics& g)
{
    const auto r = cloudPlot();
    ui::paintScope (g, kPal, screenRect (5), "SEMITONES", "HALO = GRAIN LENGTH");
    for (int st : { -12, 0, 12 })
    {
        const float y = r.getY() + r.getHeight() * (1.0f - (0.5f + (float) st / 48.0f));
        g.setColour (kPal.phosphor.withAlpha (st == 0 ? 0.30f : 0.14f));
        g.drawLine (r.getX(), y, r.getRight(), y, 1.0f);
        drawText (g, st == 0 ? "0" : juce::String::formatted ("%+d", st), r.getX() - 10.0f, y, 13.0f, kPal.phosphorDim, false, 0.0f, juce::Justification::centredRight, ui::Face::Mono);
    }
    const float ty = screenRect (5).getBottom() - 17.0f;
    drawText (g, "PAST", r.getX(), ty, 13.0f, kPal.phosphorDim, true, 0.14f, juce::Justification::centredLeft, ui::Face::Mono);
    drawText (g, "NOW", r.getRight(), ty, 13.0f, kPal.phosphorDim, true, 0.14f, juce::Justification::centredRight, ui::Face::Mono);
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
        g.setColour (kPal.phosphor.withAlpha (0.16f * fade));
        g.fillEllipse (juce::Rectangle<float> (2.0f * rad, 2.0f * rad).withCentre ({ x, y }));
        g.setColour (kPal.phosphor.withAlpha (0.25f + 0.75f * fade));
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
    if (midiUI != nullptr) midiUI->paintMarks (g, kPal.accent);
}

void TF1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    const bool on = isOn ("power");
    ui::paintBody (g, kPal, kW, kH);
    ui::paintHeader (g, kPal, "TF-1", "TIME FRACTURE", "BROKEN RHYTHMS");
    ui::paintPresetBar (g, kPal, proc.presets, proc.undoManager, on);
    ui::paintFooter (g, kPal, kH - 15.0f);

    const int mode = modeNow();
    ui::paintPanel (g, kPal, panelRect (0), mode == 0 ? "SEQUENCER" : "CONTROLS");
    ui::paintTabs (g, kPal, panelRect (0), kModeTabs, mode, kTabsTop, kTabsFont, kTabsLeft);
    ui::paintPanel (g, kPal, panelRect (3), "OUTPUT");
    if (mode != 0)
    {
        ui::paintPanel (g, kPal, panelRect (2), "TYPE");
        ui::paintPanel (g, kPal, panelRect (5), mode == 1 ? "SLICES" : "GRAIN CLOUD");
        ui::paintFlatButton (g, kPal, syncRect(), "SYNC", isOn (mode == 1 ? "scrubSync" : "grainSync"));
        ui::paintFlatButton (g, kPal, freezeRect(), "FREEZE", isOn (mode == 1 ? "scrubFreeze" : "grainFreeze"));
        const auto& names = mode == 1 ? kScrubTabs : kGrainTabs;
        const int sel = mode == 1 ? choice ("scrubMode") : choice ("grainShape");
        for (int i = 0; i < (int) names.size(); ++i) ui::paintFlatButton (g, kPal, typeRect (i), names[(size_t) i], i == sel);
        if (mode == 1) drawSlices (g); else drawCloud (g);
    }
    else
    {
        ui::paintPanel (g, kPal, panelRect (1), "FRACTURE");
        ui::paintPanel (g, kPal, panelRect (2), "GLITCH");
        ui::paintPanel (g, kPal, panelRect (4), "BUFFER");
        drawBuffer (g);

        const int length = juce::jlimit (1, tf1::kSteps, (int) std::lround (value ("length"))), playing = on ? proc.engine.currentStep() : -1;
        for (int s = 0; s < tf1::kSteps; ++s)                                                                  // the 16 steps
        {
            const auto b = stepRect (s);
            const int op = stepOp (s);
            const bool inside = s < length, now = s == playing && inside;
            const float a = inside ? 1.0f : 0.35f;
            g.setColour (kOpColour[op].withAlpha (a));
            g.fillRoundedRectangle (b, 4.0f);
            g.setColour (kPal.ink.withAlpha (a));
            g.drawRoundedRectangle (b.reduced (1.0f), 4.0f, 2.0f);
            if (now) { g.setColour (kPal.accent); g.drawRoundedRectangle (b.reduced (1.0f), 4.0f, 3.5f); }
            drawText (g, tf1::opShort (op), b.getCentreX(), b.getCentreY() - 8.0f, 17.0f, op == tf1::Play ? kPal.inkSoft.withAlpha (a) : kPal.ink.withAlpha (a), true, 0.06f);
            drawText (g, juce::String (s + 1), b.getCentreX(), b.getBottom() - 16.0f, 14.0f, kPal.inkSoft.withAlpha (a), true, 0.0f, juce::Justification::centred, ui::Face::Mono);
            if (now) { juce::Path tri; tri.addTriangle (b.getCentreX() - 7.0f, b.getBottom() + 12.0f, b.getCentreX() + 7.0f, b.getBottom() + 12.0f, b.getCentreX(), b.getBottom() + 4.0f); g.setColour (kPal.accent); g.fillPath (tri); }
        }
        const int div = juce::jlimit (0, 5, (int) std::lround (value ("division")));
        for (int d = 0; d < 6; ++d) ui::paintFlatButton (g, kPal, divisionRect (d), tf1::divisionName (d), d == div);
        ui::paintFlatButton (g, kPal, lengthMinus(), "-", false, 20.0f);
        ui::paintFlatButton (g, kPal, lengthPlus(), "+", false, 20.0f);
        drawText (g, "LENGTH " + juce::String (length), (lengthMinus().getRight() + lengthPlus().getX()) * 0.5f, lengthMinus().getCentreY(), 17.0f, kPal.ink, true, 0.08f);
        ui::paintFlatButton (g, kPal, holdRect(), "HOLD BUFFER", isOn ("hold"));
        ui::paintFlatButton (g, kPal, clearRect(), "CLEAR", false);
        ui::paintFlatButton (g, kPal, randomRect(), "RANDOM", false);
        {   // the legend of the operations
            const auto r = panelRect (0);
            const float ly = r.getY() + 164.0f;
            float x = r.getX() + 20.0f;
            for (int o = 0; o < tf1::kOps; ++o)
            {
                const auto sw = juce::Rectangle<float> (14.0f, 14.0f).withCentre ({ x + 7.0f, ly });
                g.setColour (kOpColour[o]); g.fillRoundedRectangle (sw, 3.0f);
                g.setColour (kPal.ink); g.drawRoundedRectangle (sw.reduced (0.75f), 3.0f, 1.5f);
                drawText (g, tf1::opName (o), x + 22.0f, ly, 14.0f, kPal.inkSoft, true, 0.08f, juce::Justification::centredLeft);
                x += 22.0f + ui::textWidth (ui::font (14.0f, true).withExtraKerningFactor (0.08f), tf1::opName (o)) + 26.0f;
            }
            drawText (g, "CLICK A STEP TO CHANGE ITS OPERATION", r.getRight() - 20.0f, ly, 14.0f, kPal.inkSoft, true, 0.08f, juce::Justification::centredRight);
        }
        drawText (g, "NO HOST TEMPO", panelRect (2).getCentreX(), panelRect (2).getY() + 38.0f, 13.0f, kPal.inkSoft, false, 0.04f, juce::Justification::centred, ui::Face::Mono);
    }

    for (int i = 0; i < (int) knobs.size(); ++i)
    {
        if (! knobShown (i)) continue;
        auto& k = knobs[(size_t) i];
        const double v = k.slider.getValue();
        static const char* lo[11] = { "0", "-24", "40", "20 ms", ".25x", "10", "2", "0", "-12", "0", "0" }, *hi[11] = { "100", "+12", "240", "2 s", "4x", "500", "80", "2 s", "+12", "+-12", "100" };
        const char* name = k.name != nullptr ? k.name : rv1::chaosName (choice ("scrubMode"), k.index == 3 ? 0 : 1);
        ui::paintKnob (g, kPal, knobCentre (k.panel, k.index, k.count), name, knobText (k.kind, v), lo[k.kind], hi[k.kind], (float) k.slider.valueToProportionOfLength (v));
    }
    ui::paintMeter (g, kPal, meter, meterOrigin().x, meterOrigin().y, on);
}

void TF1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / scale();
    proc.undoManager.beginNewTransaction();
    if (ui::powerRect().contains (p)) { toggle ("power"); return; }
    if (ui::presetBarMouseDown (p, *this, proc.presets, proc.undoManager, scale(), [this] { repaint(); })) { layoutKnobs(); return; }
    const int mode = modeNow();
    {
        const auto tabs = modeTabRects();
        for (int i = 0; i < (int) tabs.size(); ++i)
            if (tabs[(size_t) i].expanded (3.0f).contains (p)) { setValue ("mode", (float) i); layoutKnobs(); repaint(); return; }
    }
    if (mode != 0)
    {
        const int n = mode == 1 ? (int) kScrubTabs.size() : (int) kGrainTabs.size();
        for (int i = 0; i < n; ++i)
            if (typeRect (i).expanded (3.0f).contains (p)) { setValue (mode == 1 ? "scrubMode" : "grainShape", (float) i); repaint(); return; }
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
