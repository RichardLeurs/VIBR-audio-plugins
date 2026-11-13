// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "VibrLogo.h"

namespace
{
// layout units: the design is 1200 x 690 (style.md: 20 side margin, 10 gap, name centre 55, rule 104, panels 30 below the rule, 69 above the bottom)
constexpr float kW = 1200.0f, kH = 690.0f, kMargin = 20.0f, kRuleY = 104.0f;
constexpr float kPanelY = 134.0f, kPanelH = 236.0f, kKnobR = 26.0f, kPad = 5.0f, kSeqY = 394.0f, kSeqH = 227.0f;
constexpr float kPanelX[5] = { 20.0f, 210.0f, 400.0f, 710.0f, 950.0f }, kPanelW[5] = { 180.0f, 180.0f, 300.0f, 230.0f, 230.0f };
constexpr const char* kPanelName[5] = { "FILTER", "SOURCE", "TONE", "MOTION", "OUTPUT" };

const juce::Colour kBody (199, 125, 74), kDark (36, 20, 10), kPanel (26, 27, 30), kInk (238, 238, 232), kIce (143, 211, 244);

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

juce::Rectangle<float> panelRect (int i) { return { kPanelX[i], kPanelY, kPanelW[i], kPanelH }; }
juce::Rectangle<float> seqRect() { return { kMargin, kSeqY, kW - 2.0f * kMargin, kSeqH }; }
juce::Rectangle<float> typeRect (int m) { const auto r = panelRect (0); return { r.getX() + 14.0f, r.getY() + 46.0f + (float) m * 38.0f, r.getWidth() - 28.0f, 30.0f }; }
juce::Rectangle<float> sourceRect (int m) { const auto r = panelRect (1); return { r.getX() + 14.0f, r.getY() + 46.0f + (float) m * 44.0f, r.getWidth() - 28.0f, 36.0f }; }
juce::Rectangle<float> syncRect() { const auto r = panelRect (1); return { r.getX() + 14.0f, r.getY() + 184.0f, r.getWidth() - 28.0f, 36.0f }; }
juce::Rectangle<float> powerRect() { return juce::Rectangle<float> (50.0f, 50.0f).withCentre ({ kW - kMargin - 16.5f, 43.5f }); }
juce::Rectangle<float> barsArea() { const auto r = seqRect(); return { r.getX() + 30.0f, r.getY() + 48.0f, r.getWidth() - 60.0f, 124.0f }; }
juce::Rectangle<float> barRect (int i)
{
    const auto a = barsArea();
    const float cell = a.getWidth() / (float) mf1::kNumSteps;
    return { a.getX() + cell * (float) i + (cell - 84.0f) * 0.5f, a.getY(), 84.0f, a.getHeight() };
}
juce::Point<float> knobCentre (int panel, int k, int n)
{
    const auto r = panelRect (panel);
    // the knobs (ticks and legends reach 44 each side) keep 14 clear of the panel edge: first and last centre 58 inside it
    const float inset = 58.0f, x = n > 1 ? r.getX() + inset + (r.getWidth() - 2.0f * inset) * (float) k / (float) (n - 1) : r.getCentreX();
    return { x, r.getY() + 100.0f };
}
} // namespace

//==============================================================================
void MF1LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider&)
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
MF1AudioProcessorEditor::MF1AudioProcessorEditor (MF1AudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      knobs { { { "cutoff", 2, 0, 3, "CUTOFF", 1, {}, {} },    { "res", 2, 1, 3, "RESONANCE", 0, {}, {} },
                { "depth", 2, 2, 3, "DEPTH", 0, {}, {} },
                { "m1", 3, 0, 2, nullptr, 2, {}, {} },         { "m2", 3, 1, 2, nullptr, 3, {}, {} },
                { "mix", 4, 0, 2, "MIX", 0, {}, {} },          { "level", 4, 1, 2, "LEVEL", 4, {}, {} } } },
      powerAtt  (*p.apvts.getParameter ("power"),  [this] (float) { repaint(); }, nullptr),
      typeAtt   (*p.apvts.getParameter ("type"),   [this] (float) { repaint(); }, nullptr),
      sourceAtt (*p.apvts.getParameter ("source"), [this] (float) { repaint(); }, nullptr),
      syncAtt   (*p.apvts.getParameter ("sync"),   [this] (float) { repaint(); }, nullptr)
{
    setLookAndFeel (&laf);
    for (auto& k : knobs)
    {
        auto& s = k.slider;
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        s.setRotaryParameters (juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
        s.setMouseDragSensitivity (160);
        s.setScrollWheelEnabled (false);
        s.setPopupDisplayEnabled (true, true, this);
        s.setTextValueSuffix (k.kind == 4 ? " dB" : " %");
        auto* par = proc.apvts.getParameter (k.id);
        s.setDoubleClickReturnValue (true, par->convertFrom0to1 (par->getDefaultValue()));
        addAndMakeVisible (s);
        k.att = std::make_unique<juce::SliderParameterAttachment> (*dynamic_cast<juce::RangedAudioParameter*> (par), s, nullptr);
        s.onValueChange = [this] { repaint(); };
    }
    powerAtt.sendInitialUpdate();
    typeAtt.sendInitialUpdate();
    sourceAtt.sendInitialUpdate();
    syncAtt.sendInitialUpdate();

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
    startTimerHz (30);
}

MF1AudioProcessorEditor::~MF1AudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void MF1AudioProcessorEditor::timerCallback()
{
    const int st = proc.currentStep.load();
    if (st != lastStep)
    {
        lastStep = st;
        repaint (seqRect().expanded (4.0f).transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer());
    }
}

void MF1AudioProcessorEditor::toggle (const char* id)
{
    auto* par = proc.apvts.getParameter (id);
    par->beginChangeGesture();
    par->setValueNotifyingHost (par->getValue() > 0.5f ? 0.0f : 1.0f);
    par->endChangeGesture();
}

void MF1AudioProcessorEditor::layoutKnobs()
{
    const float sc = scale(), d = 2.0f * (kKnobR + kPad) * sc;
    for (auto& k : knobs)
        k.slider.setBounds (juce::Rectangle<float> (d, d).withCentre (knobCentre (k.panel, k.index, k.count) * sc).toNearestInt());
}

void MF1AudioProcessorEditor::resized() { layoutKnobs(); }

//==============================================================================
void MF1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));

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

    // header: name left, two description lines right, power switch, rule
    drawText (g, "MF-1", kMargin + 3.0f, 55.0f, 70.0f, kDark, true, 0.0f, juce::Justification::centredLeft, true);
    drawText (g, "MOTION FILTER", kW - kMargin - 95.0f, 43.0f, 20.0f, kDark, false, 0.2f, juce::Justification::centredRight);
    drawText (g, "RHYTHMIC MOVEMENT", kW - kMargin - 95.0f, 67.0f, 17.0f, kDark, false, 0.2f, juce::Justification::centredRight);
    g.setColour (kDark.withAlpha (0.85f));
    g.fillRect (kMargin, kRuleY, kW - 2.0f * kMargin, 1.6f);

    {   // power switch: slot, chrome ball, OFF / ON legend
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

    // panels: charcoal, light outline, title pill on the top edge
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
    for (int i = 0; i < 5; ++i) drawPanel (panelRect (i), kPanelName[i]);
    drawPanel (seqRect(), "SEQUENCER");

    const auto button = [&] (juce::Rectangle<float> b, const char* label, bool sel, float size)
    {
        if (sel) { g.setColour (kBody); g.fillRoundedRectangle (b, 10.0f); }
        g.setColour (sel ? kBody : kInk.withAlpha (0.35f));
        g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, 1.2f);
        drawText (g, label, b.getCentreX(), b.getCentreY(), size, sel ? kDark : kInk.withAlpha (0.85f), true, 0.1f);
    };
    const int type = choice ("type"), source = choice ("source");
    for (int m = 0; m < mf1::kNumTypes; ++m) button (typeRect (m), mf1::typeName (m), m == type, 15.0f);
    for (int m = 0; m < mf1::kNumSources; ++m) button (sourceRect (m), mf1::sourceName (m), m == source, 16.0f);
    {
        const bool on = isOn ("sync");
        const auto b = syncRect();
        if (on) { g.setColour (kIce.withAlpha (0.22f)); g.fillRoundedRectangle (b.expanded (3.0f), 13.0f); g.setColour (kIce); g.fillRoundedRectangle (b, 10.0f); }
        g.setColour (on ? kIce : kInk.withAlpha (0.5f));
        g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, 1.4f);
        drawText (g, "SYNC", b.getCentreX(), b.getCentreY(), 16.0f, on ? kDark : kInk.withAlpha (0.9f), true, 0.16f);
    }

    // knobs: recessed ring, 11 ticks, legends, name and value
    for (auto& k : knobs)
    {
        const auto c = knobCentre (k.panel, k.index, k.count);
        const auto ring = juce::Rectangle<float> (2.0f * (kKnobR + 5.0f), 2.0f * (kKnobR + 5.0f)).withCentre (c);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillEllipse (ring);
        g.setColour (juce::Colours::white.withAlpha (0.1f));
        g.drawEllipse (ring, 1.0f);
        for (int t = 0; t <= 10; ++t)
        {
            const float a = juce::degreesToRadians (-135.0f + 27.0f * (float) t), s = std::sin (a), co = std::cos (a);
            const bool major = t == 0 || t == 5 || t == 10;
            const float r0 = kKnobR + 10.0f, r1 = r0 + (major ? 8.0f : 5.0f);
            g.setColour (kInk.withAlpha (0.85f));
            g.drawLine (c.x + s * r0, c.y - co * r0, c.x + s * r1, c.y - co * r1, major ? 2.0f : 1.2f);
        }
        const float lx = (kKnobR + 18.0f) * 0.7071f, ly = lx + 13.0f;
        const char* lo = k.kind == 4 ? "-24" : k.kind == 1 ? "LOW" : "0";
        const char* hi = k.kind == 4 ? "+12" : k.kind == 1 ? "HIGH" : "10";
        drawText (g, lo, c.x - lx, c.y + ly, 12.0f, kInk.withAlpha (0.85f), false, 0.0f);
        drawText (g, hi, c.x + lx, c.y + ly, 12.0f, kInk.withAlpha (0.85f), false, 0.0f);

        drawText (g, k.name != nullptr ? k.name : mf1::motionName (source, k.index), c.x, kPanelY + 178.0f, 16.0f, kInk.withAlpha (0.95f), true, 0.12f);
        drawText (g, knobText (k.kind, k.slider.getValue()), c.x, kPanelY + 206.0f, 15.0f, kBody, false, 0.04f);
    }

    // sequencer: eight bars (dim unless the source is STEPS), the playing step lit
    {
        const bool active = source == mf1::Steps;
        const int cur = proc.currentStep.load();
        for (int i = 0; i < mf1::kNumSteps; ++i)
        {
            const auto b = barRect (i);
            const float v = proc.apvts.getRawParameterValue ("step" + juce::String (i + 1))->load() * 0.01f;
            const bool now = active && i == cur;
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.fillRoundedRectangle (b, 8.0f);
            g.setColour (juce::Colours::white.withAlpha (0.1f));
            g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.0f);
            const float h = juce::jmax (8.0f, v * b.getHeight());
            const auto fill = juce::Rectangle<float> (b.getX() + 4.0f, b.getBottom() - h - 0.0f, b.getWidth() - 8.0f, h - 4.0f > 4.0f ? h - 4.0f : 4.0f).withBottomY (b.getBottom() - 4.0f);
            if (now) { g.setColour (kBody.withAlpha (0.25f)); g.fillRoundedRectangle (fill.expanded (4.0f), 8.0f); }
            g.setColour (active ? (now ? kBody.brighter (0.35f) : kBody) : kBody.withAlpha (0.35f));
            g.fillRoundedRectangle (fill, 5.0f);
            drawText (g, juce::String (i + 1), b.getCentreX(), b.getBottom() + 18.0f, 14.0f, now ? kBody : kInk.withAlpha (0.55f), true, 0.1f);
        }
    }

    drawText (g, "Made by Richard Leurs | VIBR", kW - kMargin - 20.0f, kSeqY + kSeqH + 35.0f, 13.0f, kDark, false, 0.06f, juce::Justification::centredRight);
    drawVibrLogo (g, juce::Rectangle<float> (13.0f, 13.0f).withCentre ({ kW - kMargin - 6.5f, kSeqY + kSeqH + 35.0f }), kDark);
}

juce::String MF1AudioProcessorEditor::knobText (int kind, double v) const
{
    const float f = (float) v * 0.01f;
    switch (kind)
    {
        case 1: { const float hz = mf1::cutoffHz (f); return hz >= 1000.0f ? juce::String (hz * 0.001f, 1) + " kHz" : juce::String ((int) std::lround (hz)) + " Hz"; }
        case 2:
            if (choice ("source") == mf1::Envelope) return juce::String ((int) std::lround (v)) + " %";
            if (isOn ("sync")) return mf1::divisionName (mf1::divisionIndex (f));
            return juce::String (mf1::rateHz (f), 2) + " Hz";
        case 3:
            if (choice ("source") == mf1::Lfo) return mf1::shapeName (f);
            if (choice ("source") == mf1::Envelope) return juce::String ((int) std::lround (mf1::decayMs (f))) + " ms";
            return juce::String ((int) std::lround (v)) + " %";
        case 4: return juce::String::formatted ("%+d dB", (int) std::lround (v));
        default: return juce::String ((int) std::lround (v)) + " %";
    }
}

void MF1AudioProcessorEditor::setChoice (const char* id, int index)
{
    auto* par = proc.apvts.getParameter (id);
    par->beginChangeGesture();
    par->setValueNotifyingHost (par->convertTo0to1 ((float) index));
    par->endChangeGesture();
}

void MF1AudioProcessorEditor::setStep (int step, float value01, bool begin)
{
    auto* par = proc.apvts.getParameter ("step" + juce::String (step + 1));
    if (begin) par->beginChangeGesture();
    par->setValueNotifyingHost (std::round (juce::jlimit (0.0f, 1.0f, value01) * 100.0f) / 100.0f);
}

void MF1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / scale();
    if (powerRect().contains (p)) { toggle ("power"); return; }
    if (syncRect().expanded (3.0f).contains (p)) { toggle ("sync"); return; }
    for (int m = 0; m < mf1::kNumTypes; ++m)
        if (typeRect (m).expanded (3.0f).contains (p)) { setChoice ("type", m); return; }
    for (int m = 0; m < mf1::kNumSources; ++m)
        if (sourceRect (m).expanded (3.0f).contains (p)) { setChoice ("source", m); return; }
    if (seqRect().contains (p))
        for (int i = 0; i < mf1::kNumSteps; ++i)
        {
            const auto b = barRect (i);
            if (p.x >= b.getX() - 6.0f && p.x <= b.getRight() + 6.0f && p.y >= b.getY() - 8.0f && p.y <= b.getBottom() + 8.0f)
            {
                dragStep = i;
                setStep (i, 1.0f - (p.y - b.getY()) / b.getHeight(), true);
                return;
            }
        }
}

void MF1AudioProcessorEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (dragStep < 0) return;
    const auto p = e.position / scale();
    const auto b = barRect (dragStep);
    setStep (dragStep, 1.0f - (p.y - b.getY()) / b.getHeight(), false);
}

void MF1AudioProcessorEditor::mouseUp (const juce::MouseEvent&)
{
    if (dragStep >= 0) proc.apvts.getParameter ("step" + juce::String (dragStep + 1))->endChangeGesture();
    dragStep = -1;
}
