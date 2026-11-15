// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "ui/Presets.h"

namespace
{
using vibr::ui::drawText;
namespace ui = vibr::ui;

// layout (1200 x 690, style.md): 20 side margin, 10 between side-by-side panels, 24 between the rows, panels 30 below the rule (134), 69 above the bottom
constexpr float kW = 1200.0f, kH = 690.0f, kMargin = 20.0f, kPad = 5.0f, kBarY = 39.0f;
constexpr float kKnobR = 23.0f;                                                                    // three knobs in a 282.5 wide VOICE panel: 23 keeps the ticks 6 apart
constexpr float kRow1Y = 134.0f, kRow1H = 247.0f, kRow2Y = 405.0f, kRow2H = 216.0f;

const ui::Palette kPal { juce::Colour (96, 190, 168), juce::Colour (10, 36, 30) };                 // the Phantom colour #60BEA8 and its print

// panels: 0..3 VOICE 1..4 (282.5 wide, 10 apart), 4 KEY, 5 CHORDS, 6 OUTPUT (row 2: 370 + 370 + 400 + two gaps of 10 = the 1160 of row 1)
juce::Rectangle<float> panelRect (int i)
{
    if (i < 4) return { kMargin + 292.5f * (float) i, kRow1Y, 282.5f, kRow1H };
    static const float x[3] = { 20.0f, 400.0f, 780.0f }, w[3] = { 370.0f, 370.0f, 400.0f };
    return { x[i - 4], kRow2Y, w[i - 4], kRow2H };
}
juce::Rectangle<float> voiceRect (int v) { return panelRect (v); }
juce::Rectangle<float> keyRect() { return panelRect (4); }
juce::Rectangle<float> chordsRect() { return panelRect (5); }
juce::Rectangle<float> outputRect() { return panelRect (6); }
// KEY: knobs x + 61 / x + 151 (ticks 20.5 from the left border), INTELLIGENT 124 wide with 20 to the right border
juce::Rectangle<float> intelRect() { const auto r = keyRect(); return juce::Rectangle<float> (124.0f, 40.0f).withCentre ({ r.getRight() - 20.0f - 62.0f, r.getY() + 92.0f }); }
// CHORDS: three buttons over two, 100 x 36, 110 apart, centred in the panel
juce::Rectangle<float> chordRect (int c)
{
    const auto r = chordsRect();
    const float cx = c < 3 ? r.getCentreX() + 110.0f * (float) (c - 1) : r.getCentreX() + 55.0f * (float) (c == 3 ? -1 : 1);
    return juce::Rectangle<float> (100.0f, 36.0f).withCentre ({ cx, r.getY() + (c < 3 ? 64.0f : 110.0f) });
}
// knob column k of n in a VOICE panel (the ticks reach 40.5 each side: 13.5 clear of the panel edge, 6.25 between neighbours)
float voiceColX (int v, int k) { const auto r = voiceRect (v); return r.getX() + 54.0f + (r.getWidth() - 108.0f) * (float) k * 0.5f; }
juce::Point<float> meterOrigin() { const auto r = outputRect(); return { r.getX() + 310.0f, r.getY() + 46.0f }; }       // bars x + 310 .. x + 352, the scale labels to about x + 379: 20.5 left of the first knob's ticks and 21 right of the labels; 110 high: the bars end on the SPREAD / MIX / LEVEL names (y + 154), the peak values sit on the readout row (y + 182)
} // namespace

//==============================================================================
PT1AudioProcessorEditor::PT1AudioProcessorEditor (PT1AudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      powerAtt (*p.apvts.getParameter ("power"), [this] (float) { repaint(); }, nullptr),
      intelAtt (*p.apvts.getParameter ("intel"), [this] (float) { repaint(); }, nullptr),
      rootAtt  (*p.apvts.getParameter ("root"),  [this] (float) { repaint(); }, nullptr),
      scaleAtt (*p.apvts.getParameter ("scale"), [this] (float) { repaint(); }, nullptr)
{
    setLookAndFeel (&laf);
    const auto add = [this] (const juce::String& id, int kind, float cx, float cy, float nameY, float valueY, const char* name)
    {
        auto k = std::make_unique<Knob>();
        k->id = id; k->kind = kind; k->cx = cx; k->cy = cy; k->nameY = nameY; k->valueY = valueY; k->name = name;
        auto& s = k->slider;
        ui::setupKnob (s, *this);
        s.setTextValueSuffix (kind == 6 ? " dB" : " %");
        auto* par = proc.apvts.getParameter (id);
        s.setDoubleClickReturnValue (true, par->convertFrom0to1 (par->getDefaultValue()));
        s.onDragStart = [this] { proc.undoManager.beginNewTransaction(); };
        addAndMakeVisible (s);
        k->att = std::make_unique<juce::SliderParameterAttachment> (*dynamic_cast<juce::RangedAudioParameter*> (par), s, nullptr);
        s.onValueChange = [this] { repaint(); };
        midiUI.add (s, id);
        knobs.push_back (std::move (k));
    };
    for (int v = 0; v < pt1::kNumVoices; ++v)
    {
        const float cy = kRow1Y + 114.0f;
        const juce::String n (v + 1);
        add ("int" + n, 0, voiceColX (v, 0), cy, cy + 64.0f, cy + 86.0f, "INTERVAL");
        add ("det" + n, 1, voiceColX (v, 1), cy, cy + 64.0f, cy + 86.0f, "DETUNE");
        add ("lvl" + n, 2, voiceColX (v, 2), cy, cy + 64.0f, cy + 86.0f, "LEVEL");
    }
    const float cy2 = kRow2Y + 92.0f, n2 = kRow2Y + 154.0f, v2 = kRow2Y + 182.0f;
    add ("root",  3, keyRect().getX() + 61.0f,  cy2, n2, v2, "KEY");     add ("scale", 4, keyRect().getX() + 151.0f, cy2, n2, v2, "SCALE");
    add ("spread", 5, outputRect().getX() + 61.0f, cy2, n2, v2, "SPREAD");  add ("mix", 5, outputRect().getX() + 151.0f, cy2, n2, v2, "MIX");  add ("level", 6, outputRect().getX() + 241.0f, cy2, n2, v2, "LEVEL");
    powerAtt.sendInitialUpdate(); intelAtt.sendInitialUpdate(); rootAtt.sendInitialUpdate(); scaleAtt.sendInitialUpdate();

    grain = ui::makeGrain();
    constrainer.setSizeLimits (720, 414, 2400, 1380);
    constrainer.setFixedAspectRatio ((double) kW / (double) kH);
    setConstrainer (&constrainer);
    setResizable (true, true);
    setSize ((int) kW, (int) kH);
    layoutKnobs();
    startTimerHz (30);
}

PT1AudioProcessorEditor::~PT1AudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void PT1AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    midiUI.paintMarks (g, kPal.body);
}

void PT1AudioProcessorEditor::timerCallback()
{
    const auto px = [this] (juce::Rectangle<float> r) { return r.transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer(); };
    meter.update (proc.outMeter);
    repaint (px (outputRect().withTrimmedLeft (outputRect().getWidth() - 100.0f).expanded (2.0f)));
    const auto& eng = proc.getEngine();
    for (int v = 0; v < pt1::kNumVoices; ++v)                                                      // the played and the heard notes: repaint only on a change
        if (const float n = eng.voiceNote[v].load (std::memory_order_relaxed); n != shownNote[v]) { shownNote[v] = n; repaint (px (voiceRect (v).expanded (2.0f))); }
    if (const float h = eng.detected.load (std::memory_order_relaxed); h != shownHeard) { shownHeard = h; repaint (px (keyRect().expanded (2.0f))); }
    if (midiUI.needsRepaint()) { midiUI.tick(); repaint(); }
    if (barState.copiedAt != 0) { repaint (px (juce::Rectangle<float> (0.0f, 30.0f, 900.0f, 50.0f))); if (! barState.copiedRecently()) barState.copiedAt = 0; }
}

void PT1AudioProcessorEditor::toggle (const char* id)
{
    auto* par = proc.apvts.getParameter (id);
    par->beginChangeGesture();
    par->setValueNotifyingHost (par->getValue() > 0.5f ? 0.0f : 1.0f);
    par->endChangeGesture();
}

void PT1AudioProcessorEditor::layoutKnobs()
{
    const float sc = scale();
    for (auto& k : knobs)
    {
        const float d = 2.0f * (kKnobR + kPad) * sc;
        k->slider.setBounds (juce::Rectangle<float> (d, d).withCentre (juce::Point<float> (k->cx, k->cy) * sc).toNearestInt());
    }
}

void PT1AudioProcessorEditor::resized() { layoutKnobs(); }

juce::String PT1AudioProcessorEditor::knobText (int kind, double v) const
{
    const float f = (float) v * 0.01f;
    switch (kind)
    {
        case 0:
        {
            static const char* names[13] = { "UNISON", "m2", "2nd", "m3", "3rd", "4th", "b5", "5th", "m6", "6th", "b7", "7th", "OCTAVE" };
            const int s = pt1::intervalSemis (f);
            return std::abs (s) <= 12 ? (s == 0 ? juce::String (names[0]) : (s > 0 ? "+" : "-") + juce::String (names[std::abs (s)])) : juce::String::formatted ("%+d st", s);
        }
        case 1: return juce::String::formatted ("%+d ct", (int) std::lround (pt1::detuneCents (f)));
        case 3: return pt1::noteName (choice ("root"));
        case 4: return pt1::scaleName (choice ("scale"));
        case 6: return juce::String::formatted ("%+d dB", (int) std::lround (v));
        default: return juce::String ((int) std::lround (v)) + " %";
    }
}

//==============================================================================
void PT1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    const bool power = isOn ("power");
    ui::paintBody (g, kPal, kW, kH, grain);
    ui::paintHeader (g, kPal, "PT-1", "HARMONIZER", "PHANTOM VOICES", power);
    ui::paintPresetBar (g, kPal, proc.presets, proc.undoManager, kBarY, barState, false, 6.0f);

    for (int v = 0; v < pt1::kNumVoices; ++v) ui::paintPanel (g, kPal, voiceRect (v), "VOICE " + juce::String (v + 1));
    ui::paintPanel (g, kPal, keyRect(), "KEY");
    ui::paintPanel (g, kPal, chordsRect(), "CHORDS");
    ui::paintPanel (g, kPal, outputRect(), "OUTPUT");

    const auto& eng = proc.getEngine();
    const auto midiName = [] (float m) { return m < 0.0f ? juce::String ("--") : juce::String (pt1::noteName ((int) std::lround (m))) + juce::String ((int) std::lround (m) / 12 - 1); };
    const bool intel = isOn ("intel");
    for (int v = 0; v < pt1::kNumVoices; ++v)                                                     // the note each voice plays now
    {
        const float n = eng.voiceNote[v].load (std::memory_order_relaxed);
        drawText (g, midiName (n), voiceRect (v).getCentreX(), kRow1Y + 50.0f, 30.0f, n < 0.0f ? kPal.ink.withAlpha (0.45f) : kPal.body, true, 0.06f);
    }
    ui::paintFlatButton (g, kPal, intelRect(), "INTELLIGENT", intel, 13.0f, 1.0f, 10.0f, 0.06f);
    drawText (g, "HEARD: " + midiName (eng.detected.load (std::memory_order_relaxed)), intelRect().getCentreX(), kRow2Y + 154.0f, 14.0f, kPal.ink.withAlpha (0.85f), true, 0.08f);
    for (int c = 0; c < pt1::kNumChords; ++c)
    {
        // a chord button is "on" while the four interval knobs still match its chord
        bool on = true;
        for (int v = 0; v < pt1::kNumVoices && on; ++v)
            on = std::abs (proc.apvts.getParameter ("int" + juce::String (v + 1))->getValue() - pt1::intervalValue (pt1::chordSemis (c, v))) < 0.0075f;
        ui::paintFlatButton (g, kPal, chordRect (c), pt1::chordName (c), on, 13.0f, 1.0f, 10.0f, 0.06f);
    }
    drawText (g, "SETS THE FOUR INTERVALS", chordsRect().getCentreX(), kRow2Y + 160.0f, 14.0f, kPal.ink.withAlpha (0.75f), true, 0.1f);

    for (auto& kp : knobs)                                                                         // knobs: recessed ring, 11 ticks, legends, name, value and the value ring
    {
        Knob& k = *kp;
        const char* lo = k.kind == 0 ? "-24" : k.kind == 1 ? "-50" : k.kind == 6 ? "-24" : k.kind == 3 ? "C" : k.kind == 4 ? "" : "0";
        const char* hi = k.kind == 0 ? "+24" : k.kind == 1 ? "+50" : k.kind == 6 ? "+12" : k.kind == 3 ? "B" : k.kind == 4 ? "" : "100";
        const juce::Point<float> c (k.cx, k.cy);
        ui::paintKnob (g, kPal, c, k.name, knobText (k.kind, k.slider.getValue()), lo, hi, k.nameY, k.valueY, 1.0f, kKnobR);
        ui::paintValueArc (g, c, kKnobR, (float) k.slider.valueToProportionOfLength (k.slider.getValue()), kPal.body);
    }
    meter.paint (g, meterOrigin().x, meterOrigin().y, power, kPal.body, kPal.ink, 110.0f, 26.0f, 13.0f);
    ui::paintFooter (g, kPal, kRow2Y + kRow2H + 35.0f);
}

void PT1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / scale();
    proc.undoManager.beginNewTransaction();
    if (ui::powerRect().contains (p)) { toggle ("power"); return; }
    if (ui::presetBarMouseDown (p, *this, proc.presets, proc.undoManager, kBarY, scale(), barState, [this] { repaint(); }, false)) { layoutKnobs(); return; }
    if (intelRect().expanded (3.0f).contains (p)) { toggle ("intel"); return; }
    for (int c = 0; c < pt1::kNumChords; ++c)
        if (chordRect (c).expanded (3.0f).contains (p))
        {
            for (int v = 0; v < pt1::kNumVoices; ++v)
            {
                auto* par = proc.apvts.getParameter ("int" + juce::String (v + 1));
                par->beginChangeGesture();
                par->setValueNotifyingHost (pt1::intervalValue (pt1::chordSemis (c, v)));
                par->endChangeGesture();
            }
            return;
        }
}
