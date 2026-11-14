// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "ui/Presets.h"

namespace
{
using vibr::ui::drawText;
namespace ui = vibr::ui;

constexpr float kW = 1200.0f, kH = 690.0f, kRow1Y = 134.0f, kRow1H = 247.0f, kRow2Y = 405.0f, kRow2H = 216.0f, kKnobR = 24.0f, kPad = 5.0f, kBarY = 39.0f;
const ui::Palette kPal { juce::Colour (244, 197, 66), juce::Colour (44, 35, 12) };            // the Dynamics colour #F4C542 and its print

// panels: 0 CONTROLS (with the mode tabs), 1 SIDECHAIN, 2 ACTION (display), 3 OUTPUT
constexpr float kPanelX[4] = { 20.0f, 730.0f, 20.0f, 930.0f }, kPanelW[4] = { 700.0f, 450.0f, 900.0f, 250.0f };
juce::Rectangle<float> panelRect (int i) { return { kPanelX[i], i < 2 ? kRow1Y : kRow2Y, kPanelW[i], i < 2 ? kRow1H : kRow2H }; }
juce::Point<float> knobCentre (int panel, int k, int n)
{
    const auto r = panelRect (panel);
    const float pitch = panel == 0 ? (n >= 7 ? 94.0f : 108.0f) : 108.0f;
    return { r.getCentreX() + pitch * ((float) k - 0.5f * (float) (n - 1)), r.getY() + 92.0f };
}
juce::Rectangle<float> plotRect() { const auto r = panelRect (2); return { r.getX() + 24.0f, r.getY() + 34.0f, r.getWidth() - 48.0f, r.getHeight() - 74.0f }; }
// the buttons in the bottom row of CONTROLS: the stage switches of the shaper (3) or the types (2 or 3)
juce::Rectangle<float> buttonRect (int i) { const auto r = panelRect (0); return { r.getX() + 30.0f + (float) i * 130.0f, r.getY() + 204.0f, 120.0f, 30.0f }; }
juce::Rectangle<float> autoRect() { const auto r = panelRect (1); return { r.getCentreX() - 95.0f, r.getY() + 204.0f, 190.0f, 30.0f }; }
const std::vector<juce::String> kModeTabs { "SHAPER", "COMPRESSOR", "LIMITER" };
constexpr float kDbTop = 0.0f, kDbBottom = -60.0f, kGainTop = 12.0f, kGainBottom = -36.0f;
}

// kinds: 0 dB threshold, 1 ms (release), 2 ratio, 3 percent, 4 +- percent, 5 level dB, 6 sidechain high-pass, 7 ms attack, 8 make-up dB, 9 ceiling dB
//==============================================================================
DN1AudioProcessorEditor::DN1AudioProcessorEditor (DN1AudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
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
                { "mix", 3, 0, 2, "MIX", 3, -1, 7, {}, {} },               { "level", 3, 1, 2, "LEVEL", 5, -1, 7, {}, {} } } },
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

DN1AudioProcessorEditor::~DN1AudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void DN1AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    if (midiUI != nullptr) midiUI->paintMarks (g, kPal.body);
}

void DN1AudioProcessorEditor::timerCallback()
{
    if (midiUI != nullptr && midiUI->needsRepaint()) { midiUI->tick(); repaint(); }
    repaint (panelRect (2).expanded (4.0f).transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer());
    if (barState.copiedAt != 0) { repaint (juce::Rectangle<float> (0.0f, 30.0f, 900.0f, 50.0f).transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer()); if (! barState.copiedRecently()) barState.copiedAt = 0; }
}

void DN1AudioProcessorEditor::toggle (const char* id)
{
    auto* par = proc.apvts.getParameter (id);
    par->beginChangeGesture(); par->setValueNotifyingHost (par->getValue() > 0.5f ? 0.0f : 1.0f); par->endChangeGesture();
}
void DN1AudioProcessorEditor::setChoice (const char* id, int index)
{
    auto* par = proc.apvts.getParameter (id);
    par->beginChangeGesture(); par->setValueNotifyingHost (par->convertTo0to1 ((float) index)); par->endChangeGesture();
}

bool DN1AudioProcessorEditor::knobShown (int i) const
{
    const auto& k = knobs[(size_t) i];
    return (k.onlyMode < 0 || k.onlyMode == mode()) && (k.typeMask & (1 << (mode() == 0 ? 0 : variant()))) != 0;
}

void DN1AudioProcessorEditor::layoutKnobs()
{
    const float sc = scale(), d = 2.0f * (kKnobR + kPad) * sc;
    for (int i = 0; i < (int) knobs.size(); ++i)
    {
        auto& k = knobs[(size_t) i];
        k.slider.setBounds (juce::Rectangle<float> (d, d).withCentre (knobCentre (k.panel, k.index, k.count) * sc).toNearestInt());
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
// ACTION: five seconds of the input level (grey) and of the gain of the chain (body colour, hanging from the 0 dB line), with the thresholds of the mode dashed.
void DN1AudioProcessorEditor::drawAction (juce::Graphics& g)
{
    const auto r = plotRect();
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (r.expanded (8.0f), 10.0f);
    const auto yIn = [&] (float db) { return r.getBottom() - r.getHeight() * (juce::jlimit (kDbBottom, kDbTop, db) - kDbBottom) / (kDbTop - kDbBottom); };
    const auto yGain = [&] (float db) { return r.getY() + r.getHeight() * (kGainTop - juce::jlimit (kGainBottom, kGainTop, db)) / (kGainTop - kGainBottom); };
    for (float db = -12.0f; db >= kDbBottom; db -= 12.0f)
    {
        g.setColour (kPal.ink.withAlpha (0.1f));
        g.drawLine (r.getX(), yIn (db), r.getRight(), yIn (db), 1.0f);
        drawText (g, juce::String ((int) db), r.getX() - 14.0f, yIn (db), 10.0f, kPal.ink.withAlpha (0.5f), false, 0.0f);
    }
    const auto& eng = proc.getEngine();
    const bool power = isOn ("power");
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
    g.setColour (kPal.ink.withAlpha (0.22f)); g.fillPath (inPath);
    g.setColour (kPal.ink.withAlpha (0.5f)); g.strokePath (inPath, juce::PathStrokeType (1.2f));
    g.setColour (kPal.ink.withAlpha (0.35f)); g.drawLine (r.getX(), yGain (0.0f), r.getRight(), yGain (0.0f), 1.0f);
    g.setColour (kPal.body); g.strokePath (gainPath, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved));
    drawText (g, "GAIN", r.getRight() - 28.0f, r.getY() + 12.0f, 10.0f, kPal.body.withAlpha (0.8f), true, 0.1f);
    drawText (g, "INPUT", r.getRight() - 28.0f, r.getBottom() - 12.0f, 10.0f, kPal.ink.withAlpha (0.55f), true, 0.1f);

    // the thresholds of the mode (dashed)
    const auto dashed = [&] (float db, const juce::String& label)
    {
        const float y = yIn (db);
        g.setColour (kPal.body.withAlpha (0.55f));
        const float dash[] = { 5.0f, 4.0f };
        g.drawDashedLine (juce::Line<float> (r.getX(), y, r.getRight(), y), dash, 2, 1.1f);
        drawText (g, label, r.getX() + 6.0f, y - 8.0f, 9.0f, kPal.body.withAlpha (0.8f), true, 0.1f, juce::Justification::centredLeft);
    };
    const int m = mode();
    if (m == 0)
    {
        if (isOn ("gateOn")) dashed (value ("gateThresh"), "GATE");
        if (isOn ("expOn")) dashed (value ("expThresh"), "EXPANDER");
    }
    else if (m == 1) dashed (variant() == 0 ? dn1::cs3ThresholdDb (value ("compSustain") * 0.01f) : value ("compThresh"), "THRESHOLD");
    else dashed (value ("limThresh"), "THRESHOLD");

    const float gr = power ? eng.grNow.load (std::memory_order_relaxed) : 0.0f;
    drawText (g, (m == 0 ? juce::String ("GAIN OF THE CHAIN, LAST 5 SECONDS") : juce::String::formatted ("GAIN REDUCTION %.1f dB", (double) gr)) + "     LATENCY " + juce::String (eng.latency()) + " SAMPLES",
              r.getX() + 12.0f, r.getY() + r.getHeight() + 34.0f, 11.0f, kPal.ink.withAlpha (0.6f), true, 0.1f, juce::Justification::centredLeft);
}

//==============================================================================
void DN1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    ui::paintBody (g, kPal, kW, kH, grain);
    ui::paintHeader (g, kPal, "DN-1", "DYNAMICS LAB", "TOUCH AND CONTROL", isOn ("power"));
    ui::paintPresetBar (g, kPal, proc.presets, proc.undoManager, kBarY, barState, false);

    const int m = mode();
    ui::paintPanel (g, kPal, panelRect (0), {});
    ui::paintTabs (g, kPal, panelRect (0), kModeTabs, m);
    ui::paintPanel (g, kPal, panelRect (1), "SIDECHAIN");
    ui::paintPanel (g, kPal, panelRect (2), "ACTION");
    ui::paintPanel (g, kPal, panelRect (3), "OUTPUT");
    drawAction (g);

    if (m == 0)
    {
        static const char* labels[3] = { "GATE", "EXPANDER", "TRANSIENT" }, *ids[3] = { "gateOn", "expOn", "transOn" };
        for (int i = 0; i < 3; ++i) ui::paintFlatButton (g, kPal, buttonRect (i), labels[i], isOn (ids[i]), 11.0f);
    }
    else
        for (int v = 0; v < dn1::variantCount (m); ++v) ui::paintFlatButton (g, kPal, buttonRect (v), dn1::variantName (m, v), v == variant(), 11.0f);
    ui::paintFlatButton (g, kPal, autoRect(), "AUTO LEVEL", isOn ("autoLevel"), 11.0f);

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
        ui::paintKnob (g, kPal, c, k.name, txt, l, h, panelRect (k.panel).getY() + 154.0f, panelRect (k.panel).getY() + 182.0f);
    }
    // the output knobs (MIX, LEVEL) sit in the OUTPUT panel
    ui::paintFooter (g, kPal, kRow2Y + kRow2H + 35.0f);
}

void DN1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / scale();
    proc.undoManager.beginNewTransaction();
    if (ui::powerRect().contains (p)) { toggle ("power"); return; }
    if (ui::presetBarMouseDown (p, *this, proc.presets, proc.undoManager, kBarY, scale(), barState, [this] { repaint(); }, false)) { layoutKnobs(); return; }
    const auto tabs = ui::tabRects (panelRect (0), kModeTabs);
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
