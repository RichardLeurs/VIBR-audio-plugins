// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "VibrLogo.h"

namespace
{
// layout units: the design is 1200 wide (style.md: 20 side margin, 10 gap, name centre 55, rule 104, panels 30 below the rule)
constexpr float kW = 1200.0f, kH = 574.0f, kMargin = 20.0f, kGap = 10.0f, kRuleY = 104.0f;
constexpr float kStripY = 122.0f, kStripH = 84.0f, kAddW = 60.0f, kTileMin = 84.0f, kTileMax = 128.0f;
constexpr float kPanelW = 720.0f, kPanelTop = 250.0f, kPanelH = 250.0f, kKnobR = 42.0f, kKnobY = 112.0f, kPad = 5.0f;

const juce::Colour kBody (145, 187, 54), kDark (24, 33, 10), kPanel (26, 27, 30), kInk (238, 238, 232);

juce::Font font (float h, bool bold = false, bool italic = false)
{
    if (italic) return juce::Font (juce::FontOptions ("Avenir Next", "Heavy Italic", h));
    if (! bold) return juce::Font (juce::FontOptions ("Avenir Next", "Medium", h));
    return juce::Font (juce::FontOptions ("Avenir Next", h, bold ? juce::Font::bold : juce::Font::plain));
}

float textWidth (const juce::Font& f, const juce::String& s)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (f, s, 0.0f, 0.0f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}

void drawText (juce::Graphics& g, const juce::String& s, float cx, float cy, float size, juce::Colour c, bool bold = true,
               float kerning = 0.08f, juce::Justification j = juce::Justification::centred, bool italic = false)
{
    auto f = font (size, bold, italic).withExtraKerningFactor (kerning);
    g.setColour (c);
    g.setFont (f);
    const float w = textWidth (f, s) + 8.0f;
    juce::Rectangle<float> r (w, size * 1.6f);
    if (j == juce::Justification::centred) r = r.withCentre ({ cx, cy });
    else if (j == juce::Justification::centredRight) r = r.withCentre ({ cx - w * 0.5f, cy });
    else r = r.withCentre ({ cx + w * 0.5f, cy });
    g.drawText (s, r, j, false);
}
} // namespace

//==============================================================================
void ME1LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider&)
{
    const auto c = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).getCentre();
    const float R = (float) juce::jmin (w, h) * 0.5f - 5.0f;
    const auto circle = [&c] (float r) { return juce::Rectangle<float> (2.0f * r, 2.0f * r).withCentre (c); };

    g.setColour (juce::Colours::black.withAlpha (0.5f));                                            // shadow
    g.fillEllipse (circle (R).translated (1.0f, R * 0.14f));

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff35373d), c.x - R * 0.6f, c.y - R * 0.8f, juce::Colour (0xff08090a), c.x + R * 0.6f, c.y + R * 0.9f, false));
    g.fillEllipse (circle (R));                                                                     // matt black body
    for (int k = 0; k < 60; ++k)                                                                    // knurled grip edge
    {
        const float a = (float) k * juce::MathConstants<float>::twoPi / 60.0f, s = std::sin (a), co = std::cos (a);
        g.setColour ((k & 1) ? juce::Colours::black.withAlpha (0.45f) : juce::Colours::white.withAlpha (0.13f));
        g.drawLine (c.x + s * R * 0.86f, c.y - co * R * 0.86f, c.x + s * R * 0.995f, c.y - co * R * 0.995f, 1.1f);
    }
    g.setColour (juce::Colours::white.withAlpha (0.16f));
    g.drawEllipse (circle (R - 0.5f), 1.0f);

    const float cr = R * 0.74f;                                                                     // raised cap
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3f4147), c.x - cr * 0.7f, c.y - cr * 0.8f, juce::Colour (0xff121315), c.x + cr * 0.7f, c.y + cr * 0.9f, false));
    g.fillEllipse (circle (cr));
    g.setColour (juce::Colours::white.withAlpha (0.22f));
    g.drawEllipse (circle (cr), 1.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.16f), c.x - cr * 0.4f, c.y - cr * 0.5f, juce::Colours::transparentWhite, c.x, c.y, true));
    g.fillEllipse (circle (cr * 0.95f));

    const float a = startAngle + pos * (endAngle - startAngle), s = std::sin (a), co = std::cos (a);   // white pointer bar
    juce::Path bar;
    bar.addRoundedRectangle (-R * 0.05f, -R * 0.84f, R * 0.10f, R * 0.5f, R * 0.05f);
    g.setColour (juce::Colour (0xfff2f2ee));
    g.fillPath (bar, juce::AffineTransform::rotation (a).translated (c));
    (void) s; (void) co;
}

//==============================================================================
ME1AudioProcessorEditor::ME1AudioProcessorEditor (ME1AudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      powerAtt (*p.apvts.getParameter ("power"), [this] (float) { repaint(); }, nullptr)
{
    setLookAndFeel (&laf);
    const auto setup = [this] (juce::Slider& s, const char* id, std::unique_ptr<juce::SliderParameterAttachment>& att)
    {
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        s.setRotaryParameters (juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
        s.setMouseDragSensitivity (160);
        s.setDoubleClickReturnValue (true, 0.0);
        s.setScrollWheelEnabled (false);
        s.setPopupDisplayEnabled (true, true, this);
        s.setTextValueSuffix (" dB");
        addAndMakeVisible (s);
        att = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter (id), s, nullptr);
    };
    setup (inKnob, "inGain", inAtt);
    setup (outKnob, "outGain", outAtt);
    powerAtt.sendInitialUpdate();

    juce::Random rng (11);                                                                          // satin grain tile
    grain = juce::Image (juce::Image::ARGB, 128, 128, true);
    for (int y = 0; y < 128; ++y)
        for (int x = 0; x < 128; ++x)
            grain.setPixelAt (x, y, (rng.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (rng.nextFloat() * 0.05f));

    proc.addChangeListener (this);
    constrainer.setSizeLimits (720, 344, 2400, 1148);
    constrainer.setFixedAspectRatio ((double) kW / (double) kH);
    setConstrainer (&constrainer);
    setResizable (true, true);
    setSize ((int) kW, (int) kH);
    rebuildKnobs();
    ensureVisible (proc.selectedSlot);
    startTimerHz (30);
}

ME1AudioProcessorEditor::~ME1AudioProcessorEditor()
{
    stopTimer();
    proc.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void ME1AudioProcessorEditor::timerCallback()
{
    std::atomic<float>* src[4] = { &proc.meterIn[0], &proc.meterIn[1], &proc.meterOut[0], &proc.meterOut[1] };
    bool changed = false;
    for (int i = 0; i < 4; ++i)
    {
        const float peak = src[i]->exchange (0.0f);
        const float v = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (peak, -60.0f) + 60.0f) / 60.0f);
        const float old = level[i];
        level[i] = v > level[i] ? v : level[i] * 0.86f + v * 0.14f;                 // instant rise, fall in about 150 ms
        if (v >= hold[i]) { hold[i] = v; holdAge[i] = 0.0f; }
        else if ((holdAge[i] += 1.0f / 30.0f) > 1.2f) hold[i] = juce::jmax (v, hold[i] - 0.03f);
        changed = changed || std::abs (level[i] - old) > 0.002f;
    }
    if (changed) repaint (juce::Rectangle<float> (445.0f, 26.0f, 335.0f, 34.0f).transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer());
}

void ME1AudioProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    rebuildKnobs();
    ensureVisible (proc.selectedSlot);
    repaint();
}

//==============================================================================
float ME1AudioProcessorEditor::tileW() const
{
    const float visible = kW - 2.0f * kMargin - kAddW - kGap;
    return juce::jlimit (kTileMin, kTileMax, visible / (float) juce::jmax (1, proc.numSlots()) - kGap);
}

float ME1AudioProcessorEditor::maxScroll() const
{
    const float content = (float) proc.numSlots() * (tileW() + kGap) + kAddW;
    return juce::jmax (0.0f, content - (kW - 2.0f * kMargin));
}

juce::Rectangle<float> ME1AudioProcessorEditor::tileRect (int i) const
{
    const float tw = tileW(), x = kMargin + (float) i * (tw + kGap) - scroll;
    return { x, kStripY, i >= proc.numSlots() ? kAddW : tw, kStripH };
}

int ME1AudioProcessorEditor::tileAt (juce::Point<float> p) const
{
    if (p.x < kMargin - 4.0f || p.x > kW - kMargin + 4.0f) return -1;
    for (int i = 0; i <= proc.numSlots(); ++i)
        if (tileRect (i).expanded (kGap * 0.5f, 0.0f).contains (p)) return i;
    return -1;
}

void ME1AudioProcessorEditor::ensureVisible (int slot)
{
    if (slot < 0 || slot >= proc.numSlots()) { scroll = juce::jlimit (0.0f, maxScroll(), scroll); return; }
    const auto r = tileRect (slot);
    if (r.getX() < kMargin) scroll -= kMargin - r.getX();
    else if (r.getRight() > kW - kMargin) scroll += r.getRight() - (kW - kMargin);
    scroll = juce::jlimit (0.0f, maxScroll(), scroll);
}

juce::Rectangle<float> ME1AudioProcessorEditor::panelRect() const { return { (kW - kPanelW) * 0.5f, kPanelTop, kPanelW, kPanelH }; }

juce::Rectangle<float> ME1AudioProcessorEditor::pillRect() const
{
    const auto r = panelRect();
    const auto name = juce::String (me1::moduleInfo (proc.slotEffect (proc.selectedSlot)).name);
    const float w = textWidth (font (24.0f, true).withExtraKerningFactor (0.08f), name) + 38.0f;
    return juce::Rectangle<float> (w, 30.0f).withCentre ({ r.getCentreX(), r.getY() });
}

juce::Rectangle<float> ME1AudioProcessorEditor::syncRect() const
{
    const auto r = panelRect();
    return { r.getX() + 28.0f, r.getY() + 30.0f, 58.0f, 22.0f };
}

juce::Point<float> ME1AudioProcessorEditor::knobCentre (int k, int n) const
{
    const auto r = panelRect();
    return { r.getX() + r.getWidth() * ((float) k + 0.5f) / (float) n, r.getY() + kKnobY };
}

void ME1AudioProcessorEditor::rebuildKnobs()
{
    for (auto& k : knobs) k.reset();
    if (proc.numSlots() > 0)
    {
        proc.selectedSlot = juce::jlimit (0, proc.numSlots() - 1, proc.selectedSlot);
        const int i = proc.selectedSlot;
        const auto& info = me1::moduleInfo (proc.slotEffect (i));
        for (int k = 0; k < info.numParams; ++k)
        {
            auto spec = info.params[(size_t) k];
            const bool synced = k == info.syncParam && proc.slotSync (i);
            if (synced) { spec.min = 0.0f; spec.max = (float) me1::kNumDivisions - 1.0f; spec.def = (float) me1::kDefaultDivision; spec.centre = 0.0f; spec.decimals = 0; }
            auto s = std::make_unique<juce::Slider>();
            s->setSliderStyle (juce::Slider::RotaryVerticalDrag);
            s->setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
            s->setRotaryParameters (juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
            s->setMouseDragSensitivity (160);
            s->setScrollWheelEnabled (false);
            juce::NormalisableRange<double> range (spec.min, spec.max, std::pow (10.0, -spec.decimals));
            if (spec.centre > spec.min && spec.centre < spec.max) range.setSkewForCentre (spec.centre);
            s->setNormalisableRange (range);
            s->setDoubleClickReturnValue (true, spec.def);
            s->setValue (proc.slotParam (i, k), juce::dontSendNotification);
            s->setPopupDisplayEnabled (true, true, this);
            s->textFromValueFunction = [spec, synced] (double v) { return synced ? juce::String (me1::divisionName ((int) std::lround (v))) : me1::formatValue (spec, (float) v); };
            s->onValueChange = [this, i, k, sl = s.get()]
            {
                proc.setSlotParam (i, k, (float) sl->getValue());
                repaint (panelRect().expanded (2.0f).transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer());
            };
            addAndMakeVisible (*s);
            knobs[(size_t) k] = std::move (s);
        }
    }
    layoutKnobs();
}

void ME1AudioProcessorEditor::layoutKnobs()
{
    const float sc = scale();
    const auto place = [sc] (juce::Component& c, juce::Point<float> centre, float r)
    {
        const float d = 2.0f * (r + kPad) * sc;
        c.setBounds (juce::Rectangle<float> (d, d).withCentre (centre * sc).toNearestInt());
    };
    place (inKnob,  { 560.0f, 42.0f }, 20.0f);
    place (outKnob, { 660.0f, 42.0f }, 20.0f);
    if (proc.numSlots() == 0) return;
    const int n = me1::moduleInfo (proc.slotEffect (proc.selectedSlot)).numParams;
    for (int k = 0; k < n; ++k)
        if (knobs[(size_t) k] != nullptr) place (*knobs[(size_t) k], knobCentre (k, n), kKnobR);
}

void ME1AudioProcessorEditor::resized() { layoutKnobs(); }

//==============================================================================
void ME1AudioProcessorEditor::paintKnobScale (juce::Graphics& g, juce::Point<float> c, float knobR, bool dim) const
{
    const auto ink = kInk.withAlpha (dim ? 0.3f : 0.85f);
    const auto ring = juce::Rectangle<float> (2.0f * (knobR + 5.0f), 2.0f * (knobR + 5.0f)).withCentre (c);
    g.setColour (juce::Colours::black.withAlpha (0.5f));                                            // recessed ring
    g.fillEllipse (ring);
    g.setColour (juce::Colours::white.withAlpha (0.1f));
    g.drawEllipse (ring, 1.0f);
    for (int t = 0; t <= 10; ++t)                                                                   // 11 ticks over 270 degrees
    {
        const float a = juce::degreesToRadians (-135.0f + 27.0f * (float) t), s = std::sin (a), co = std::cos (a);
        const bool major = t == 0 || t == 5 || t == 10;
        const float r0 = knobR + 10.0f, r1 = r0 + (major ? 8.0f : 5.0f);
        g.setColour (ink);
        g.drawLine (c.x + s * r0, c.y - co * r0, c.x + s * r1, c.y - co * r1, major ? 2.0f : 1.2f);
    }
    const float lx = (knobR + 18.0f) * 0.7071f, ly = lx + 13.0f;                                   // below the end ticks
    drawText (g, "0", c.x - lx, c.y + ly, 13.0f, ink, false, 0.0f);
    drawText (g, "10", c.x + lx, c.y + ly, 13.0f, ink, false, 0.0f);
}

void ME1AudioProcessorEditor::paint (juce::Graphics& g)
{
    const float sc = scale();
    g.addTransform (juce::AffineTransform::scale (sc));

    // satin body
    g.setGradientFill (juce::ColourGradient (kBody.brighter (0.22f), 0.0f, 0.0f, kBody.darker (0.18f), kW, kH, false));
    g.fillRect (0.0f, 0.0f, kW, kH);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.14f), kW * 0.3f, kH * 0.1f, juce::Colours::transparentWhite, kW * 0.3f, kH * 0.75f, true));
    g.fillRect (0.0f, 0.0f, kW, kH);
    g.setTiledImageFill (grain, 0, 0, 1.0f);
    g.fillRect (0.0f, 0.0f, kW, kH);
    g.setColour (juce::Colours::white.withAlpha (0.28f));                                           // rim: bright top / left, soft bottom / right
    g.drawLine (0.0f, 1.5f, kW, 1.5f, 3.0f);
    g.drawLine (1.5f, 0.0f, 1.5f, kH, 3.0f);
    g.setColour (juce::Colours::black.withAlpha (0.12f));
    g.drawLine (0.0f, kH - 1.5f, kW, kH - 1.5f, 3.0f);
    g.drawLine (kW - 1.5f, 0.0f, kW - 1.5f, kH, 3.0f);

    // header (same as the other plugins: name left, two description lines right, power switch, rule)
    drawText (g, "ME-1", kMargin + 3.0f, 55.0f, 70.0f, kDark, true, 0.0f, juce::Justification::centredLeft, true);
    drawText (g, "MULTI EFFECT", kW - kMargin - 95.0f, 43.0f, 20.0f, kDark, false, 0.2f, juce::Justification::centredRight);
    drawText (g, "ENDLESS COMBINATIONS", kW - kMargin - 95.0f, 67.0f, 17.0f, kDark, false, 0.2f, juce::Justification::centredRight);
    drawText (g, "INPUT",  560.0f, 82.0f, 13.0f, kDark, true, 0.14f);
    drawText (g, "OUTPUT", 660.0f, 82.0f, 13.0f, kDark, true, 0.14f);

    {   // level meters: input (left of the INPUT knob) and output (right of the OUTPUT knob), 16 segments for -60 .. 0 dB, a hold tick
        const auto meter = [&] (float x0, int ch0)
        {
            g.setColour (kPanel);
            g.fillRoundedRectangle (juce::Rectangle<float> (x0 - 4.0f, 28.0f, 72.0f, 30.0f), 6.0f);
            for (int ch = 0; ch < 2; ++ch)
                for (int k = 0; k < 16; ++k)
                {
                    const float pos = ((float) k + 1.0f) / 16.0f, db = -60.0f + 60.0f * pos;
                    const bool lit = level[ch0 + ch] >= pos - 0.001f, held = std::abs (hold[ch0 + ch] - pos) < 0.5f / 16.0f;
                    const auto col = db > -3.0f ? juce::Colour (0xffff4d4d) : db > -12.0f ? juce::Colour (0xffe8c33a) : juce::Colour (0xff40d070);
                    g.setColour (lit ? col : held ? col.withAlpha (0.8f) : juce::Colours::white.withAlpha (0.07f));
                    g.fillRect (x0 + (float) k * 4.0f, 32.0f + (float) ch * 12.0f, 3.0f, 9.0f);
                }
        };
        meter (455.0f, 0);
        meter (702.0f, 2);
    }
    g.setColour (kDark.withAlpha (0.85f));
    g.fillRect (kMargin, kRuleY, kW - 2.0f * kMargin, 1.6f);

    {   // power switch: slot, chrome ball, OFF / ON legend
        const bool on = proc.apvts.getRawParameterValue ("power")->load() > 0.5f;
        const auto sr = juce::Rectangle<float> (33.0f, 17.0f).withCentre ({ kW - kMargin - 16.5f, 43.5f });
        g.setGradientFill (on ? juce::ColourGradient (juce::Colour (0xff0e3a1e), sr.getX(), sr.getY(), juce::Colour (0xff1a7a3e), sr.getX(), sr.getBottom(), false)
                              : juce::ColourGradient (juce::Colour (0xff111214), sr.getX(), sr.getY(), juce::Colour (0xff25272b), sr.getX(), sr.getBottom(), false));
        g.fillRoundedRectangle (sr, 8.5f);
        g.setColour (juce::Colours::white.withAlpha (0.22f));
        g.drawRoundedRectangle (sr, 8.5f, 1.0f);
        const auto ball = juce::Rectangle<float> (14.0f, 14.0f).withCentre ({ on ? sr.getRight() - 8.5f : sr.getX() + 8.5f, sr.getCentreY() });
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillEllipse (ball.translated (0.5f, 1.2f));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffafbfd), ball.getX() + 3.0f, ball.getY() + 2.0f, juce::Colour (0xff6e727e), ball.getRight(), ball.getBottom(), false));
        g.fillEllipse (ball);
        drawText (g, "OFF / ON", kW - kMargin, 68.5f, 13.0f, kDark, false, 0.12f, juce::Justification::centredRight);   // right edge on the rule end
    }

    // module strip
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (juce::Rectangle<float> (kMargin - 8.0f, kStripY - 10.0f, kW - 2.0f * kMargin + 16.0f, kStripH + 24.0f).toNearestInt());
        for (int i = 0; i < proc.numSlots(); ++i)
        {
            const auto info = me1::moduleInfo (proc.slotEffect (i));
            const juce::Colour col (me1::groupColour (info.group));
            const bool byp = proc.slotBypassed (i), sel = i == proc.selectedSlot;
            const auto r = tileRect (i);
            const auto d = (dragged && i == dragFrom) ? r.translated (0.0f, -4.0f) : r;

            g.setColour (sel ? kPanel.brighter (0.14f) : kPanel);
            g.fillRoundedRectangle (d, 13.0f);
            {
                juce::Graphics::ScopedSaveState bar (g);
                juce::Path shape;
                shape.addRoundedRectangle (d, 13.0f);
                g.reduceClipRegion (shape);
                g.setColour (col.withAlpha (byp ? 0.25f : 1.0f));
                g.fillRect (d.withHeight (7.0f));
            }
            g.setColour (sel ? col.withAlpha (byp ? 0.5f : 1.0f) : kInk.withAlpha (0.25f));
            g.drawRoundedRectangle (d.reduced (sel ? 1.2f : 0.6f), 13.0f, sel ? 2.4f : 1.0f);

            // name, shrunk to fit the tile
            float size = 17.0f;
            while (size > 10.0f && textWidth (font (size, true).withExtraKerningFactor (0.04f), info.name) > d.getWidth() - 14.0f) size -= 0.5f;
            drawText (g, info.name, d.getCentreX(), d.getY() + 45.0f, size, byp ? kInk.withAlpha (0.4f) : col, true, 0.04f);
            drawText (g, me1::groupName (info.group), d.getCentreX(), d.getY() + 67.0f, 9.5f, kInk.withAlpha (byp ? 0.25f : 0.5f), false, 0.06f);

            // bypass lamp (top left), slot number, remove cross (top right)
            const auto lamp = juce::Rectangle<float> (11.0f, 11.0f).withCentre ({ d.getX() + 17.0f, d.getY() + 21.0f });
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff0f2f6), lamp.getX(), lamp.getY(), juce::Colour (0xff5b5f6a), lamp.getRight(), lamp.getBottom(), false));
            g.fillEllipse (lamp.expanded (3.0f));
            g.setColour (juce::Colours::black);
            g.fillEllipse (lamp.expanded (0.5f));
            if (! byp) { g.setColour (juce::Colour (0xff40d070).withAlpha (0.22f)); g.fillEllipse (lamp.expanded (5.0f)); }
            g.setColour (byp ? juce::Colour (0xff1b3a25) : juce::Colour (0xff4cf080));
            g.fillEllipse (lamp.reduced (1.0f));
            drawText (g, juce::String (i + 1), d.getX() + 33.0f, d.getY() + 21.0f, 11.0f, kInk.withAlpha (0.4f), true, 0.0f, juce::Justification::centredLeft);
            const auto x = juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ d.getRight() - 16.0f, d.getY() + 21.0f });
            g.setColour (kInk.withAlpha (0.5f));
            g.drawLine (x.getX(), x.getY(), x.getRight(), x.getBottom(), 1.5f);
            g.drawLine (x.getX(), x.getBottom(), x.getRight(), x.getY(), 1.5f);

            if (i < proc.numSlots() - 1)                                                            // signal arrow
            {
                const float ax = d.getRight() + 1.0f, ay = d.getCentreY() + 8.0f;
                juce::Path a; a.addTriangle (ax, ay - 4.0f, ax, ay + 4.0f, ax + 7.0f, ay);
                g.setColour (kDark.withAlpha (0.7f));
                g.fillPath (a);
            }
        }
        const auto r = tileRect (proc.numSlots());                                                  // add tile
        g.setColour (kDark.withAlpha (0.18f));
        g.fillRoundedRectangle (r, 13.0f);
        g.setColour (kDark.withAlpha (0.7f));
        g.drawRoundedRectangle (r.reduced (0.9f), 13.0f, 1.8f);
        g.drawLine (r.getCentreX() - 11.0f, r.getCentreY(), r.getCentreX() + 11.0f, r.getCentreY(), 3.0f);
        g.drawLine (r.getCentreX(), r.getCentreY() - 11.0f, r.getCentreX(), r.getCentreY() + 11.0f, 3.0f);
    }

    g.setColour (kDark.withAlpha (0.85f));                                                          // rule under the strip
    g.fillRect (kMargin, 222.0f, kW - 2.0f * kMargin, 1.6f);

    // the selected module
    const auto r = panelRect();
    if (proc.numSlots() == 0)
    {
        drawText (g, "THE CHAIN IS EMPTY. CLICK + TO ADD A MODULE.", r.getCentreX(), r.getCentreY(), 18.0f, kDark, false, 0.12f);
    }
    else
    {
        const auto& info = me1::moduleInfo (proc.slotEffect (proc.selectedSlot));
        const bool byp = proc.slotBypassed (proc.selectedSlot);
        g.setColour (kPanel);
        g.fillRoundedRectangle (r, 24.0f);
        g.setColour (kInk.withAlpha (0.55f));
        g.drawRoundedRectangle (r.reduced (0.5f), 24.0f, 1.4f);
        const auto pill = pillRect();
        g.setColour (kPanel);
        g.fillRoundedRectangle (pill, 15.0f);
        const juce::Colour gcol (me1::groupColour (info.group));
        drawText (g, info.name, pill.getCentreX(), pill.getCentreY(), 24.0f, byp ? gcol.withAlpha (0.4f) : gcol, true, 0.08f);
        drawText (g, info.description, r.getCentreX(), r.getY() + 40.0f, 13.0f, kInk.withAlpha (0.55f), false, 0.14f);

        if (info.syncParam >= 0)                                                                    // tempo sync button
        {
            const bool on = proc.slotSync (proc.selectedSlot);
            const auto b = syncRect();
            if (on) { g.setColour (gcol.withAlpha (byp ? 0.4f : 1.0f)); g.fillRoundedRectangle (b, 8.0f); }
            g.setColour (on ? gcol : kInk.withAlpha (0.45f));
            g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.2f);
            drawText (g, "SYNC", b.getCentreX(), b.getCentreY(), 12.0f, on ? kPanel : kInk.withAlpha (0.7f), true, 0.14f);
        }

        for (int k = 0; k < info.numParams; ++k)
        {
            const auto c = knobCentre (k, info.numParams);
            paintKnobScale (g, c, kKnobR, byp);
            drawText (g, info.params[k].name, c.x, r.getY() + 196.0f, 17.0f, kInk.withAlpha (byp ? 0.35f : 0.95f), true, 0.14f);
            const float v = knobs[(size_t) k] != nullptr ? (float) knobs[(size_t) k]->getValue() : info.params[k].def;
            const bool synced = k == info.syncParam && proc.slotSync (proc.selectedSlot);
            drawText (g, synced ? juce::String (me1::divisionName ((int) std::lround (v))) : me1::formatValue (info.params[k], v), c.x, r.getY() + 220.0f, 15.0f, byp ? kInk.withAlpha (0.3f) : kBody, false, 0.04f);
        }
    }

    drawText (g, "Made by Richard Leurs | VIBR", kW - kMargin - 20.0f, kPanelTop + kPanelH + 35.0f, 13.0f, kDark, false, 0.06f, juce::Justification::centredRight);
    drawVibrLogo (g, juce::Rectangle<float> (13.0f, 13.0f).withCentre ({ kW - kMargin - 6.5f, kPanelTop + kPanelH + 35.0f }), kDark);
}

//==============================================================================
void ME1AudioProcessorEditor::addMenu()
{
    juce::PopupMenu menu;
    for (int gi = 0; gi < me1::numGroups(); ++gi)
    {
        juce::PopupMenu sub;
        for (int i = 0; i < me1::numModules(); ++i)
            if (me1::moduleInfo (i).group == gi)
                sub.addItem (i + 1, juce::String (me1::moduleInfo (i).name) + " - " + me1::moduleInfo (i).description);
        menu.addSubMenu (me1::groupName (gi), sub);
    }
    const auto area = (tileRect (proc.numSlots()) * scale()).toNearestInt();
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (localAreaToGlobal (area)),
                        [safe = juce::Component::SafePointer<ME1AudioProcessorEditor> (this)] (int result)
                        {
                            if (safe != nullptr && result > 0) safe->proc.addSlot (result - 1);
                        });
}

void ME1AudioProcessorEditor::slotMenu (int slot)
{
    juce::PopupMenu menu, replace;
    menu.addItem (1, proc.slotBypassed (slot) ? "Switch on" : "Bypass");
    menu.addItem (2, "Move left", slot > 0);
    menu.addItem (3, "Move right", slot < proc.numSlots() - 1);
    menu.addItem (4, "Remove");
    menu.addSeparator();
    for (int gi = 0; gi < me1::numGroups(); ++gi)
    {
        juce::PopupMenu sub;
        for (int i = 0; i < me1::numModules(); ++i)
            if (me1::moduleInfo (i).group == gi)
                sub.addItem (100 + i, juce::String (me1::moduleInfo (i).name) + " - " + me1::moduleInfo (i).description, true, i == proc.slotEffect (slot));
        replace.addSubMenu (me1::groupName (gi), sub);
    }
    menu.addSubMenu ("Replace with", replace);
    const auto area = (pillRect() * scale()).toNearestInt();
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (localAreaToGlobal (area)),
                        [safe = juce::Component::SafePointer<ME1AudioProcessorEditor> (this), slot] (int result)
                        {
                            if (safe == nullptr || result <= 0) return;
                            auto& pr = safe->proc;
                            if (result == 1) { pr.setSlotBypassed (slot, ! pr.slotBypassed (slot)); safe->repaint(); }
                            else if (result == 2) pr.moveSlot (slot, slot - 1);
                            else if (result == 3) pr.moveSlot (slot, slot + 1);
                            else if (result == 4) pr.removeSlot (slot);
                            else if (result >= 100) pr.replaceSlot (slot, result - 100);
                        });
}

void ME1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    dragFrom = -1; dragged = false;
    const auto p = e.position / scale();

    if (juce::Rectangle<float> (50.0f, 50.0f).withCentre ({ kW - kMargin - 16.5f, 43.5f }).contains (p))     // power switch
    {
        powerAtt.setValueAsCompleteGesture (proc.apvts.getRawParameterValue ("power")->load() > 0.5f ? 0.0f : 1.0f);
        return;
    }
    if (proc.numSlots() > 0 && pillRect().expanded (4.0f).contains (p)) { slotMenu (proc.selectedSlot); return; }
    if (proc.numSlots() > 0 && me1::moduleInfo (proc.slotEffect (proc.selectedSlot)).syncParam >= 0 && syncRect().expanded (4.0f).contains (p))
    {
        proc.setSlotSync (proc.selectedSlot, ! proc.slotSync (proc.selectedSlot));
        return;
    }

    const int t = tileAt (p);
    if (t < 0) return;
    if (t >= proc.numSlots()) { addMenu(); return; }
    const auto r = tileRect (t);
    if (p.x < r.getX() + 32.0f && p.y < r.getY() + 36.0f)                                                  // bypass lamp
    {
        proc.setSlotBypassed (t, ! proc.slotBypassed (t));
        repaint();
        return;
    }
    if (p.x > r.getRight() - 30.0f && p.y < r.getY() + 36.0f) { proc.removeSlot (t); return; }              // remove
    dragFrom = t;
    if (proc.selectedSlot != t) { proc.selectedSlot = t; rebuildKnobs(); }
    repaint();
}

void ME1AudioProcessorEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (dragFrom < 0) return;
    if (e.getDistanceFromDragStart() > 8) dragged = true;
    if (! dragged) return;
    const auto p = e.position / scale();
    const int over = tileAt ({ p.x, kStripY + kStripH * 0.5f });
    if (over >= 0 && over < proc.numSlots() && over != dragFrom)
    {
        proc.moveSlot (dragFrom, over);
        dragFrom = over;
    }
    repaint();
}

void ME1AudioProcessorEditor::mouseUp (const juce::MouseEvent&)
{
    dragFrom = -1; dragged = false;
    repaint();
}

void ME1AudioProcessorEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const auto p = e.position / scale();
    if (p.y < kStripY - 12.0f || p.y > kStripY + kStripH + 12.0f || maxScroll() <= 0.0f) return;
    const float d = std::abs (w.deltaX) > std::abs (w.deltaY) ? w.deltaX : w.deltaY;
    scroll = juce::jlimit (0.0f, maxScroll(), scroll - d * 300.0f);
    repaint();
}
