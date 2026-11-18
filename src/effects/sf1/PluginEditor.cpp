// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "ui/Presets.h"
#include "ui/OutputMeter.h"

namespace
{
using vibr::ui::drawText;
namespace ui = vibr::ui;

constexpr float kW = 1200.0f, kH = 890.0f, kRow1Y = 134.0f, kRow1H = 247.0f, kRow2Y = 405.0f, kRow2H = 216.0f, kRow3Y = 645.0f, kRow3H = 190.0f, kBarY = 39.0f;
const ui::Palette kPal { juce::Colour (150, 59, 208), juce::Colour (30, 10, 44) };             // the Experimental colour #963BD0 and its print
const juce::Colour kIce (143, 211, 244);

// panels: 0 HOLD, 1 TEXTURE, 2 SHIFT (row 1), 3 WINDOW, 4 OUTPUT, 5 STATE (row 2), 6 SPECTRUM (row 3)
constexpr float kPanelX[7] = { 20.0f, 350.0f, 720.0f, 20.0f, 490.0f, 950.0f, 20.0f };
constexpr float kPanelW[7] = { 320.0f, 360.0f, 460.0f, 460.0f, 450.0f, 230.0f, 1160.0f };
juce::Rectangle<float> panelRect (int i)
{
    const float y = i < 3 ? kRow1Y : i < 6 ? kRow2Y : kRow3Y, h = i < 3 ? kRow1H : i < 6 ? kRow2H : kRow3H;
    return { kPanelX[i], y, kPanelW[i], h };
}
// HARMONY (the PT-1 layout in the same window): panels 7 .. 10 VOICE 1 .. 4 (row 1), 11 KEY, 12 CHORDS, 13 OUTPUT (row 2), 14 KEYBOARD (row 3)
juce::Rectangle<float> harmonyRect (int i)
{
    if (i < 11) return { 20.0f + 292.5f * (float) (i - 7), kRow1Y, 282.5f, kRow1H };
    if (i == 14) return { 20.0f, kRow3Y, 1160.0f, kRow3H };
    static const float x[3] = { 20.0f, 400.0f, 780.0f }, w[3] = { 370.0f, 370.0f, 400.0f };
    return { x[i - 11], kRow2Y, w[i - 11], kRow2H };
}
juce::Rectangle<float> intelRect() { const auto r = harmonyRect (11); return juce::Rectangle<float> (124.0f, 40.0f).withCentre ({ r.getRight() - 20.0f - 62.0f, r.getY() + 92.0f }); }
juce::Rectangle<float> chordRect (int c)
{
    const auto r = harmonyRect (12);
    const float cx = c < 3 ? r.getCentreX() + 110.0f * (float) (c - 1) : r.getCentreX() + 55.0f * (float) (c == 3 ? -1 : 1);
    return juce::Rectangle<float> (100.0f, 36.0f).withCentre ({ cx, r.getY() + (c < 3 ? 64.0f : 110.0f) });
}
juce::Rectangle<float> keyboardRect() { const auto r = harmonyRect (14); return { r.getX() + 40.0f, r.getY() + 48.0f, r.getWidth() - 80.0f, 110.0f }; }
const std::vector<juce::String> kModeTabs { "FOLD", "HARMONY" };
juce::Rectangle<float> modeTabsRect() { return { 690.0f, 55.0f, 220.0f, 0.0f }; }              // the tab pills sit in the header, right of the preset bar
juce::Point<float> knobCentre (int panel, int k, int n)
{
    if (panel >= 7 && panel <= 10) { const auto r = harmonyRect (panel); return { r.getX() + 54.0f + (r.getWidth() - 108.0f) * (float) k * 0.5f, kRow1Y + 114.0f }; }      // a voice: INTERVAL, DETUNE, LEVEL
    if (panel == 11 || panel == 13) { const auto r = harmonyRect (panel); return { r.getX() + 61.0f + 90.0f * (float) k, r.getY() + 92.0f }; }                         // KEY: key, scale; OUTPUT: spread, mix, level
    const auto r = panelRect (panel);
    if (panel == 0) return { r.getX() + 95.0f + 130.0f * (float) k, r.getY() + 128.0f };                          // under the buttons
    const float cx = panel == 4 ? r.getX() + 165.0f : r.getCentreX();                                                // the output knobs sit left, the dB meters on the right
    return { cx + (panel == 4 ? 120.0f : 105.0f) * ((float) k - 0.5f * (float) (n - 1)), r.getY() + 92.0f };
}
float nameY (int panel) { return panel >= 7 && panel <= 10 ? kRow1Y + 114.0f + 64.0f : panel >= 11 ? kRow2Y + 154.0f : panelRect (panel).getY() + (panel == 0 ? 190.0f : 154.0f); }
float valueY (int panel) { return panel >= 7 && panel <= 10 ? kRow1Y + 114.0f + 86.0f : panel >= 11 ? kRow2Y + 182.0f : panelRect (panel).getY() + (panel == 0 ? 218.0f : 182.0f); }
juce::Point<float> meterOrigin (bool harmony) { const auto r = harmony ? harmonyRect (13) : panelRect (4); return { r.getX() + (harmony ? 310.0f : 317.0f), r.getY() + 46.0f }; }
juce::Rectangle<float> freezeRect() { const auto r = panelRect (0); return { r.getX() + 16.0f, r.getY() + 22.0f, 170.0f, 32.0f }; }
juce::Rectangle<float> captureRect() { const auto r = panelRect (0); return { r.getX() + 196.0f, r.getY() + 22.0f, 108.0f, 32.0f }; }
juce::Rectangle<float> bandRect (int b) { const auto r = panelRect (0); return { r.getX() + 16.0f + 74.0f * (float) b, r.getY() + 62.0f, 66.0f, 26.0f }; }
juce::Rectangle<float> plotRect() { const auto r = panelRect (6); return { r.getX() + 56.0f, r.getY() + 46.0f, r.getWidth() - 96.0f, 110.0f }; }
const char* const kBandIds[sf1::kBands] = { "fzLow", "fzLowMid", "fzHighMid", "fzHigh" };
const char* const kBandShort[sf1::kBands] = { "LOW", "L-MID", "H-MID", "HIGH" };

float xOfHz (float hz)                                                                                         // log frequency across the plot, 30 Hz .. 18 kHz
{
    const auto r = plotRect();
    return r.getX() + r.getWidth() * std::log (juce::jmax (1.0f, hz) / sf1::kDisplayLoHz) / std::log (sf1::kDisplayHiHz / sf1::kDisplayLoHz);
}
float hzOfX (float x)
{
    const auto r = plotRect();
    return sf1::kDisplayLoHz * std::pow (sf1::kDisplayHiHz / sf1::kDisplayLoHz, juce::jlimit (0.0f, 1.0f, (x - r.getX()) / r.getWidth()));
}
float yOfDb (float db) { const auto r = plotRect(); return r.getBottom() - r.getHeight() * juce::jlimit (0.0f, 1.0f, (db + 90.0f) / 84.0f); }

juce::String hzText (float hz) { return hz >= 1000.0f ? juce::String (hz * 0.001f, hz >= 10000.0f ? 1 : 2) + " kHz" : juce::String ((int) std::lround (hz)) + " Hz"; }
}

//==============================================================================
SF1AudioProcessorEditor::SF1AudioProcessorEditor (SF1AudioProcessor& p)
    : LabEditor (p, kW, kH), proc (p),
      knobs { { { "drift", 0, 0, 2, "DRIFT", 0, 1, {}, {} },       { "morph", 0, 1, 2, "MORPH", 0, 1, {}, {} },
                { "smear", 1, 0, 3, "SMEAR", 0, 1, {}, {} },       { "blur", 1, 1, 3, "BLUR", 0, 1, {}, {} },          { "feedback", 1, 2, 3, "FEEDBACK", 0, 1, {}, {} },
                { "shift", 2, 0, 4, "SHIFT", 1, 1, {}, {} },       { "spread", 2, 1, 4, "SPREAD", 0, 1, {}, {} },      { "ring", 2, 2, 4, "RING", 0, 1, {}, {} },      { "ringFreq", 2, 3, 4, "RING FREQ", 3, 1, {}, {} },
                { "win", 3, 0, 4, "WINDOW", 0, 1, {}, {} },        { "winCentre", 3, 1, 4, "CENTRE", 4, 1, {}, {} },   { "winWidth", 3, 2, 4, "WIDTH", 5, 1, {}, {} }, { "scan", 3, 3, 4, "SCAN", 6, 1, {}, {} },
                { "mix", 4, 0, 2, "MIX", 0, 1, {}, {} },           { "level", 4, 1, 2, "LEVEL", 2, 1, {}, {} },
                { "hInt1", 7, 0, 3, "INTERVAL", 10, 2, {}, {} },   { "hDet1", 7, 1, 3, "DETUNE", 11, 2, {}, {} },     { "hLvl1", 7, 2, 3, "LEVEL", 0, 2, {}, {} },
                { "hInt2", 8, 0, 3, "INTERVAL", 10, 2, {}, {} },   { "hDet2", 8, 1, 3, "DETUNE", 11, 2, {}, {} },     { "hLvl2", 8, 2, 3, "LEVEL", 0, 2, {}, {} },
                { "hInt3", 9, 0, 3, "INTERVAL", 10, 2, {}, {} },   { "hDet3", 9, 1, 3, "DETUNE", 11, 2, {}, {} },     { "hLvl3", 9, 2, 3, "LEVEL", 0, 2, {}, {} },
                { "hInt4", 10, 0, 3, "INTERVAL", 10, 2, {}, {} },  { "hDet4", 10, 1, 3, "DETUNE", 11, 2, {}, {} },    { "hLvl4", 10, 2, 3, "LEVEL", 0, 2, {}, {} },
                { "hRoot", 11, 0, 2, "KEY", 12, 2, {}, {} },       { "hScale", 11, 1, 2, "SCALE", 13, 2, {}, {} },
                { "hSpread", 13, 0, 3, "SPREAD", 0, 2, {}, {} },   { "mix", 13, 1, 3, "MIX", 0, 2, {}, {} },          { "level", 13, 2, 3, "LEVEL", 2, 2, {}, {} } } }
{
    for (auto& k : knobs)
    {
        attachKnob (k.slider, k.att, k.id);
    }
    layoutKnobs();
    startTimerHz (30);
}

SF1AudioProcessorEditor::~SF1AudioProcessorEditor() = default;

void SF1AudioProcessorEditor::timerCallback()
{
    meter.update (proc.outMeter);
    const auto region = [this] (juce::Rectangle<float> r) { return r.expanded (4.0f).transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer(); };
    const int mode = modeNow();
    if (mode != shownMode) { layoutKnobs(); repaint(); }                                                       // a preset, undo or automation changed the mode
    if (mode == 0)
    {
        repaint (region (panelRect (6)));                                                                      // the spectrum
        repaint (region (panelRect (5)));                                                                      // the state cards
        repaint (region (panelRect (4).withTrimmedLeft (panelRect (4).getWidth() - 150.0f)));                   // the output meters
        repaint (region (panelRect (0).withHeight (96.0f)));                                                   // the frozen band lamps
    }
    else
    {
        const auto& eng = proc.engine.harmony();
        for (int v = 0; v < pt1::kNumVoices; ++v)                                                              // the played and the heard notes: repaint on a change
            if (const float n = eng.voiceNote[v].load (std::memory_order_relaxed); n != shownNote[v]) { shownNote[v] = n; repaint (region (harmonyRect (7 + v))); repaint (region (harmonyRect (14))); }
        if (const float hd = eng.detected.load (std::memory_order_relaxed); hd != shownHeard) { shownHeard = hd; repaint (region (harmonyRect (11))); repaint (region (harmonyRect (14))); }
        repaint (region (harmonyRect (13).withTrimmedLeft (harmonyRect (13).getWidth() - 100.0f)));            // the output meters
    }
    if (midiUI != nullptr && midiUI->needsRepaint()) { midiUI->tick(); repaint(); }
    if (capturedAt != 0 && juce::Time::currentTimeMillis() - capturedAt > 400) { capturedAt = 0; repaint (region (captureRect())); }
    tickPresetBar();
}


bool SF1AudioProcessorEditor::knobShown (int i) const { return (knobs[(size_t) i].modes & (1 << modeNow())) != 0; }

void SF1AudioProcessorEditor::layoutKnobs()
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
void SF1AudioProcessorEditor::resized() { layoutKnobs(); }

//==============================================================================
// SPECTRUM: the input (grey), the output (body colour), the MORPH snapshot (ice, thin), the four frozen bands (ice washes with their names) and the moving
// window (a bell in log frequency, one per channel when SPREAD pulls them apart). Click or drag in the plot to move the window's centre.
void SF1AudioProcessorEditor::drawSpectrum (juce::Graphics& g)
{
    const auto r = plotRect();
    g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (r.expanded (8.0f, 8.0f), 10.0f);
    const bool on = isOn ("power");

    for (int b = 0; b < sf1::kBands; ++b)                                                                      // the bands: edges, names, frozen wash
    {
        const float lo = b == 0 ? sf1::kDisplayLoHz : sf1::kBandEdgesHz[b - 1], hi = b == sf1::kBands - 1 ? sf1::kDisplayHiHz : sf1::kBandEdgesHz[b];
        const float xa = xOfHz (lo), xb = xOfHz (hi), w = on ? proc.engine.bandFrozen (b) : 0.0f;
        if (w > 0.0f) { g.setColour (kIce.withAlpha (0.08f + 0.17f * w)); g.fillRect (xa, r.getY(), xb - xa, r.getHeight()); }
        if (b > 0) { g.setColour (kPal.ink.withAlpha (0.18f)); g.drawLine (xa, r.getY(), xa, r.getBottom(), 1.0f); }
        drawText (g, sf1::bandName (b), (xa + xb) * 0.5f, r.getY() - 13.0f, 12.0f, w > 0.5f ? kIce : kPal.ink.withAlpha (0.55f), true, 0.1f);
        if (w > 0.5f) drawText (g, "FROZEN", (xa + xb) * 0.5f, r.getY() + 12.0f, 11.0f, kIce, true, 0.12f);
    }
    for (const float db : { -12.0f, -36.0f, -60.0f, -84.0f })
    {
        g.setColour (kPal.ink.withAlpha (0.08f)); g.drawLine (r.getX(), yOfDb (db), r.getRight(), yOfDb (db), 1.0f);
        drawText (g, juce::String ((int) db), r.getX() - 22.0f, yOfDb (db), 12.0f, kPal.ink.withAlpha (0.6f), false, 0.0f);
    }
    for (const float hz : { 100.0f, 1000.0f, 10000.0f })
    {
        g.setColour (kPal.ink.withAlpha (0.08f)); g.drawLine (xOfHz (hz), r.getY(), xOfHz (hz), r.getBottom(), 1.0f);
        drawText (g, hz >= 1000.0f ? juce::String ((int) (hz / 1000.0f)) + "k" : juce::String ((int) hz), xOfHz (hz), r.getBottom() + 14.0f, 12.0f, kPal.ink.withAlpha (0.6f), false, 0.0f);
    }

    const auto curve = [&] (auto db, bool close)
    {
        juce::Path p;
        for (int d = 0; d < sf1::kDisplayBands; ++d)
        {
            const float x = r.getX() + r.getWidth() * ((float) d + 0.5f) / (float) sf1::kDisplayBands, y = yOfDb (db (d));
            if (d == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        if (close) { p.lineTo (r.getRight() - r.getWidth() * 0.5f / (float) sf1::kDisplayBands, r.getBottom()); p.lineTo (r.getX() + r.getWidth() * 0.5f / (float) sf1::kDisplayBands, r.getBottom()); p.closeSubPath(); }
        return p;
    };
    const auto inDb = [this] (int d) { return proc.engine.displayIn (d); };
    const auto outDb = [this] (int d) { return proc.engine.displayOut (d); };
    const auto memDb = [this] (int d) { return proc.engine.displayMemory (d); };

    {   // clip the curves to the plot
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (r.toNearestInt());
        g.setColour (kPal.ink.withAlpha (0.14f)); g.fillPath (curve (inDb, true));
        g.setColour (kPal.ink.withAlpha (0.45f)); g.strokePath (curve (inDb, false), juce::PathStrokeType (1.4f));
        if (on)
        {
            g.setColour (kPal.body.withAlpha (0.25f)); g.fillPath (curve (outDb, true));
            g.setColour (kPal.body); g.strokePath (curve (outDb, false), juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        if (proc.engine.hasMemory())
        {
            juce::Path dashed; const auto src = curve (memDb, false);
            const float dash[] = { 5.0f, 4.0f };
            juce::PathStrokeType (1.4f).createDashedStroke (dashed, src, dash, 2);
            g.setColour (kIce.withAlpha (0.9f)); g.fillPath (dashed);
        }
    }

    const float amount = value ("win") * 0.01f;                                                                // the window (one bell per channel)
    if (amount > 0.0f && on)
    {
        const float sigma = std::max (0.05f, value ("winWidth")) / 2.355f;
        for (int c = 0; c < 2; ++c)
        {
            if (c == 1 && value ("spread") < 1.0f && value ("scan") <= 0.0f) break;
            const float centre = proc.engine.windowCentreNow (c) > 0.0f ? proc.engine.windowCentreNow (c) : value ("winCentre");
            juce::Path bell;
            for (int i = 0; i <= 120; ++i)
            {
                const float f = sf1::displayBandHz ((float) i / 120.0f), d = (std::log2 (f) - std::log2 (centre)) / sigma, wv = std::exp (-0.5f * d * d);
                const float x = xOfHz (f), y = r.getBottom() - r.getHeight() * (0.06f + 0.8f * amount * wv);
                if (i == 0) bell.startNewSubPath (x, y); else bell.lineTo (x, y);
            }
            g.setColour (juce::Colours::white.withAlpha (c == 0 ? 0.85f : 0.5f));
            g.strokePath (bell, juce::PathStrokeType (c == 0 ? 1.6f : 1.2f));
            g.drawLine (xOfHz (centre), r.getBottom() - 6.0f, xOfHz (centre), r.getBottom(), 2.0f);
        }
    }

    drawText (g, "IN", r.getX(), r.getY() - 34.0f, 12.0f, kPal.ink.withAlpha (0.6f), true, 0.12f, juce::Justification::centredLeft);
    drawText (g, "OUT", r.getX() + 36.0f, r.getY() - 34.0f, 12.0f, kPal.body, true, 0.12f, juce::Justification::centredLeft);
    drawText (g, "MEMORY", r.getX() + 84.0f, r.getY() - 34.0f, 12.0f, proc.engine.hasMemory() ? kIce : kPal.ink.withAlpha (0.3f), true, 0.12f, juce::Justification::centredLeft);
    drawText (g, "WINDOW", r.getX() + 160.0f, r.getY() - 34.0f, 12.0f, amount > 0.0f ? juce::Colours::white.withAlpha (0.85f) : kPal.ink.withAlpha (0.3f), true, 0.12f, juce::Justification::centredLeft);
    drawText (g, "CLICK OR DRAG IN THE PLOT TO MOVE THE WINDOW", r.getRight(), r.getY() - 34.0f, 11.0f, kPal.ink.withAlpha (0.4f), true, 0.1f, juce::Justification::centredRight);
}

//==============================================================================
// KEYBOARD (HARMONY): four octaves; the notes of the chosen key and scale are tinted, the note heard now has a ring above the keys and each voice a numbered marker
// on the key it plays (a note outside the four octaves is shown an octave or two nearer).
void SF1AudioProcessorEditor::drawKeyboard (juce::Graphics& g)
{
    const auto r = keyboardRect();
    constexpr int first = 36, octaves = 4, whites = 7 * octaves;
    const float kw = r.getWidth() / (float) whites;
    const int root = choice ("hRoot"), scale = choice ("hScale");
    const bool intel = isOn ("hIntel");
    static const int whiteSemi[7] = { 0, 2, 4, 5, 7, 9, 11 };
    const auto keyX = [&] (int midi) -> juce::Rectangle<float>                                        // the key of a midi note
    {
        const int m = midi - first, oct = m / 12, pc = m % 12;
        int wi = -1; bool black = false;
        for (int i = 0; i < 7; ++i) if (whiteSemi[i] == pc) wi = i;
        if (wi < 0) { black = true; for (int i = 0; i < 7; ++i) if (whiteSemi[i] == pc - 1) wi = i; }
        const float x = r.getX() + kw * (float) (oct * 7 + wi);
        return black ? juce::Rectangle<float> (x + kw * 0.68f, r.getY(), kw * 0.64f, r.getHeight() * 0.6f) : juce::Rectangle<float> (x, r.getY(), kw, r.getHeight());
    };
    const auto fold = [&] (float m) { int n = (int) std::lround (m); while (n < first) n += 12; while (n >= first + 12 * octaves) n -= 12; return n; };
    for (int pass = 0; pass < 2; ++pass)                                                              // white keys first, then the black ones over them
        for (int m = first; m < first + 12 * octaves; ++m)
        {
            const int pc = (m - first) % 12;
            const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
            if (black != (pass == 1)) continue;
            const auto k = keyX (m);
            const bool inKey = pt1::inScale ((((m - root) % 12) + 12) % 12, scale);
            g.setColour (black ? juce::Colour (20, 21, 24) : juce::Colour (200, 202, 206).withAlpha (0.9f)); g.fillRect (k);
            if (inKey && intel) { g.setColour (kPal.body.withAlpha (black ? 0.55f : 0.38f)); g.fillRect (k); }
            g.setColour (juce::Colours::black.withAlpha (0.6f)); g.drawRect (k, 1.0f);
            if (pc == 0) drawText (g, juce::String ("C") + juce::String (m / 12 - 1), k.getCentreX(), r.getBottom() - 10.0f, 11.0f, juce::Colour (40, 40, 44), true, 0.04f);
        }
    const auto& eng = proc.engine.harmony();
    const float heard = eng.detected.load (std::memory_order_relaxed);
    if (heard >= 0.0f) { const auto k = keyX (fold (heard)); g.setColour (kIce); g.drawEllipse (juce::Rectangle<float> (20.0f, 20.0f).withCentre ({ k.getCentreX(), r.getY() - 14.0f }), 2.0f); drawText (g, "HEARD", k.getCentreX(), r.getY() - 32.0f, 11.0f, kIce, true, 0.1f); }
    for (int v = 0; v < pt1::kNumVoices; ++v)
    {
        const float n = eng.voiceNote[v].load (std::memory_order_relaxed);
        if (n < 0.0f) continue;
        const auto k = keyX (fold (n)); const float cy = k.getY() + k.getHeight() * (k.getHeight() < r.getHeight() ? 0.40f : 0.78f);
        g.setColour (kPal.body); g.fillEllipse (juce::Rectangle<float> (20.0f, 20.0f).withCentre ({ k.getCentreX(), cy }));
        drawText (g, juce::String (v + 1), k.getCentreX(), cy, 12.0f, kPal.dark, true, 0.0f);
    }
}

juce::String SF1AudioProcessorEditor::harmonyText (int kind, double v) const
{
    const float f = (float) v * 0.01f;
    switch (kind)
    {
        case 10:
        {
            static const char* names[13] = { "UNISON", "m2", "2nd", "m3", "3rd", "4th", "b5", "5th", "m6", "6th", "b7", "7th", "OCTAVE" };
            const int s = pt1::intervalSemis (f);
            return std::abs (s) <= 12 ? (s == 0 ? juce::String (names[0]) : (s > 0 ? "+" : "-") + juce::String (names[std::abs (s)])) : juce::String::formatted ("%+d st", s);
        }
        case 11: return juce::String::formatted ("%+d ct", (int) std::lround (pt1::detuneCents (f)));
        case 12: return pt1::noteName (choice ("hRoot"));
        default: return pt1::scaleName (choice ("hScale"));
    }
}

//==============================================================================
void SF1AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    if (midiUI != nullptr) midiUI->paintMarks (g, kPal.body);
}

void SF1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    ui::paintBody (g, kPal, kW, kH, grain);
    ui::paintHeader (g, kPal, "SF-1", "SPECTRAL FOLD", "FROZEN HARMONICS", isOn ("power"));
    ui::paintPresetBar (g, kPal, proc.presets, proc.undoManager, kBarY, barState, false, 6.0f);

    const int mode = modeNow();
    ui::paintTabs (g, kPal, modeTabsRect(), kModeTabs, mode, 13.0f);
    if (mode == 1)
    {
        for (int v = 0; v < pt1::kNumVoices; ++v) ui::paintPanel (g, kPal, harmonyRect (7 + v), "VOICE " + juce::String (v + 1));
        ui::paintPanel (g, kPal, harmonyRect (11), "KEY");
        ui::paintPanel (g, kPal, harmonyRect (12), "CHORDS");
        ui::paintPanel (g, kPal, harmonyRect (13), "OUTPUT");
        ui::paintPanel (g, kPal, harmonyRect (14), "KEYBOARD");
        drawKeyboard (g);
        const auto& eng = proc.engine.harmony();
        const auto midiName = [] (float m) { return m < 0.0f ? juce::String ("--") : juce::String (pt1::noteName ((int) std::lround (m))) + juce::String ((int) std::lround (m) / 12 - 1); };
        for (int v = 0; v < pt1::kNumVoices; ++v)                                                              // the note each voice plays now
        {
            const float n = eng.voiceNote[v].load (std::memory_order_relaxed);
            drawText (g, midiName (n), harmonyRect (7 + v).getCentreX(), kRow1Y + 50.0f, 30.0f, n < 0.0f ? kPal.ink.withAlpha (0.45f) : kPal.body, true, 0.06f);
        }
        ui::paintFlatButton (g, kPal, intelRect(), "INTELLIGENT", isOn ("hIntel"), 13.0f, 1.0f, 10.0f, 0.06f);
        drawText (g, "HEARD: " + midiName (eng.detected.load (std::memory_order_relaxed)), intelRect().getCentreX(), kRow2Y + 154.0f, 14.0f, kPal.ink.withAlpha (0.85f), true, 0.08f);
        for (int c = 0; c < pt1::kNumChords; ++c)
        {
            bool on = true;                                                                                    // a chord button is "on" while the four interval knobs still match its chord
            for (int v = 0; v < pt1::kNumVoices && on; ++v)
                on = std::abs (proc.apvts.getParameter ("hInt" + juce::String (v + 1))->getValue() - pt1::intervalValue (pt1::chordSemis (c, v))) < 0.0075f;
            ui::paintFlatButton (g, kPal, chordRect (c), pt1::chordName (c), on, 13.0f, 1.0f, 10.0f, 0.06f);
        }
        drawText (g, "SETS THE FOUR INTERVALS", harmonyRect (12).getCentreX(), kRow2Y + 160.0f, 14.0f, kPal.ink.withAlpha (0.75f), true, 0.1f);
    }
    else
    {
    ui::paintPanel (g, kPal, panelRect (0), "HOLD");
    ui::paintPanel (g, kPal, panelRect (1), "TEXTURE");
    ui::paintPanel (g, kPal, panelRect (2), "SHIFT");
    ui::paintPanel (g, kPal, panelRect (3), "WINDOW");
    ui::paintPanel (g, kPal, panelRect (4), "OUTPUT");
    ui::paintPanel (g, kPal, panelRect (5), "STATE");
    ui::paintPanel (g, kPal, panelRect (6), "SPECTRUM");
    drawSpectrum (g);

    const bool on = isOn ("power"), all = isOn ("freeze");
    const auto iceButton = [&] (juce::Rectangle<float> b, const juce::String& label, bool lit, float size, float alpha = 1.0f)
    {
        if (lit) { g.setColour (kIce.withMultipliedAlpha (alpha)); g.fillRoundedRectangle (b, 9.0f); }
        g.setColour ((lit ? kIce : kPal.ink.withAlpha (0.4f)).withMultipliedAlpha (alpha)); g.drawRoundedRectangle (b.reduced (0.5f), 9.0f, 1.3f);
        drawText (g, label, b.getCentreX(), b.getCentreY(), size, lit ? kPal.dark : kPal.ink.withAlpha (0.9f * alpha), true, 0.1f);
    };
    iceButton (freezeRect(), "FREEZE", all, 15.0f);
    ui::paintFlatButton (g, kPal, captureRect(), "CAPTURE", capturedAt != 0, 13.0f);
    for (int b = 0; b < sf1::kBands; ++b) iceButton (bandRect (b), kBandShort[b], isOn (kBandIds[b]) || all, 11.0f, all && ! isOn (kBandIds[b]) ? 0.6f : 1.0f);

    }

    for (int i = 0; i < (int) knobs.size(); ++i)                                                               // the knobs with their value text
    {
        if (! knobShown (i)) continue;
        auto& k = knobs[(size_t) i];
        const double v = k.slider.getValue();
        juce::String txt, lo = "0", hi = "100";
        switch (k.kind)
        {
            case 1: txt = (v > 0.5 ? "+" : "") + juce::String ((int) std::lround (v)) + " Hz"; lo = "-1k"; hi = "+1k"; break;
            case 2: txt = juce::String::formatted ("%+d dB", (int) std::lround (v)); lo = "-24"; hi = "+12"; break;
            case 3: txt = hzText ((float) v); lo = "20"; hi = "2k"; break;
            case 4: txt = hzText ((float) v); lo = "40"; hi = "12k"; break;
            case 5: txt = juce::String (v, 2) + " oct"; lo = ".15"; hi = "3"; break;
            case 6: txt = v < 0.005 ? juce::String ("STILL") : juce::String (v, 2) + " Hz"; lo = "0"; hi = "4"; break;
            case 10: txt = harmonyText (10, v); lo = "-24"; hi = "+24"; break;
            case 11: txt = harmonyText (11, v); lo = "-50"; hi = "+50"; break;
            case 12: txt = harmonyText (12, v); lo = "C"; hi = "B"; break;
            case 13: txt = harmonyText (13, v); lo = ""; hi = ""; break;
            default: txt = juce::String ((int) std::lround (v)) + " %"; break;
        }
        const auto c = knobCentre (k.panel, k.index, k.count);
        const float kr = k.panel >= 7 ? 23.0f : ui::kKnobR;                                                    // HARMONY: three knobs in a 282 wide voice panel, the PT-1 radius
        ui::paintKnob (g, kPal, c, k.name, txt, lo, hi, nameY (k.panel), valueY (k.panel), 1.0f, kr);
        ui::paintValueArc (g, c, kr, (float) k.slider.valueToProportionOfLength (k.slider.getValue()), kPal.body);
    }
    meter.paint (g, meterOrigin (mode == 1).x, meterOrigin (mode == 1).y, isOn ("power"), kPal.body, kPal.ink, 110.0f, 26.0f, 13.0f);

    if (mode == 0)
    {   // STATE: value cards
        const auto r = panelRect (5);
        const double fs = proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 48000.0;
        int frozen = 0; for (int b = 0; b < sf1::kBands; ++b) if (proc.engine.bandFrozen (b) > 0.5f) ++frozen;
        const float winHz = proc.engine.windowCentreNow (0);
        const juce::String cards[4][2] = { { "LATENCY", juce::String (1000.0 * proc.engine.latency() / fs, 1) + " MS" },
                                           { "WINDOW", value ("win") > 0.0f && winHz > 0.0f ? hzText (winHz) : juce::String ("--") },
                                           { "FROZEN", juce::String (frozen) + " / 4" },
                                           { "MEMORY", proc.engine.hasMemory() ? juce::String ("SET") : juce::String ("EMPTY") } };
        for (int i = 0; i < 4; ++i)
        {
            const juce::Rectangle<float> c (r.getX() + 14.0f + 102.0f * (float) (i % 2), r.getY() + 34.0f + 86.0f * (float) (i / 2), 100.0f, 64.0f);
            const bool hot = (i == 2 && frozen > 0) || (i == 3 && proc.engine.hasMemory());
            ui::paintValueCard (g, kPal, c.withTrimmedRight (4.0f), cards[i][0], cards[i][1], hot ? kIce : kPal.body, 15.0f);
        }
    }
    ui::paintFooter (g, kPal, kRow3Y + kRow3H + 35.0f);
}

void SF1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / scale();
    proc.undoManager.beginNewTransaction();
    if (ui::powerRect().contains (p)) { toggle ("power"); return; }
    if (ui::presetBarMouseDown (p, *this, proc.presets, proc.undoManager, kBarY, scale(), barState, [this] { repaint(); }, false)) { layoutKnobs(); return; }
    {
        const auto tabs = ui::tabRects (modeTabsRect(), kModeTabs, 13.0f);
        for (int i = 0; i < (int) tabs.size(); ++i)
            if (tabs[(size_t) i].expanded (3.0f).contains (p))
            {
                auto* par = proc.apvts.getParameter ("mode");
                par->beginChangeGesture(); par->setValueNotifyingHost (par->convertTo0to1 ((float) i)); par->endChangeGesture();
                layoutKnobs(); repaint(); return;
            }
    }
    if (modeNow() == 1)
    {
        if (intelRect().expanded (3.0f).contains (p)) { toggle ("hIntel"); repaint(); return; }
        for (int c = 0; c < pt1::kNumChords; ++c)
            if (chordRect (c).expanded (3.0f).contains (p))
            {
                for (int v = 0; v < pt1::kNumVoices; ++v)
                {
                    auto* par = proc.apvts.getParameter ("hInt" + juce::String (v + 1));
                    par->beginChangeGesture(); par->setValueNotifyingHost (pt1::intervalValue (pt1::chordSemis (c, v))); par->endChangeGesture();
                }
                repaint(); return;
            }
        return;
    }
    if (freezeRect().expanded (3.0f).contains (p)) { toggle ("freeze"); repaint(); return; }
    if (captureRect().expanded (3.0f).contains (p)) { proc.engine.requestCapture(); capturedAt = juce::Time::currentTimeMillis(); repaint(); return; }
    for (int b = 0; b < sf1::kBands; ++b)
        if (bandRect (b).expanded (3.0f).contains (p)) { toggle (kBandIds[b]); repaint(); return; }
    if (plotRect().expanded (8.0f).contains (p))                                                                // the window centre follows the mouse
    {
        auto* par = proc.apvts.getParameter ("winCentre");
        par->beginChangeGesture(); par->setValueNotifyingHost (par->convertTo0to1 (juce::jlimit (40.0f, 12000.0f, hzOfX (p.x)))); par->endChangeGesture();
        repaint();
    }
}
