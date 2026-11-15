// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "ui/Presets.h"
#include "ui/OutputMeter.h"

namespace
{
using vibr::ui::drawText;
namespace ui = vibr::ui;

constexpr float kW = 1200.0f, kH = 690.0f, kKnobR = 24.0f, kPad = 5.0f, kBarY = 39.0f;
// the Harmonic colour #7A6EE8 with a dark violet print: the light print (244, 246, 250) only reaches 3.7 : 1 on this body, this one 4.8 : 1
const ui::Palette kPal { juce::Colour (122, 110, 232), juce::Colour (14, 12, 33) };

// panels: 0 CHARACTER (row 2, full width), 1 HARMONICS, 2 FREQUENCY, 3 OUTPUT (row 1: 20 .. 466, 476 .. 810, 820 .. 1180, 10 apart)
constexpr float kRow1Y = 134.0f, kRow1H = 247.0f, kRow2Y = 405.0f, kRow2H = 216.0f;
constexpr float kPanelX[4] = { 20.0f, 20.0f, 476.0f, 820.0f }, kPanelW[4] = { 1160.0f, 446.0f, 334.0f, 360.0f };
juce::Rectangle<float> panelRect (int i) { return { kPanelX[i], i == 0 ? kRow2Y : kRow1Y, kPanelW[i], i == 0 ? kRow2H : kRow1H }; }
constexpr const char* kPanelName[4] = { "CHARACTER", "HARMONICS", "FREQUENCY", "OUTPUT" };

juce::Point<float> knobCentre (int panel, int k, int n)
{
    const auto r = panelRect (panel);
    if (panel == 3) return { r.getX() + 61.0f + 120.0f * (float) k, r.getY() + 92.0f };          // the output knobs sit left (centres x + 61 / x + 181), the dB meters on the right
    return { r.getCentreX() + 112.0f * ((float) k - 0.5f * (float) (n - 1)), r.getY() + 92.0f };   // 112 apart, centred: 55 from the panel edge to the first / last centre
}
// bars x + 273 .. x + 315, the scale labels to about x + 342: 19 px padding left of the first knob's ticks and 18 right of the labels; 110 high: the bars end on the MIX / LEVEL names (y + 154 .. 156), the peak values sit on the readout row (y + 182)
juce::Point<float> meterOrigin() { const auto r = panelRect (3); return { r.getX() + 273.0f, r.getY() + 46.0f }; }
// CHARACTER: a 2 x 2 block of buttons (165 x 50, 10 apart) at x + 30 and the harmonic display (x + 410 .. x + 1130), both y + 50 .. y + 160
juce::Rectangle<float> charRect (int m) { const auto r = panelRect (0); return { r.getX() + 30.0f + (float) (m % 2) * 175.0f, r.getY() + 50.0f + (float) (m / 2) * 60.0f, 165.0f, 50.0f }; }
juce::Rectangle<float> specRect() { const auto r = panelRect (0); return { r.getX() + 410.0f, r.getY() + 50.0f, 720.0f, 110.0f }; }
}

//==============================================================================
HM2AudioProcessorEditor::HM2AudioProcessorEditor (HM2AudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      knobs { { { "drive", 1, 0, 4, "DRIVE", 0, {}, {} },      { "oddeven", 1, 1, 4, "ODD/EVEN", 3, {}, {} },
                { "octave", 1, 2, 4, "OCTAVE", 0, {}, {} },     { "sub", 1, 3, 4, "SUB", 0, {}, {} },
                { "focus", 2, 0, 3, "FOCUS", 1, {}, {} },       { "tilt", 2, 1, 3, "TILT", 4, {}, {} },
                { "tone", 2, 2, 3, "TONE", 0, {}, {} },
                { "mix", 3, 0, 2, "MIX", 0, {}, {} },           { "level", 3, 1, 2, "LEVEL", 2, {}, {} } } },
      powerAtt  (*p.apvts.getParameter ("power"),  [this] (float) { repaint(); }, nullptr),
      charAtt   (*p.apvts.getParameter ("character"), [this] (float) { repaint(); }, nullptr)
{
    setLookAndFeel (&laf);
    for (auto& k : knobs)
    {
        ui::setupKnob (k.slider, *this);
        auto* par = proc.apvts.getParameter (k.id);
        k.slider.setTextValueSuffix (k.kind == 2 ? " dB" : " %");
        k.slider.setDoubleClickReturnValue (true, par->convertFrom0to1 (par->getDefaultValue()));
        k.slider.onDragStart = [this] { proc.undoManager.beginNewTransaction(); };
        addAndMakeVisible (k.slider);
        k.att = std::make_unique<juce::SliderParameterAttachment> (*dynamic_cast<juce::RangedAudioParameter*> (par), k.slider, nullptr);
        midiUI.add (k.slider, k.id);
        k.slider.onValueChange = [this] { repaint(); };
    }
    powerAtt.sendInitialUpdate();
    charAtt.sendInitialUpdate();
    grain = ui::makeGrain();

    constrainer.setSizeLimits (720, 414, 2400, 1380);
    constrainer.setFixedAspectRatio ((double) kW / (double) kH);
    setConstrainer (&constrainer);
    setResizable (true, true);
    setSize ((int) kW, (int) kH);
    layoutKnobs();
    startTimerHz (30);
}

HM2AudioProcessorEditor::~HM2AudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void HM2AudioProcessorEditor::timerCallback()
{
    meter.update (proc.outMeter);
    repaint (panelRect (3).withTrimmedLeft (230.0f).expanded (2.0f).transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer());
    if (midiUI.needsRepaint()) { midiUI.tick(); repaint(); }
    if (barState.copiedAt != 0) { repaint (juce::Rectangle<float> (0.0f, 30.0f, 900.0f, 50.0f).transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer()); if (! barState.copiedRecently()) barState.copiedAt = 0; }
}

void HM2AudioProcessorEditor::toggle (const char* id)
{
    auto* par = proc.apvts.getParameter (id);
    par->beginChangeGesture();
    par->setValueNotifyingHost (par->getValue() > 0.5f ? 0.0f : 1.0f);
    par->endChangeGesture();
}

void HM2AudioProcessorEditor::setChoice (const char* id, int index)
{
    auto* par = proc.apvts.getParameter (id);
    par->beginChangeGesture(); par->setValueNotifyingHost (par->convertTo0to1 ((float) index)); par->endChangeGesture();
}

void HM2AudioProcessorEditor::layoutKnobs()
{
    const float sc = scale(), d = 2.0f * (kKnobR + kPad) * sc;
    for (auto& k : knobs)
        k.slider.setBounds (juce::Rectangle<float> (d, d).withCentre (knobCentre (k.panel, k.index, k.count) * sc).toNearestInt());
}

void HM2AudioProcessorEditor::resized() { layoutKnobs(); }

//==============================================================================
void HM2AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    midiUI.paintMarks (g, kPal.body);
}

void HM2AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    ui::paintBody (g, kPal, kW, kH, grain);
    ui::paintHeader (g, kPal, "HM-2", "HARMONIC MACHINE", "HARMONIC COLOUR", isOn ("power"));
    ui::paintPresetBar (g, kPal, proc.presets, proc.undoManager, kBarY, barState, false, 6.0f);

    for (int i = 0; i < 4; ++i) ui::paintPanel (g, kPal, panelRect (i), kPanelName[i]);

    // CHARACTER panel: four saturator buttons and the harmonic display
    const int character = currentCharacter();
    for (int m = 0; m < hm2::kNumCharacters; ++m)
        ui::paintFlatButton (g, kPal, charRect (m), hm2::characterName (m), m == character, 15.0f, 1.0f, 10.0f);
    {
        hm2::Params hp;
        hp.character = character;
        hp.drive = (float) proc.apvts.getRawParameterValue ("drive")->load() * 0.01f;
        hp.oddEven = (float) proc.apvts.getRawParameterValue ("oddeven")->load() * 0.01f;
        float db[8];
        hm2::harmonicSpectrum (hp, db);
        const auto r = specRect();
        const auto oddCol = kPal.body.withHue (kPal.body.getHue() + 0.12f).brighter (0.2f);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (r.expanded (6.0f, 8.0f), 8.0f);
        const float cell = r.getWidth() / 8.0f;
        for (int k = 0; k < 8; ++k)
        {
            const float v = juce::jlimit (0.0f, 1.0f, (db[k] + 60.0f) / 60.0f);
            const bool even = (k % 2) == 0;                                      // H2, H4, H6, H8
            const auto bar = juce::Rectangle<float> (cell - 7.0f, juce::jmax (2.0f, v * r.getHeight())).withBottomY (r.getBottom()).withX (r.getX() + cell * (float) k + 3.5f);
            g.setColour (even ? kPal.body : oddCol);
            g.fillRoundedRectangle (bar, 2.5f);
            drawText (g, "H" + juce::String (k + 2), bar.getCentreX(), r.getBottom() + 22.0f, 13.0f, kPal.ink.withAlpha (0.8f), true, 0.0f);
        }
        drawText (g, "HARMONIC SPECTRUM", r.getX() - 6.0f, r.getY() - 24.0f, 13.0f, kPal.ink.withAlpha (0.85f), true, 0.12f, juce::Justification::centredLeft);
        drawText (g, "ODD", r.getRight() + 6.0f, r.getY() - 24.0f, 13.0f, oddCol, true, 0.12f, juce::Justification::centredRight);
        drawText (g, "EVEN", r.getRight() - 40.0f, r.getY() - 24.0f, 13.0f, kPal.body, true, 0.12f, juce::Justification::centredRight);
    }

    // knobs: recessed ring, 11 ticks, legends, name and value (on the shared rows y + 154 / y + 182) and the value ring
    for (auto& k : knobs)
    {
        const auto c = knobCentre (k.panel, k.index, k.count);
        const double v = k.slider.getValue();
        juce::String txt;
        const char* lo = k.kind == 2 ? "-24" : k.kind == 1 || k.kind == 4 ? "LOW" : k.kind == 3 ? "EVEN" : "0";
        const char* hi = k.kind == 2 ? "+12" : k.kind == 1 || k.kind == 4 ? "HIGH" : k.kind == 3 ? "ODD" : "100";
        if (k.kind == 2) txt = juce::String::formatted ("%+d dB", (int) std::lround (v));
        else if (k.kind == 1) { const float hz = hm2::focusHz ((float) v * 0.01f); txt = hz >= 1000.0f ? juce::String (hz * 0.001f, 1) + " kHz" : juce::String ((int) std::lround (hz)) + " Hz"; }
        else if (k.kind == 3) txt = (int) std::lround (v) == 50 ? juce::String ("BOTH") : (v > 50.0 ? juce::String ((int) std::lround (v)) + " % ODD" : juce::String ((int) std::lround (100.0 - v)) + " % EVEN");
        else if (k.kind == 4) txt = (int) std::lround (v) == 50 ? juce::String ("FLAT") : (v > 50.0 ? "HIGH +" + juce::String ((int) std::lround ((v - 50.0) * 2.0)) : "LOW +" + juce::String ((int) std::lround ((50.0 - v) * 2.0)));
        else txt = juce::String ((int) std::lround (v)) + " %";
        ui::paintKnob (g, kPal, c, k.name, txt, lo, hi, panelRect (k.panel).getY() + 154.0f, panelRect (k.panel).getY() + 182.0f);
        ui::paintValueArc (g, c, kKnobR, (float) k.slider.valueToProportionOfLength (k.slider.getValue()), kPal.body);
    }
    meter.paint (g, meterOrigin().x, meterOrigin().y, isOn ("power"), kPal.body, kPal.ink, 110.0f, 26.0f, 13.0f);
    ui::paintFooter (g, kPal, kRow2Y + kRow2H + 35.0f);
}

void HM2AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / scale();
    proc.undoManager.beginNewTransaction();
    if (ui::powerRect().contains (p)) { toggle ("power"); return; }
    if (ui::presetBarMouseDown (p, *this, proc.presets, proc.undoManager, kBarY, scale(), barState, [this] { repaint(); }, false)) { layoutKnobs(); return; }
    for (int m = 0; m < hm2::kNumCharacters; ++m)
        if (charRect (m).expanded (3.0f).contains (p)) { setChoice ("character", m); repaint(); return; }
}
