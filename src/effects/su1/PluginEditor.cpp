// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"

namespace
{
namespace ui = vibr::ui::vintage;          // the default look (src/common/ui/VintageUI.h)

constexpr float kW = ui::kW, kH = ui::kH;
constexpr float kRow1Y = 114.0f, kRow1H = 216.0f, kRow2Y = 348.0f, kRow2H = 282.0f;       // the content area is y 114 to 630, 18 between the rows
const ui::Palette kPal { juce::Colour (177, 139, 240) };                                  // the Stereo Utility colour #B18BF0: value rings, knob values and the plugin code

// row 1: PAN, WIDTH, CHANNELS (three equal panels); row 2: FIELD (as wide as the first two panels) and OUTPUT (as wide as CHANNELS)
constexpr float kColW = (kW - 2.0f * ui::kMargin - 2.0f * 18.0f) / 3.0f;
juce::Rectangle<float> panelRect (int i)         // 0 PAN, 1 WIDTH, 2 CHANNELS, 3 OUTPUT, 4 FIELD
{
    if (i < 3) return { ui::kMargin + (float) i * (kColW + 18.0f), kRow1Y, kColW, kRow1H };
    if (i == 3) return { ui::kMargin + 2.0f * (kColW + 18.0f), kRow2Y, kColW, kRow2H };
    return { ui::kMargin, kRow2Y, 2.0f * kColW + 18.0f, kRow2H };
}
constexpr const char* kPanelName[5] = { "PAN", "WIDTH", "CHANNELS", "OUTPUT", "FIELD" };

juce::Point<float> knobCentre (int panel, int k, int n)
{
    const auto r = panelRect (panel);
    if (panel == 3) return { r.getX() + 16.0f + (r.getWidth() - 20.0f - ui::kMeterW - 14.0f - 16.0f) * 0.5f + 8.0f, r.getY() + 112.0f };     // GAIN: the only knob, left of the meter
    const float pitch = (r.getWidth() - 122.0f) * 0.5f;                                      // 16 from the border to the ring at the outer knobs
    return { r.getCentreX() + pitch * ((float) k - 0.5f * (float) (n - 1)), r.getY() + 86.0f };
}
juce::Point<float> meterOrigin() { const auto r = panelRect (3); return { r.getRight() - 20.0f - ui::kMeterW, r.getY() + (r.getHeight() - ui::kMeterH) * 0.5f }; }
juce::Rectangle<float> fieldScreen() { const auto r = panelRect (4); return { r.getX() + 20.0f, r.getY() + 22.0f, r.getWidth() - 40.0f, r.getHeight() - 42.0f }; }

// the six buttons of CHANNELS: INVERT L, INVERT R, SWAP L / R, MONO, MID, SIDE (two columns, three rows)
juce::Rectangle<float> buttonRect (int i)
{
    const auto r = panelRect (2);
    const float w = (r.getWidth() - 40.0f - 6.0f) * 0.5f;
    return { r.getX() + 20.0f + (float) (i % 2) * (w + 6.0f), r.getY() + 38.0f + (float) (i / 2) * 57.0f, w, 30.0f };
}
constexpr const char* kButtonName[6] = { "INVERT L", "INVERT R", "SWAP L / R", "MONO", "MID", "SIDE" };

} // namespace

//==============================================================================
SU1AudioProcessorEditor::SU1AudioProcessorEditor (SU1AudioProcessor& p)
    : LabEditor (p, kW, kH), proc (p),
      knobs { { { "gain", 3, 0, 1, "GAIN", 0, {}, {} },
                { "pan", 0, 0, 3, "POSITION", 1, {}, {} },     { "autoRate", 0, 1, 3, "RATE", 2, {}, {} },     { "autoDepth", 0, 2, 3, "DEPTH", 3, {}, {} },
                { "width", 1, 0, 3, "WIDTH", 4, {}, {} },      { "spread", 1, 1, 3, "SPREAD", 3, {}, {} },     { "bassMono", 1, 2, 3, "BASS MONO", 5, {}, {} } } },
      invLAtt  (*p.apvts.getParameter ("invL"),  [this] (float) { repaint(); }, nullptr),
      invRAtt  (*p.apvts.getParameter ("invR"),  [this] (float) { repaint(); }, nullptr),
      swapAtt  (*p.apvts.getParameter ("swap"),  [this] (float) { repaint(); }, nullptr),
      monoAtt  (*p.apvts.getParameter ("mono"),  [this] (float) { repaint(); }, nullptr),
      soloAtt  (*p.apvts.getParameter ("solo"),  [this] (float) { repaint(); }, nullptr)
{
    for (auto& k : knobs) { attachKnob (k.slider, k.att, k.id); midiUI.add (k.slider, k.id); }
    invLAtt.sendInitialUpdate(); invRAtt.sendInitialUpdate(); swapAtt.sendInitialUpdate(); monoAtt.sendInitialUpdate(); soloAtt.sendInitialUpdate();
    layoutKnobs();
    startTimerHz (30);
}

SU1AudioProcessorEditor::~SU1AudioProcessorEditor() { stopTimer(); }

void SU1AudioProcessorEditor::timerCallback()
{
    meter.update (proc.outMeter);
    if (midiUI.needsRepaint()) { midiUI.tick(); repaint(); }
    repaintDesign (panelRect (4).expanded (4.0f));                       // the goniometer and the correlation move
    repaintDesign (juce::Rectangle<float> (meterOrigin().x, meterOrigin().y, ui::kMeterW, ui::kMeterH).expanded (2.0f));
}

void SU1AudioProcessorEditor::layoutKnobs()
{
    for (auto& k : knobs) placeKnob (k.slider, knobCentre (k.panel, k.index, k.count));
}

void SU1AudioProcessorEditor::paintOverChildren (juce::Graphics& g) { midiUI.paintMarks (g, kPal.accent); }

void SU1AudioProcessorEditor::resized() { layoutKnobs(); }

//==============================================================================
// FIELD display: left a goniometer (vertical = mid, horizontal = side: a mono signal is a vertical line, left only leans to the left, opposite channels make a horizontal
// line), right the phase correlation (-1 .. +1), on a phosphor screen.
void SU1AudioProcessorEditor::drawField (juce::Graphics& g)
{
    const auto sc = fieldScreen();
    const auto& eng = proc.getEngine();
    const bool power = isOn ("power");
    const auto dim = [] (float a) { return kPal.phosphorDim.withAlpha (a); };

    ui::paintScreen (g, kPal, sc);
    {
        juce::Graphics::ScopedSaveState ss (g);                          // the grid of the screen
        g.reduceClipRegion (sc.reduced (3.0f).toNearestInt());
        g.setColour (kPal.phosphor.withAlpha (0.10f));
        for (float x = sc.getX() + 40.0f; x < sc.getRight(); x += 40.0f) g.drawLine (x, sc.getY(), x, sc.getBottom(), 1.0f);
        for (float y = sc.getY() + 20.0f; y < sc.getBottom(); y += 20.0f) g.drawLine (sc.getX(), y, sc.getRight(), y, 1.0f);
    }
    ui::drawText (g, "GONIOMETER", sc.getX() + 12.0f, sc.getY() + 16.0f, 15.0f, kPal.phosphorDim, false, 0.08f, juce::Justification::centredLeft, ui::Face::Mono);
    ui::drawText (g, "CORRELATION", sc.getX() + 350.0f, sc.getY() + 16.0f, 15.0f, kPal.phosphorDim, false, 0.08f, juce::Justification::centredLeft, ui::Face::Mono);
    g.setColour (dim (0.35f));
    g.drawLine (sc.getX() + 320.0f, sc.getY() + 12.0f, sc.getX() + 320.0f, sc.getBottom() - 12.0f, 1.0f);

    // goniometer
    const float side = 170.0f;
    const juce::Rectangle<float> gon (sc.getX() + 160.0f - side * 0.5f, sc.getY() + 40.0f, side, side);
    g.setColour (kPal.screen.brighter (0.12f));
    g.fillRect (gon);
    g.setColour (dim (0.4f));
    g.drawLine (gon.getCentreX(), gon.getY(), gon.getCentreX(), gon.getBottom(), 1.0f);
    g.drawLine (gon.getX(), gon.getCentreY(), gon.getRight(), gon.getCentreY(), 1.0f);
    g.drawLine (gon.getX(), gon.getY(), gon.getRight(), gon.getBottom(), 1.0f);
    g.drawLine (gon.getX(), gon.getBottom(), gon.getRight(), gon.getY(), 1.0f);
    g.drawRect (gon, 1.0f);
    ui::drawText (g, "M", gon.getCentreX(), gon.getY() + 12.0f, 14.0f, kPal.phosphorDim, true, 0.0f, juce::Justification::centred, ui::Face::Mono);
    ui::drawText (g, "S", gon.getRight() - 11.0f, gon.getCentreY() - 12.0f, 14.0f, kPal.phosphorDim, true, 0.0f, juce::Justification::centred, ui::Face::Mono);
    ui::drawText (g, "L", gon.getX() + 12.0f, gon.getY() + 12.0f, 14.0f, kPal.phosphorDim, true, 0.0f, juce::Justification::centred, ui::Face::Mono);
    ui::drawText (g, "R", gon.getRight() - 12.0f, gon.getY() + 12.0f, 14.0f, kPal.phosphorDim, true, 0.0f, juce::Justification::centred, ui::Face::Mono);
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (gon.toNearestInt());
        const int pos = eng.scopePos.load (std::memory_order_acquire);
        for (int k = 0; k < su1::kScope; ++k)
        {
            const int idx = (pos + k) % su1::kScope;
            const float l = eng.scopeL[idx].load (std::memory_order_relaxed), rr = eng.scopeR[idx].load (std::memory_order_relaxed);
            const float x = gon.getCentreX() + (rr - l) * side * 0.5f * 0.7071f * 1.4f, y = gon.getCentreY() - (l + rr) * side * 0.5f * 0.7071f * 1.4f;
            const float age = (float) k / (float) su1::kScope;
            g.setColour (kPal.phosphor.withAlpha (0.1f + 0.8f * age * age));
            g.fillEllipse (juce::Rectangle<float> (3.0f, 3.0f).withCentre ({ x, y }));
        }
    }

    // correlation
    const float corr = power ? eng.corr.load (std::memory_order_relaxed) : 1.0f;
    const juce::Rectangle<float> cm (sc.getX() + 350.0f, sc.getY() + 100.0f, sc.getWidth() - 350.0f - 30.0f, 22.0f);
    g.setColour (kPal.screen.brighter (0.12f));
    g.fillRect (cm);
    g.setColour (dim (0.5f));
    g.drawRect (cm, 1.0f);
    const float xc = cm.getCentreX() + corr * cm.getWidth() * 0.5f;
    const auto bad = kPal.segRed;
    g.setColour ((corr < 0.0f ? bad : kPal.phosphor).withAlpha (0.85f));
    g.fillRect (juce::Rectangle<float> (std::min (xc, cm.getCentreX()), cm.getY() + 2.0f, std::abs (xc - cm.getCentreX()), cm.getHeight() - 4.0f));
    g.setColour (kPal.cream);
    g.fillRect (juce::Rectangle<float> (3.0f, cm.getHeight() + 14.0f).withCentre ({ xc, cm.getCentreY() }));
    g.setColour (dim (0.8f));
    g.drawLine (cm.getCentreX(), cm.getY() - 4.0f, cm.getCentreX(), cm.getBottom() + 4.0f, 1.0f);
    ui::drawText (g, "-1", cm.getX(), cm.getBottom() + 18.0f, 13.0f, kPal.phosphorDim, false, 0.0f, juce::Justification::centredLeft, ui::Face::Mono);
    ui::drawText (g, "0", cm.getCentreX(), cm.getBottom() + 18.0f, 13.0f, kPal.phosphorDim, false, 0.0f, juce::Justification::centred, ui::Face::Mono);
    ui::drawText (g, "+1", cm.getRight(), cm.getBottom() + 18.0f, 13.0f, kPal.phosphorDim, false, 0.0f, juce::Justification::centredRight, ui::Face::Mono);
    ui::drawText (g, juce::String::formatted ("%+.2f", corr), cm.getCentreX(), cm.getBottom() + 56.0f, 25.0f, corr < 0.0f ? bad : kPal.phosphor, true, 0.05f, juce::Justification::centred, ui::Face::Mono);
    ui::drawText (g, corr > 0.9f ? "MONO COMPATIBLE" : corr > 0.3f ? "GOOD STEREO" : corr > -0.1f ? "VERY WIDE" : "OUT OF PHASE", cm.getCentreX(), cm.getBottom() + 86.0f, 15.0f,
                  kPal.phosphorDim, false, 0.1f, juce::Justification::centred, ui::Face::Mono);
}

//==============================================================================
void SU1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    ui::paintBody (g, kPal, kW, kH);
    ui::paintHeader (g, kPal, "SU-1", "STEREO UTILITY", "SPATIAL CONTROL");
    ui::paintPresetBar (g, kPal, proc.presets, proc.undoManager, isOn ("power"));
    ui::paintFooter (g, kPal, kH - 15.0f);

    for (int i = 0; i < 5; ++i) ui::paintPanel (g, kPal, panelRect (i), kPanelName[i]);
    drawField (g);
    ui::paintMeter (g, kPal, meter, meterOrigin().x, meterOrigin().y, isOn ("power"));

    // the buttons of CHANNELS
    const bool btnOn[6] = { isOn ("invL"), isOn ("invR"), isOn ("swap"), isOn ("mono"), solo() == 1, solo() == 2 };
    for (int i = 0; i < 6; ++i) ui::paintFlatButton (g, kPal, buttonRect (i), kButtonName[i], btnOn[i], 17.0f);

    // knobs: value ring, legends, name and value
    static const char* lo[6] = { "-60", "L", "0.1", "0", "0", "off" }, * hi[6] = { "+12", "R", "10", "100", "200", "300" };
    for (auto& k : knobs)
    {
        const auto c = knobCentre (k.panel, k.index, k.count);
        const double v = k.slider.getValue();
        juce::String txt;
        switch (k.kind)
        {
            case 0: txt = v <= -59.9 ? juce::String ("MUTE") : juce::String::formatted ("%+.1f dB", v); break;
            case 1: txt = std::abs (v) < 0.5 ? juce::String ("CENTRE") : juce::String::formatted ("%s %d", v < 0 ? "L" : "R", (int) std::lround (std::abs (v))); break;
            case 2: txt = juce::String (v, 2) + " Hz"; break;
            case 5: txt = v < 20.0 ? juce::String ("OFF") : juce::String ((int) std::lround (v)) + " Hz"; break;
            default: txt = juce::String ((int) std::lround (v)) + " %"; break;
        }
        if (k.kind == 1)                                                 // position: a mark at the top = centre, the ring runs from there
        {
            ui::paintKnob (g, kPal, c, k.name, txt, lo[k.kind], hi[k.kind], 0.0f);
            g.setColour (kPal.inkSoft);
            g.drawLine (c.x, c.y - ui::ringRadius (ui::kKnobR) - 7.0f, c.x, c.y - ui::ringRadius (ui::kKnobR) - 1.0f, 1.6f);
            ui::paintBipolarArc (g, c, ui::kKnobR, (float) (v / 100.0), kPal.accent, kPal.track);
        }
        else ui::paintKnob (g, kPal, c, k.name, txt, lo[k.kind], hi[k.kind], (float) k.slider.valueToProportionOfLength (v));
    }
}

void SU1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / scale();
    proc.undoManager.beginNewTransaction();
    if (ui::powerRect().expanded (3.0f).contains (p)) { toggle ("power"); return; }
    if (ui::presetBarMouseDown (p, *this, proc.presets, proc.undoManager, scale(), [this] { repaint(); })) return;
    static const char* ids[4] = { "invL", "invR", "swap", "mono" };
    for (int i = 0; i < 6; ++i)
        if (buttonRect (i).expanded (3.0f).contains (p))
        {
            if (i < 4) toggle (ids[i]);
            else setChoice ("solo", solo() == i - 3 ? 0 : i - 3);          // MID and SIDE: click again to go back to stereo
            return;
        }
}
