// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "ui/Presets.h"

namespace
{
namespace ui = vibr::ui::vintage;          // the default look (src/common/ui/VintageUI.h)
using ui::drawText;

constexpr float kW = ui::kW, kH = ui::kH, kRow1Y = 114.0f, kRow1H = 244.0f, kRow2Y = 376.0f, kRow2H = 254.0f;
const ui::Palette kPal { juce::Colour (53, 184, 232) };            // the Delay & Echo colour #35B8E8: value rings, full steps, knob values and the plugin code

// panels: 0 TIME (with the mode tabs), 1 VOICING, 2 MODULATION, 3 OUTPUT, 4 ECHOES (display)
constexpr float kPanelX[5] = { 28.0f, 558.0f, 28.0f, 749.0f, 380.0f }, kPanelW[5] = { 512.0f, 534.0f, 334.0f, 343.0f, 351.0f };
juce::Rectangle<float> panelRect (int i) { const bool row1 = i < 2; return { kPanelX[i], row1 ? kRow1Y : kRow2Y, kPanelW[i], row1 ? kRow1H : kRow2H }; }
juce::Point<float> knobCentre (int panel, int k, int n)
{
    const auto r = panelRect (panel);
    if (panel == 3) return { r.getX() + 66.0f + 106.0f * (float) k, r.getY() + 112.0f };                   // MIX and LEVEL left of the meters
    const float pitch = panel == 0 ? 116.0f : panel == 2 ? 106.0f : n >= 5 ? 104.0f : n == 4 ? 116.0f : n == 3 ? 120.0f : 108.0f;
    return { r.getCentreX() + pitch * ((float) k - 0.5f * (float) (n - 1)), r.getY() + (panel < 2 ? 86.0f : 112.0f) };
}
juce::Point<float> meterOrigin() { const auto r = panelRect (3); return { r.getRight() - 20.0f - ui::kMeterW, r.getY() + (r.getHeight() - ui::kMeterH) * 0.5f }; }
juce::Rectangle<float> echoScreenRect() { return panelRect (4).withTrimmedTop (26.0f).reduced (16.0f, 0.0f).withTrimmedBottom (16.0f); }
juce::Rectangle<float> echoPlotRect() { return ui::scopePlotRect (echoScreenRect()); }                                       // the bars and the time axis
float echoX0() { return echoPlotRect().getX() + 22.0f; }
float echoW() { return echoPlotRect().getWidth() - 44.0f; }

// the flat buttons: row 1 under the knobs (200 below the panel top). TIME: SYNC, note length, HOLD, TAP (four of equal width). VOICING: the types (up to four of equal width)
juce::Rectangle<float> timeButtonRect (int i)
{
    const auto r = panelRect (0); const float w = (r.getWidth() - 40.0f - 18.0f) / 4.0f;
    return { r.getX() + 20.0f + (float) i * (w + 6.0f), r.getY() + 200.0f, w, 30.0f };
}
juce::Rectangle<float> variantRect (int i)                  // the types of the mode; DUAL: PARALLEL, SERIES, PING A, PING B (slots 0 to 3)
{
    const auto r = panelRect (1); const float w = (r.getWidth() - 40.0f - 18.0f) / 4.0f;
    return { r.getX() + 20.0f + (float) i * (w + 6.0f), r.getY() + 200.0f, w, 30.0f };
}
juce::Rectangle<float> voiceRect() { return variantRect (0).withRight (variantRect (3).getRight()); }                 // MODULATED: the voice list
juce::Rectangle<float> pingRect (int i) { return variantRect (2 + i); }
const std::vector<juce::String> kModeTabs { "DIGITAL", "ANALOG", "TAPE", "DUAL", "MODULATED" };
constexpr float kTabsTop = -15.0f, kTabsFont = 16.0f;
const float kTabsLeft = ui::titleEnd ("TIME");
} // namespace

//==============================================================================
DL1AudioProcessorEditor::DL1AudioProcessorEditor (DL1AudioProcessor& p)
    : LabEditor (p, kW, kH), proc (p),
      knobs { { { "time", 0, 0, 4, "TIME", 0, -1, {}, {} },        { "feedback", 0, 1, 4, "FEEDBACK", 1, -1, {}, {} }, { "low", 0, 2, 4, "LOW CUT", 2, -1, {}, {} },
                { "high", 0, 3, 4, "HI CUT", 3, -1, {}, {} },      { "modRate", 2, 0, 3, "MOD RATE", 4, -1, {}, {} },  { "modDepth", 2, 1, 3, "MOD DEPTH", 1, -1, {}, {} },
                { "spread", 2, 2, 3, "PING-PONG", 1, -1, {}, {} },
                { "digFactor", 1, 0, 4, "FACTOR", 1, 0, {}, {} },  { "digDirect", 1, 1, 4, "DIRECT", 1, 0, {}, {} },   { "digEffect", 1, 2, 4, "EFFECT", 1, 0, {}, {} },
                { "digInput", 1, 3, 4, "INPUT", 1, 0, {}, {} },
                { "anaBlend", 1, 0, 3, "BLEND", 1, 1, {}, {} },    { "anaDepth", 1, 1, 3, "DEPTH", 1, 1, {}, {} },     { "anaGrit", 1, 2, 3, "GRIT", 1, 1, {}, {} },
                { "tapeWow", 1, 0, 4, "WOW", 1, 2, {}, {} },       { "tapeSat", 1, 1, 4, "SATURATION", 1, 2, {}, {} }, { "tapeAge", 1, 2, 4, "AGE", 1, 2, {}, {} },
                { "tapeSpacing", 1, 3, 4, "SPACING", 1, 2, {}, {} },
                { "dualTimeB", 1, 0, 5, "TIME B", 6, 3, {}, {} },  { "dualFbB", 1, 1, 5, "FEEDBACK B", 1, 3, {}, {} }, { "dualLevelA", 1, 2, 5, "LEVEL A", 1, 3, {}, {} },
                { "dualLevelB", 1, 3, 5, "LEVEL B", 1, 3, {}, {} }, { "dualHighB", 1, 4, 5, "HI CUT B", 3, 3, {}, {} },
                { "modVariation", 1, 0, 2, "VARIATION", 1, 4, {}, {} }, { "modVolume", 1, 1, 2, "VOLUME", 1, 4, {}, {} },
                { "mix", 3, 0, 2, "MIX", 1, -1, {}, {} },          { "level", 3, 1, 2, "LEVEL", 5, -1, {}, {} } } }
{
    for (auto& k : knobs)
    {
        attachKnob (k.slider, k.att, k.id);
        midiUI.add (k.slider, k.id);
    }
    layoutKnobs();
    startTimerHz (30);
}

DL1AudioProcessorEditor::~DL1AudioProcessorEditor() = default;

void DL1AudioProcessorEditor::timerCallback()
{
    meter.update (proc.outMeter);
    if (const int shown = mode() * 8 + choice ("variant"); shown != shownKey) { shownKey = shown; layoutKnobs(); repaint(); }       // a preset, undo, A / B or automation changed the tab
    repaintDesign (juce::Rectangle<float> (meterOrigin().x, meterOrigin().y, ui::kMeterW, ui::kMeterH).expanded (2.0f));
    if (midiUI.needsRepaint()) { midiUI.tick(); repaint(); }
    repaintDesign (panelRect (4).expanded (4.0f));
}

void DL1AudioProcessorEditor::layoutKnobs()
{
    for (auto& k : knobs)
    {
        placeKnob (k.slider, knobCentre (k.panel, k.index, k.count));
        k.slider.setVisible (k.onlyMode < 0 || k.onlyMode == mode());
    }
}
void DL1AudioProcessorEditor::resized() { layoutKnobs(); }

//==============================================================================
// ECHOES: the input (the bar at 0 follows the level), the first repeat at TIME (drag it), and the repeats after it falling by FEEDBACK, on a time axis of 2.4 s, on a phosphor
// screen. SPREAD 100 % alternates the repeats between the upper half (left) and the lower half (right); the analog mode draws every repeat darker than the one before.
void DL1AudioProcessorEditor::drawEchoes (juce::Graphics& g)
{
    const auto& eng = proc.getEngine();
    const bool power = isOn ("power"), analog = mode() == 1, hold = isOn ("hold");
    const float timeMs = power ? eng.shownTimeMs.load() : value ("time"), fb = hold ? 1.0f : juce::jlimit (0.0f, 1.0f, value ("feedback") * 0.01f * (analog ? 1.03f : 1.02f));
    const juce::String t = timeMs >= 1000.0f ? juce::String (timeMs / 1000.0f, 2) + " s" : juce::String ((int) std::lround (timeMs)) + " ms";
    const juce::String label = "TIME " + (isOn ("sync") ? juce::String (dl1::divisionName (choice ("division"))) : t);
    ui::paintScope (g, kPal, echoScreenRect(), label, "FB " + juce::String ((int) std::lround (value ("feedback"))) + " %" + (hold ? " HOLD" : ""));

    const auto plot = echoPlotRect(), r = plot.withTrimmedBottom (22.0f);                                       // r: the bars; under it the time axis
    const float axisMs = 2400.0f;
    const auto xOf = [&] (float ms) { return echoX0() + echoW() * ms / axisMs; };
    g.setColour (kPal.phosphor.withAlpha (0.18f));
    g.drawLine (plot.getX(), r.getCentreY(), plot.getRight(), r.getCentreY(), 1.0f);
    for (float ms = 0.0f; ms <= axisMs + 1.0f; ms += 400.0f)
    {
        g.setColour (kPal.phosphor.withAlpha (0.12f));
        g.drawLine (xOf (ms), r.getY() + 2.0f, xOf (ms), r.getBottom() - 2.0f, 1.0f);
        if (ms > 1.0f && ((int) std::lround (ms)) % 800 == 0)
            drawText (g, juce::String (ms / 1000.0f, 1) + " s", xOf (ms), plot.getBottom() - 9.0f, 13.0f, kPal.phosphorDim, false, 0.0f, juce::Justification::centred, ui::Face::Mono);
    }

    const float spread = value ("spread") * 0.01f, mixAmt = value ("mix") * 0.01f;
    const float half = r.getHeight() * 0.5f - 6.0f;
    {   // the bars stay inside the screen
        juce::Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (echoScreenRect().reduced (3.0f).toNearestInt());
        // the input
        const float inLevel = power ? std::clamp (eng.inLevel.load(), 0.0f, 1.0f) : 0.0f;
        const float dryBase = analog ? std::cos (value ("anaBlend") * 0.01f * 0.5f * juce::MathConstants<float>::pi) : mode() >= 2 ? 0.7f : std::pow (value ("digDirect") * 0.01f, 1.6f);
        g.setColour (kPal.cream.withAlpha (0.8f));
        const float dh = std::max (3.0f, half * std::clamp (0.25f + inLevel * 1.4f, 0.0f, 1.0f) * (0.4f + 0.6f * std::clamp (dryBase, 0.0f, 1.0f) * (1.0f - 0.4f * mixAmt)));
        g.fillRoundedRectangle (juce::Rectangle<float> (8.0f, 2.0f * dh).withCentre ({ xOf (0.0f), r.getCentreY() }), 3.0f);
        // the repeats. Every tap is one bar: tape heads and the DUAL delay B are further taps of the same kind; `fall` is what each pass of the loop keeps
        const int m = mode();
        const float effGain = analog ? std::sin (value ("anaBlend") * 0.01f * 0.5f * juce::MathConstants<float>::pi) : m == 0 ? std::pow (value ("digEffect") * 0.01f, 1.4f) : 0.7f;
        const auto drawTaps = [&] (float firstMs, float stepMs, float startLevel, float fall, float darkening, bool bright, bool grip, float widthK)
        {
            float level = juce::jlimit (0.0f, 1.0f, startLevel);
            for (int k = 0; k < 60; ++k)
            {
                const float x = xOf (firstMs + stepMs * (float) k);
                if (x > r.getRight() - 6.0f || level < 0.025f) break;
                const float h = std::max (2.0f, half * level);
                const float tone = darkening > 0.0f ? std::pow (1.0f - darkening, (float) k) : 1.0f;
                const auto base = bright ? kPal.phosphor : kPal.phosphorDim;
                const auto col = base.withMultipliedBrightness (0.55f + 0.45f * tone).withAlpha (0.35f + 0.65f * std::clamp (level * 1.4f, 0.0f, 1.0f));
                const bool left = ((k + 1) % 2) == 1;                                                  // ping-pong: odd repeats left (up), even right (down)
                const float up = spread > 0.5f ? (left ? h : 0.0f) : h, down = spread > 0.5f ? (left ? 0.0f : h) : h;
                const float bw = (analog ? std::max (4.0f, 10.0f - 0.7f * (float) (k + 1)) : 8.0f) * widthK;
                g.setColour (col);
                if (up > 0.0f) g.fillRoundedRectangle (juce::Rectangle<float> (bw, up).withX (x - bw * 0.5f).withBottomY (r.getCentreY() - 1.0f), 2.0f);
                if (down > 0.0f) g.fillRoundedRectangle (juce::Rectangle<float> (bw, down).withX (x - bw * 0.5f).withY (r.getCentreY() + 1.0f), 2.0f);
                if (k == 0 && grip)                                                                        // the grip of the first repeat: drag it to set the time
                {
                    g.setColour (kPal.cream);
                    g.fillEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre ({ x, r.getCentreY() }));
                    g.setColour (kPal.screen);
                    g.drawEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre ({ x, r.getCentreY() }).reduced (0.9f), 1.8f);
                }
                level *= fall;
            }
        };
        const float fallA = hold ? 0.995f : fb;
        if (m == 2)
        {
            const int heads = 1 + juce::jlimit (0, 2, choice ("variant"));
            const float sp = value ("tapeSpacing") * 0.01f, lvl = juce::jlimit (0.0f, 1.0f, 0.35f + 0.65f * effGain);
            const float w[3] = { 1.0f, 0.75f, 0.55f };
            for (int hd = heads - 1; hd >= 0; --hd)
                drawTaps (timeMs * dl1::tapeHeadRatio (hd, sp), timeMs, lvl * w[hd], fallA, 0.0f, hd == 0, hd == 0, hd == 0 ? 1.0f : 0.7f);
        }
        else if (m == 3)
        {
            const float fbB = hold ? 0.995f : juce::jlimit (0.0f, 1.0f, value ("dualFbB") * 0.01f * 1.03f), timeB = power ? eng.shownTimeBMs.load() : timeMs * value ("dualTimeB");
            drawTaps (timeB, timeB, 0.75f * value ("dualLevelB") * 0.01f + 0.1f, fbB, 0.0f, false, false, 0.8f);
            drawTaps (timeMs, timeMs, 0.75f * value ("dualLevelA") * 0.01f + 0.1f, fallA, 0.0f, true, true, 1.0f);
        }
        else
            drawTaps (timeMs, timeMs, 0.35f + 0.65f * effGain, fallA, analog ? 0.18f : 0.0f, true, true, 1.0f);
    }
    drawText (g, "IN", xOf (0.0f), plot.getBottom() - 9.0f, 13.0f, kPal.phosphor, true, 0.1f, juce::Justification::centred, ui::Face::Mono);
}

//==============================================================================
void DL1AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    midiUI.paintMarks (g, kPal.accent);
}

void DL1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    ui::paintBody (g, kPal, kW, kH);
    ui::paintHeader (g, kPal, "DL-1", "DELAY LAB", "ECHO EVOLUTION");
    ui::paintPresetBar (g, kPal, proc.presets, proc.undoManager, isOn ("power"));
    ui::paintFooter (g, kPal, kH - 15.0f);

    const int m = mode();
    ui::paintPanel (g, kPal, panelRect (0), "TIME");
    ui::paintTabs (g, kPal, panelRect (0), kModeTabs, m, kTabsTop, kTabsFont, kTabsLeft);
    ui::paintPanel (g, kPal, panelRect (1), "VOICING");
    ui::paintPanel (g, kPal, panelRect (2), "MODULATION");
    ui::paintPanel (g, kPal, panelRect (3), "OUTPUT");
    ui::paintPanel (g, kPal, panelRect (4), "ECHOES");
    drawEchoes (g);

    // buttons: SYNC, the note length, HOLD, TAP; the types of the mode
    const int variant = juce::jlimit (0, dl1::variantCount (m) - 1, choice ("variant"));
    if (m == 4) ui::paintFlatButton (g, kPal, voiceRect(), juce::String (dl1::voiceName (choice ("modVoice"))) + "  v", true, 17.0f);
    else for (int v = 0; v < dl1::variantCount (m); ++v)
        ui::paintFlatButton (g, kPal, variantRect (v), dl1::variantName (m, v), v == variant, 17.0f);
    ui::paintFlatButton (g, kPal, timeButtonRect (0), "SYNC", isOn ("sync"), 17.0f);
    ui::paintFlatButton (g, kPal, timeButtonRect (1), dl1::divisionName (choice ("division")), isOn ("sync"), 17.0f, isOn ("sync") ? 1.0f : 0.55f);
    ui::paintFlatButton (g, kPal, timeButtonRect (2), "HOLD", isOn ("hold"), 17.0f);
    ui::paintFlatButton (g, kPal, timeButtonRect (3), "TAP", false, 17.0f);
    if (m == 3)
    {
        ui::paintFlatButton (g, kPal, pingRect (0), "PING A", isOn ("dualPingA"), 17.0f);
        ui::paintFlatButton (g, kPal, pingRect (1), "PING B", isOn ("dualPingB"), 17.0f);
    }

    // knobs
    for (auto& k : knobs)
    {
        if (k.onlyMode >= 0 && k.onlyMode != m) continue;
        const double v = k.slider.getValue();
        juce::String txt, lo, hi;
        switch (k.kind)
        {
            case 0: txt = isOn ("sync") ? juce::String (dl1::divisionName (choice ("division"))) : v >= 1000.0 ? juce::String (v / 1000.0, 2) + " s" : juce::String ((int) std::lround (v)) + " ms"; lo = "1"; hi = "2 s"; break;
            case 2: txt = v <= 20.5 ? juce::String ("OFF") : juce::String ((int) std::lround (v)) + " Hz"; lo = "off"; hi = "1k"; break;
            case 3: txt = juce::String (v / 1000.0, 1) + " kHz"; lo = "1k"; hi = "20k"; break;
            case 4: txt = juce::String (v, 2) + " Hz"; lo = ".05"; hi = "10"; break;
            case 5: txt = juce::String::formatted ("%+.1f dB", v); lo = "-24"; hi = "+12"; break;
            case 6: txt = "x" + juce::String (v, 2); lo = ".25"; hi = "x4"; break;
            default: txt = juce::String ((int) std::lround (v)) + " %"; lo = "0"; hi = "100"; break;
        }
        ui::paintKnob (g, kPal, knobCentre (k.panel, k.index, k.count), k.name, txt, lo, hi, (float) k.slider.valueToProportionOfLength (k.slider.getValue()));
    }
    ui::paintMeter (g, kPal, meter, meterOrigin().x, meterOrigin().y, isOn ("power"));
}

void DL1AudioProcessorEditor::showVoiceMenu()
{
    juce::PopupMenu menu;
    for (int i = 0; i < dl1::kNumVoices; ++i) menu.addItem (i + 1, dl1::voiceName (i), true, i == choice ("modVoice"));
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (voiceRect().transformedBy (juce::AffineTransform::scale (scale())).toNearestInt())),
                        [this] (int r) { if (r > 0) { proc.undoManager.beginNewTransaction(); setChoice ("modVoice", r - 1); repaint(); } });
}

void DL1AudioProcessorEditor::setTimeFromX (float x)
{
    const float ms = (x - echoX0()) / echoW() * 2400.0f;
    const float shown = proc.getEngine().shownTimeMs.load(), param = value ("time"), factor = param > 0.5f ? std::max (0.25f, shown / param) : 1.0f;
    if (auto* par = proc.apvts.getParameter ("time")) par->setValueNotifyingHost (par->convertTo0to1 (juce::jlimit (1.0f, 2000.0f, ms / factor)));
}

void DL1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / scale();
    proc.undoManager.beginNewTransaction();
    if (ui::powerRect().contains (p)) { toggle ("power"); return; }
    if (ui::presetBarMouseDown (p, *this, proc.presets, proc.undoManager, scale(), [this] { repaint(); })) { layoutKnobs(); return; }
    const auto tabs = ui::tabRects (panelRect (0), kModeTabs, kTabsTop, kTabsFont, kTabsLeft);
    for (int i = 0; i < (int) tabs.size(); ++i)
        if (tabs[(size_t) i].expanded (3.0f).contains (p)) { setChoice ("mode", i); setChoice ("variant", 0); layoutKnobs(); repaint(); return; }
    if (mode() == 4 && voiceRect().expanded (3.0f).contains (p)) { showVoiceMenu(); return; }
    for (int v = 0; v < (mode() == 4 ? 0 : dl1::variantCount (mode())); ++v)
        if (variantRect (v).expanded (3.0f).contains (p)) { setChoice ("variant", v); repaint(); return; }
    if (mode() == 3)
        for (int i = 0; i < 2; ++i)
            if (pingRect (i).expanded (3.0f).contains (p)) { toggle (i == 0 ? "dualPingA" : "dualPingB"); repaint(); return; }
    if (timeButtonRect (0).expanded (3.0f).contains (p)) { toggle ("sync"); repaint(); return; }
    if (timeButtonRect (1).expanded (3.0f).contains (p)) { setChoice ("division", (choice ("division") + (e.mods.isShiftDown() ? dl1::kNumDivisions - 1 : 1)) % dl1::kNumDivisions); if (! isOn ("sync")) toggle ("sync"); repaint(); return; }
    if (timeButtonRect (2).expanded (3.0f).contains (p)) { toggle ("hold"); repaint(); return; }
    if (timeButtonRect (3).expanded (3.0f).contains (p)) { proc.midiTap.tap(); repaint(); return; }          // TAP: the same tap logic as the MIDI tap (in the processor)
    if (echoScreenRect().contains (p) && ! isOn ("sync"))                                                  // the display: drag sets the time
    {
        if (auto* par = proc.apvts.getParameter ("time")) par->beginChangeGesture();
        draggingTime = true;
        setTimeFromX (p.x);
        repaint();
    }
}

void DL1AudioProcessorEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (! draggingTime) return;
    setTimeFromX (e.position.x / scale());
    repaint();
}

void DL1AudioProcessorEditor::mouseUp (const juce::MouseEvent&)
{
    if (! draggingTime) return;
    draggingTime = false;
    if (auto* par = proc.apvts.getParameter ("time")) par->endChangeGesture();
}
