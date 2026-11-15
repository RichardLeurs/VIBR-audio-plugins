// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "ui/Presets.h"

namespace
{
using vibr::ui::drawText;
namespace ui = vibr::ui;

// 1200 x 450: one row of two panels (20 side margin, 10 gap, panels from y 134, 69 below them)
constexpr float kW = 1200.0f, kH = 450.0f, kRowY = 134.0f, kRowH = 247.0f, kKnobR = 24.0f, kPad = 5.0f, kBarY = 39.0f;
const ui::Palette kPal { juce::Colour (177, 92, 214), juce::Colour (36, 12, 42) };            // the Space colour #B15CD6 and its print
const juce::Colour kIce (143, 211, 244);

// panels: 0 CONTROLS (mode tabs), 1 OUTPUT
constexpr float kPanelX[2] = { 20.0f, 730.0f }, kPanelW[2] = { 700.0f, 450.0f };
juce::Rectangle<float> panelRect (int i) { return { kPanelX[i], kRowY, kPanelW[i], kRowH }; }
juce::Point<float> knobCentre (int panel, int k, int n)
{
    const auto r = panelRect (panel);
    const float cx = panel == 1 ? r.getX() + 165.0f : r.getCentreX();          // the output knobs sit left (centres x + 105 / x + 225), the dB meters on the right
    return { cx + (panel == 0 ? 112.0f : 120.0f) * ((float) k - 0.5f * (float) (n - 1)), r.getY() + 92.0f };
}
juce::Point<float> meterOrigin() { const auto r = panelRect (1); return { r.getX() + 317.0f, r.getY() + 46.0f }; }       // bars x + 317 .. x + 359, scale labels to about x + 386; 110 high: the bars end on the MIX / LEVEL names (y + 156), the peak values sit on the readout row (y + 182)
juce::Rectangle<float> freezeRect() { const auto r = panelRect (0); return juce::Rectangle<float> (160.0f, 30.0f).withCentre ({ r.getCentreX(), r.getY() + 219.0f }); }
const std::vector<juce::String> kModeTabs { "SHIMMER", "INFINITE", "SPECTRAL", "MODULATED", "ROOMS" };
}

//==============================================================================
SP1AudioProcessorEditor::SP1AudioProcessorEditor (SP1AudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      knobs { { { "size", 1, 0, 3, "SIZE", false, {}, {} },        { "decay", 1, 1, 3, "DECAY", false, {}, {} },
                { "diffusion", 1, 2, 3, "DIFFUSION", false, {}, {} },
                { "tone", 2, 0, 3, "TONE", false, {}, {} },        { "motion", 2, 1, 3, "MOTION", false, {}, {} },
                { "texture", 2, 2, 3, nullptr, false, {}, {} },
                { "mix", 3, 0, 2, "MIX", false, {}, {} },          { "level", 3, 1, 2, "LEVEL", true, {}, {} } } },
      powerAtt  (*p.apvts.getParameter ("power"),  [this] (float) { repaint(); }, nullptr),
      modeAtt   (*p.apvts.getParameter ("mode"),   [this] (float) { repaint(); }, nullptr),
      freezeAtt (*p.apvts.getParameter ("freeze"), [this] (float) { repaint(); }, nullptr)
{
    setLookAndFeel (&laf);
    for (auto& k : knobs)
    {
        auto& s = k.slider;
        ui::setupKnob (s, *this);
        s.setTextValueSuffix (k.dB ? " dB" : " %");
        auto* par = proc.apvts.getParameter (k.id);
        s.setDoubleClickReturnValue (true, par->convertFrom0to1 (par->getDefaultValue()));
        s.onDragStart = [this] { proc.undoManager.beginNewTransaction(); };
        addAndMakeVisible (s);
        k.att = std::make_unique<juce::SliderParameterAttachment> (*dynamic_cast<juce::RangedAudioParameter*> (par), s, nullptr);
        midiUI.add (s, k.id);
        s.onValueChange = [this] { repaint(); };
    }
    powerAtt.sendInitialUpdate();
    modeAtt.sendInitialUpdate();
    freezeAtt.sendInitialUpdate();

    grain = ui::makeGrain();
    constrainer.setSizeLimits (720, 270, 2400, 900);
    constrainer.setFixedAspectRatio ((double) kW / (double) kH);
    setConstrainer (&constrainer);
    setResizable (true, true);
    setSize ((int) kW, (int) kH);
    layoutKnobs();
    startTimerHz (20);
}

SP1AudioProcessorEditor::~SP1AudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void SP1AudioProcessorEditor::timerCallback()
{
    meter.update (proc.outMeter);
    repaint (panelRect (1).withTrimmedLeft (panelRect (1).getWidth() - 150.0f).expanded (2.0f).transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer());
    if (midiUI.needsRepaint()) { midiUI.tick(); repaint(); }
    if (currentMode() != lastMode) { lastMode = currentMode(); repaint(); }      // a preset, undo, A / B or automation changed the mode: the TEXTURE name follows it
    if (barState.copiedAt != 0) { repaint (juce::Rectangle<float> (0.0f, 30.0f, 900.0f, 50.0f).transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer()); if (! barState.copiedRecently()) barState.copiedAt = 0; }
}

void SP1AudioProcessorEditor::toggle (const char* id)
{
    auto* par = proc.apvts.getParameter (id);
    par->beginChangeGesture();
    par->setValueNotifyingHost (par->getValue() > 0.5f ? 0.0f : 1.0f);
    par->endChangeGesture();
}

void SP1AudioProcessorEditor::setChoice (const char* id, int index)
{
    auto* par = proc.apvts.getParameter (id);
    par->beginChangeGesture(); par->setValueNotifyingHost (par->convertTo0to1 ((float) index)); par->endChangeGesture();
}

void SP1AudioProcessorEditor::layoutKnobs()
{
    const float sc = scale(), d = 2.0f * (kKnobR + kPad) * sc;
    for (auto& k : knobs)
        k.slider.setBounds (juce::Rectangle<float> (d, d).withCentre (knobCentre (k.panel, k.index, k.count) * sc).toNearestInt());
}

void SP1AudioProcessorEditor::resized() { layoutKnobs(); }

//==============================================================================
void SP1AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    midiUI.paintMarks (g, kPal.body);
}

void SP1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    ui::paintBody (g, kPal, kW, kH, grain);
    ui::paintHeader (g, kPal, "SP-1", "SPACE ENGINE", "EVOLVING SPACES", isOn ("power"));
    ui::paintPresetBar (g, kPal, proc.presets, proc.undoManager, kBarY, barState, false, 6.0f);

    const int mode = currentMode();
    ui::paintPanel (g, kPal, panelRect (0), {});
    ui::paintTabs (g, kPal, panelRect (0), kModeTabs, mode);
    ui::paintPanel (g, kPal, panelRect (1), "OUTPUT");

    {   // FREEZE has its own ice colour
        const auto b = freezeRect(); const bool on = isOn ("freeze");
        if (on) { g.setColour (kIce); g.fillRoundedRectangle (b, 9.0f); }
        g.setColour (on ? kIce : kPal.ink.withAlpha (0.4f)); g.drawRoundedRectangle (b.reduced (0.5f), 9.0f, 1.3f);
        drawText (g, "FREEZE", b.getCentreX(), b.getCentreY(), 13.0f, on ? kPal.dark : kPal.ink.withAlpha (0.9f), true, 0.1f);
    }

    for (auto& k : knobs)
    {
        const double v = k.slider.getValue();
        const auto txt = k.dB ? juce::String::formatted ("%+d dB", (int) std::lround (v)) : juce::String ((int) std::lround (v)) + " %";
        const auto c = knobCentre (k.panel, k.index, k.count);
        ui::paintKnob (g, kPal, c, k.name != nullptr ? k.name : sp1::textureName (mode), txt, k.dB ? "-24" : "0", k.dB ? "+12" : "100", panelRect (k.panel).getY() + 154.0f, panelRect (k.panel).getY() + 182.0f);
        ui::paintValueArc (g, c, kKnobR, (float) k.slider.valueToProportionOfLength (k.slider.getValue()), kPal.body);
    }
    meter.paint (g, meterOrigin().x, meterOrigin().y, isOn ("power"), kPal.body, kPal.ink, 110.0f, 26.0f, 13.0f);
    ui::paintFooter (g, kPal, kRowY + kRowH + 35.0f);
}

void SP1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / scale();
    proc.undoManager.beginNewTransaction();
    if (ui::powerRect().contains (p)) { toggle ("power"); return; }
    if (ui::presetBarMouseDown (p, *this, proc.presets, proc.undoManager, kBarY, scale(), barState, [this] { repaint(); }, false)) { layoutKnobs(); return; }
    const auto tabs = ui::tabRects (panelRect (0), kModeTabs);
    for (int i = 0; i < (int) tabs.size(); ++i)
        if (tabs[(size_t) i].expanded (3.0f).contains (p)) { setChoice ("mode", i); repaint(); return; }
    if (freezeRect().expanded (3.0f).contains (p)) { toggle ("freeze"); repaint(); return; }
}
