// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "VibrLogo.h"

namespace
{
// layout units: the design is 1200 x 690 (style.md: 20 side margin, 10 gap, name centre 55, rule 104, panels 30 below the rule, 69 above the bottom)
constexpr float kW = 1200.0f, kH = 690.0f, kMargin = 20.0f, kRuleY = 104.0f, kPad = 5.0f;
constexpr float kMatX = 20.0f, kMatY = 134.0f, kMatW = 740.0f, kMatH = 486.0f;
constexpr float kRowY0 = 222.0f, kRowPitch = 82.0f, kColX0 = 350.0f, kColPitch = 100.0f;

const juce::Colour kBody (82, 201, 107), kDark (11, 35, 18), kPanel (26, 27, 30), kInk (238, 238, 232);

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

juce::Rectangle<float> matrixRect() { return { kMatX, kMatY, kMatW, kMatH }; }
juce::Rectangle<float> filterRect() { return { 770.0f, 134.0f, 410.0f, 236.0f }; }
juce::Rectangle<float> outputRect() { return { 770.0f, 394.0f, 410.0f, 226.0f }; }
juce::Rectangle<float> typeRect (int m) { const auto r = filterRect(); return { r.getRight() - 140.0f, r.getY() + 44.0f + (float) m * 46.0f, 120.0f, 36.0f }; }
juce::Rectangle<float> shapeRect (int lfo) { return juce::Rectangle<float> (74.0f, 26.0f).withCentre ({ 292.0f, kRowY0 + kRowPitch * (float) (lfo == 1 ? 1 : 2) }); }
juce::Rectangle<float> syncRect() { return juce::Rectangle<float> (88.0f, 26.0f).withCentre ({ 225.0f, 164.0f }); }
juce::Rectangle<float> powerRect() { return juce::Rectangle<float> (50.0f, 50.0f).withCentre ({ kW - kMargin - 16.5f, 43.5f }); }
} // namespace

//==============================================================================
void VC1LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider&)
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

    const float a = startAngle + pos * (endAngle - startAngle);                                     // white pointer bar
    juce::Path bar;
    bar.addRoundedRectangle (-R * 0.05f, -R * 0.84f, R * 0.10f, R * 0.5f, R * 0.05f);
    g.setColour (juce::Colour (0xfff2f2ee));
    g.fillPath (bar, juce::AffineTransform::rotation (a).translated (c));
}

//==============================================================================
VC1AudioProcessorEditor::VC1AudioProcessorEditor (VC1AudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      powerAtt (*p.apvts.getParameter ("power"), [this] (float) { repaint(); }, nullptr),
      syncAtt (*p.apvts.getParameter ("sync"), [this] (float) { repaint(); }, nullptr),
      typeAtt  (*p.apvts.getParameter ("type"),  [this] (float) { repaint(); }, nullptr),
      shape1Att (*p.apvts.getParameter ("shape1"), [this] (float) { repaint(); }, nullptr),
      shape2Att (*p.apvts.getParameter ("shape2"), [this] (float) { repaint(); }, nullptr)
{
    setLookAndFeel (&laf);
    // the sources' own knobs (kind: 0 percent, 1 cutoff, 2 LFO rate, 3 random rate, 4 release, 5 matrix depth, 6 dB, 7 sensitivity)
    const auto add = [this] (const char* id, int kind, float cx, float cy, float r, const char* name)
    {
        auto k = std::make_unique<Knob>();
        k->id = id; k->kind = kind; k->cx = cx; k->cy = cy; k->r = r; k->name = name;
        auto& s = k->slider;
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        s.setRotaryParameters (juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
        s.setMouseDragSensitivity (160);
        s.setScrollWheelEnabled (false);
        s.setPopupDisplayEnabled (true, true, this);
        s.setTextValueSuffix (kind == 6 ? " dB" : " %");
        auto* par = proc.apvts.getParameter (id);
        s.setDoubleClickReturnValue (true, par->convertFrom0to1 (par->getDefaultValue()));
        addAndMakeVisible (s);
        k->att = std::make_unique<juce::SliderParameterAttachment> (*dynamic_cast<juce::RangedAudioParameter*> (par), s, nullptr);
        s.onValueChange = [this] { repaint(); };
        knobs.push_back (std::move (k));
    };
    add ("envsens", 7, 190.0f, kRowY0, 20.0f, "SENS");        add ("envrel", 4, 260.0f, kRowY0, 20.0f, "REL");
    add ("rate1", 2, 190.0f, kRowY0 + kRowPitch, 20.0f, "RATE");
    add ("rate2", 2, 190.0f, kRowY0 + 2.0f * kRowPitch, 20.0f, "RATE");
    add ("rand", 3, 190.0f, kRowY0 + 3.0f * kRowPitch, 20.0f, "RATE");
    for (int s = 0; s < vc1::kNumSources; ++s)
        for (int d = 0; d < vc1::kNumDests; ++d)
            add (matrixIds[s][d], 5, kColX0 + kColPitch * (float) d, kRowY0 + kRowPitch * (float) s, 20.0f, nullptr);
    add ("cutoff", 1, 860.0f, 234.0f, 30.0f, "CUTOFF");       add ("res", 0, 970.0f, 234.0f, 30.0f, "RESONANCE");
    add ("xmod", 0, 838.0f, 490.0f, 26.0f, "X-MOD");          add ("mix", 0, 975.0f, 490.0f, 26.0f, "MIX");    add ("level", 6, 1112.0f, 490.0f, 26.0f, "LEVEL");
    powerAtt.sendInitialUpdate(); syncAtt.sendInitialUpdate(); typeAtt.sendInitialUpdate(); shape1Att.sendInitialUpdate(); shape2Att.sendInitialUpdate();

    juce::Random rng (11);                                                                          // satin grain tile
    grain = juce::Image (juce::Image::ARGB, 128, 128, true);
    for (int y = 0; y < 128; ++y)
        for (int x = 0; x < 128; ++x)
            grain.setPixelAt (x, y, (rng.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (rng.nextFloat() * 0.05f));

    constrainer.setSizeLimits (720, 414, 2400, 1380);
    constrainer.setFixedAspectRatio ((double) kW / (double) kH);
    setConstrainer (&constrainer);
    setResizable (true, true);
    setSize ((int) kW, (int) kH);
    layoutKnobs();
    startTimerHz (20);
}

VC1AudioProcessorEditor::~VC1AudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void VC1AudioProcessorEditor::timerCallback()
{
    repaint (matrixRect().withWidth (140.0f).transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer());      // the source lamps
}

void VC1AudioProcessorEditor::toggle (const char* id)
{
    auto* par = proc.apvts.getParameter (id);
    par->beginChangeGesture();
    par->setValueNotifyingHost (par->getValue() > 0.5f ? 0.0f : 1.0f);
    par->endChangeGesture();
}

void VC1AudioProcessorEditor::setChoice (const char* id, int index)
{
    auto* par = proc.apvts.getParameter (id);
    par->beginChangeGesture();
    par->setValueNotifyingHost (par->convertTo0to1 ((float) index));
    par->endChangeGesture();
}

void VC1AudioProcessorEditor::layoutKnobs()
{
    const float sc = scale();
    for (auto& k : knobs)
    {
        const float d = 2.0f * (k->r + kPad) * sc;
        k->slider.setBounds (juce::Rectangle<float> (d, d).withCentre (juce::Point<float> (k->cx, k->cy) * sc).toNearestInt());
    }
}

void VC1AudioProcessorEditor::resized() { layoutKnobs(); }

juce::String VC1AudioProcessorEditor::knobText (int kind, double v) const
{
    const float f = (float) v * 0.01f;
    switch (kind)
    {
        case 1: { const float hz = vc1::cutoffHz (f); return hz >= 1000.0f ? juce::String (hz * 0.001f, 1) + " kHz" : juce::String ((int) std::lround (hz)) + " Hz"; }
        case 2: return isOn ("sync") ? juce::String (vc1::divisionName (vc1::divisionIndex (f))) : juce::String (vc1::lfoHz (f), 2) + " Hz";
        case 3: return isOn ("sync") ? juce::String (vc1::divisionName (vc1::divisionIndex (f))) : juce::String (vc1::randomHz (f), 1) + " /s";
        case 4: return juce::String ((int) std::lround (vc1::releaseMs (f))) + " ms";
        case 5: { const int d = (int) std::lround ((v - 50.0) * 2.0); return d == 0 ? juce::String ("0") : juce::String::formatted ("%+d", d); }
        case 6: return juce::String::formatted ("%+d dB", (int) std::lround (v));
        case 7: return juce::String (vc1::sensGain (f), 1) + "x";
        default: return juce::String ((int) std::lround (v)) + " %";
    }
}

//==============================================================================
void VC1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));

    // satin body
    g.setGradientFill (juce::ColourGradient (kBody.brighter (0.22f), 0.0f, 0.0f, kBody.darker (0.18f), kW, kH, false));
    g.fillRect (0.0f, 0.0f, kW, kH);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.14f), kW * 0.3f, kH * 0.1f, juce::Colours::transparentWhite, kW * 0.3f, kH * 0.75f, true));
    g.fillRect (0.0f, 0.0f, kW, kH);
    g.setTiledImageFill (grain, 0, 0, 1.0f);
    g.fillRect (0.0f, 0.0f, kW, kH);
    g.setColour (juce::Colours::white.withAlpha (0.28f));
    g.drawLine (0.0f, 1.5f, kW, 1.5f, 3.0f);
    g.drawLine (1.5f, 0.0f, 1.5f, kH, 3.0f);
    g.setColour (juce::Colours::black.withAlpha (0.12f));
    g.drawLine (0.0f, kH - 1.5f, kW, kH - 1.5f, 3.0f);
    g.drawLine (kW - 1.5f, 0.0f, kW - 1.5f, kH, 3.0f);

    // header
    drawText (g, "VC-1", kMargin + 3.0f, 55.0f, 70.0f, kDark, true, 0.0f, juce::Justification::centredLeft, true);
    drawText (g, "VOLTAGE CONTROLLER", kW - kMargin - 95.0f, 43.0f, 20.0f, kDark, false, 0.2f, juce::Justification::centredRight);
    drawText (g, "EXPRESSIVE MOTION", kW - kMargin - 95.0f, 67.0f, 17.0f, kDark, false, 0.2f, juce::Justification::centredRight);
    g.setColour (kDark.withAlpha (0.85f));
    g.fillRect (kMargin, kRuleY, kW - 2.0f * kMargin, 1.6f);
    {
        const bool on = isOn ("power");
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
        drawText (g, "OFF / ON", kW - kMargin, 68.5f, 13.0f, kDark, false, 0.12f, juce::Justification::centredRight);
    }

    const auto drawPanel = [&] (juce::Rectangle<float> r, const char* name)
    {
        g.setColour (kPanel);
        g.fillRoundedRectangle (r, 20.0f);
        g.setColour (kInk.withAlpha (0.55f));
        g.drawRoundedRectangle (r.reduced (0.5f), 20.0f, 1.4f);
        const float w = textWidth (font (18.0f, true).withExtraKerningFactor (0.08f), name) + 38.0f;
        const auto pill = juce::Rectangle<float> (w, 22.0f).withCentre ({ r.getCentreX(), r.getY() });
        g.setColour (kPanel);
        g.fillRoundedRectangle (pill, 11.0f);
        drawText (g, name, pill.getCentreX(), pill.getCentreY(), 18.0f, kBody, true, 0.08f);
    };
    drawPanel (matrixRect(), "MODULATION MATRIX");
    drawPanel (filterRect(), "FILTER");
    drawPanel (outputRect(), "CROSS / OUTPUT");

    {   // SYNC: the LFO 1, LFO 2 and RANDOM rate knobs become note lengths of the host tempo
        const bool on = isOn ("sync");
        const auto b = syncRect();
        if (on) { g.setColour (kBody); g.fillRoundedRectangle (b, 9.0f); }
        g.setColour (on ? kBody : kInk.withAlpha (0.5f));
        g.drawRoundedRectangle (b.reduced (0.5f), 9.0f, 1.3f);
        drawText (g, "SYNC", b.getCentreX(), b.getCentreY(), 14.0f, on ? kDark : kInk.withAlpha (0.9f), true, 0.14f);
    }

    // matrix: destination names on top, one row per source with its lamp and knobs
    for (int d = 0; d < vc1::kNumDests; ++d)
    {
        drawText (g, vc1::destName (d), kColX0 + kColPitch * (float) d, 178.0f, 15.0f, kBody, true, 0.14f);
        g.setColour (kInk.withAlpha (0.12f));
        g.drawLine (kColX0 + kColPitch * (float) d, 194.0f, kColX0 + kColPitch * (float) d, kMatY + kMatH - 14.0f, 1.0f);
    }
    const auto& eng = proc.getEngine();
    for (int s = 0; s < vc1::kNumSources; ++s)
    {
        const float y = kRowY0 + kRowPitch * (float) s;
        g.setColour (kInk.withAlpha (0.1f));
        if (s > 0) g.drawLine (kMatX + 16.0f, y - 41.0f, kMatX + kMatW - 16.0f, y - 41.0f, 1.0f);
        const float v = eng.sourceVis[s].load (std::memory_order_relaxed);
        const float lamp = s == vc1::Env || s == vc1::Midi ? v : std::fabs (v);
        g.setColour (kBody.withAlpha (0.12f + 0.88f * juce::jlimit (0.0f, 1.0f, lamp)));
        g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre ({ kMatX + 22.0f, y }));
        drawText (g, vc1::sourceName (s), kMatX + 38.0f, y, 15.0f, kInk.withAlpha (0.95f), true, 0.1f, juce::Justification::centredLeft);
    }
    drawText (g, "MOD WHEEL / EXPRESSION", 238.0f, kRowY0 + 4.0f * kRowPitch, 10.5f, kInk.withAlpha (0.5f), true, 0.06f);
    for (int lfo = 1; lfo <= 2; ++lfo)
    {
        const auto b = shapeRect (lfo);
        g.setColour (kInk.withAlpha (0.4f));
        g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.2f);
        drawText (g, vc1::shapeName (choice (lfo == 1 ? "shape1" : "shape2")), b.getCentreX(), b.getCentreY(), 11.0f, kInk.withAlpha (0.9f), true, 0.08f);
    }
    for (int m = 0; m < vc1::kNumTypes; ++m)
    {
        const auto b = typeRect (m);
        const bool sel = m == choice ("type");
        if (sel) { g.setColour (kBody); g.fillRoundedRectangle (b, 10.0f); }
        g.setColour (sel ? kBody : kInk.withAlpha (0.35f));
        g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, 1.2f);
        drawText (g, vc1::typeName (m), b.getCentreX(), b.getCentreY(), 14.0f, sel ? kDark : kInk.withAlpha (0.85f), true, 0.1f);
    }

    // knobs
    for (auto& kp : knobs)
    {
        const Knob& k = *kp;
        const juce::Point<float> c (k.cx, k.cy);
        const bool small = k.r < 24.0f;
        const auto ring = juce::Rectangle<float> (2.0f * (k.r + 5.0f), 2.0f * (k.r + 5.0f)).withCentre (c);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillEllipse (ring);
        g.setColour (juce::Colours::white.withAlpha (0.1f));
        g.drawEllipse (ring, 1.0f);
        const double v = k.slider.getValue();
        if (k.kind == 5)                                                                             // matrix depth: a mark at the top = 0
        {
            g.setColour (kInk.withAlpha (0.55f));
            g.drawLine (c.x, c.y - k.r - 10.0f, c.x, c.y - k.r - 5.0f, 1.6f);
            drawText (g, knobText (5, v), c.x, c.y + k.r + 16.0f, 12.0f, (int) std::lround (v) == 50 ? kInk.withAlpha (0.4f) : kBody, true, 0.04f);
            continue;
        }
        for (int t = 0; t <= 10; ++t)
        {
            const float a = juce::degreesToRadians (-135.0f + 27.0f * (float) t), s = std::sin (a), co = std::cos (a);
            const bool major = t == 0 || t == 5 || t == 10;
            const float r0 = k.r + 10.0f, r1 = r0 + (major ? (small ? 4.0f : 8.0f) : (small ? 2.5f : 5.0f));
            g.setColour (kInk.withAlpha (0.85f));
            g.drawLine (c.x + s * r0, c.y - co * r0, c.x + s * r1, c.y - co * r1, major ? 1.8f : 1.1f);
        }
        if (small)                                                                                 // name and value on one line (the next row's knob is close)
        {
            const auto name = juce::String (k.name) + " ";
            const auto val = knobText (k.kind, v);
            const auto fn = font (10.5f, true).withExtraKerningFactor (0.04f);
            const float wn = textWidth (fn, name), wv = textWidth (fn, val), x0 = c.x - (wn + wv) * 0.5f;
            drawText (g, name, x0 + wn * 0.5f, c.y + k.r + 15.0f, 10.5f, kInk.withAlpha (0.8f), true, 0.04f);
            drawText (g, val, x0 + wn + wv * 0.5f, c.y + k.r + 15.0f, 10.5f, kBody, false, 0.0f);
        }
        else
        {
            const float lx = (k.r + 18.0f) * 0.7071f, ly = lx + 13.0f;
            const char* lo = k.kind == 6 ? "-24" : "0";
            const char* hi = k.kind == 6 ? "+12" : "10";
            drawText (g, lo, c.x - lx, c.y + ly, 12.0f, kInk.withAlpha (0.85f), false, 0.0f);
            drawText (g, hi, c.x + lx, c.y + ly, 12.0f, kInk.withAlpha (0.85f), false, 0.0f);
            const float below = k.r > 28.0f ? 68.0f : (k.r > 25.0f ? 50.0f : 62.0f);
            drawText (g, k.name, c.x, c.y + below + 14.0f, 15.0f, kInk.withAlpha (0.95f), true, 0.1f);
            drawText (g, knobText (k.kind, v), c.x, c.y + below + 36.0f, 14.0f, kBody, false, 0.04f);
        }
    }

    drawText (g, "Made by Richard Leurs | VIBR", kW - kMargin - 20.0f, 620.0f + 35.0f, 13.0f, kDark, false, 0.06f, juce::Justification::centredRight);
    drawVibrLogo (g, juce::Rectangle<float> (13.0f, 13.0f).withCentre ({ kW - kMargin - 6.5f, 620.0f + 35.0f }), kDark);
}

void VC1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / scale();
    if (powerRect().contains (p)) { toggle ("power"); return; }
    if (syncRect().expanded (3.0f).contains (p)) { toggle ("sync"); return; }
    for (int m = 0; m < vc1::kNumTypes; ++m)
        if (typeRect (m).expanded (3.0f).contains (p)) { setChoice ("type", m); return; }
    for (int lfo = 1; lfo <= 2; ++lfo)
        if (shapeRect (lfo).expanded (3.0f).contains (p))
        {
            const char* id = lfo == 1 ? "shape1" : "shape2";
            setChoice (id, (choice (id) + 1) % vc1::kNumShapes);
            return;
        }
}
