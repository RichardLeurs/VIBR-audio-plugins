// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "ui/Presets.h"

namespace
{
using vibr::ui::drawText;
namespace ui = vibr::ui;

constexpr float kW = 1200.0f, kH = 690.0f, kRow1Y = 134.0f, kRow1H = 247.0f, kRow2Y = 405.0f, kRow2H = 216.0f, kKnobR = 24.0f, kPad = 5.0f, kBarY = 39.0f;
const ui::Palette kPal { juce::Colour (53, 184, 232), juce::Colour (10, 33, 42) };            // the Delay & Echo colour #35B8E8 and its print

// panels: 0 TIME (with the mode tabs), 1 VOICING, 2 ECHOES (display), 3 OUTPUT
constexpr float kPanelX[4] = { 20.0f, 730.0f, 20.0f, 930.0f }, kPanelW[4] = { 700.0f, 450.0f, 900.0f, 250.0f };
juce::Rectangle<float> panelRect (int i) { return { kPanelX[i], i < 2 ? kRow1Y : kRow2Y, kPanelW[i], i < 2 ? kRow1H : kRow2H }; }
juce::Point<float> knobCentre (int panel, int k, int n)
{
    const auto r = panelRect (panel);
    const float pitch = panel == 0 ? 96.0f : n >= 5 ? 86.0f : 108.0f;
    return { r.getCentreX() + pitch * ((float) k - 0.5f * (float) (n - 1)), r.getY() + 92.0f };
}
juce::Rectangle<float> plotRect() { const auto r = panelRect (2); return { r.getX() + 24.0f, r.getY() + 34.0f, r.getWidth() - 48.0f, r.getHeight() - 74.0f }; }

// the flat buttons of the TIME panel: the types (2 or 3), SYNC, the note length, HOLD, TAP
juce::Rectangle<float> variantRect (int i) { const auto r = panelRect (0); return { r.getX() + 30.0f + (float) i * 96.0f, r.getY() + 204.0f, 90.0f, 30.0f }; }
juce::Rectangle<float> timeButtonRect (int i)                  // 0 SYNC, 1 note length, 2 HOLD, 3 TAP
{
    static const float x[4] = { 336.0f, 420.0f, 538.0f, 616.0f }, w[4] = { 76.0f, 110.0f, 70.0f, 70.0f };
    const auto r = panelRect (0);
    return { r.getX() + x[i], r.getY() + 204.0f, w[i], 30.0f };
}
const std::vector<juce::String> kModeTabs { "DIGITAL", "ANALOG", "TAPE", "DUAL", "MODULATED" };
juce::Rectangle<float> pingRect (int i) { const auto r = panelRect (1); return { r.getX() + 30.0f + (float) i * 110.0f, r.getY() + 204.0f, 100.0f, 30.0f }; }     // DUAL: PING A, PING B
} // namespace

//==============================================================================
DL1AudioProcessorEditor::DL1AudioProcessorEditor (DL1AudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      knobs { { { "time", 0, 0, 7, "TIME", 0, -1, {}, {} },        { "feedback", 0, 1, 7, "FEEDBACK", 1, -1, {}, {} }, { "low", 0, 2, 7, "LOW CUT", 2, -1, {}, {} },
                { "high", 0, 3, 7, "HI CUT", 3, -1, {}, {} },      { "modRate", 0, 4, 7, "MOD RATE", 4, -1, {}, {} },  { "modDepth", 0, 5, 7, "MOD DEPTH", 1, -1, {}, {} },
                { "spread", 0, 6, 7, "PING-PONG", 1, -1, {}, {} },
                { "digFactor", 1, 0, 4, "FACTOR", 1, 0, {}, {} },  { "digDirect", 1, 1, 4, "DIRECT", 1, 0, {}, {} },   { "digEffect", 1, 2, 4, "EFFECT", 1, 0, {}, {} },
                { "digInput", 1, 3, 4, "INPUT", 1, 0, {}, {} },
                { "anaBlend", 1, 0, 3, "BLEND", 1, 1, {}, {} },    { "anaDepth", 1, 1, 3, "DEPTH", 1, 1, {}, {} },     { "anaGrit", 1, 2, 3, "GRIT", 1, 1, {}, {} },
                { "tapeWow", 1, 0, 4, "WOW", 1, 2, {}, {} },       { "tapeSat", 1, 1, 4, "SATURATION", 1, 2, {}, {} }, { "tapeAge", 1, 2, 4, "AGE", 1, 2, {}, {} },
                { "tapeSpacing", 1, 3, 4, "SPACING", 1, 2, {}, {} },
                { "dualTimeB", 1, 0, 5, "TIME B", 6, 3, {}, {} },  { "dualFbB", 1, 1, 5, "FEEDBACK B", 1, 3, {}, {} }, { "dualLevelA", 1, 2, 5, "LEVEL A", 1, 3, {}, {} },
                { "dualLevelB", 1, 3, 5, "LEVEL B", 1, 3, {}, {} }, { "dualHighB", 1, 4, 5, "HI CUT B", 3, 3, {}, {} },
                { "modVariation", 1, 0, 2, "VARIATION", 1, 4, {}, {} }, { "modVolume", 1, 1, 2, "VOLUME", 1, 4, {}, {} },
                { "mix", 3, 0, 2, "MIX", 1, -1, {}, {} },          { "level", 3, 1, 2, "LEVEL", 5, -1, {}, {} } } },
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

DL1AudioProcessorEditor::~DL1AudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void DL1AudioProcessorEditor::timerCallback()
{
    if (midiUI.needsRepaint()) { midiUI.tick(); repaint(); }
    repaint (panelRect (2).expanded (4.0f).transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer());
    if (barState.copiedAt != 0) { repaint (juce::Rectangle<float> (0.0f, 30.0f, 900.0f, 50.0f).transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer()); if (! barState.copiedRecently()) barState.copiedAt = 0; }
}

void DL1AudioProcessorEditor::toggle (const char* id)
{
    auto* par = proc.apvts.getParameter (id);
    par->beginChangeGesture(); par->setValueNotifyingHost (par->getValue() > 0.5f ? 0.0f : 1.0f); par->endChangeGesture();
}
void DL1AudioProcessorEditor::setChoice (const char* id, int index) { setFloat (id, (float) index); }
void DL1AudioProcessorEditor::setFloat (const char* id, float v)
{
    auto* par = proc.apvts.getParameter (id);
    par->beginChangeGesture(); par->setValueNotifyingHost (par->convertTo0to1 (v)); par->endChangeGesture();
}

void DL1AudioProcessorEditor::layoutKnobs()
{
    const float sc = scale(), d = 2.0f * (kKnobR + kPad) * sc;
    for (auto& k : knobs)
    {
        k.slider.setBounds (juce::Rectangle<float> (d, d).withCentre (knobCentre (k.panel, k.index, k.count) * sc).toNearestInt());
        k.slider.setVisible (k.onlyMode < 0 || k.onlyMode == mode());
    }
}
void DL1AudioProcessorEditor::resized() { layoutKnobs(); }

// TAP: the time between the last taps sets TIME (a pause of more than 2 s starts again)
void DL1AudioProcessorEditor::tap() { proc.midiTap.tap(); }   // the same tap logic as the MIDI tap (in the processor)

//==============================================================================
// ECHOES: the input (the bar at 0 follows the level), the first repeat at TIME (drag it), and the repeats after it falling by FEEDBACK, on a time axis of 2.4 s.
// SPREAD 100 % alternates the repeats between the upper half (left) and the lower half (right); the analog mode draws every repeat darker than the one before.
void DL1AudioProcessorEditor::drawEchoes (juce::Graphics& g)
{
    const auto r = plotRect();
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (r.expanded (8.0f), 10.0f);
    const float axisMs = 2400.0f;
    const auto xOf = [&] (float ms) { return r.getX() + 16.0f + (r.getWidth() - 32.0f) * ms / axisMs; };
    g.setColour (kPal.ink.withAlpha (0.12f));
    g.drawLine (r.getX(), r.getCentreY(), r.getRight(), r.getCentreY(), 1.0f);
    for (float ms = 0.0f; ms <= axisMs + 1.0f; ms += 400.0f)
    {
        g.setColour (kPal.ink.withAlpha (0.1f));
        g.drawLine (xOf (ms), r.getY() + 4.0f, xOf (ms), r.getBottom() - 4.0f, 1.0f);
        drawText (g, ms < 1.0f ? "0" : ms >= 1000.0f ? juce::String (ms / 1000.0f, 1) + " s" : juce::String ((int) ms) + " ms", xOf (ms), r.getBottom() + 14.0f, 10.0f, kPal.ink.withAlpha (0.5f), false, 0.0f);
    }

    const auto& eng = proc.getEngine();
    const bool power = isOn ("power"), analog = mode() == 1, hold = isOn ("hold");
    const float timeMs = power ? eng.shownTimeMs.load() : value ("time"), fb = hold ? 1.0f : juce::jlimit (0.0f, 1.0f, value ("feedback") * 0.01f * (analog ? 1.03f : 1.02f));
    const float spread = value ("spread") * 0.01f, mixAmt = value ("mix") * 0.01f;
    const float half = r.getHeight() * 0.5f - 10.0f;
    {   // the bars stay inside the plot
        juce::Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (r.expanded (8.0f).toNearestInt());
        // the input
        const float inLevel = power ? std::clamp (eng.inLevel.load(), 0.0f, 1.0f) : 0.0f;
        const float dryBase = analog ? std::cos (value ("anaBlend") * 0.01f * 0.5f * juce::MathConstants<float>::pi) : mode() >= 2 ? 0.7f : std::pow (value ("digDirect") * 0.01f, 1.6f);
        g.setColour (kPal.ink.withAlpha (0.8f));
        const float dh = std::max (3.0f, half * std::clamp (0.25f + inLevel * 1.4f, 0.0f, 1.0f) * (0.4f + 0.6f * std::clamp (dryBase, 0.0f, 1.0f) * (1.0f - 0.4f * mixAmt)));
        g.fillRoundedRectangle (juce::Rectangle<float> (8.0f, 2.0f * dh).withCentre ({ xOf (0.0f), r.getCentreY() }), 3.0f);
        drawText (g, "IN", xOf (0.0f), r.getY() + 8.0f, 10.0f, kPal.ink.withAlpha (0.55f), true, 0.1f);
        // the repeats. Every tap is one bar: tape heads and the DUAL delay B are further taps of the same kind; `fall` is what each pass of the loop keeps
        const int m = mode();
        const float effGain = analog ? std::sin (value ("anaBlend") * 0.01f * 0.5f * juce::MathConstants<float>::pi) : m == 0 ? std::pow (value ("digEffect") * 0.01f, 1.4f) : 0.7f;
        const auto drawTaps = [&] (float firstMs, float stepMs, float startLevel, float fall, float darkening, bool bodyColour, bool grip, float widthK)
        {
            float level = juce::jlimit (0.0f, 1.0f, startLevel);
            for (int k = 0; k < 60; ++k)
            {
                const float x = xOf (firstMs + stepMs * (float) k);
                if (x > r.getRight() - 6.0f || level < 0.025f) break;
                const float h = std::max (2.0f, half * level);
                const float tone = darkening > 0.0f ? std::pow (1.0f - darkening, (float) k) : 1.0f;
                const auto base = bodyColour ? kPal.body : kPal.ink.withAlpha (0.9f);
                const auto col = base.withMultipliedBrightness (0.55f + 0.45f * tone).withAlpha (0.35f + 0.65f * std::clamp (level * 1.4f, 0.0f, 1.0f));
                const bool left = ((k + 1) % 2) == 1;                                                  // ping-pong: odd repeats left (up), even right (down)
                const float up = spread > 0.5f ? (left ? h : 0.0f) : h, down = spread > 0.5f ? (left ? 0.0f : h) : h;
                const float bw = (analog ? std::max (4.0f, 10.0f - 0.7f * (float) (k + 1)) : 8.0f) * widthK;
                g.setColour (col);
                if (up > 0.0f) g.fillRoundedRectangle (juce::Rectangle<float> (bw, up).withX (x - bw * 0.5f).withBottomY (r.getCentreY() - 1.0f), 2.0f);
                if (down > 0.0f) g.fillRoundedRectangle (juce::Rectangle<float> (bw, down).withX (x - bw * 0.5f).withY (r.getCentreY() + 1.0f), 2.0f);
                if (k == 0 && grip)                                                                        // the grip of the first repeat: drag it to set the time
                {
                    g.setColour (kPal.body);
                    g.fillEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre ({ x, r.getCentreY() }));
                    g.setColour (kPal.dark);
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
    // the readout
    const juce::String t = timeMs >= 1000.0f ? juce::String (timeMs / 1000.0f, 2) + " s" : juce::String ((int) std::lround (timeMs)) + " ms";
    drawText (g, "TIME " + t + (isOn ("sync") ? "   (" + juce::String (dl1::divisionName (choice ("division"))) + " AT " + juce::String ((int) std::lround (proc.hostBpm())) + " BPM)" : "")
                 + "     FEEDBACK " + juce::String ((int) std::lround (value ("feedback"))) + " %" + (hold ? "     HOLD" : ""),
              r.getX() + 12.0f, r.getY() + r.getHeight() + 34.0f, 11.0f, kPal.ink.withAlpha (0.6f), true, 0.1f, juce::Justification::centredLeft);
}

//==============================================================================
void DL1AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    midiUI.paintMarks (g, kPal.body);
}

void DL1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    ui::paintBody (g, kPal, kW, kH, grain);
    ui::paintHeader (g, kPal, "DL-1", "DELAY LAB", "ECHO EVOLUTION", isOn ("power"));
    ui::paintPresetBar (g, kPal, proc.presets, proc.undoManager, kBarY, barState, false);

    const int m = mode();
    ui::paintPanel (g, kPal, panelRect (0), {});
    ui::paintTabs (g, kPal, panelRect (0), kModeTabs, m);
    ui::paintPanel (g, kPal, panelRect (1), juce::String (dl1::modeName (m)) + " VOICING");
    ui::paintPanel (g, kPal, panelRect (2), "ECHOES");
    ui::paintPanel (g, kPal, panelRect (3), "OUTPUT");
    drawEchoes (g);

    // buttons: the types of the mode, SYNC, the note length, HOLD, TAP
    const int variant = juce::jlimit (0, dl1::variantCount (m) - 1, choice ("variant"));
    if (m == 4) ui::paintFlatButton (g, kPal, variantRect (0).withWidth (282.0f), juce::String (dl1::voiceName (choice ("modVoice"))) + "  v", true, 11.0f);
    else for (int v = 0; v < dl1::variantCount (m); ++v)
        ui::paintFlatButton (g, kPal, variantRect (v), dl1::variantName (m, v), v == variant, 11.0f);
    ui::paintFlatButton (g, kPal, timeButtonRect (0), "SYNC", isOn ("sync"), 11.0f);
    ui::paintFlatButton (g, kPal, timeButtonRect (1), dl1::divisionName (choice ("division")), isOn ("sync"), 11.0f, isOn ("sync") ? 1.0f : 0.55f);
    ui::paintFlatButton (g, kPal, timeButtonRect (2), "HOLD", isOn ("hold"), 11.0f);
    ui::paintFlatButton (g, kPal, timeButtonRect (3), "TAP", false, 11.0f);
    if (m == 3)
    {
        ui::paintFlatButton (g, kPal, pingRect (0), "PING A", isOn ("dualPingA"), 11.0f);
        ui::paintFlatButton (g, kPal, pingRect (1), "PING B", isOn ("dualPingB"), 11.0f);
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
        const auto c = knobCentre (k.panel, k.index, k.count);
        ui::paintKnob (g, kPal, c, k.name, txt, lo, hi, panelRect (k.panel).getY() + 154.0f, panelRect (k.panel).getY() + 182.0f);
    }
    ui::paintFooter (g, kPal, kRow2Y + kRow2H + 35.0f);
}

void DL1AudioProcessorEditor::showVoiceMenu()
{
    juce::PopupMenu menu;
    for (int i = 0; i < dl1::kNumVoices; ++i) menu.addItem (i + 1, dl1::voiceName (i), true, i == choice ("modVoice"));
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (variantRect (0).withWidth (282.0f).transformedBy (juce::AffineTransform::scale (scale())).toNearestInt())),
                        [this] (int r) { if (r > 0) { proc.undoManager.beginNewTransaction(); setChoice ("modVoice", r - 1); repaint(); } });
}

void DL1AudioProcessorEditor::setTimeFromX (float x)
{
    const auto r = plotRect();
    const float ms = (x - (r.getX() + 16.0f)) / (r.getWidth() - 32.0f) * 2400.0f;
    const float shown = proc.getEngine().shownTimeMs.load(), param = value ("time"), factor = param > 0.5f ? std::max (0.25f, shown / param) : 1.0f;
    if (auto* par = proc.apvts.getParameter ("time")) par->setValueNotifyingHost (par->convertTo0to1 (juce::jlimit (1.0f, 2000.0f, ms / factor)));
}

void DL1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / scale();
    proc.undoManager.beginNewTransaction();
    if (ui::powerRect().contains (p)) { toggle ("power"); return; }
    if (ui::presetBarMouseDown (p, *this, proc.presets, proc.undoManager, kBarY, scale(), barState, [this] { repaint(); }, false)) { layoutKnobs(); return; }
    const auto tabs = ui::tabRects (panelRect (0), kModeTabs);
    for (int i = 0; i < (int) tabs.size(); ++i)
        if (tabs[(size_t) i].expanded (3.0f).contains (p)) { setChoice ("mode", i); setChoice ("variant", 0); layoutKnobs(); repaint(); return; }
    if (mode() == 4 && variantRect (0).withWidth (282.0f).expanded (3.0f).contains (p)) { showVoiceMenu(); return; }
    for (int v = 0; v < (mode() == 4 ? 0 : dl1::variantCount (mode())); ++v)
        if (variantRect (v).expanded (3.0f).contains (p)) { setChoice ("variant", v); repaint(); return; }
    if (mode() == 3)
        for (int i = 0; i < 2; ++i)
            if (pingRect (i).expanded (3.0f).contains (p)) { toggle (i == 0 ? "dualPingA" : "dualPingB"); repaint(); return; }
    if (timeButtonRect (0).expanded (3.0f).contains (p)) { toggle ("sync"); repaint(); return; }
    if (timeButtonRect (1).expanded (3.0f).contains (p)) { setChoice ("division", (choice ("division") + (e.mods.isShiftDown() ? dl1::kNumDivisions - 1 : 1)) % dl1::kNumDivisions); if (! isOn ("sync")) toggle ("sync"); repaint(); return; }
    if (timeButtonRect (2).expanded (3.0f).contains (p)) { toggle ("hold"); repaint(); return; }
    if (timeButtonRect (3).expanded (3.0f).contains (p)) { tap(); repaint(); return; }
    if (plotRect().expanded (8.0f).contains (p) && ! isOn ("sync"))                       // the display: drag sets the time
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
