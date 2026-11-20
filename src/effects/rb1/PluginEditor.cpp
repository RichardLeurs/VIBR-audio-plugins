// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "ui/Presets.h"

namespace
{
namespace ui = vibr::ui::vintage;          // the default look (src/common/ui/VintageUI.h)

constexpr float kW = ui::kW, kH = ui::kH, kRow1Y = 114.0f, kRow1H = 244.0f, kRow2Y = 376.0f, kRow2H = 254.0f;
const ui::Palette kPal { juce::Colour (186, 105, 232) };            // the Reverb & Space colour #BA69E8: value rings, knob values and the plugin code

// panels: 0 CONTROLS (mode tabs, full width), 1 OUTPUT, 2 TAIL (screen). Row 2: TAIL 486 wide, 18 gap, OUTPUT 560 wide
constexpr float kPanelX[3] = { 28.0f, 532.0f, 28.0f }, kPanelW[3] = { 1064.0f, 560.0f, 486.0f };
constexpr float kRowW = 900.0f, kBtnGap = 6.0f;                      // CONTROLS: knob pitch 140, the button row 900 wide, centred
juce::Rectangle<float> panelRect (int i) { return { kPanelX[i], i == 0 ? kRow1Y : kRow2Y, kPanelW[i], i == 0 ? kRow1H : kRow2H }; }
juce::Point<float> knobCentre (int panel, int k, int n)
{
    const auto r = panelRect (panel);
    if (panel == 1) return { r.getX() + 61.0f + 106.0f * (float) k, r.getY() + 86.0f };          // the output knobs left of the meter, 20 from the border to the ring
    return { r.getCentreX() + 140.0f * ((float) k - 0.5f * (float) (n - 1)), r.getY() + 86.0f };
}
juce::Point<float> meterOrigin() { const auto r = panelRect (1); return { r.getRight() - 20.0f - ui::kMeterW, r.getY() + (r.getHeight() - ui::kMeterH) * 0.5f }; }
juce::Rectangle<float> buttonRect (int i)
{
    const auto r = panelRect (0);
    const float w = (kRowW - 4.0f * kBtnGap) / 5.0f;
    return { r.getCentreX() - 0.5f * kRowW + (float) i * (w + kBtnGap), r.getY() + 200.0f, w, 30.0f };
}
juce::Rectangle<float> typeChipRect() { const auto r = panelRect (0); return { r.getCentreX() - 0.5f * kRowW, r.getY() + 200.0f, kRowW, 30.0f }; }
juce::Rectangle<float> textureRect (int i) { return buttonRect (i); }
juce::Rectangle<float> rootRect() { return buttonRect (3); }
juce::Rectangle<float> scaleRect() { return buttonRect (4); }
juce::Rectangle<float> syncRect() { const auto r = panelRect (1); return { r.getX() + 20.0f, r.getY() + 200.0f, 92.0f, 30.0f }; }          // SYNC of the pre-delay, under its knob
juce::Rectangle<float> freezeRect() { const auto r = panelRect (1); return { r.getX() + 118.0f, r.getY() + 200.0f, 302.0f, 30.0f }; }     // the rest of the row under the four knobs, to the ring edge
juce::Rectangle<float> extRect() { const auto r = panelRect (1); return { r.getRight() - 88.0f, r.getY() - 12.0f, 72.0f, 24.0f }; }      // the EXT lamp on the top edge of OUTPUT
juce::Rectangle<float> scopeRect() { const auto r = panelRect (2); return { r.getX() + 20.0f, r.getY() + 22.0f, r.getWidth() - 40.0f, r.getHeight() - 42.0f }; }
const std::vector<juce::String> kModeTabs { "CLASSIC", "SPACE", "RESONATE" };
constexpr float kTabsTop = -15.0f, kTabsFont = 16.0f;
const float kTabsLeft = ui::titleEnd ("CONTROLS");
}

// kinds: 0 percent, 1 ms, 2 level dB, 3 cents, 4 strings

// the two type knobs of CLASSIC (the engine's PARAM 1 and PARAM 2) by what they do in each of the 12 types
namespace
{
const char* classicP1Name (int t)
{
    static const char* n[bigsky::kNumTypes] = { "SIZE", "SIZE", "SIZE", "DRIP", "RISE", "RISE", "SIZE", "VOWEL", "SHIMMER", "TIME", "SHAPE", "SIZE" };
    return n[juce::jlimit (0, bigsky::kNumTypes - 1, t)];
}
const char* classicP2Name (int t)
{
    static const char* n[bigsky::kNumTypes] = { "DAMPING", "DAMPING", "DAMPING", "DAMPING", "DAMPING", "DAMPING", "DAMPING", "BLEND", "INTERVAL", "REPEATS", "THRESHOLD", "SLOPE" };
    return n[juce::jlimit (0, bigsky::kNumTypes - 1, t)];
}
// the value text where a percent says little: the SHIMMER interval and the NONLINEAR shape
juce::String classicP1Text (int t, double v)
{
    if (t == bigsky::Nonlinear) return v < 34.0 ? "GATE" : v < 67.0 ? "REVERSE" : "DECAY";
    return juce::String ((int) std::lround (v)) + " %";
}
juce::String classicP2Text (int t, double v)
{
    if (t == bigsky::Shimmer) { static const char* n[5] = { "-OCT", "+5TH", "+OCT", "+OCT +5", "+2 OCT" }; return n[juce::jlimit (0, 4, (int) (v * 0.05))]; }
    return juce::String ((int) std::lround (v)) + " %";
}
}
//==============================================================================
RB1AudioProcessorEditor::RB1AudioProcessorEditor (RB1AudioProcessor& p)
    : LabEditor (p, kW, kH), proc (p),
      knobs { { { "decay", 0, 0, 6, "DECAY", 0, 0, {}, {} },        { "size", 0, 1, 6, "PARAM 1", 0, 0, {}, {} },       { "param2", 0, 2, 6, "PARAM 2", 0, 0, {}, {} },
                { "tone", 0, 3, 6, "TONE", 0, 0, {}, {} },          { "motion", 0, 4, 6, "MOTION", 0, 0, {}, {} },      { "value", 0, 5, 6, "VALUE", 0, 0, {}, {} },
                { "decay", 0, 0, 6, "DECAY", 0, 1, {}, {} },        { "size", 0, 1, 6, "SIZE", 0, 1, {}, {} },          { "diffusion", 0, 2, 6, "DIFFUSION", 0, 1, {}, {} },
                { "tone", 0, 3, 6, "TONE", 0, 1, {}, {} },          { "motion", 0, 4, 6, "MOTION", 0, 1, {}, {} },      { "texture", 0, 5, 6, "TEXTURE", 0, 1, {}, {} },
                { "decay", 0, 0, 7, "DECAY", 0, 2, {}, {} },        { "size", 0, 1, 7, "STRINGS", 4, 2, {}, {} },       { "damping", 0, 2, 7, "DAMPING", 0, 2, {}, {} },
                { "tension", 0, 3, 7, "TENSION", 0, 2, {}, {} },    { "motion", 0, 4, 7, "MOTION", 0, 2, {}, {} },      { "fine", 0, 5, 7, "FINE", 3, 2, {}, {} },
                { "blend", 0, 6, 7, "BLEND", 0, 2, {}, {} },
                { "preDelay", 1, 0, 4, "PRE-DELAY", 1, -1, {}, {} },{ "duck", 1, 1, 4, "DUCK", 0, -1, {}, {} },        { "mix", 1, 2, 4, "MIX", 0, -1, {}, {} },
                { "level", 1, 3, 4, "LEVEL", 2, -1, {}, {} } } },
      syncAtt (*p.apvts.getParameter ("preSync"), [this] (float v) { preSyncOn = v > 0.5f; rebindPre(); repaint(); }, nullptr)
{
    for (auto& k : knobs)
    {
        attachKnob (k.slider, k.att, k.id);
        midiUI.add (k.slider, k.id);
    }
    syncAtt.sendInitialUpdate();
    layoutKnobs();
    startTimerHz (30);
}

void RB1AudioProcessorEditor::rebindPre()
{
    for (auto& k : knobs)
        if (juce::String (k.id) == "preDelay") { k.att.reset(); attachKnob (k.slider, k.att, preSyncOn ? "preDivision" : "preDelay"); }
}

RB1AudioProcessorEditor::~RB1AudioProcessorEditor() { stopTimer(); }

void RB1AudioProcessorEditor::timerCallback()
{
    meter.update (proc.outMeter);
    if (const int shown = mode() * 8 + variant(); shown != shownKey) { shownKey = shown; layoutKnobs(); repaint(); }       // a preset, undo, A / B or automation changed the tab
    repaintDesign (juce::Rectangle<float> (meterOrigin().x, meterOrigin().y, ui::kMeterW, ui::kMeterH).expanded (2.0f));
    if (midiUI.needsRepaint()) { midiUI.tick(); repaint(); }
    if (proc.extKey.load (std::memory_order_relaxed) != extShown) { extShown = proc.extKey.load(); repaintDesign (extRect().expanded (4.0f)); }
    repaintDesign (scopeRect().expanded (4.0f));
}


void RB1AudioProcessorEditor::layoutKnobs()
{
    for (int i = 0; i < (int) knobs.size(); ++i)
    {
        auto& k = knobs[(size_t) i];
        placeKnob (k.slider, knobCentre (k.panel, k.index, k.count));
        k.slider.setVisible (knobShown (i));
    }
}
void RB1AudioProcessorEditor::resized() { layoutKnobs(); }

void RB1AudioProcessorEditor::showMenu (const char* id, const juce::StringArray& items, int current, juce::Rectangle<float> area)
{
    juce::PopupMenu menu;
    for (int i = 0; i < items.size(); ++i) menu.addItem (i + 1, items[i], true, i == current);
    const juce::String pid (id);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (area.transformedBy (juce::AffineTransform::scale (scale())).toNearestInt())),
                        [this, pid] (int r) { if (r > 0) { proc.undoManager.beginNewTransaction(); setChoice (pid.toRawUTF8(), r - 1); repaint(); } });
}

//==============================================================================
// TAIL: CLASSIC and SPACE show the level of the wet sound over the last five seconds (the tail, its decay and the ducking and freeze as they happen);
// RESONATE shows the strings as lines with their notes, lit by how strongly they ring.
void RB1AudioProcessorEditor::drawTail (juce::Graphics& g)
{
    const auto& eng = proc.getEngine();
    const bool power = isOn ("power");
    const auto sc = scopeRect();
    const bool frozen = isOn ("freeze");
    const float secs = bigsky::decaySeconds (variant(), value ("decay") * 0.01f);
    const juce::String hint = frozen ? juce::String ("FROZEN") : mode() == 2 ? juce::String ("ENERGY")
                            : mode() == 0 ? juce::String (variant() == bigsky::Nonlinear ? "GATE " : variant() == bigsky::Reflections ? "SPAN " : "RT60 ") + juce::String (secs, 1) + " S" : juce::String();
    ui::paintScope (g, kPal, sc, mode() == 2 ? "STRINGS" : "WET LEVEL, 5 SECONDS", hint);
    const auto r = ui::scopePlotRect (sc);
    g.saveState();
    g.reduceClipRegion (sc.reduced (3.0f).toNearestInt());
    if (mode() == 2)
    {
        const int n = sr1::stringCount (value ("size") * 0.01f), root = choice ("root"), scale = choice ("scale");
        const float cw = r.getWidth() / (float) sr1::kMaxStrings;
        for (int i = 0; i < sr1::kMaxStrings; ++i)
        {
            const float e = power && i < n ? std::clamp (eng.resonator().energy[i].load (std::memory_order_relaxed), 0.0f, 1.0f) : 0.0f, cx = r.getX() + (float) i * cw + 0.5f * cw;
            const bool active = i < n;
            g.setColour (kPal.phosphorDim.withAlpha (active ? 0.45f : 0.15f));
            g.drawLine (cx, r.getY() + 6.0f, cx, r.getBottom() - 26.0f, 1.2f);
            if (active)
            {
                const float h = (r.getHeight() - 40.0f) * std::sqrt (e);
                g.setColour (kPal.phosphor.withAlpha (0.35f + 0.65f * e));
                g.fillRoundedRectangle (juce::Rectangle<float> (7.0f, std::max (4.0f, h)).withCentre ({ cx, 0.0f }).withBottomY (r.getBottom() - 26.0f), 3.0f);
            }
            ui::drawText (g, active ? juce::String (sr1::noteName (sr1::stringNote (i, root, scale))) : juce::String ("-"), cx, r.getBottom() - 10.0f, 13.0f, active ? kPal.phosphor : kPal.phosphorDim.withAlpha (0.5f), false, 0.0f, juce::Justification::centred, ui::Face::Mono);
        }
        g.restoreState();
        return;
    }
    const auto yOf = [&] (float db) { return r.getBottom() - r.getHeight() * (std::clamp (db, -60.0f, 0.0f) + 60.0f) / 60.0f; };
    for (float db = -12.0f; db >= -60.0f; db -= 12.0f)
    {
        g.setColour (kPal.phosphor.withAlpha (0.15f)); g.drawLine (r.getX(), yOf (db), r.getRight(), yOf (db), 1.0f);
        ui::drawText (g, juce::String ((int) db), r.getX() + 6.0f, yOf (db) - 9.0f, 13.0f, kPal.phosphorDim, false, 0.0f, juce::Justification::centredLeft, ui::Face::Mono);
    }
    juce::Path area, line;
    const int pos = eng.histPos.load (std::memory_order_acquire), N = rb1::kHistory;
    area.startNewSubPath (r.getX(), r.getBottom());
    for (int i = 0; i < N; ++i)
    {
        const float x = r.getX() + r.getWidth() * (float) i / (float) (N - 1), db = power ? eng.histWet[(pos + i) % N].load (std::memory_order_relaxed) : -90.0f;
        area.lineTo (x, yOf (db));
        if (i == 0) line.startNewSubPath (x, yOf (db)); else line.lineTo (x, yOf (db));
    }
    area.lineTo (r.getRight(), r.getBottom()); area.closeSubPath();
    g.setColour (kPal.phosphorDim.withAlpha (frozen ? 0.35f : 0.2f)); g.fillPath (area);
    g.setColour (kPal.phosphor.withAlpha (0.18f)); g.strokePath (line, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved));
    g.setColour (kPal.phosphor); g.strokePath (line, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved));
    if (eng.duckNow.load (std::memory_order_relaxed) < 0.95f && power)
        ui::drawText (g, "DUCKING", r.getRight() - 8.0f, r.getY() + 14.0f, 13.0f, kPal.segYellow, false, 0.1f, juce::Justification::centredRight, ui::Face::Mono);
    g.restoreState();
}

//==============================================================================
void RB1AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    midiUI.paintMarks (g, kPal.accent);
}

void RB1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    ui::paintBody (g, kPal, kW, kH);
    ui::paintHeader (g, kPal, "RB-1", "REVERB LAB", "INFINITE SPACES");
    ui::paintPresetBar (g, kPal, proc.presets, proc.undoManager, isOn ("power"));
    ui::paintFooter (g, kPal, kH - 15.0f);

    const int m = mode();
    ui::paintPanel (g, kPal, panelRect (0), "CONTROLS");
    ui::paintTabs (g, kPal, panelRect (0), kModeTabs, m, kTabsTop, kTabsFont, kTabsLeft);
    ui::paintPanel (g, kPal, panelRect (1), "OUTPUT");
    {   // EXT lamp: lit while the host's sidechain input is the key of DUCK
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
    ui::paintPanel (g, kPal, panelRect (2), "TAIL");
    drawTail (g);

    if (m == 0) ui::paintFlatButton (g, kPal, typeChipRect(), juce::String (rb1::classicLongName (variant())) + "  v   (12 TYPES)", true);
    else if (m == 1) for (int v = 0; v < sp1::kNumModes; ++v) ui::paintFlatButton (g, kPal, buttonRect (v), sp1::modeName (v), v == variant());
    else
    {
        for (int v = 0; v < sr1::kNumTextures; ++v) ui::paintFlatButton (g, kPal, textureRect (v), sr1::textureName (v), v == variant());
        ui::paintFlatButton (g, kPal, rootRect(), "ROOT " + juce::String (sr1::noteName (choice ("root"))) + " v", false);
        ui::paintFlatButton (g, kPal, scaleRect(), juce::String (sr1::scaleName (choice ("scale"))) + " v", false);
    }
    ui::paintFlatButton (g, kPal, syncRect(), "SYNC", preSyncOn);
    ui::paintFlatButton (g, kPal, freezeRect(), "FREEZE", isOn ("freeze"));

    static const char* lo[5] = { "0", "0", "-24", "-50", "1" }, *hi[5] = { "100", "250", "+12", "+50", "12" };
    for (int i = 0; i < (int) knobs.size(); ++i)
    {
        auto& k = knobs[(size_t) i];
        if (! knobShown (i)) continue;
        const double v = k.slider.getValue();
        juce::String txt;
        switch (k.kind)
        {
            case 1: txt = juce::String ((int) std::lround (v)) + " ms"; break;
            case 2: txt = juce::String::formatted ("%+.1f dB", v); break;
            case 3: txt = juce::String::formatted ("%+d CENTS", (int) std::lround (v - 50.0)); break;
            case 4: txt = juce::String (sr1::stringCount ((float) v * 0.01f)) + " STRINGS"; break;
            default: txt = juce::String ((int) std::lround (v)) + " %"; break;
        }
        juce::String name = k.name, lowText = lo[k.kind], highText = hi[k.kind];
        const juce::String kid (k.id);
        if (k.onlyMode == 1 && kid == "texture") name = sp1::textureName (variant());
        else if (k.onlyMode == 0 && kid == "size") { name = classicP1Name (variant()); txt = classicP1Text (variant(), v); }        // CLASSIC: each type names its two knobs
        else if (k.onlyMode == 0 && kid == "param2") { name = classicP2Name (variant()); txt = classicP2Text (variant(), v); }
        else if (kid == "preDelay" && preSyncOn) { txt = vibr::sync::divisionNames()[juce::jlimit (0, vibr::sync::kNumDivisions - 1, (int) std::lround (v))]; lowText = "LONG"; highText = "SHORT"; }
        ui::paintKnob (g, kPal, knobCentre (k.panel, k.index, k.count), name, txt, lowText, highText, (float) k.slider.valueToProportionOfLength (v));
    }
    ui::paintMeter (g, kPal, meter, meterOrigin().x, meterOrigin().y, isOn ("power"));
}

void RB1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / scale();
    proc.undoManager.beginNewTransaction();
    if (ui::powerRect().contains (p)) { toggle ("power"); return; }
    if (ui::presetBarMouseDown (p, *this, proc.presets, proc.undoManager, scale(), [this] { repaint(); })) { layoutKnobs(); return; }
    const auto tabs = ui::tabRects (panelRect (0), kModeTabs, kTabsTop, kTabsFont, kTabsLeft);
    for (int i = 0; i < (int) tabs.size(); ++i)
        if (tabs[(size_t) i].expanded (3.0f).contains (p)) { setChoice ("mode", i); setChoice ("variant", 0); layoutKnobs(); repaint(); return; }
    if (syncRect().expanded (3.0f).contains (p)) { toggle ("preSync"); repaint(); return; }
    if (freezeRect().expanded (3.0f).contains (p)) { toggle ("freeze"); repaint(); return; }
    if (mode() == 0)
    {
        if (typeChipRect().expanded (3.0f).contains (p))
        {
            juce::StringArray names; for (int i = 0; i < bigsky::kNumTypes; ++i) names.add (rb1::classicLongName (i));
            showMenu ("variant", names, variant(), typeChipRect());
        }
    }
    else if (mode() == 1)
    {
        for (int v = 0; v < sp1::kNumModes; ++v) if (buttonRect (v).expanded (3.0f).contains (p)) { setChoice ("variant", v); repaint(); return; }
    }
    else
    {
        for (int v = 0; v < sr1::kNumTextures; ++v) if (textureRect (v).expanded (3.0f).contains (p)) { setChoice ("variant", v); repaint(); return; }
        if (rootRect().expanded (3.0f).contains (p)) { juce::StringArray n; for (int i = 0; i < 12; ++i) n.add (sr1::noteName (i)); showMenu ("root", n, choice ("root"), rootRect()); return; }
        if (scaleRect().expanded (3.0f).contains (p)) { juce::StringArray n; for (int i = 0; i < sr1::kNumScales; ++i) n.add (sr1::scaleName (i)); showMenu ("scale", n, choice ("scale"), scaleRect()); return; }
    }
}
