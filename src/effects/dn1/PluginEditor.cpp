// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "ui/Presets.h"

namespace
{
namespace ui = vibr::ui::vintage;          // the default look (src/common/ui/VintageUI.h)

constexpr float kW = ui::kW, kH = ui::kH, kRow1Y = 114.0f, kRow1H = 244.0f, kRow2Y = 376.0f, kRow2H = 254.0f;
const ui::Palette kPal { juce::Colour (207, 155, 10) };             // the Dynamics colour #CF9B0A (a darker yellow, readable on the cream plate): value rings, knob values and the plugin code

// panels: 0 CONTROLS (with the mode tabs), 1 SIDECHAIN, 2 ACTION (the scope), 3 OUTPUT. Row 1 and row 2 share their columns (x 28 / 703 wide, x 749 / 343 wide)
constexpr float kPanelX[4] = { 28.0f, 749.0f, 28.0f, 749.0f }, kPanelW[4] = { 703.0f, 343.0f, 703.0f, 343.0f };
juce::Rectangle<float> panelRect (int i) { return { kPanelX[i], i < 2 ? kRow1Y : kRow2Y, kPanelW[i], i < 2 ? kRow1H : kRow2H }; }
juce::Point<float> knobCentre (int panel, int k, int n)
{
    const auto r = panelRect (panel);
    if (panel == 3) return { r.getX() + 66.0f + 106.0f * (float) k, r.getY() + 112.0f };         // MIX and LEVEL left of the meter
    const float x0 = r.getX() + 61.0f, x1 = r.getRight() - 61.0f;                                // 16 from the border to the ring
    const float pitch = panel == 1 ? 140.0f : n > 1 ? juce::jmin (160.0f, (x1 - x0) / (float) (n - 1)) : 0.0f;
    return { r.getCentreX() + pitch * ((float) k - 0.5f * (float) (n - 1)), r.getY() + 86.0f };
}
juce::Point<float> meterOrigin() { const auto r = panelRect (3); return { r.getRight() - 20.0f - ui::kMeterW, r.getY() + (r.getHeight() - ui::kMeterH) * 0.5f }; }
juce::Rectangle<float> scopeRect() { const auto r = panelRect (2); return { r.getX() + 20.0f, r.getY() + 22.0f, r.getWidth() - 40.0f, r.getHeight() - 42.0f }; }
// the buttons in the bottom row of CONTROLS: the stage switches of the shaper (3) or the types (2 or 3), of equal width
juce::Rectangle<float> buttonRect (int i) { const auto r = panelRect (0); const float w = (r.getWidth() - 40.0f - 12.0f) / 3.0f; return { r.getX() + 20.0f + (float) i * (w + 6.0f), r.getY() + 200.0f, w, 30.0f }; }
juce::Rectangle<float> autoRect() { const auto r = panelRect (1); return { r.getX() + 20.0f, r.getY() + 200.0f, r.getWidth() - 40.0f, 30.0f }; }
juce::Rectangle<float> extRect() { const auto r = panelRect (1); return { r.getRight() - 88.0f, r.getY() - 12.0f, 72.0f, 24.0f }; }      // the EXT lamp on the top edge of SIDECHAIN
const std::vector<juce::String> kModeTabs { "SHAPER", "COMPRESSOR", "LIMITER" };
constexpr float kTabsTop = -15.0f, kTabsFont = 16.0f;
const float kTabsLeft = ui::titleEnd ("CONTROLS");
constexpr float kDbTop = 0.0f, kDbBottom = -60.0f, kGainTop = 12.0f, kGainBottom = -36.0f;
}

// kinds: 0 dB threshold, 1 ms (release), 2 ratio, 3 percent, 4 +- percent, 5 level dB, 6 sidechain high-pass, 7 ms attack, 8 make-up dB, 9 ceiling dB
//==============================================================================
DN1AudioProcessorEditor::DN1AudioProcessorEditor (DN1AudioProcessor& p)
    : LabEditor (p, kW, kH), proc (p),
      knobs { { { "gateThresh", 0, 0, 7, "GATE", 0, 0, 7, {}, {} },        { "gateRelease", 0, 1, 7, "GATE REL.", 1, 0, 7, {}, {} },   { "expThresh", 0, 2, 7, "EXPANDER", 0, 0, 7, {}, {} },
                { "expRatio", 0, 3, 7, "RATIO", 2, 0, 7, {}, {} },         { "expRelease", 0, 4, 7, "EXP REL.", 1, 0, 7, {}, {} },     { "attack", 0, 5, 7, "ATTACK", 4, 0, 7, {}, {} },
                { "sustain", 0, 6, 7, "SUSTAIN", 4, 0, 7, {}, {} },
                { "compSustain", 0, 0, 3, "SUSTAIN", 3, 1, 1, {}, {} },    { "compAttack", 0, 1, 3, "ATTACK", 3, 1, 1, {}, {} },       { "tone", 0, 2, 3, "TONE", 3, 1, 1, {}, {} },
                { "compThresh", 0, 0, 7, "THRESHOLD", 0, 1, 6, {}, {} },   { "compRatio", 0, 1, 7, "RATIO", 2, 1, 6, {}, {} },        { "compAttackMs", 0, 2, 7, "ATTACK", 7, 1, 6, {}, {} },
                { "compRelease", 0, 3, 7, "RELEASE", 1, 1, 6, {}, {} },    { "compMakeup", 0, 4, 7, "MAKE-UP", 8, 1, 6, {}, {} },      { "tone", 0, 5, 7, "TONE", 3, 1, 6, {}, {} },
                { "compMix", 0, 6, 7, "MIX", 3, 1, 6, {}, {} },
                { "limThresh", 0, 0, 4, "THRESHOLD", 0, 2, 7, {}, {} },    { "limRelease", 0, 1, 4, "RELEASE", 1, 2, 7, {}, {} },      { "limCeiling", 0, 2, 4, "CEILING", 9, 2, 7, {}, {} },
                { "tone", 0, 3, 4, "TONE", 3, 2, 7, {}, {} },
                { "scHp", 1, 0, 2, "SC HIGH-PASS", 6, -1, 7, {}, {} },     { "link", 1, 1, 2, "STEREO LINK", 3, -1, 7, {}, {} },
                { "mix", 3, 0, 2, "MIX", 3, -1, 7, {}, {} },               { "level", 3, 1, 2, "LEVEL", 5, -1, 7, {}, {} } } }
{
    for (auto& k : knobs)
    {
        attachKnob (k.slider, k.att, k.id);
    }
    layoutKnobs();
    startTimerHz (30);
}

DN1AudioProcessorEditor::~DN1AudioProcessorEditor() { stopTimer(); }

void DN1AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    if (midiUI != nullptr) midiUI->paintMarks (g, kPal.accent);
}

void DN1AudioProcessorEditor::timerCallback()
{
    meter.update (proc.outMeter);
    if (const int shown = mode() * 8 + variant(); shown != shownKey) { shownKey = shown; layoutKnobs(); repaint(); }       // a preset, undo, A / B or automation changed the tab
    repaintDesign (juce::Rectangle<float> (meterOrigin().x, meterOrigin().y, ui::kMeterW, ui::kMeterH).expanded (2.0f));
    if (midiUI != nullptr && midiUI->needsRepaint()) { midiUI->tick(); repaint(); }
    if (proc.extKey.load (std::memory_order_relaxed) != extShown) { extShown = proc.extKey.load(); repaintDesign (extRect().expanded (4.0f)); }
    repaintDesign (scopeRect().expanded (4.0f));
}


bool DN1AudioProcessorEditor::knobShown (int i) const
{
    const auto& k = knobs[(size_t) i];
    return (k.onlyMode < 0 || k.onlyMode == mode()) && (k.typeMask & (1 << (mode() == 0 ? 0 : variant()))) != 0;
}

void DN1AudioProcessorEditor::layoutKnobs()
{
    for (int i = 0; i < (int) knobs.size(); ++i)
    {
        auto& k = knobs[(size_t) i];
        placeKnob (k.slider, knobShown (i) ? knobCentre (k.panel, k.index, k.count) : juce::Point<float> (-2000.0f, -2000.0f));
        k.slider.setVisible (knobShown (i));
    }

    juce::uint64 shown = 0;                                          // MIDI learn marks only for the knobs that are on screen
    for (int i = 0; i < (int) knobs.size(); ++i) if (knobShown (i)) shown |= (juce::uint64) 1 << i;
    if (midiUI == nullptr || shown != midiShown)
    {
        midiShown = shown;
        midiUI = std::make_unique<MidiLearnUI> (proc.midiMap);
        for (int i = 0; i < (int) knobs.size(); ++i) if (knobShown (i)) midiUI->add (knobs[(size_t) i].slider, knobs[(size_t) i].id);
    }
}
void DN1AudioProcessorEditor::resized() { layoutKnobs(); }

//==============================================================================
// ACTION: a phosphor scope with five seconds of the input level (dim) and of the gain of the chain (bright, hanging from the 0 dB line), with the thresholds of the mode dashed.
void DN1AudioProcessorEditor::drawAction (juce::Graphics& g)
{
    const auto& eng = proc.getEngine();
    const bool power = isOn ("power");
    const int m = mode();
    const float gr = power ? eng.grNow.load (std::memory_order_relaxed) : 0.0f;
    const auto sc = scopeRect();
    ui::paintScope (g, kPal, sc, m == 0 ? juce::String ("GAIN OF THE CHAIN, 5 SECONDS") : juce::String::formatted ("GAIN REDUCTION %.1f dB", (double) gr),
                    "LATENCY " + juce::String (eng.latency()) + " SAMPLES");
    const auto r = ui::scopePlotRect (sc);
    g.saveState();
    g.reduceClipRegion (sc.reduced (3.0f).toNearestInt());
    const auto yIn = [&] (float db) { return r.getBottom() - r.getHeight() * (juce::jlimit (kDbBottom, kDbTop, db) - kDbBottom) / (kDbTop - kDbBottom); };
    const auto yGain = [&] (float db) { return r.getY() + r.getHeight() * (kGainTop - juce::jlimit (kGainBottom, kGainTop, db)) / (kGainTop - kGainBottom); };
    for (float db = -12.0f; db >= kDbBottom + 1.0f; db -= 12.0f)
    {
        g.setColour (kPal.phosphor.withAlpha (0.15f));
        g.drawLine (r.getX(), yIn (db), r.getRight(), yIn (db), 1.0f);
        ui::drawText (g, juce::String ((int) db), r.getX() + 6.0f, yIn (db) - 9.0f, 13.0f, kPal.phosphorDim, false, 0.0f, juce::Justification::centredLeft, ui::Face::Mono);
    }
    const int pos = eng.histPos.load (std::memory_order_acquire), N = dn1::kHistory;
    juce::Path inPath, gainPath;
    for (int i = 0; i < N; ++i)
    {
        const int idx = (pos + i) % N;
        const float x = r.getX() + r.getWidth() * (float) i / (float) (N - 1);
        const float in = power ? eng.histIn[idx].load (std::memory_order_relaxed) : -90.0f, gn = power ? eng.histGain[idx].load (std::memory_order_relaxed) : 0.0f;
        if (i == 0) { inPath.startNewSubPath (x, r.getBottom()); gainPath.startNewSubPath (x, yGain (gn)); }
        inPath.lineTo (x, yIn (in)); gainPath.lineTo (x, yGain (gn));
    }
    inPath.lineTo (r.getRight(), r.getBottom()); inPath.closeSubPath();
    g.setColour (kPal.phosphorDim.withAlpha (0.22f)); g.fillPath (inPath);
    g.setColour (kPal.phosphorDim.withAlpha (0.8f)); g.strokePath (inPath, juce::PathStrokeType (1.3f));
    g.setColour (kPal.phosphorDim.withAlpha (0.5f)); g.drawLine (r.getX(), yGain (0.0f), r.getRight(), yGain (0.0f), 1.0f);
    g.setColour (kPal.phosphor.withAlpha (0.18f)); g.strokePath (gainPath, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved));
    g.setColour (kPal.phosphor); g.strokePath (gainPath, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved));
    ui::drawText (g, "GAIN", r.getRight() - 8.0f, r.getY() + 14.0f, 13.0f, kPal.phosphor, false, 0.1f, juce::Justification::centredRight, ui::Face::Mono);
    ui::drawText (g, "INPUT", r.getRight() - 8.0f, r.getBottom() - 12.0f, 13.0f, kPal.phosphorDim, false, 0.1f, juce::Justification::centredRight, ui::Face::Mono);

    // the thresholds of the mode (dashed)
    const auto dashed = [&] (float db, const juce::String& label)
    {
        const float y = yIn (db);
        g.setColour (kPal.segYellow.withAlpha (0.7f));
        const float dash[] = { 5.0f, 4.0f };
        g.drawDashedLine (juce::Line<float> (r.getX(), y, r.getRight(), y), dash, 2, 1.2f);
        ui::drawText (g, label, r.getX() + 52.0f, y - 10.0f, 13.0f, kPal.segYellow, false, 0.1f, juce::Justification::centredLeft, ui::Face::Mono);
    };
    if (m == 0)
    {
        if (isOn ("gateOn")) dashed (value ("gateThresh"), "GATE");
        if (isOn ("expOn")) dashed (value ("expThresh"), "EXPANDER");
    }
    else if (m == 1) dashed (variant() == 0 ? dn1::cs3ThresholdDb (value ("compSustain") * 0.01f) : value ("compThresh"), "THRESHOLD");
    else dashed (value ("limThresh"), "THRESHOLD");
    g.restoreState();
}

//==============================================================================
void DN1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    ui::paintBody (g, kPal, kW, kH);
    ui::paintHeader (g, kPal, "DN-1", "DYNAMICS LAB", "DYNAMIC PUNCH");
    ui::paintPresetBar (g, kPal, proc.presets, proc.undoManager, isOn ("power"));
    ui::paintFooter (g, kPal, kH - 15.0f);

    const int m = mode();
    ui::paintPanel (g, kPal, panelRect (0), "CONTROLS");
    ui::paintTabs (g, kPal, panelRect (0), kModeTabs, m, kTabsTop, kTabsFont, kTabsLeft);
    ui::paintPanel (g, kPal, panelRect (1), "SIDECHAIN");
    {   // EXT lamp: lit while the host's sidechain input is the key (shaper and compressor modes); the limiters always read their own signal
        const auto e = extRect();
        const bool ext = proc.extKey.load (std::memory_order_relaxed);
        g.setColour (kPal.paper);
        g.fillRect (e);
        const auto lamp = juce::Rectangle<float> (14.0f, 14.0f).withCentre ({ e.getX() + 13.0f, e.getCentreY() });
        if (ext) { g.setColour (kPal.ledOn.withAlpha (0.25f)); g.fillEllipse (lamp.expanded (4.0f)); }
        g.setColour (ext ? kPal.ledOn : kPal.ledOff);
        g.fillEllipse (lamp);
        g.setColour (kPal.ink);
        g.drawEllipse (lamp.reduced (1.0f), 2.0f);
        ui::drawText (g, "EXT", lamp.getRight() + 8.0f, e.getCentreY(), 16.0f, kPal.ink, true, 0.1f, juce::Justification::centredLeft);
    }
    ui::paintPanel (g, kPal, panelRect (2), "ACTION");
    ui::paintPanel (g, kPal, panelRect (3), "OUTPUT");
    drawAction (g);

    if (m == 0)
    {
        static const char* labels[3] = { "GATE", "EXPANDER", "TRANSIENT" }, *ids[3] = { "gateOn", "expOn", "transOn" };
        for (int i = 0; i < 3; ++i) ui::paintFlatButton (g, kPal, buttonRect (i), labels[i], isOn (ids[i]), 17.0f);
    }
    else
        for (int v = 0; v < dn1::variantCount (m); ++v) ui::paintFlatButton (g, kPal, buttonRect (v), dn1::variantName (m, v), v == variant(), 17.0f);
    ui::paintFlatButton (g, kPal, autoRect(), "AUTO LEVEL", isOn ("autoLevel"), 17.0f);

    static const char* lo[10] = { "-80", "10", "1", "0", "-100", "-24", "off", ".1", "0", "-3" }, *hi[10] = { "-10", "800", "10", "100", "+100", "+12", "300", "100", "24", "0" };
    for (int i = 0; i < (int) knobs.size(); ++i)
    {
        auto& k = knobs[(size_t) i];
        if (! knobShown (i)) continue;
        const double v = k.slider.getValue();
        juce::String txt;
        switch (k.kind)
        {
            case 0: txt = juce::String::formatted ("%.1f dB", v); break;
            case 1: txt = v >= 1000.0 ? juce::String (v / 1000.0, 2) + " s" : juce::String ((int) std::lround (v)) + " ms"; break;
            case 2: txt = juce::String (v, 1) + " : 1"; break;
            case 4: txt = juce::String::formatted ("%+d %%", (int) std::lround (v)); break;
            case 5: txt = juce::String::formatted ("%+.1f dB", v); break;
            case 6: txt = v <= 20.5 ? juce::String ("OFF") : juce::String ((int) std::lround (v)) + " Hz"; break;
            case 7: txt = juce::String (v, 1) + " ms"; break;
            case 8: txt = juce::String::formatted ("+%.1f dB", v); break;
            case 9: txt = juce::String::formatted ("%.1f dBTP", v); break;
            default: txt = juce::String ((int) std::lround (v)) + " %"; break;
        }
        const char* l = k.kind == 3 ? lo[3] : lo[k.kind], * h = k.kind == 3 ? hi[3] : hi[k.kind];
        if (k.kind == 0 && std::string (k.id) != "gateThresh" && std::string (k.id) != "expThresh") { l = std::string (k.id) == "limThresh" ? "-30" : "-60"; h = "0"; }
        const auto c = knobCentre (k.panel, k.index, k.count);
        const float prop = (float) k.slider.valueToProportionOfLength (v);
        if (k.kind == 4)                                                                             // +- percent: a mark at the top = 0, the ring runs from there
        {
            ui::paintKnob (g, kPal, c, k.name, txt, l, h, 0.0f);
            g.setColour (kPal.inkSoft);
            g.drawLine (c.x, c.y - ui::ringRadius (ui::kKnobR) - 7.0f, c.x, c.y - ui::ringRadius (ui::kKnobR) - 1.0f, 1.6f);
            ui::paintBipolarArc (g, c, ui::kKnobR, (float) (v / 100.0), kPal.accent, kPal.track);
        }
        else ui::paintKnob (g, kPal, c, k.name, txt, l, h, prop);
    }
    ui::paintMeter (g, kPal, meter, meterOrigin().x, meterOrigin().y, isOn ("power"));
}

void DN1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / scale();
    proc.undoManager.beginNewTransaction();
    if (ui::powerRect().contains (p)) { toggle ("power"); return; }
    if (ui::presetBarMouseDown (p, *this, proc.presets, proc.undoManager, scale(), [this] { repaint(); })) { layoutKnobs(); return; }
    const auto tabs = ui::tabRects (panelRect (0), kModeTabs, kTabsTop, kTabsFont, kTabsLeft);
    for (int i = 0; i < (int) tabs.size(); ++i)
        if (tabs[(size_t) i].expanded (3.0f).contains (p)) { setChoice ("mode", i); setChoice ("variant", 0); layoutKnobs(); repaint(); return; }
    if (mode() == 0)
    {
        static const char* ids[3] = { "gateOn", "expOn", "transOn" };
        for (int i = 0; i < 3; ++i) if (buttonRect (i).expanded (3.0f).contains (p)) { toggle (ids[i]); repaint(); return; }
    }
    else
        for (int v = 0; v < dn1::variantCount (mode()); ++v)
            if (buttonRect (v).expanded (3.0f).contains (p)) { setChoice ("variant", v); layoutKnobs(); repaint(); return; }
    if (autoRect().expanded (3.0f).contains (p)) { toggle ("autoLevel"); repaint(); return; }
}
