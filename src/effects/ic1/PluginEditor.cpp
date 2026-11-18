// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "ui/Presets.h"
#include "ui/OutputMeter.h"

namespace
{
using vibr::ui::drawText;
namespace ui = vibr::ui;

constexpr float kW = 1200.0f, kH = 890.0f, kRow1Y = 134.0f, kRow1H = 247.0f, kRow2Y = 405.0f, kRow2H = 216.0f, kRow3Y = 645.0f, kRow3H = 190.0f, kBarY = 39.0f;
const ui::Palette kPal { juce::Colour (178, 59, 210), juce::Colour (38, 10, 46) };             // the Experimental colour #B23BD2 and its print
const juce::Colour kIce (143, 211, 244);
const std::vector<juce::String> kFilterTabs { "LOW PASS", "BAND PASS", "HIGH PASS" };

// panels: 0 NODES, 1 ROUTING (row 1), 2 FILTER, 3 PHASE & MOD, 4 OUTPUT (row 2), 5 NETWORK (row 3)
constexpr float kPanelX[6] = { 20.0f, 810.0f, 20.0f, 430.0f, 770.0f, 20.0f }, kPanelW[6] = { 780.0f, 370.0f, 400.0f, 330.0f, 410.0f, 1160.0f };
juce::Rectangle<float> panelRect (int i)
{
    const float y = i < 2 ? kRow1Y : i < 5 ? kRow2Y : kRow3Y, h = i < 2 ? kRow1H : i < 5 ? kRow2H : kRow3H;
    return { kPanelX[i], y, kPanelW[i], h };
}
juce::Point<float> knobCentre (int panel, int k, int n)
{
    const auto r = panelRect (panel);
    const float cx = panel == 4 ? r.getX() + 165.0f : r.getCentreX(), sp = panel == 0 ? 95.0f : panel == 2 ? 105.0f : panel == 3 ? 100.0f : 95.0f;
    return { cx + sp * ((float) k - 0.5f * (float) (n - 1)), r.getY() + 92.0f };
}
juce::Point<float> meterOrigin() { const auto r = panelRect (4); return { r.getX() + 318.0f, r.getY() + 46.0f }; }
juce::Rectangle<float> routeRect (int i) { const auto r = panelRect (1); return { r.getX() + 14.0f + 118.0f * (float) (i % 3), r.getY() + 40.0f + 34.0f * (float) (i / 3), 110.0f, 28.0f }; }
juce::Rectangle<float> injectRect (int i) { const auto r = panelRect (1); return { r.getX() + 14.0f + 118.0f * (float) i, r.getY() + 122.0f, 110.0f, 28.0f }; }
juce::Rectangle<float> modeRect (int i) { const auto r = panelRect (1); return { r.getX() + 14.0f + 118.0f * (float) i, r.getY() + 174.0f, 110.0f, 30.0f }; }
juce::Rectangle<float> netRect() { const auto r = panelRect (5); return { r.getX() + 30.0f, r.getY() + 28.0f, r.getWidth() - 60.0f, 150.0f }; }
const char* const kLevelIds[ic1::kNodes] = { "f1", "f2", "f3", "f4" };

juce::Point<float> nodePos (int i)                                                                              // L nodes (1, 3) on the top row, R nodes (2, 4) below
{
    const auto r = netRect();
    return { r.getX() + r.getWidth() * (i < 2 ? 0.40f : 0.62f), r.getY() + r.getHeight() * ((i & 1) ? 0.74f : 0.26f) };
}
}

//==============================================================================
IC1AudioProcessorEditor::IC1AudioProcessorEditor (IC1AudioProcessor& p)
    : LabEditor (p, kW, kH), proc (p),
      knobs { { { "time", 0, 0, 8, "TIME", 1, {}, {} },       { "spread", 0, 1, 8, "SPREAD", 0, {}, {} },
                { "f1", 0, 2, 8, "FB 1", 0, {}, {} },         { "f2", 0, 3, 8, "FB 2", 0, {}, {} },          { "f3", 0, 4, 8, "FB 3", 0, {}, {} },        { "f4", 0, 5, 8, "FB 4", 0, {}, {} },
                { "cross", 0, 6, 8, "CROSS", 0, {}, {} },     { "regen", 0, 7, 8, "REGEN", 0, {}, {} },
                { "cutoff", 2, 0, 3, "CUTOFF", 2, {}, {} },   { "res", 2, 1, 3, "RES", 0, {}, {} },          { "tilt", 2, 2, 3, "TILT", 0, {}, {} },
                { "phase", 3, 0, 3, "PHASE", 0, {}, {} },     { "rate", 3, 1, 3, "RATE", 3, {}, {} },        { "depth", 3, 2, 3, "DEPTH", 0, {}, {} },
                { "drive", 4, 0, 3, "DRIVE", 0, {}, {} },     { "mix", 4, 1, 3, "MIX", 0, {}, {} },          { "level", 4, 2, 3, "LEVEL", 4, {}, {} } } }
{
    for (auto& k : knobs)
    {
        attachKnob (k.slider, k.att, k.id);
        midiUI.add (k.slider, k.id);
    }
    layoutKnobs();
    startTimerHz (30);
}

IC1AudioProcessorEditor::~IC1AudioProcessorEditor() = default;

void IC1AudioProcessorEditor::timerCallback()
{
    meter.update (proc.outMeter);
    for (int i = 0; i < ic1::kNodes; ++i) { const float v = proc.engine.nodeLevel (i); pulse[i] = v > pulse[i] ? v : pulse[i] + (v - pulse[i]) * 0.25f; }
    const auto region = [this] (juce::Rectangle<float> r) { return r.expanded (4.0f).transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer(); };
    repaint (region (panelRect (5)));                                                                          // the network
    repaint (region (panelRect (4).withTrimmedLeft (panelRect (4).getWidth() - 110.0f)));                       // the output meters
    if (midiUI.needsRepaint()) { midiUI.tick(); repaint(); }
    if (exciteAt != 0 && juce::Time::currentTimeMillis() - exciteAt > 300) { exciteAt = 0; repaint (region (modeRect (2))); }
    tickPresetBar();
}

void IC1AudioProcessorEditor::setValue (const char* id, float v)
{
    auto* par = proc.apvts.getParameter (id);
    par->beginChangeGesture(); par->setValueNotifyingHost (par->convertTo0to1 (v)); par->endChangeGesture();
}

void IC1AudioProcessorEditor::layoutKnobs()
{
    for (auto& k : knobs)
        placeKnob (k.slider, knobCentre (k.panel, k.index, k.count));
}
void IC1AudioProcessorEditor::resized() { layoutKnobs(); }

//==============================================================================
// NETWORK: the two inputs (L above, R below), the four nodes (1, 3 left, 2, 4 right: the same rows as the channels they feed) and the two outputs. Every feedback
// path of the routing is a curve from the node it comes from to the node it goes into: its width is the loop gain of that path (routing x feedback level x
// regeneration), its brightness follows the level of the node it leaves, a dashed light curve is a path that inverts the phase. A node grows and glows with its level.
void IC1AudioProcessorEditor::drawNetwork (juce::Graphics& g)
{
    const auto r = netRect();
    g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (r.expanded (14.0f, 8.0f), 10.0f);
    const bool on = isOn ("power"), hold = isOn ("hold"), selfOsc = isOn ("selfosc");
    const float regen = hold ? 1.0f : value ("regen") * 0.01f * (selfOsc ? 1.15f : 0.97f);
    float R[ic1::kNodes][ic1::kNodes];
    ic1::routingMatrix (choice ("route"), value ("cross") * 0.01f, R);
    const int inject = choice ("inject");

    const juce::Point<float> inPos[2] = { { r.getX() + r.getWidth() * 0.08f, r.getY() + r.getHeight() * 0.26f }, { r.getX() + r.getWidth() * 0.08f, r.getY() + r.getHeight() * 0.74f } };
    const juce::Point<float> outPos[2] = { { r.getX() + r.getWidth() * 0.92f, r.getY() + r.getHeight() * 0.26f }, { r.getX() + r.getWidth() * 0.92f, r.getY() + r.getHeight() * 0.74f } };
    const auto line = [&] (juce::Point<float> a, juce::Point<float> b, float alpha, float w)
    {
        g.setColour (kPal.ink.withAlpha (alpha)); g.drawLine (a.x, a.y, b.x, b.y, w);
    };
    for (int c = 0; c < 2; ++c)                                                                                // the input and output boxes
        for (const auto& pt : { inPos[c], outPos[c] })
        {
            const auto b = juce::Rectangle<float> (34.0f, 24.0f).withCentre (pt);
            g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (b, 6.0f);
            g.setColour (kPal.ink.withAlpha (0.45f)); g.drawRoundedRectangle (b.reduced (0.5f), 6.0f, 1.2f);
            drawText (g, c == 0 ? "L" : "R", b.getCentreX(), b.getCentreY(), 13.0f, kPal.ink.withAlpha (0.85f), true, 0.06f);
        }
    drawText (g, "IN", inPos[0].x, r.getY() - 6.0f, 11.0f, kPal.ink.withAlpha (0.5f), true, 0.12f);
    drawText (g, "OUT", outPos[0].x, r.getY() - 6.0f, 11.0f, kPal.ink.withAlpha (0.5f), true, 0.12f);
    for (int i = 0; i < ic1::kNodes; ++i)                                                                      // input and output wires
    {
        const bool fed = inject == ic1::InParallel || (inject == ic1::InSeries && i == 0) || (inject == ic1::InSplit && i < 2);
        if (fed && ! hold) { if (inject == ic1::InSeries) { line (inPos[0], nodePos (i), 0.35f, 1.4f); line (inPos[1], nodePos (i), 0.35f, 1.4f); } else line (inPos[i & 1], nodePos (i), 0.35f, 1.4f); }
        line (nodePos (i), outPos[i & 1], 0.3f, 1.4f);
    }

    for (int i = 0; i < ic1::kNodes; ++i)                                                                      // the feedback paths
        for (int j = 0; j < ic1::kNodes; ++j)
        {
            const float gain = std::fabs (R[i][j]) * (hold ? 1.0f : value (kLevelIds[j]) * 0.01f) * regen;
            if (std::fabs (R[i][j]) < 0.02f || gain < 0.01f) continue;
            const float act = on ? juce::jlimit (0.0f, 1.0f, pulse[j] * 2.0f) : 0.0f;
            const float width = 1.0f + 5.0f * std::min (1.2f, gain), alpha = 0.22f + 0.7f * act;
            const auto colour = R[i][j] < 0.0f ? kIce : kPal.body;
            juce::Path path;
            const auto a = nodePos (j), b = nodePos (i);
            if (i == j)                                                                                        // a node feeding itself: a loop above or below it
            {
                const float up = (i & 1) ? 1.0f : -1.0f;
                path.startNewSubPath (a.x - 8.0f, a.y + up * 17.0f);
                path.cubicTo (a.x - 34.0f, a.y + up * 46.0f, a.x + 34.0f, a.y + up * 46.0f, a.x + 8.0f, a.y + up * 17.0f);
            }
            else
            {
                const auto mid = (a + b) * 0.5f; auto d = b - a; const float len = std::max (1.0f, d.getDistanceFromOrigin());
                const juce::Point<float> nrm (-d.y / len, d.x / len);
                const float off = (j < i ? 1.0f : -1.0f) * (18.0f + 6.0f * (float) std::abs (i - j));
                const auto from = a + (d / len) * 20.0f, to = b - (d / len) * 20.0f;
                path.startNewSubPath (from); path.quadraticTo (mid + nrm * off, to);
            }
            g.setColour (colour.withAlpha (alpha * 0.35f)); g.strokePath (path, juce::PathStrokeType (width + 4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            if (R[i][j] < 0.0f) { juce::Path dashed; const float dash[] = { 6.0f, 4.0f }; juce::PathStrokeType (width).createDashedStroke (dashed, path, dash, 2); g.setColour (colour.withAlpha (alpha)); g.fillPath (dashed); }
            else { g.setColour (colour.withAlpha (alpha)); g.strokePath (path, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)); }
            if (i != j)                                                                                        // an arrow head at the end
            {
                const auto end = path.getPointAlongPath (path.getLength()), before = path.getPointAlongPath (path.getLength() - 9.0f);
                auto d = end - before; const float len = std::max (0.001f, d.getDistanceFromOrigin()); d = d / len;
                juce::Path head; head.addTriangle (end.x, end.y, end.x - d.x * 9.0f - d.y * 5.0f, end.y - d.y * 9.0f + d.x * 5.0f, end.x - d.x * 9.0f + d.y * 5.0f, end.y - d.y * 9.0f - d.x * 5.0f);
                g.setColour (colour.withAlpha (std::min (1.0f, alpha + 0.15f))); g.fillPath (head);
            }
        }

    for (int i = 0; i < ic1::kNodes; ++i)                                                                      // the nodes
    {
        const auto c = nodePos (i);
        const float act = on ? juce::jlimit (0.0f, 1.0f, pulse[i] * 1.6f) : 0.0f, rad = 15.0f + 9.0f * act;
        g.setColour (kPal.body.withAlpha (0.22f * act)); g.fillEllipse (juce::Rectangle<float> (2.0f * (rad + 8.0f), 2.0f * (rad + 8.0f)).withCentre (c));
        g.setColour (juce::Colour (26, 27, 30)); g.fillEllipse (juce::Rectangle<float> (2.0f * rad, 2.0f * rad).withCentre (c));
        g.setColour (kPal.body.withAlpha (0.35f + 0.65f * act)); g.fillEllipse (juce::Rectangle<float> (2.0f * rad, 2.0f * rad).withCentre (c).reduced (3.0f));
        g.setColour (kPal.body); g.drawEllipse (juce::Rectangle<float> (2.0f * rad, 2.0f * rad).withCentre (c), 1.6f);
        drawText (g, juce::String (i + 1), c.x, c.y, 15.0f, kPal.dark, true, 0.0f);
        const float ms = value ("time") * (1.0f + value ("spread") * 0.01f * (ic1::kTimeRatio[i] - 1.0f));
        drawText (g, juce::String ((int) std::lround (ms)) + " MS   " + juce::String ((int) std::lround (hold ? 100.0f : value (kLevelIds[i]))) + " %", c.x + (i < 2 ? -rad - 8.0f : rad + 8.0f), c.y,
                  11.0f, kPal.ink.withAlpha (0.65f), true, 0.06f, i < 2 ? juce::Justification::centredRight : juce::Justification::centredLeft);
    }
    const float loop = regen;
    drawText (g, hold ? "HOLD: THE LOOP KEEPS ITSELF" : loop > 1.0f ? "SELF-OSCILLATING: THE LOOP GAIN IS ABOVE 1" : "LOOP GAIN " + juce::String ((int) std::lround (loop * 100.0f)) + " %", r.getCentreX(), r.getBottom() + 6.0f, 12.0f,
              loop > 1.0f || hold ? kIce : kPal.ink.withAlpha (0.6f), true, 0.12f);
}

//==============================================================================
void IC1AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    midiUI.paintMarks (g, kPal.body);
}

void IC1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    ui::paintBody (g, kPal, kW, kH, grain);
    ui::paintHeader (g, kPal, "IC-1", "INFINITE CIRCUIT", "ENDLESS RECURSION", isOn ("power"));
    ui::paintPresetBar (g, kPal, proc.presets, proc.undoManager, kBarY, barState, false, 6.0f);

    ui::paintPanel (g, kPal, panelRect (0), "NODES");
    ui::paintPanel (g, kPal, panelRect (1), "ROUTING");
    ui::paintPanel (g, kPal, panelRect (2), {});
    ui::paintTabs (g, kPal, panelRect (2), kFilterTabs, choice ("filter"));
    ui::paintPanel (g, kPal, panelRect (3), "PHASE & MOD");
    ui::paintPanel (g, kPal, panelRect (4), "OUTPUT");
    ui::paintPanel (g, kPal, panelRect (5), "NETWORK");
    drawNetwork (g);

    {   // ROUTING: the matrix, the input paths and the three regeneration buttons
        const auto r = panelRect (1);
        drawText (g, "ROUTING", r.getX() + 14.0f, r.getY() + 30.0f, 11.0f, kPal.ink.withAlpha (0.55f), true, 0.14f, juce::Justification::centredLeft);
        for (int i = 0; i < ic1::kRoutes; ++i) ui::paintFlatButton (g, kPal, routeRect (i), ic1::routeName (i), choice ("route") == i, 11.0f);
        drawText (g, "INPUT PATHS", r.getX() + 14.0f, r.getY() + 112.0f, 11.0f, kPal.ink.withAlpha (0.55f), true, 0.14f, juce::Justification::centredLeft);
        for (int i = 0; i < 3; ++i) ui::paintFlatButton (g, kPal, injectRect (i), ic1::injectName (i), choice ("inject") == i, 11.0f);
        const auto iceButton = [&] (juce::Rectangle<float> b, const juce::String& label, bool lit)
        {
            if (lit) { g.setColour (kIce); g.fillRoundedRectangle (b, 9.0f); }
            g.setColour (lit ? kIce : kPal.ink.withAlpha (0.4f)); g.drawRoundedRectangle (b.reduced (0.5f), 9.0f, 1.3f);
            drawText (g, label, b.getCentreX(), b.getCentreY(), 12.0f, lit ? kPal.dark : kPal.ink.withAlpha (0.9f), true, 0.08f);
        };
        iceButton (modeRect (0), "SELF-OSC", isOn ("selfosc"));
        iceButton (modeRect (1), "HOLD", isOn ("hold"));
        iceButton (modeRect (2), "EXCITE", exciteAt != 0);
        drawText (g, "SELF-OSC LETS REGEN GO ABOVE 100 %", r.getCentreX(), r.getY() + 226.0f, 10.0f, kPal.ink.withAlpha (0.4f), true, 0.1f);
    }

    for (auto& k : knobs)
    {
        const double v = k.slider.getValue();
        juce::String txt, lo = "0", hi = "100";
        switch (k.kind)
        {
            case 1: txt = juce::String ((int) std::lround (v)) + " ms"; lo = "5"; hi = "1.5s"; break;
            case 2: txt = v >= 1000.0 ? juce::String (v * 0.001, 2) + " kHz" : juce::String ((int) std::lround (v)) + " Hz"; lo = "40"; hi = "16k"; break;
            case 3: txt = juce::String (v, 2) + " Hz"; lo = ".02"; hi = "8"; break;
            case 4: txt = juce::String::formatted ("%+d dB", (int) std::lround (v)); lo = "-24"; hi = "+12"; break;
            default: txt = juce::String ((int) std::lround (v)) + " %"; break;
        }
        const auto c = knobCentre (k.panel, k.index, k.count);
        ui::paintKnob (g, kPal, c, k.name, txt, lo, hi, panelRect (k.panel).getY() + 154.0f, panelRect (k.panel).getY() + 182.0f);
        ui::paintValueArc (g, c, ui::kKnobR, (float) k.slider.valueToProportionOfLength (k.slider.getValue()), kPal.body);
    }
    meter.paint (g, meterOrigin().x, meterOrigin().y, isOn ("power"), kPal.body, kPal.ink, 110.0f, 26.0f, 13.0f);
    ui::paintFooter (g, kPal, kRow3Y + kRow3H + 35.0f);
}

void IC1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / scale();
    proc.undoManager.beginNewTransaction();
    if (ui::powerRect().contains (p)) { toggle ("power"); return; }
    if (ui::presetBarMouseDown (p, *this, proc.presets, proc.undoManager, kBarY, scale(), barState, [this] { repaint(); }, false)) { layoutKnobs(); return; }
    const auto tabs = ui::tabRects (panelRect (2), kFilterTabs);
    for (int i = 0; i < (int) tabs.size(); ++i)
        if (tabs[(size_t) i].expanded (3.0f).contains (p)) { setValue ("filter", (float) i); repaint(); return; }
    for (int i = 0; i < ic1::kRoutes; ++i)
        if (routeRect (i).expanded (3.0f).contains (p)) { setValue ("route", (float) i); repaint(); return; }
    for (int i = 0; i < 3; ++i)
        if (injectRect (i).expanded (3.0f).contains (p)) { setValue ("inject", (float) i); repaint(); return; }
    if (modeRect (0).expanded (3.0f).contains (p)) { toggle ("selfosc"); repaint(); return; }
    if (modeRect (1).expanded (3.0f).contains (p)) { toggle ("hold"); repaint(); return; }
    if (modeRect (2).expanded (3.0f).contains (p)) { proc.engine.requestExcite(); exciteAt = juce::Time::currentTimeMillis(); repaint(); return; }
}
