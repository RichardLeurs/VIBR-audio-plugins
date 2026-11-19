// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "ui/Presets.h"
#include "ui/OutputMeter.h"

namespace
{
namespace ui = vibr::ui::vintage;          // the default look (src/common/ui/VintageUI.h); the old grey look is vibr::ui (VibrUI.h)
using ui::drawText;

constexpr float kW = ui::kW, kRow1Y = 114.0f, kRow1H = 204.0f, kRow2Y = 336.0f, kRow2H = 254.0f, kRow3Y = 608.0f, kRow3H = 234.0f, kH = kRow3Y + kRow3H + 30.0f;
const ui::Palette kPal { juce::Colour (150, 59, 208) };                                              // the Experimental colour #963BD0: value rings, full steps and the plugin code
const juce::Colour kMemory = kPal.accent.interpolatedWith (juce::Colours::white, 0.5f);               // the MORPH snapshot and the frozen bands on the phosphor screen

// FOLD panels: 0 HOLD, 1 TEXTURE, 2 SHIFT (row 1), 3 WINDOW, 4 STATE, 5 OUTPUT (row 2), 6 SPECTRUM (row 3); the knob table keeps its old panel numbers (4 = OUTPUT, 5 = STATE)
constexpr float kPanelX[7] = { 28.0f, 310.0f, 658.0f, 28.0f, 480.0f, 749.0f, 28.0f };
constexpr float kPanelW[7] = { 264.0f, 330.0f, 434.0f, 434.0f, 251.0f, 343.0f, 1064.0f };
juce::Rectangle<float> panelRect (int i)
{
    const float y = i < 3 ? kRow1Y : i < 6 ? kRow2Y : kRow3Y, h = i < 3 ? kRow1H : i < 6 ? kRow2H : kRow3H;
    return { kPanelX[i], y, kPanelW[i], h };
}
// the table's panel numbers: 0 hold, 1 texture, 2 shift, 3 window, 4 output; HARMONY: 7 .. 10 VOICE 1 .. 4, 11 KEY, 13 OUTPUT
int foldPanel (int tablePanel) { return tablePanel == 4 ? 5 : tablePanel; }
// HARMONY: panels 7 .. 10 VOICE 1 .. 4 (row 1), 11 KEY, 12 CHORDS, 13 OUTPUT (row 2), 14 KEYBOARD (row 3)
juce::Rectangle<float> harmonyRect (int i)
{
    if (i < 11) return { 28.0f + 270.5f * (float) (i - 7), kRow1Y, 252.5f, kRow1H };
    if (i == 14) return { 28.0f, kRow3Y, 1064.0f, kRow3H };
    static const float x[3] = { 28.0f, 431.0f, 644.0f }, w[3] = { 385.0f, 195.0f, 448.0f };
    return { x[i - 11], kRow2Y, w[i - 11], kRow2H };
}
juce::Rectangle<float> intelRect() { const auto r = harmonyRect (11); return { r.getRight() - 20.0f - 130.0f, r.getY() + 90.0f, 130.0f, 30.0f }; }
juce::Rectangle<float> heardRect() { const auto r = harmonyRect (11); return { r.getRight() - 20.0f - 130.0f, r.getY() + 134.0f, 130.0f, 30.0f }; }
juce::Rectangle<float> chordRect (int c) { const auto r = harmonyRect (12); return { r.getX() + 20.0f, r.getY() + 24.0f + 36.0f * (float) c, r.getWidth() - 40.0f, 30.0f }; }
juce::Rectangle<float> voiceScreenRect (int v) { const auto r = harmonyRect (7 + v); return { r.getX() + 20.0f, r.getY() + 24.0f, r.getWidth() - 40.0f, 30.0f }; }
juce::Rectangle<float> keyboardRect() { const auto r = harmonyRect (14); return { r.getX() + 20.0f, r.getY() + 62.0f, r.getWidth() - 40.0f, 152.0f }; }

const std::vector<juce::String> kModeTabs { "FOLD", "HARMONY" }, kHoldButtons { "FREEZE", "CAPTURE" };
constexpr float kTabsTop = -15.0f, kTabsFont = 16.0f;
const float kModeTabsLeft = std::max (ui::titleEnd ("SPECTRUM"), ui::titleEnd ("KEYBOARD")), kHoldLeft = ui::titleEnd ("HOLD");    // the same place in both modes
juce::Rectangle<float> bottomPanel (bool harmony) { return harmony ? harmonyRect (14) : panelRect (6); }
std::vector<juce::Rectangle<float>> modeTabRects (bool harmony) { return ui::tabRects (bottomPanel (harmony), kModeTabs, kTabsTop, kTabsFont, kModeTabsLeft); }
ui::TabLayout holdLayout() { return ui::layoutTabs (panelRect (0), kHoldButtons, kTabsTop, kTabsFont, kHoldLeft); }      // FREEZE and CAPTURE sit on the edge of HOLD like tabs

constexpr float kSmallR = 20.0f;                                                                   // the three knobs of a VOICE panel
float knobRadius (int panel) { return panel >= 7 && panel <= 10 ? kSmallR : ui::kKnobR; }
juce::Point<float> knobCentre (int panel, int k, int n)
{
    if (panel >= 7 && panel <= 10) { const auto r = harmonyRect (panel); return { r.getCentreX() + 76.0f * (float) (k - 1), r.getY() + 100.0f }; }       // a voice: INTERVAL, DETUNE, LEVEL
    if (panel == 11) { const auto r = harmonyRect (11); return { r.getX() + 66.0f + 106.0f * (float) k, r.getY() + 112.0f }; }                           // KEY: key, scale
    if (panel == 13) { const auto r = harmonyRect (13); return { r.getX() + 66.0f + 104.0f * (float) k, r.getY() + 112.0f }; }                           // OUTPUT: spread, mix, level
    const auto r = panelRect (foldPanel (panel));
    if (panel == 4) return { r.getX() + 66.0f + 106.0f * (float) k, r.getY() + 112.0f };                              // MIX and LEVEL left of the meters
    const float pitch = panel == 0 || panel == 1 ? 108.0f : 104.0f;
    return { r.getCentreX() + pitch * ((float) k - 0.5f * (float) (n - 1)), r.getY() + (panel == 3 ? 112.0f : 86.0f) };
}
juce::Point<float> meterOrigin() { const auto r = panelRect (5); return { r.getRight() - 20.0f - ui::kMeterW, r.getY() + (r.getHeight() - ui::kMeterH) * 0.5f }; }       // the same place in both modes
juce::Rectangle<float> screenRect() { const auto r = panelRect (6); return { r.getX() + 20.0f, r.getY() + 24.0f, r.getWidth() - 40.0f, 146.0f }; }
juce::Rectangle<float> plotRect() { const auto s = screenRect(); return { s.getX() + 48.0f, s.getY() + 36.0f, s.getWidth() - 96.0f, 82.0f }; }
const char* const kBandIds[sf1::kBands] = { "fzLow", "fzLowMid", "fzHighMid", "fzHigh" };

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
float bandLoHz (int b) { return b == 0 ? sf1::kDisplayLoHz : sf1::kBandEdgesHz[b - 1]; }
float bandHiHz (int b) { return b == sf1::kBands - 1 ? sf1::kDisplayHiHz : sf1::kBandEdgesHz[b]; }
juce::Rectangle<float> bandRect (int b)                                                                        // the switch of a band, under its column of the plot
{
    const float xa = xOfHz (bandLoHz (b)), xb = xOfHz (bandHiHz (b));
    return { xa + 3.0f, screenRect().getBottom() + 10.0f, xb - xa - 6.0f, 30.0f };
}

juce::String hzText (float hz) { return hz >= 1000.0f ? juce::String (hz * 0.001f, hz >= 10000.0f ? 1 : 2) + " kHz" : juce::String ((int) std::lround (hz)) + " Hz"; }

// a small knob (VOICE panels): the value ring, the legends, the name (16) and the value
void paintSmallKnob (juce::Graphics& g, juce::Point<float> c, const juce::String& name, const juce::String& value, const juce::String& lo, const juce::String& hi, float prop)
{
    ui::paintValueArc (g, c, kSmallR, prop, kPal.accent, kPal.track);
    const float ly = c.y + ui::ringRadius (kSmallR) * 0.707f + 15.0f;
    drawText (g, lo, c.x - kSmallR * 1.4f, ly, 13.0f, kPal.inkSoft, false, 0.0f, juce::Justification::centredLeft, ui::Face::Mono);
    drawText (g, hi, c.x + kSmallR * 1.4f, ly, 13.0f, kPal.inkSoft, false, 0.0f, juce::Justification::centredRight, ui::Face::Mono);
    drawText (g, name, c.x, c.y + kSmallR * 1.71f + 20.0f, 16.0f, kPal.ink, true, 0.08f);
    drawText (g, value, c.x, c.y + kSmallR * 1.71f + 38.0f, 14.0f, kPal.accent, true, 0.0f, juce::Justification::centred, ui::Face::Mono);
}
// a small read-out on a phosphor screen: the label dim on the left, the value bright on the right
void paintReadout (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& label, const juce::String& value, bool bright)
{
    ui::paintScreen (g, kPal, r, 4.0f);
    drawText (g, label, r.getX() + 10.0f, r.getCentreY(), 14.0f, kPal.phosphorDim, false, 0.1f, juce::Justification::centredLeft, ui::Face::Mono);
    drawText (g, value, r.getRight() - 10.0f, r.getCentreY(), 17.0f, bright ? kPal.phosphor : kPal.phosphorDim.withAlpha (0.7f), true, 0.0f, juce::Justification::centredRight, ui::Face::Mono);
}
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
    const int mode = modeNow();
    if (mode != shownMode) { layoutKnobs(); repaint(); }                                                       // a preset, undo or automation changed the mode
    const auto meterRect = juce::Rectangle<float> (meterOrigin().x, meterOrigin().y, ui::kMeterW, ui::kMeterH).expanded (2.0f);
    if (mode == 0)
    {
        repaintDesign (panelRect (6));                                                                         // the spectrum and the band switches
        repaintDesign (panelRect (4));                                                                         // the state cards
        repaintDesign (meterRect);
    }
    else
    {
        const auto& eng = proc.engine.harmony();
        for (int v = 0; v < pt1::kNumVoices; ++v)                                                              // the played and the heard notes: repaint on a change
            if (const float n = eng.voiceNote[v].load (std::memory_order_relaxed); n != shownNote[v]) { shownNote[v] = n; repaintDesign (harmonyRect (7 + v)); repaintDesign (harmonyRect (14)); }
        if (const float hd = eng.detected.load (std::memory_order_relaxed); hd != shownHeard) { shownHeard = hd; repaintDesign (harmonyRect (11)); repaintDesign (harmonyRect (14)); }
        repaintDesign (meterRect);
    }
    if (midiUI != nullptr && midiUI->needsRepaint()) { midiUI->tick(); repaint(); }
    if (capturedAt != 0 && juce::Time::currentTimeMillis() - capturedAt > 400) { capturedAt = 0; repaintDesign (panelRect (0).withHeight (40.0f).translated (0.0f, -12.0f)); }
}


bool SF1AudioProcessorEditor::knobShown (int i) const { return (knobs[(size_t) i].modes & (1 << modeNow())) != 0; }

void SF1AudioProcessorEditor::layoutKnobs()
{
    shownMode = modeNow();
    juce::uint64 shown = 0;
    for (int i = 0; i < (int) knobs.size(); ++i)
    {
        auto& k = knobs[(size_t) i];
        placeKnob (k.slider, knobCentre (k.panel, k.index, k.count), knobRadius (k.panel));
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
// SPECTRUM: a phosphor screen with the input (dim), the output (bright), the MORPH snapshot (dashed), the four frozen bands (washes) and the moving window (a bell in log frequency,
// one per channel when SPREAD pulls them apart). Click or drag in the plot to move the window's centre.
void SF1AudioProcessorEditor::drawSpectrum (juce::Graphics& g)
{
    const auto s = screenRect(), r = plotRect();
    ui::paintScope (g, kPal, s, "", "CLICK OR DRAG TO MOVE THE WINDOW");
    const bool on = isOn ("power");
    const float amount = value ("win") * 0.01f;

    float x = s.getX() + 12.0f;                                                                                // the legend, left in the strip above the plot
    const auto legend = [&] (const juce::String& t, juce::Colour c)
    {
        drawText (g, t, x, s.getY() + 16.0f, 15.0f, c, false, 0.08f, juce::Justification::centredLeft, ui::Face::Mono);
        x += ui::textWidth (ui::font (15.0f, false, ui::Face::Mono).withExtraKerningFactor (0.08f), t) + 28.0f;
    };
    legend ("IN", kPal.phosphorDim);
    legend ("OUT", kPal.phosphor);
    legend ("MEMORY", proc.engine.hasMemory() ? kMemory : kPal.phosphorDim.withAlpha (0.5f));
    legend ("WINDOW", amount > 0.0f ? kPal.cream : kPal.phosphorDim.withAlpha (0.5f));

    g.saveState();
    g.reduceClipRegion (s.reduced (3.0f).toNearestInt());
    for (int b = 0; b < sf1::kBands; ++b)                                                                      // the bands: edges and the frozen wash
    {
        const float xa = xOfHz (bandLoHz (b)), xb = xOfHz (bandHiHz (b)), w = on ? proc.engine.bandFrozen (b) : 0.0f;
        if (w > 0.0f) { g.setColour (kMemory.withAlpha (0.10f + 0.20f * w)); g.fillRect (xa, r.getY(), xb - xa, r.getHeight()); }
        if (b > 0) { g.setColour (kPal.phosphorDim.withAlpha (0.45f)); g.drawLine (xa, r.getY(), xa, r.getBottom(), 1.0f); }
    }
    g.restoreState();
    for (const float db : { -12.0f, -36.0f, -60.0f, -84.0f })
    {
        g.setColour (kPal.phosphor.withAlpha (0.10f)); g.drawLine (r.getX(), yOfDb (db), r.getRight(), yOfDb (db), 1.0f);
        drawText (g, juce::String ((int) db), r.getX() - 6.0f, yOfDb (db), 13.0f, kPal.phosphorDim, false, 0.0f, juce::Justification::centredRight, ui::Face::Mono);
    }
    for (const float hz : { 100.0f, 1000.0f, 10000.0f })
    {
        g.setColour (kPal.phosphor.withAlpha (0.10f)); g.drawLine (xOfHz (hz), r.getY(), xOfHz (hz), r.getBottom(), 1.0f);
        drawText (g, hz >= 1000.0f ? juce::String ((int) (hz / 1000.0f)) + "k" : juce::String ((int) hz), xOfHz (hz), r.getBottom() + 14.0f, 13.0f, kPal.phosphorDim, false, 0.0f, juce::Justification::centred, ui::Face::Mono);
    }

    const auto curve = [&] (auto db, bool close)
    {
        juce::Path p;
        for (int d = 0; d < sf1::kDisplayBands; ++d)
        {
            const float px = r.getX() + r.getWidth() * ((float) d + 0.5f) / (float) sf1::kDisplayBands, py = yOfDb (db (d));
            if (d == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
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
        g.setColour (kPal.phosphorDim.withAlpha (0.18f)); g.fillPath (curve (inDb, true));
        g.setColour (kPal.phosphorDim.withAlpha (0.8f)); g.strokePath (curve (inDb, false), juce::PathStrokeType (1.3f));
        if (on)
        {
            g.setColour (kPal.phosphor.withAlpha (0.12f)); g.fillPath (curve (outDb, true));
            g.setColour (kPal.phosphor.withAlpha (0.18f)); g.strokePath (curve (outDb, false), juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));      // glow
            g.setColour (kPal.phosphor); g.strokePath (curve (outDb, false), juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        if (proc.engine.hasMemory())
        {
            juce::Path dashed; const auto src = curve (memDb, false);
            const float dash[] = { 5.0f, 4.0f };
            juce::PathStrokeType (1.4f).createDashedStroke (dashed, src, dash, 2);
            g.setColour (kMemory); g.fillPath (dashed);
        }
    }

    if (amount > 0.0f && on)                                                                                   // the window (one bell per channel)
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
                const float px = xOfHz (f), py = r.getBottom() - r.getHeight() * (0.06f + 0.8f * amount * wv);
                if (i == 0) bell.startNewSubPath (px, py); else bell.lineTo (px, py);
            }
            g.setColour (kPal.cream.withAlpha (c == 0 ? 0.9f : 0.55f));
            g.strokePath (bell, juce::PathStrokeType (c == 0 ? 1.6f : 1.2f));
            g.drawLine (xOfHz (centre), r.getBottom() - 6.0f, xOfHz (centre), r.getBottom(), 2.0f);
        }
    }
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
            g.setColour (black ? kPal.ink : kPal.paperLight); g.fillRect (k);
            if (inKey && intel) { g.setColour (kPal.accent.withAlpha (black ? 0.7f : 0.35f)); g.fillRect (k); }
            g.setColour (kPal.ink); g.drawRect (k, 1.0f);
            if (pc == 0) drawText (g, juce::String ("C") + juce::String (m / 12 - 1), k.getCentreX(), r.getBottom() - 12.0f, 13.0f, kPal.inkSoft, true, 0.0f, juce::Justification::centred, ui::Face::Mono);
        }
    const auto& eng = proc.engine.harmony();
    const float heard = eng.detected.load (std::memory_order_relaxed);
    if (heard >= 0.0f)
    {
        const auto k = keyX (fold (heard));
        g.setColour (kPal.accent); g.drawEllipse (juce::Rectangle<float> (18.0f, 18.0f).withCentre ({ k.getCentreX(), r.getY() - 14.0f }), 2.0f);
        drawText (g, "HEARD", k.getCentreX(), r.getY() - 33.0f, 13.0f, kPal.accent, true, 0.1f, juce::Justification::centred, ui::Face::Mono);
    }
    for (int v = 0; v < pt1::kNumVoices; ++v)
    {
        const float n = eng.voiceNote[v].load (std::memory_order_relaxed);
        if (n < 0.0f) continue;
        const auto k = keyX (fold (n)); const float cy = k.getY() + k.getHeight() * (k.getHeight() < r.getHeight() ? 0.40f : 0.70f);
        g.setColour (kPal.accent); g.fillEllipse (juce::Rectangle<float> (22.0f, 22.0f).withCentre ({ k.getCentreX(), cy }));
        g.setColour (kPal.ink); g.drawEllipse (juce::Rectangle<float> (22.0f, 22.0f).withCentre ({ k.getCentreX(), cy }).reduced (1.0f), 2.0f);
        drawText (g, juce::String (v + 1), k.getCentreX(), cy, 14.0f, kPal.cream, true, 0.0f, juce::Justification::centred, ui::Face::Mono);
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
    if (midiUI != nullptr) midiUI->paintMarks (g, kPal.accent);
}

void SF1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    ui::paintBody (g, kPal, kW, kH);
    ui::paintHeader (g, kPal, "SF-1", "SPECTRAL FOLD", "FROZEN HARMONICS");
    ui::paintPresetBar (g, kPal, proc.presets, proc.undoManager, isOn ("power"));
    ui::paintFooter (g, kPal, kH - 15.0f);

    const int mode = modeNow();
    if (mode == 1)
    {
        for (int v = 0; v < pt1::kNumVoices; ++v) ui::paintPanel (g, kPal, harmonyRect (7 + v), "VOICE " + juce::String (v + 1));
        ui::paintPanel (g, kPal, harmonyRect (11), "KEY");
        ui::paintPanel (g, kPal, harmonyRect (12), "CHORDS");
        ui::paintPanel (g, kPal, harmonyRect (13), "OUTPUT");
        ui::paintPanel (g, kPal, harmonyRect (14), "KEYBOARD");
        ui::paintTabs (g, kPal, harmonyRect (14), kModeTabs, mode, kTabsTop, kTabsFont, kModeTabsLeft);
        drawKeyboard (g);
        const auto& eng = proc.engine.harmony();
        const auto midiName = [] (float m) { return m < 0.0f ? juce::String ("--") : juce::String (pt1::noteName ((int) std::lround (m))) + juce::String ((int) std::lround (m) / 12 - 1); };
        for (int v = 0; v < pt1::kNumVoices; ++v)                                                              // the note each voice plays now
        {
            const float n = eng.voiceNote[v].load (std::memory_order_relaxed);
            paintReadout (g, voiceScreenRect (v), "NOTE", midiName (n), n >= 0.0f);
        }
        ui::paintFlatButton (g, kPal, intelRect(), "INTELLIGENT", isOn ("hIntel"), 17.0f);
        const float heard = eng.detected.load (std::memory_order_relaxed);
        paintReadout (g, heardRect(), "HEARD", midiName (heard), heard >= 0.0f);
        for (int c = 0; c < pt1::kNumChords; ++c)
        {
            bool on = true;                                                                                    // a chord button is "on" while the four interval knobs still match its chord
            for (int v = 0; v < pt1::kNumVoices && on; ++v)
                on = std::abs (proc.apvts.getParameter ("hInt" + juce::String (v + 1))->getValue() - pt1::intervalValue (pt1::chordSemis (c, v))) < 0.0075f;
            ui::paintFlatButton (g, kPal, chordRect (c), pt1::chordName (c), on, 17.0f);
        }
        {
            const auto cr = harmonyRect (12);
            const juce::String hint = "SETS THE 4 INTERVALS";
            drawText (g, hint, cr.getCentreX(), cr.getY() + 218.0f, 14.0f, kPal.inkSoft, false, 0.05f);
        }
    }
    else
    {
        ui::paintPanel (g, kPal, panelRect (0), "HOLD");
        ui::paintPanel (g, kPal, panelRect (1), "TEXTURE");
        ui::paintPanel (g, kPal, panelRect (2), "SHIFT");
        ui::paintPanel (g, kPal, panelRect (3), "WINDOW");
        ui::paintPanel (g, kPal, panelRect (4), "STATE");
        ui::paintPanel (g, kPal, panelRect (5), "OUTPUT");
        ui::paintPanel (g, kPal, panelRect (6), "SPECTRUM");
        ui::paintTabs (g, kPal, panelRect (6), kModeTabs, mode, kTabsTop, kTabsFont, kModeTabsLeft);
        drawSpectrum (g);

        const bool all = isOn ("freeze");
        {                                                                                                      // FREEZE and CAPTURE on the top edge of HOLD (the line is cut round them like round tabs)
            const auto lay = holdLayout();
            const float x0 = lay.rects.front().getX() - 6.0f, x1 = lay.rects.back().getRight() + 6.0f;
            g.setColour (kPal.paper); g.fillRect (x0, panelRect (0).getY() - 1.0f, x1 - x0, 4.0f);
            ui::paintFlatButton (g, kPal, lay.rects[0], "FREEZE", all, lay.size);
            ui::paintFlatButton (g, kPal, lay.rects[1], "CAPTURE", capturedAt != 0, lay.size);
        }
        for (int b = 0; b < sf1::kBands; ++b)
            ui::paintFlatButton (g, kPal, bandRect (b), sf1::bandName (b), isOn (kBandIds[b]) || all, 17.0f, all && ! isOn (kBandIds[b]) ? 0.6f : 1.0f);

        // STATE: value cards on small phosphor screens
        const auto r = panelRect (4);
        const double fs = proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 48000.0;
        int frozen = 0; for (int b = 0; b < sf1::kBands; ++b) if (proc.engine.bandFrozen (b) > 0.5f) ++frozen;
        const float winHz = proc.engine.windowCentreNow (0);
        const juce::String cards[4][2] = { { "LATENCY", juce::String (1000.0 * proc.engine.latency() / fs, 1) + " MS" },
                                           { "WINDOW", value ("win") > 0.0f && winHz > 0.0f ? hzText (winHz) : juce::String ("--") },
                                           { "FROZEN", juce::String (frozen) + " / 4" },
                                           { "MEMORY", proc.engine.hasMemory() ? juce::String ("SET") : juce::String ("EMPTY") } };
        const float cw = (r.getWidth() - 40.0f - 10.0f) * 0.5f, ch = (r.getHeight() - 48.0f - 10.0f) * 0.5f;
        for (int i = 0; i < 4; ++i)
        {
            const juce::Rectangle<float> c (r.getX() + 20.0f + (cw + 10.0f) * (float) (i % 2), r.getY() + 24.0f + (ch + 10.0f) * (float) (i / 2), cw, ch);
            const bool hot = (i == 2 && frozen > 0) || (i == 3 && proc.engine.hasMemory());
            ui::paintScreen (g, kPal, c, 4.0f);
            drawText (g, cards[i][0], c.getCentreX(), c.getY() + 22.0f, 14.0f, kPal.phosphorDim, false, 0.1f, juce::Justification::centred, ui::Face::Mono);
            drawText (g, cards[i][1], c.getCentreX(), c.getBottom() - 38.0f, 20.0f, hot ? kMemory : kPal.phosphor, true, 0.0f, juce::Justification::centred, ui::Face::Mono);
        }
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
        const float prop = (float) k.slider.valueToProportionOfLength (k.slider.getValue());
        if (k.panel >= 7 && k.panel <= 10) paintSmallKnob (g, c, k.name, txt, lo, hi, prop);
        else ui::paintKnob (g, kPal, c, k.name, txt, lo, hi, prop);
    }
    ui::paintMeter (g, kPal, meter, meterOrigin().x, meterOrigin().y, isOn ("power"));
}

void SF1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / scale();
    proc.undoManager.beginNewTransaction();
    if (ui::powerRect().contains (p)) { toggle ("power"); return; }
    if (ui::presetBarMouseDown (p, *this, proc.presets, proc.undoManager, scale(), [this] { repaint(); })) { layoutKnobs(); return; }
    {
        const auto tabs = modeTabRects (modeNow() == 1);
        for (int i = 0; i < (int) tabs.size(); ++i)
            if (tabs[(size_t) i].expanded (3.0f).contains (p)) { setChoice ("mode", i); layoutKnobs(); repaint(); return; }
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
    const auto hold = holdLayout().rects;
    if (hold[0].expanded (3.0f).contains (p)) { toggle ("freeze"); repaint(); return; }
    if (hold[1].expanded (3.0f).contains (p)) { proc.engine.requestCapture(); capturedAt = juce::Time::currentTimeMillis(); repaint(); return; }
    for (int b = 0; b < sf1::kBands; ++b)
        if (bandRect (b).expanded (3.0f).contains (p)) { toggle (kBandIds[b]); repaint(); return; }
    if (plotRect().expanded (8.0f).contains (p))                                                                // the window centre follows the mouse
    {
        auto* par = proc.apvts.getParameter ("winCentre");
        par->beginChangeGesture(); par->setValueNotifyingHost (par->convertTo0to1 (juce::jlimit (40.0f, 12000.0f, hzOfX (p.x)))); par->endChangeGesture();
        repaint();
    }
}
