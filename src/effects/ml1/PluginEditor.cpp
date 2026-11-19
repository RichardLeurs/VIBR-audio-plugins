// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "ui/Presets.h"
#include <complex>
#include <cstring>
#include <functional>

namespace
{
namespace ui = vibr::ui::vintage;          // the default look (src/common/ui/VintageUI.h)
using ui::drawText;

constexpr float kW = ui::kW, kH = ui::kH, kRow1Y = 114.0f, kRow1H = 244.0f, kRow2Y = 376.0f, kRow2H = 254.0f;
const ui::Palette kPal { juce::Colour (19, 191, 160) };            // the Modulation colour #13BFA0: value rings, knob values and the plugin code

// panels: 0 MODE (mode tabs), 1 OUTPUT, 2 MOTION (display)
constexpr float kPanelX[3] = { 28.0f, 749.0f, 28.0f }, kPanelW[3] = { 703.0f, 343.0f, 1064.0f };
juce::Rectangle<float> panelRect (int i) { return { kPanelX[i], i < 2 ? kRow1Y : kRow2Y, kPanelW[i], i < 2 ? kRow1H : kRow2H }; }
juce::Point<float> knobCentre (int panel, int k, int n)
{
    const auto r = panelRect (panel);
    if (panel == 1) return { r.getX() + 66.0f + 106.0f * (float) k, r.getY() + 112.0f };                         // MIX and LEVEL left of the meters (as in the RH-1)
    return { r.getCentreX() + (n > 2 ? 140.0f : 108.0f) * ((float) k - 0.5f * (float) (n - 1)), r.getY() + 86.0f };       // the knobs of a panel with tabs: centre 86 below the top
}
juce::Point<float> meterOrigin() { const auto r = panelRect (1); return { r.getRight() - 20.0f - ui::kMeterW, r.getY() + (r.getHeight() - ui::kMeterH) * 0.5f }; }
juce::Rectangle<float> variantRect (int v) { const auto r = panelRect (0); return { r.getX() + 20.0f + (float) v * 126.0f, r.getY() + 200.0f, 120.0f, 30.0f }; }
juce::Rectangle<float> syncRect() { const auto r = panelRect (0); return { r.getRight() - 20.0f - 120.0f, r.getY() + 200.0f, 120.0f, 30.0f }; }
juce::Rectangle<float> motionRect() { const auto r = panelRect (2); return { r.getX() + 20.0f, r.getY() + 26.0f, r.getWidth() - 40.0f, r.getHeight() - 46.0f }; }       // the phosphor screen
// ui::paintHeader with the name line shrunk to end 14 before the preset screen ("MODULATION LAB" at 31 is wider than the room left of the screen)
void paintHeader (juce::Graphics& g)
{
    const juce::String name ("ML-1"), line1 ("MODULATION LAB"), line2 ("WAVE MOVEMENT");
    drawText (g, name, ui::kMargin, 48.0f, 74.0f, kPal.accent, true, -0.01f, juce::Justification::centredLeft, ui::Face::Title);
    const float x = ui::kMargin + ui::textWidth (ui::font (74.0f, true, ui::Face::Title).withExtraKerningFactor (-0.01f), name) + 30.0f;
    float size = 31.0f;
    const float room = ui::presetChipRect().getX() - 14.0f - x - 8.0f;
    if (const float wd = ui::textWidth (ui::font (size, true).withExtraKerningFactor (0.1f), line1); wd > room) size *= room / wd;
    drawText (g, line1, x, 37.0f, size, kPal.ink, true, 0.1f, juce::Justification::centredLeft);
    drawText (g, line2, x, 64.0f, 20.0f, kPal.inkSoft, true, 0.2f, juce::Justification::centredLeft);
}
const std::vector<juce::String> kModeTabs { "CHORUS", "VIBRATO", "FLANGER", "PHASER", "UNI-VIBE", "ROTARY", "RING MOD" };
constexpr float kTabsTop = -15.0f, kTabsFont = 16.0f;
const float kTabsLeft = ui::titleEnd ("MODE");
std::vector<juce::Rectangle<float>> modeTabRects() { return ui::tabRects (panelRect (0), kModeTabs, kTabsTop, kTabsFont, kTabsLeft); }
}

//==============================================================================
ML1AudioProcessorEditor::ML1AudioProcessorEditor (ML1AudioProcessor& p)
    : LabEditor (p, kW, kH), proc (p),
      knobs { { { "rate", 0, 0, 3, "RATE", 0, {}, {} },     { "depth", 0, 1, 3, "DEPTH", 1, {}, {} },    { "extra", 0, 2, 3, nullptr, 1, {}, {} },
                { "mix", 1, 0, 2, "MIX", 1, {}, {} },       { "level", 1, 1, 2, "LEVEL", 2, {}, {} } } }
{
    for (auto& k : knobs)
    {
        attachKnob (k.slider, k.att, k.id);
        midiUI.add (k.slider, k.id);
    }
    layoutKnobs();
    startTimerHz (30);
}

ML1AudioProcessorEditor::~ML1AudioProcessorEditor() = default;

void ML1AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    midiUI.paintMarks (g, kPal.accent);
}

void ML1AudioProcessorEditor::timerCallback()
{
    meter.update (proc.outMeter);
    repaintDesign (juce::Rectangle<float> (meterOrigin().x, meterOrigin().y, ui::kMeterW, ui::kMeterH).expanded (2.0f));
    if (midiUI.needsRepaint()) { midiUI.tick(); repaint(); }
    if (currentMode() != shownMode) { layoutKnobs(); repaint(); }                                   // the mode changed by a preset, the host or a click
    repaintDesign (panelRect (2).expanded (4.0f));     // the picture moves
}


int ML1AudioProcessorEditor::countOf (const Knob& k) const
{
    return k.panel == 0 && ! ml1::hasExtra (currentMode()) ? 2 : k.count;       // the two knobs of the new modes are centred
}

void ML1AudioProcessorEditor::layoutKnobs()
{
    for (auto& k : knobs)
    {
        const bool shown = k.name != nullptr || ml1::hasExtra (currentMode());                  // the third knob (WIDTH / FEEDBACK) exists in the four original modes only
        placeKnob (k.slider, shown ? knobCentre (k.panel, k.index, countOf (k)) : juce::Point<float> (-2000.0f, -2000.0f));       // hidden: parked off screen so its MIDI mark is not drawn
        k.slider.setVisible (shown);
    }
    shownMode = currentMode();
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
    const auto bright = kPal.phosphor, dim = kPal.phosphorDim;
    const auto& eng = proc.getEngine();
    const bool power = isOn ("power");
    const float phase = power ? eng.visPhase.load (std::memory_order_relaxed) : 0.0f, hz = power ? eng.visHz.load (std::memory_order_relaxed) : 0.0f;
    const float visA = power ? eng.visA.load (std::memory_order_relaxed) : 0.0f, visB = power ? eng.visB.load (std::memory_order_relaxed) : 0.0f;
    const auto param = [this] (const char* id) { return value (id) * 0.01f; };
    const float depth = param ("depth"), extra = param ("extra");
    const float x0 = r.getX() + 20.0f, x1 = r.getRight() - 20.0f, w = x1 - x0;
    constexpr float twoPi = juce::MathConstants<float>::twoPi;

    const int stages = ml1::stageCount (variant);
    const juce::String hint = mode == ml1::UniVibe ? (variant == 1 ? "PHASE VIBRATO" : "PHASE CHORUS")
                            : mode == ml1::Rotary ? (variant == 1 ? "FAST (TREMOLO)" : "SLOW (CHORALE)")
                            : mode == ml1::RingMod ? (variant == 0 ? "SINE CARRIER" : variant == 1 ? "TRIANGLE CARRIER" : "SQUARE CARRIER")
                            : mode == ml1::Chorus ? juce::String (ml1::voiceCount (variant)) + " VOICE CHORUS"
                            : mode == ml1::Vibrato ? (variant == 1 ? "TRIANGLE PITCH WOBBLE" : "SINE PITCH WOBBLE")
                            : mode == ml1::Flanger ? (variant == 1 ? "COMB, NEGATIVE FEEDBACK" : "COMB, POSITIVE FEEDBACK")
                            : juce::String (stages) + "-STAGE ALL-PASS NOTCHES";
    const juce::String rateText = hz >= 1000.0f ? juce::String (hz * 0.001f, 2) + " kHz" : (mode == ml1::RingMod ? juce::String ((int) std::lround (hz)) : juce::String (hz, 2)) + " Hz";
    ui::paintScope (g, kPal, r, rateText, hint);                                  // the screen with its grid, the rate (left) and what is drawn (right) in the strip above the plot

    juce::Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (r.reduced (3.0f).toNearestInt());
    const auto mono = [&g] (const juce::String& t, float x, float y, float size, juce::Colour c, juce::Justification j = juce::Justification::centred)
    {
        drawText (g, t, x, y, size, c, false, 0.04f, j, ui::Face::Mono);
    };

    if (mode == ml1::UniVibe)
    {
        // the four all-pass stages sliding along a frequency axis (100 Hz .. 8 kHz) with the lamp
        const float yTop = r.getY() + 52.0f, yAxis = r.getBottom() - 40.0f;
        const auto xOf = [&] (float f) { return x0 + w * std::log (f / 100.0f) / std::log (80.0f); };
        g.setColour (dim.withAlpha (0.6f));
        g.drawLine (x0, yAxis, x1, yAxis, 1.4f);
        for (float f : { 100.0f, 300.0f, 1000.0f, 3000.0f, 8000.0f })
        {
            g.drawLine (xOf (f), yAxis, xOf (f), yAxis + 6.0f, 1.4f);
            mono (f >= 1000.0f ? juce::String ((int) (f / 1000.0f)) + " kHz" : juce::String ((int) f) + " Hz", juce::jlimit (x0 + 24.0f, x1 - 24.0f, xOf (f)), yAxis + 20.0f, 13.0f, dim);
        }
        const float lamp = visA;
        for (int k = 0; k < 4; ++k)
        {
            const float x = xOf (ml1::vibeStageHz (k, lamp)), top = yTop + 8.0f * (float) (3 - k);
            const auto bar = juce::Rectangle<float> (x - 4.0f, top, 8.0f, yAxis - top);
            g.setColour (bright.withAlpha (0.18f));
            g.fillRoundedRectangle (bar.expanded (6.0f, 0.0f), 7.0f);
            g.setColour (bright.withAlpha (0.55f + 0.45f * lamp));
            g.fillRoundedRectangle (bar, 4.0f);
            drawText (g, juce::String (k + 1), x, top - 10.0f, 13.0f, dim, true, 0.0f, juce::Justification::centred, ui::Face::Mono);
        }
        const auto lampC = juce::Point<float> (r.getCentreX(), r.getY() + 16.0f);
        g.setColour (bright.withAlpha (0.12f + 0.3f * lamp));
        g.fillEllipse (juce::Rectangle<float> (26.0f, 26.0f).withCentre (lampC));
        g.setColour (bright.withAlpha (0.35f + 0.65f * lamp));
        g.fillEllipse (juce::Rectangle<float> (11.0f, 11.0f).withCentre (lampC));
    }
    else if (mode == ml1::Rotary)
    {
        // the drum and the horn turning at their real speeds
        const auto disc = [&] (juce::Point<float> c, float rad, float th, const char* label)
        {
            g.setColour (kPal.screen.brighter (0.1f));
            g.fillEllipse (juce::Rectangle<float> (2.0f * rad, 2.0f * rad).withCentre (c));
            g.setColour (dim.withAlpha (0.7f));
            g.drawEllipse (juce::Rectangle<float> (2.0f * rad, 2.0f * rad).withCentre (c), 1.6f);
            for (int t = 0; t < 12; ++t)                                                   // grille ticks round the rim
            {
                const float an = twoPi * (float) t / 12.0f;
                g.drawLine (c.x + std::sin (an) * (rad - 8.0f), c.y - std::cos (an) * (rad - 8.0f), c.x + std::sin (an) * (rad - 2.0f), c.y - std::cos (an) * (rad - 2.0f), 1.2f);
            }
            const juce::Point<float> tip (c.x + std::sin (th) * (rad - 10.0f), c.y - std::cos (th) * (rad - 10.0f));
            g.setColour (bright);
            g.drawLine (c.x, c.y, tip.x, tip.y, 3.5f);
            g.fillEllipse (juce::Rectangle<float> (13.0f, 13.0f).withCentre (tip));
            g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (c));
            mono (label, c.x, c.y + rad + 16.0f, 14.0f, dim);
        };
        disc ({ r.getCentreX() - 130.0f, r.getY() + 98.0f }, 58.0f, visB, "DRUM");
        disc ({ r.getCentreX() + 130.0f, r.getY() + 98.0f }, 40.0f, visA, "HORN");
    }
    else if (mode == ml1::RingMod)
    {
        // the input, the carrier and the resulting product
        const float rate = param ("rate"), cyc = 1.0f + 5.0f * rate, gx0 = r.getX() + 120.0f, gw = r.getRight() - 24.0f - gx0, rowH = (r.getHeight() - 44.0f) / 3.0f;
        const auto carrierAt = [&] (float u) { return ml1::ringCarrier (u * cyc - visA, variant); };
        const auto trace = [&] (int row, float thick, juce::Colour col, const std::function<float (float)>& fn, const char* label)
        {
            const float yc = r.getY() + 32.0f + rowH * ((float) row + 0.5f);
            juce::Path path;
            for (int i = 0; i <= 240; ++i)
            {
                const float u = (float) i / 240.0f, y = yc - fn (u) * rowH * 0.42f;
                if (i == 0) path.startNewSubPath (gx0, y); else path.lineTo (gx0 + gw * u, y);
            }
            g.setColour (dim.withAlpha (0.4f));
            g.drawLine (gx0, yc, gx0 + gw, yc, 1.0f);
            g.setColour (col);
            g.strokePath (path, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            mono (label, r.getX() + 14.0f, yc, 14.0f, dim, juce::Justification::centredLeft);
        };
        trace (0, 1.5f, dim, [] (float u) { return std::sin (twoPi * 6.0f * u); }, "INPUT");
        trace (1, 1.5f, dim, carrierAt, "CARRIER");
        trace (2, 2.4f, bright, [&] (float u) { return std::sin (twoPi * 6.0f * u) * ((1.0f - depth) + depth * carrierAt (u)); }, "RESULT");
    }
    else if (mode == ml1::Chorus)
    {
        const int voices = ml1::voiceCount (variant);
        const float yTop = r.getY() + 34.0f, laneH = (r.getHeight() - 76.0f) / 4.0f;
        const auto xOf = [&] (float ms) { return x0 + w * (ms - 8.0f) / 22.0f; };
        for (int v = 0; v < voices; ++v)
        {
            const float yc = yTop + laneH * ((float) v + 0.5f) + (float) (4 - voices) * laneH * 0.5f;
            const float ms = ml1::chorusDelayMs (v, ml1::lfoShape (phase + (float) v / (float) voices, false), depth);
            g.setColour (dim.withAlpha (0.5f));
            g.drawLine (x0, yc, x1, yc, 1.4f);
            g.setColour (bright.withAlpha (0.2f));
            g.fillRoundedRectangle (juce::Rectangle<float> (xOf (ml1::chorusDelayMs (v, -1.0f, depth)), yc - 5.0f, xOf (ml1::chorusDelayMs (v, 1.0f, depth)) - xOf (ml1::chorusDelayMs (v, -1.0f, depth)), 10.0f), 5.0f);
            g.setColour (bright);
            g.fillEllipse (juce::Rectangle<float> (16.0f, 16.0f).withCentre ({ xOf (ms), yc }));
        }
        g.setColour (dim.withAlpha (0.6f));
        g.drawLine (x0, r.getBottom() - 36.0f, x1, r.getBottom() - 36.0f, 1.4f);
        for (float ms : { 8.0f, 19.0f, 30.0f })
        {
            g.drawLine (xOf (ms), r.getBottom() - 36.0f, xOf (ms), r.getBottom() - 30.0f, 1.4f);
            mono (juce::String ((int) ms) + " ms", xOf (ms), r.getBottom() - 17.0f, 13.0f, dim);
        }
    }
    else if (mode == ml1::Vibrato)
    {
        const bool tri = variant == 1;
        const float yc = r.getCentreY() + 14.0f, amp = r.getHeight() * 0.3f;
        juce::Path wave;
        for (int i = 0; i <= 120; ++i)
        {
            const float u = (float) i / 120.0f, y = yc - ml1::lfoShape (u, tri) * amp;
            if (i == 0) wave.startNewSubPath (x0, y); else wave.lineTo (x0 + w * u, y);
        }
        g.setColour (dim.withAlpha (0.5f));
        g.drawLine (x0, yc, x1, yc, 1.2f);
        g.setColour (dim);
        g.strokePath (wave, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const float lfo = ml1::lfoShape (phase, tri);
        const juce::Point<float> dot (x0 + w * phase, yc - lfo * amp);
        g.setColour (bright.withAlpha (0.25f));
        g.drawLine (dot.x, yc, dot.x, dot.y, 2.0f);
        g.setColour (bright);
        g.fillEllipse (juce::Rectangle<float> (16.0f, 16.0f).withCentre (dot));
        // the pitch follows the slope of the delay: -d(delay)/dt (a rising delay lowers the pitch)
        const float slope = tri ? (phase < 0.5f ? 4.0f : -4.0f) : twoPi * std::cos (twoPi * phase);
        const float ratio = 1.0f - 0.0036f * depth * slope * hz;
        const float cents = 1200.0f * std::log2 (std::max (0.2f, ratio));
        drawText (g, juce::String::formatted ("%+d cents", (int) std::lround (power ? cents : 0.0f)), x1 - 6.0f, r.getBottom() - 20.0f, 16.0f, bright, true, 0.0f, juce::Justification::centredRight, ui::Face::Mono);
    }
    else
    {
        // frequency response, 20 Hz .. 20 kHz on a log axis, -30 .. +12 dB
        const float yTop = r.getY() + 38.0f, yBot = r.getBottom() - 34.0f;
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
            for (int k = 0; k < stages; ++k) gAll *= ap;
            const float fbp = 0.95f * fb;                                   // the engine's feedback around the chain (one sample late)
            const std::complex<float> v = gAll / (1.0f - fbp * z1 * gAll);
            return std::abs (0.5f * (1.0f + v)) * (1.0f - 0.5f * fbp);
        };
        const auto xOf = [&] (float f) { return x0 + w * std::log (f / 20.0f) / std::log (1000.0f); };
        g.setColour (dim.withAlpha (0.5f));
        g.drawLine (x0, yOf (0.0f), x1, yOf (0.0f), 1.0f);
        g.setColour (dim.withAlpha (0.6f));
        g.drawLine (x0, yBot, x1, yBot, 1.4f);
        for (float f : { 20.0f, 200.0f, 2000.0f, 20000.0f })
        {
            g.drawLine (xOf (f), yBot, xOf (f), yBot + 6.0f, 1.4f);
            const float lx = juce::jlimit (x0 + 24.0f, x1 - 24.0f, xOf (f));                       // the end labels stay inside the screen
            mono (f >= 1000.0f ? juce::String ((int) (f / 1000.0f)) + " kHz" : juce::String ((int) f) + " Hz", lx, yBot + 20.0f, 13.0f, dim);
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
        g.setColour (bright.withAlpha (0.14f));
        g.fillPath (fill);
        g.setColour (bright.withAlpha (0.18f));
        g.strokePath (curve, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));       // glow
        g.setColour (bright);
        g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}


//==============================================================================
void ML1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    ui::paintBody (g, kPal, kW, kH);
    paintHeader (g);
    ui::paintPresetBar (g, kPal, proc.presets, proc.undoManager, isOn ("power"));
    ui::paintFooter (g, kPal, kH - 15.0f);

    const int mode = currentMode(), variant = currentVariant();
    ui::paintPanel (g, kPal, panelRect (0), "MODE");
    ui::paintTabs (g, kPal, panelRect (0), kModeTabs, mode, kTabsTop, kTabsFont, kTabsLeft);
    ui::paintPanel (g, kPal, panelRect (1), "OUTPUT");
    ui::paintPanel (g, kPal, panelRect (2), "MOTION");
    drawMotion (g, mode, variant);

    // the TYPE buttons of the mode (voices, LFO wave, feedback polarity, stages) and SYNC (the rate becomes a note length of the host tempo)
    for (int v = 0; v < ml1::variantCount (mode); ++v) ui::paintFlatButton (g, kPal, variantRect (v), ml1::variantName (mode, v), v == variant);
    {
        const bool avail = ml1::hasSync (mode);                                                    // the rotary and the ring mod have no tempo sync
        ui::paintFlatButton (g, kPal, syncRect(), "SYNC", avail && isOn ("sync"), 17.0f, avail ? 1.0f : 0.4f);
    }

    for (auto& k : knobs)
    {
        if (! k.slider.isVisible()) continue;
        const double v = k.slider.getValue();
        juce::String txt, lo = "0", hi = "100";
        if (k.kind == 2) { txt = juce::String::formatted ("%+d dB", (int) std::lround (v)); lo = "-24"; hi = "+12"; }
        else if (k.kind == 0)
        {
            const float f = (float) v * 0.01f;
            if (isOn ("sync") && ml1::hasSync (mode)) txt = ml1::divisionName (ml1::divisionIndex (f));
            else
            {
                const float hz = mode == ml1::Rotary ? ml1::hornHz (f, variant == 1) : ml1::rateHz (mode, f);
                txt = hz >= 1000.0f ? juce::String (hz * 0.001f, 2) + " kHz" : (mode == ml1::RingMod ? juce::String ((int) std::lround (hz)) : juce::String (hz, 2)) + " Hz";
            }
        }
        else txt = juce::String ((int) std::lround (v)) + " %";
        const auto c = knobCentre (k.panel, k.index, countOf (k));
        ui::paintKnob (g, kPal, c, k.name == nullptr ? ml1::extraName (mode) : k.kind == 0 ? ml1::rateName (mode) : std::strcmp (k.id, "depth") == 0 ? ml1::depthName (mode) : k.name, txt, lo, hi,
                       (float) k.slider.valueToProportionOfLength (k.slider.getValue()));
    }
    ui::paintMeter (g, kPal, meter, meterOrigin().x, meterOrigin().y, isOn ("power"));
}

void ML1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / scale();
    proc.undoManager.beginNewTransaction();
    if (ui::powerRect().contains (p)) { toggle ("power"); return; }
    if (ui::presetBarMouseDown (p, *this, proc.presets, proc.undoManager, scale(), [this] { repaint(); })) { layoutKnobs(); return; }
    const auto tabs = modeTabRects();
    for (int i = 0; i < (int) tabs.size(); ++i)
        if (tabs[(size_t) i].expanded (3.0f).contains (p)) { setChoice ("mode", i); setChoice ("variant", 0); repaint(); return; }
    if (ml1::hasSync (currentMode()) && syncRect().expanded (3.0f).contains (p)) { toggle ("sync"); repaint(); return; }
    for (int v = 0; v < ml1::variantCount (currentMode()); ++v)
        if (variantRect (v).expanded (3.0f).contains (p)) { setChoice ("variant", v); repaint(); return; }
}
