// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "ui/Presets.h"

namespace
{
using vibr::ui::drawText;
namespace ui = vibr::ui;

constexpr float kW = 1200.0f, kH = 690.0f, kRow1Y = 134.0f, kRow1H = 252.0f, kRow2Y = 410.0f, kRow2H = 211.0f, kKnobR = 24.0f, kPad = 5.0f, kBarY = 39.0f;
const ui::Palette kPal { juce::Colour (186, 105, 232), juce::Colour (37, 21, 46) };            // the Reverb & Space colour #BA69E8 and its print

// panels: 0 CONTROLS (mode tabs), 1 OUTPUT, 2 TAIL (display)
constexpr float kPanelX[3] = { 20.0f, 697.0f, 20.0f }, kPanelW[3] = { 667.0f, 483.0f, 1160.0f };
constexpr float kOutPad = 20.0f, kCtlW = 622.0f, kBtnW = 118.0f, kBtnStep = 126.0f;          // OUTPUT: group (4 knobs 90 apart + 20 + meters) 443 wide, 20 padding each side; CONTROLS: button rows 622 wide, centred
juce::Rectangle<float> panelRect (int i) { return { kPanelX[i], i < 2 ? kRow1Y : kRow2Y, kPanelW[i], i < 2 ? kRow1H : kRow2H }; }
juce::Point<float> knobCentre (int panel, int k, int n)
{
    const auto r = panelRect (panel);
    const float pitch = panel == 1 ? 90.0f : n >= 7 ? 90.0f : 108.0f;
    const float cx = panel == 1 ? r.getX() + kOutPad + 42.0f + 135.0f : r.getCentreX();          // the output knobs left of the dB meters, the pair centred in the panel
    return { cx + pitch * ((float) k - 0.5f * (float) (n - 1)), r.getY() + 92.0f };
}
juce::Point<float> meterOrigin() { const auto r = panelRect (1); return { r.getX() + kOutPad + 374.0f, r.getY() + 46.0f }; }       // 110 high: the bars end on the knob name row (y + 156), the peak values sit on the readout row (y + 182)
juce::Rectangle<float> buttonRect (int i, float w = kBtnW, float step = kBtnStep) { const auto r = panelRect (0); return { r.getX() + 0.5f * (r.getWidth() - kCtlW) + (float) i * step, r.getY() + 204.0f, w, 30.0f }; }
juce::Rectangle<float> typeChipRect() { return buttonRect (0, kCtlW); }
juce::Rectangle<float> textureRect (int i) { return buttonRect (i); }
juce::Rectangle<float> rootRect() { return buttonRect (3); }
juce::Rectangle<float> scaleRect() { return buttonRect (4); }
juce::Rectangle<float> freezeRect() { const auto r = panelRect (1); return { r.getX() + kOutPad, r.getY() + 204.0f, r.getWidth() - 2.0f * kOutPad, 30.0f }; }
juce::Rectangle<float> plotRect() { const auto r = panelRect (2); return { r.getX() + 56.0f, r.getY() + 38.0f, r.getWidth() - 112.0f, r.getHeight() - 78.0f }; }
const std::vector<juce::String> kModeTabs { "CLASSIC", "SPACE", "RESONATE" };
}

// kinds: 0 percent, 1 ms, 2 level dB, 3 cents, 4 strings
//==============================================================================
RB1AudioProcessorEditor::RB1AudioProcessorEditor (RB1AudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      knobs { { { "decay", 0, 0, 6, "DECAY", 0, 0, {}, {} },        { "size", 0, 1, 6, "PARAM 1", 0, 0, {}, {} },       { "param2", 0, 2, 6, "PARAM 2", 0, 0, {}, {} },
                { "tone", 0, 3, 6, "TONE", 0, 0, {}, {} },          { "motion", 0, 4, 6, "MOTION", 0, 0, {}, {} },      { "value", 0, 5, 6, "VALUE", 0, 0, {}, {} },
                { "decay", 0, 0, 6, "DECAY", 0, 1, {}, {} },        { "size", 0, 1, 6, "SIZE", 0, 1, {}, {} },          { "diffusion", 0, 2, 6, "DIFFUSION", 0, 1, {}, {} },
                { "tone", 0, 3, 6, "TONE", 0, 1, {}, {} },          { "motion", 0, 4, 6, "MOTION", 0, 1, {}, {} },      { "texture", 0, 5, 6, "TEXTURE", 0, 1, {}, {} },
                { "decay", 0, 0, 7, "DECAY", 0, 2, {}, {} },        { "size", 0, 1, 7, "STRINGS", 4, 2, {}, {} },       { "damping", 0, 2, 7, "DAMPING", 0, 2, {}, {} },
                { "tension", 0, 3, 7, "TENSION", 0, 2, {}, {} },    { "motion", 0, 4, 7, "MOTION", 0, 2, {}, {} },      { "fine", 0, 5, 7, "FINE", 3, 2, {}, {} },
                { "blend", 0, 6, 7, "BLEND", 0, 2, {}, {} },
                { "preDelay", 1, 0, 4, "PRE-DELAY", 1, -1, {}, {} },{ "duck", 1, 1, 4, "DUCK", 0, -1, {}, {} },        { "mix", 1, 2, 4, "MIX", 0, -1, {}, {} },
                { "level", 1, 3, 4, "LEVEL", 2, -1, {}, {} } } },
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

RB1AudioProcessorEditor::~RB1AudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void RB1AudioProcessorEditor::timerCallback()
{
    meter.update (proc.outMeter);
    if (const int shown = mode() * 8 + variant(); shown != shownKey) { shownKey = shown; layoutKnobs(); repaint(); }       // a preset, undo, A / B or automation changed the tab
    repaint (panelRect (1).withTrimmedLeft (panelRect (1).getWidth() - 150.0f).expanded (2.0f).transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer());
    if (midiUI.needsRepaint()) { midiUI.tick(); repaint(); }
    if (proc.extKey.load (std::memory_order_relaxed) != extShown) { extShown = proc.extKey.load(); repaint (panelRect (1).expanded (4.0f).transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer()); }
    repaint (panelRect (2).expanded (4.0f).transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer());
    if (barState.copiedAt != 0) { repaint (juce::Rectangle<float> (0.0f, 30.0f, 900.0f, 50.0f).transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer()); if (! barState.copiedRecently()) barState.copiedAt = 0; }
}

void RB1AudioProcessorEditor::toggle (const char* id)
{
    auto* par = proc.apvts.getParameter (id);
    par->beginChangeGesture(); par->setValueNotifyingHost (par->getValue() > 0.5f ? 0.0f : 1.0f); par->endChangeGesture();
}
void RB1AudioProcessorEditor::setChoice (const char* id, int index)
{
    auto* par = proc.apvts.getParameter (id);
    par->beginChangeGesture(); par->setValueNotifyingHost (par->convertTo0to1 ((float) index)); par->endChangeGesture();
}

void RB1AudioProcessorEditor::layoutKnobs()
{
    const float sc = scale(), d = 2.0f * (kKnobR + kPad) * sc;
    for (int i = 0; i < (int) knobs.size(); ++i)
    {
        auto& k = knobs[(size_t) i];
        k.slider.setBounds (juce::Rectangle<float> (d, d).withCentre (knobCentre (k.panel, k.index, k.count) * sc).toNearestInt());
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
    const auto r = plotRect();
    const auto& eng = proc.getEngine();
    const bool power = isOn ("power");
    g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (r.expanded (8.0f), 10.0f);
    if (mode() == 2)
    {
        const int n = sr1::stringCount (value ("size") * 0.01f), root = choice ("root"), scale = choice ("scale");
        const float cw = r.getWidth() / (float) sr1::kMaxStrings;
        for (int i = 0; i < sr1::kMaxStrings; ++i)
        {
            const float e = power && i < n ? std::clamp (eng.resonator().energy[i].load (std::memory_order_relaxed), 0.0f, 1.0f) : 0.0f, cx = r.getX() + (float) i * cw + 0.5f * cw;
            const bool active = i < n;
            g.setColour (kPal.ink.withAlpha (active ? 0.2f : 0.08f));
            g.drawLine (cx, r.getY() + 6.0f, cx, r.getBottom() - 26.0f, 1.2f);
            if (active)
            {
                const float h = (r.getHeight() - 40.0f) * std::sqrt (e);
                g.setColour (kPal.body.withAlpha (0.35f + 0.65f * e));
                g.fillRoundedRectangle (juce::Rectangle<float> (7.0f, std::max (4.0f, h)).withCentre ({ cx, 0.0f }).withBottomY (r.getBottom() - 26.0f), 3.0f);
            }
            drawText (g, active ? juce::String (sr1::noteName (sr1::stringNote (i, root, scale))) : juce::String ("-"), cx, r.getBottom() - 10.0f, 13.0f, kPal.ink.withAlpha (active ? 0.75f : 0.25f), true, 0.1f);
        }
        return;
    }
    const auto yOf = [&] (float db) { return r.getBottom() - r.getHeight() * (std::clamp (db, -60.0f, 0.0f) + 60.0f) / 60.0f; };
    for (float db = -12.0f; db >= -60.0f; db -= 12.0f)
    {
        g.setColour (kPal.ink.withAlpha (0.1f)); g.drawLine (r.getX(), yOf (db), r.getRight(), yOf (db), 1.0f);
        drawText (g, juce::String ((int) db), r.getX() - 20.0f, yOf (db), 13.0f, kPal.ink.withAlpha (0.5f), false, 0.0f);
    }
    juce::Path area;
    const int pos = eng.histPos.load (std::memory_order_acquire), N = rb1::kHistory;
    area.startNewSubPath (r.getX(), r.getBottom());
    for (int i = 0; i < N; ++i)
    {
        const float x = r.getX() + r.getWidth() * (float) i / (float) (N - 1), db = power ? eng.histWet[(pos + i) % N].load (std::memory_order_relaxed) : -90.0f;
        area.lineTo (x, yOf (db));
    }
    area.lineTo (r.getRight(), r.getBottom()); area.closeSubPath();
    g.setColour (kPal.body.withAlpha (isOn ("freeze") ? 0.5f : 0.28f)); g.fillPath (area);
    g.setColour (kPal.body); g.strokePath (area, juce::PathStrokeType (1.6f));
    drawText (g, "WET LEVEL, LAST 5 SECONDS", r.getX() + 8.0f, r.getY() + 12.0f, 13.0f, kPal.ink.withAlpha (0.5f), true, 0.1f, juce::Justification::centredLeft);
    if (isOn ("freeze")) drawText (g, "FROZEN", r.getRight() - 10.0f, r.getY() + 12.0f, 13.0f, kPal.body, true, 0.2f, juce::Justification::centredRight);
    else if (mode() == 0)
        drawText (g, "RT60 ABOUT " + juce::String (bigsky::decaySeconds (variant(), value ("decay") * 0.01f) * 0.85f, 1) + " S", r.getRight() - 10.0f, r.getY() + 12.0f, 13.0f, kPal.ink.withAlpha (0.6f), true, 0.12f, juce::Justification::centredRight);
    if (eng.duckNow.load (std::memory_order_relaxed) < 0.95f && power)
        drawText (g, "DUCKING", r.getRight() - 10.0f, r.getY() + 30.0f, 13.0f, kPal.ink.withAlpha (0.6f), true, 0.12f, juce::Justification::centredRight);
}

//==============================================================================
void RB1AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    midiUI.paintMarks (g, kPal.body);
}

void RB1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    ui::paintBody (g, kPal, kW, kH, grain);
    ui::paintHeader (g, kPal, "RB-1", "REVERB LAB", "INFINITE SPACES", isOn ("power"));
    ui::paintPresetBar (g, kPal, proc.presets, proc.undoManager, kBarY, barState, false, 6.0f);

    const int m = mode();
    ui::paintPanel (g, kPal, panelRect (0), {});
    ui::paintTabs (g, kPal, panelRect (0), kModeTabs, m);
    ui::paintPanel (g, kPal, panelRect (1), "OUTPUT");
    {   // EXT lamp: lit while the host's sidechain input is the key of DUCK
        const auto r1 = panelRect (1);
        const bool ext = proc.extKey.load (std::memory_order_relaxed);
        const auto lamp = juce::Rectangle<float> (10.0f, 10.0f).withCentre ({ r1.getX() + kOutPad + 5.0f, r1.getY() + 22.0f });
        g.setColour (ext ? kPal.body : kPal.body.withAlpha (0.25f));
        g.fillEllipse (lamp);
        ui::drawText (g, "EXT", lamp.getRight() + 6.0f, lamp.getCentreY(), 13.0f, kPal.ink.withAlpha (ext ? 0.9f : 0.5f), true, 0.1f, juce::Justification::centredLeft);
    }
    ui::paintPanel (g, kPal, panelRect (2), "TAIL");
    drawTail (g);

    if (m == 0) ui::paintFlatButton (g, kPal, typeChipRect(), juce::String (rb1::classicLongName (variant())) + "  v   (12 TYPES)", true, 13.0f);
    else if (m == 1) for (int v = 0; v < sp1::kNumModes; ++v) ui::paintFlatButton (g, kPal, buttonRect (v), sp1::modeName (v), v == variant(), 13.0f);
    else
    {
        for (int v = 0; v < sr1::kNumTextures; ++v) ui::paintFlatButton (g, kPal, textureRect (v), sr1::textureName (v), v == variant(), 13.0f);
        ui::paintFlatButton (g, kPal, rootRect(), "ROOT " + juce::String (sr1::noteName (choice ("root"))) + " v", false, 13.0f);
        ui::paintFlatButton (g, kPal, scaleRect(), juce::String (sr1::scaleName (choice ("scale"))) + " v", false, 13.0f);
    }
    ui::paintFlatButton (g, kPal, freezeRect(), "FREEZE", isOn ("freeze"), 13.0f);

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
        ui::paintKnob (g, kPal, knobCentre (k.panel, k.index, k.count), (k.onlyMode == 1 && juce::String (k.id) == "texture") ? sp1::textureName (variant()) : k.name, txt, lo[k.kind], hi[k.kind], panelRect (k.panel).getY() + 154.0f, panelRect (k.panel).getY() + 182.0f);
        ui::paintValueArc (g, knobCentre (k.panel, k.index, k.count), kKnobR, (float) k.slider.valueToProportionOfLength (k.slider.getValue()), kPal.body);
    }
    meter.paint (g, meterOrigin().x, meterOrigin().y, isOn ("power"), kPal.body, kPal.ink, 110.0f, 26.0f, 13.0f);
    ui::paintFooter (g, kPal, kRow2Y + kRow2H + 35.0f);
}

void RB1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / scale();
    proc.undoManager.beginNewTransaction();
    if (ui::powerRect().contains (p)) { toggle ("power"); return; }
    if (ui::presetBarMouseDown (p, *this, proc.presets, proc.undoManager, kBarY, scale(), barState, [this] { repaint(); }, false)) { layoutKnobs(); return; }
    const auto tabs = ui::tabRects (panelRect (0), kModeTabs);
    for (int i = 0; i < (int) tabs.size(); ++i)
        if (tabs[(size_t) i].expanded (3.0f).contains (p)) { setChoice ("mode", i); setChoice ("variant", 0); layoutKnobs(); repaint(); return; }
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
