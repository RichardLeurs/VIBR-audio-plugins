// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "ui/Presets.h"

namespace
{
using vibr::ui::drawText;
namespace ui = vibr::ui;

// layout units: the design is 1200 x 690 (style.md: 20 side margin, 10 gap, 24 between the rows, name centre 55, rule 104, panels 30 below the rule)
constexpr float kW = 1200.0f, kH = 690.0f, kMargin = 20.0f, kBarY = 39.0f;
constexpr float kRow1Y = 134.0f, kRow1H = 247.0f, kRow2Y = 405.0f, kRow2H = 216.0f, kKnobR = 24.0f, kPad = 5.0f, kSpacing = 120.0f;
// row 1: TUNING 20..372, TEXTURE 382..578, RESONATORS 588..1180 (knobs 120 apart, 56 inside the border); row 2: STRINGS 20..850, OUTPUT 860..1180
constexpr float kPanelX[4] = { 20.0f, 382.0f, 588.0f, 860.0f }, kPanelW[4] = { 352.0f, 196.0f, 592.0f, 320.0f };
constexpr const char* kPanelName[4] = { "TUNING", "TEXTURE", "RESONATORS", "OUTPUT" };

const ui::Palette kPal { juce::Colour (204, 112, 168), juce::Colour (40, 14, 30) };
const juce::Colour kBody = kPal.body, kDark = kPal.dark, kInk = kPal.ink;

juce::Rectangle<float> panelRect (int i) { return { kPanelX[i], i < 3 ? kRow1Y : kRow2Y, kPanelW[i], i < 3 ? kRow1H : kRow2H }; }
juce::Rectangle<float> cloudRect() { return { kMargin, kRow2Y, 830.0f, kRow2H }; }
juce::Rectangle<float> plotRect() { const auto r = cloudRect(); return { r.getX() + 40.0f, r.getY() + 48.0f, r.getWidth() - 80.0f, 112.0f }; }       // the box (8 bigger, 26 more below for the note names) runs y + 40 .. y + 194
juce::Rectangle<float> textureRect (int m) { const auto r = panelRect (1); return { r.getX() + 14.0f, r.getY() + 56.0f + (float) m * 54.0f, r.getWidth() - 28.0f, 40.0f }; }
juce::Point<float> knobCentre (int panel, int k, int n)
{
    const auto r = panelRect (panel);
    if (panel == 3) return { r.getX() + 61.0f + 106.5f * (float) k, r.getY() + 92.0f };      // output knobs at x + 61 and x + 167.5: 19 to the border, 22.5 between the tick rings and between the right ring and the bars (x + 232)
    return { r.getCentreX() + kSpacing * ((float) k - 0.5f * (float) (n - 1)), r.getY() + 92.0f };
}
juce::Point<float> meterOrigin() { const auto r = panelRect (3); return { r.getX() + 232.0f, r.getY() + 46.0f }; }   // bars x + 232 .. x + 274, scale labels to about x + 301: bars 110 high end on the knob names (y + 156), peak values on the readout row (y + 182)
} // namespace

//==============================================================================
SR1AudioProcessorEditor::SR1AudioProcessorEditor (SR1AudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      knobs { { { "root", 0, 0, 3, "ROOT", 1, {}, {} },        { "scale", 0, 1, 3, "SCALE", 2, {}, {} },
                { "fine", 0, 2, 3, "FINE", 3, {}, {} },
                { "strings", 2, 0, 5, "STRINGS", 4, {}, {} },   { "decay", 2, 1, 5, "DECAY", 5, {}, {} },
                { "damping", 2, 2, 5, "DAMPING", 0, {}, {} },   { "tension", 2, 3, 5, "TENSION", 0, {}, {} },
                { "motion", 2, 4, 5, "MOTION", 0, {}, {} },
                { "mix", 3, 0, 2, "BLEND", 0, {}, {} },         { "level", 3, 1, 2, "LEVEL", 6, {}, {} } } },
      powerAtt  (*p.apvts.getParameter ("power"),  [this] (float) { repaint(); }, nullptr),
      textureAtt (*p.apvts.getParameter ("texture"), [this] (float) { repaint(); }, nullptr)
{
    setLookAndFeel (&laf);
    for (auto& k : knobs)
    {
        auto& s = k.slider;
        ui::setupKnob (s, *this);
        s.setTextValueSuffix (k.kind == 6 ? " dB" : " %");
        auto* par = proc.apvts.getParameter (k.id);
        s.setDoubleClickReturnValue (true, par->convertFrom0to1 (par->getDefaultValue()));
        s.onDragStart = [this] { proc.undoManager.beginNewTransaction(); };
        addAndMakeVisible (s);
        k.att = std::make_unique<juce::SliderParameterAttachment> (*dynamic_cast<juce::RangedAudioParameter*> (par), s, nullptr);
        s.onValueChange = [this] { repaint(); };
        midiUI.add (s, k.id);
    }
    powerAtt.sendInitialUpdate();
    textureAtt.sendInitialUpdate();

    grain = ui::makeGrain();

    constrainer.setSizeLimits (720, 414, 2400, 1380);
    constrainer.setFixedAspectRatio ((double) kW / (double) kH);
    setConstrainer (&constrainer);
    setResizable (true, true);
    setSize ((int) kW, (int) kH);
    layoutKnobs();
    startTimerHz (30);
}

SR1AudioProcessorEditor::~SR1AudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void SR1AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    midiUI.paintMarks (g, kBody);
}

void SR1AudioProcessorEditor::timerCallback()
{
    meter.update (proc.outMeter);
    const auto toPixels = [this] (juce::Rectangle<float> r) { return r.transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer(); };
    repaint (toPixels (panelRect (3).withTrimmedLeft (200.0f).expanded (2.0f)));                    // the dB meters
    if (midiUI.needsRepaint()) { midiUI.tick(); repaint(); }
    repaint (toPixels (cloudRect().expanded (4.0f)));                                               // the string display moves
    if (barState.copiedAt != 0) { repaint (toPixels (juce::Rectangle<float> (0.0f, 30.0f, 900.0f, 50.0f))); if (! barState.copiedRecently()) barState.copiedAt = 0; }
}

void SR1AudioProcessorEditor::toggle (const char* id)
{
    auto* par = proc.apvts.getParameter (id);
    par->beginChangeGesture();
    par->setValueNotifyingHost (par->getValue() > 0.5f ? 0.0f : 1.0f);
    par->endChangeGesture();
}

void SR1AudioProcessorEditor::layoutKnobs()
{
    const float sc = scale(), d = 2.0f * (kKnobR + kPad) * sc;
    for (auto& k : knobs)
        k.slider.setBounds (juce::Rectangle<float> (d, d).withCentre (knobCentre (k.panel, k.index, k.count) * sc).toNearestInt());
}

void SR1AudioProcessorEditor::resized() { layoutKnobs(); }

//==============================================================================
void SR1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    ui::paintBody (g, kPal, kW, kH, grain);
    ui::paintHeader (g, kPal, "SR-1", "SYMPATHETIC RESONATOR", "RESONANT STRINGS", isOn ("power"));
    ui::paintPresetBar (g, kPal, proc.presets, proc.undoManager, kBarY, barState, false, 6.0f);

    for (int i = 0; i < 4; ++i) ui::paintPanel (g, kPal, panelRect (i), kPanelName[i]);
    ui::paintPanel (g, kPal, cloudRect(), "STRINGS");

    const int texture = choice ("texture");
    for (int m = 0; m < sr1::kNumTextures; ++m)
    {
        const auto b = textureRect (m);
        const bool sel = m == texture;
        if (sel) { g.setColour (kBody); g.fillRoundedRectangle (b, 10.0f); }
        g.setColour (sel ? kBody : kInk.withAlpha (0.35f));
        g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, 1.2f);
        drawText (g, sr1::textureName (m), b.getCentreX(), b.getCentreY(), 16.0f, sel ? kDark : kInk.withAlpha (0.85f), true, 0.12f);
    }

    // knobs: ring, ticks, legends, name, value and the value ring
    for (auto& k : knobs)
    {
        const auto cen = knobCentre (k.panel, k.index, k.count);
        const char* lo = k.kind == 6 ? "-24" : k.kind == 3 ? "-50" : k.kind == 1 ? "C" : k.kind == 2 ? "" : k.kind == 4 ? "1" : "0";
        const char* hi = k.kind == 6 ? "+12" : k.kind == 3 ? "+50" : k.kind == 1 ? "B" : k.kind == 2 ? "" : k.kind == 4 ? "12" : "100";
        const float y = panelRect (k.panel).getY();
        ui::paintKnob (g, kPal, cen, k.name, knobText (k.kind, k.slider.getValue()), lo, hi, y + 154.0f, y + 182.0f);
        ui::paintValueArc (g, cen, kKnobR, (float) k.slider.valueToProportionOfLength (k.slider.getValue()), kBody);
    }
    meter.paint (g, meterOrigin().x, meterOrigin().y, isOn ("power"), kBody, kInk, 110.0f, 26.0f, 13.0f);

    // the harp: one string per resonator, lower notes longer, trembling and glowing while they ring
    {
        const auto r = plotRect();
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (r.expanded (8.0f).withTrimmedBottom (-26.0f), 10.0f);
        const int N = sr1::stringCount (proc.apvts.getRawParameterValue ("strings")->load() * 0.01f);
        const int root = choice ("root"), scale = choice ("scale");
        const float fLow = sr1::noteHz (sr1::stringNote (0, root, scale), 0.0f);
        const auto& eng = proc.getEngine();
        const float t = (float) juce::Time::getMillisecondCounterHiRes() * 0.001f;
        for (int k = 0; k < N; ++k)
        {
            const int midi = sr1::stringNote (k, root, scale);
            const float f = sr1::noteHz (midi, 0.0f);
            const float x = r.getX() + r.getWidth() * ((float) k + 0.5f) / (float) N;
            const float h = r.getHeight() * juce::jlimit (0.3f, 1.0f, std::pow (fLow / f, 0.45f));
            const float e = eng.energy[k].load (std::memory_order_relaxed);
            juce::Path p;
            const int seg = 24;
            for (int i = 0; i <= seg; ++i)
            {
                const float u = (float) i / (float) seg;
                const float wob = std::sin (juce::MathConstants<float>::pi * u) * std::sin (t * (18.0f + (float) k * 3.0f)) * (1.5f + 9.0f * e);
                const juce::Point<float> pt (x + wob, r.getBottom() - h * u);
                if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
            }
            if (e > 0.02f) { g.setColour (kBody.withAlpha (0.25f * e)); g.strokePath (p, juce::PathStrokeType (7.0f)); }
            g.setColour (kBody.interpolatedWith (juce::Colours::white, 0.6f * e).withAlpha (0.45f + 0.55f * e));
            g.strokePath (p, juce::PathStrokeType (2.2f));
            drawText (g, sr1::noteName (midi), x, r.getBottom() + 20.0f, 13.0f, kInk.withAlpha (0.55f + 0.4f * e), true, 0.06f);
        }
    }

    ui::paintFooter (g, kPal, kRow2Y + kRow2H + 35.0f);
}

juce::String SR1AudioProcessorEditor::knobText (int kind, double v) const
{
    const float f = (float) v * 0.01f;
    switch (kind)
    {
        case 1: return sr1::noteName (choice ("root"));
        case 2: return sr1::scaleName (choice ("scale"));
        case 3: return juce::String::formatted ("%+d ct", (int) std::lround (sr1::fineCents (f)));
        case 4: return juce::String (sr1::stringCount (f));
        case 5: return juce::String (sr1::decaySeconds (f), 1) + " s";
        case 6: return juce::String::formatted ("%+d dB", (int) std::lround (v));
        default: return juce::String ((int) std::lround (v)) + " %";
    }
}

void SR1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / scale();
    proc.undoManager.beginNewTransaction();
    if (ui::powerRect().contains (p)) { toggle ("power"); return; }
    if (ui::presetBarMouseDown (p, *this, proc.presets, proc.undoManager, kBarY, scale(), barState, [this] { repaint(); }, false)) { layoutKnobs(); return; }
    for (int m = 0; m < sr1::kNumTextures; ++m)
        if (textureRect (m).expanded (3.0f).contains (p))
        {
            auto* par = proc.apvts.getParameter ("texture");
            par->beginChangeGesture();
            par->setValueNotifyingHost (par->convertTo0to1 ((float) m));
            par->endChangeGesture();
            return;
        }
}
