// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "VibrLogo.h"

namespace
{
// layout units: the design is 1200 x 690 (style.md: 20 side margin, 10 gap, name centre 55, rule 104, panels 30 below the rule, 24 between the rows)
constexpr float kW = 1200.0f, kH = 690.0f, kMargin = 20.0f, kRuleY = 104.0f;
constexpr float kPanelY = 134.0f, kPanelH = 236.0f, kKnobR = 24.0f, kPad = 5.0f, kRow2Y = 394.0f, kRow2H = 227.0f, kKnobPitch = 116.0f;
// panels 0 .. 3: the four stages (row 1); panel 4: OUTPUT (row 2, right); the SHAPE display is the row 2 panel on the left
constexpr float kPanelX[5] = { 20.0f, 290.0f, 560.0f, 830.0f, 930.0f }, kPanelW[5] = { 260.0f, 260.0f, 260.0f, 350.0f, 250.0f };
constexpr const char* kPanelName[5] = { "BOOST", "TUBE", "TAPE", "CRUSH", "OUTPUT" };

const juce::Colour kBody (201, 54, 104), kDark (38, 6, 18), kPanel (26, 27, 30), kInk (238, 238, 232);

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

juce::Rectangle<float> panelRect (int i)
{
    if (i == 4) return { kPanelX[4], kRow2Y, kPanelW[4], kRow2H };
    return { kPanelX[i], kPanelY, kPanelW[i], kPanelH };
}
juce::Rectangle<float> shapePanelRect() { return { kMargin, kRow2Y, 900.0f, kRow2H }; }
// the vertical positions inside a panel: knob centre, name and value (row 1 panels are 236 high, OUTPUT is 227)
float knobOffset (int panel) { return panel == 4 ? 88.0f : 100.0f; }
float nameOffset (int panel) { return panel == 4 ? 150.0f : 178.0f; }
float valueOffset (int panel) { return panel == 4 ? 178.0f : 206.0f; }
juce::Rectangle<float> pillRect (int i)           // the title pill of a panel: for the four stages it is also the on / off switch
{
    const auto r = panelRect (i);
    const float w = textWidth (font (18.0f, true).withExtraKerningFactor (0.08f), kPanelName[i]) + 38.0f;
    return juce::Rectangle<float> (w, 22.0f).withCentre ({ r.getCentreX(), r.getY() });
}
juce::Rectangle<float> powerRect() { return juce::Rectangle<float> (50.0f, 50.0f).withCentre ({ kW - kMargin - 16.5f, 43.5f }); }
juce::Point<float> knobCentre (int panel, int k, int n)
{
    const auto r = panelRect (panel);
    // the knobs sit 116 apart around the middle of the panel (ticks and legends reach 42 each side)
    return { r.getCentreX() + kKnobPitch * ((float) k - 0.5f * (float) (n - 1)), r.getY() + knobOffset (panel) };
}
} // namespace

//==============================================================================
void SA1LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider&)
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
SA1AudioProcessorEditor::SA1AudioProcessorEditor (SA1AudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      knobs { { { "boostGain", 0, 0, 2, "GAIN", 0, {}, {} },    { "boostTone", 0, 1, 2, "TONE", 1, {}, {} },
                { "tubeDrive", 1, 0, 2, "DRIVE", 1, {}, {} },   { "tubeBias", 1, 1, 2, "BIAS", 1, {}, {} },
                { "tapeDrive", 2, 0, 2, "DRIVE", 1, {}, {} },   { "tapeWarmth", 2, 1, 2, "WARMTH", 1, {}, {} },
                { "bits", 3, 0, 3, "BITS", 2, {}, {} },         { "rate", 3, 1, 3, "RATE", 3, {}, {} },        { "crushMix", 3, 2, 3, "MIX", 1, {}, {} },
                { "mix", 4, 0, 2, "MIX", 1, {}, {} },           { "level", 4, 1, 2, "LEVEL", 4, {}, {} } } },
      powerAtt (*p.apvts.getParameter ("power"),   [this] (float) { repaint(); }, nullptr),
      boostAtt (*p.apvts.getParameter ("boostOn"), [this] (float) { repaint(); }, nullptr),
      tubeAtt  (*p.apvts.getParameter ("tubeOn"),  [this] (float) { repaint(); }, nullptr),
      tapeAtt  (*p.apvts.getParameter ("tapeOn"),  [this] (float) { repaint(); }, nullptr),
      crushAtt (*p.apvts.getParameter ("crushOn"), [this] (float) { repaint(); }, nullptr)
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
        auto* par = proc.apvts.getParameter (k.id);
        s.setDoubleClickReturnValue (true, par->convertFrom0to1 (par->getDefaultValue()));
        addAndMakeVisible (s);
        midiUI.add (s, k.id);
        k.att = std::make_unique<juce::SliderParameterAttachment> (*dynamic_cast<juce::RangedAudioParameter*> (par), s, nullptr);
        s.onValueChange = [this] { repaint(); };
    }
    {
        static const char* const stageIds[4] = { "boostOn", "tubeOn", "tapeOn", "crushOn" };
        for (int i = 0; i < 4; ++i)
        {
            stageTarget[i].onPress = [this, i] { toggle (stageIds[i]); };
            addAndMakeVisible (stageTarget[i]);
            midiUI.add (stageTarget[i], stageIds[i]);
        }
    }
    powerAtt.sendInitialUpdate(); boostAtt.sendInitialUpdate(); tubeAtt.sendInitialUpdate(); tapeAtt.sendInitialUpdate(); crushAtt.sendInitialUpdate();

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
    startTimerHz (10);
}

SA1AudioProcessorEditor::~SA1AudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void SA1AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    midiUI.paintMarks (g, kBody);
}

void SA1AudioProcessorEditor::timerCallback()
{
    if (midiUI.needsRepaint()) { midiUI.tick(); repaint(); }
    for (auto& k : knobs) k.slider.setAlpha (stageOn (k.panel) ? 1.0f : 0.4f);       // the knobs of a stage that is off are dimmed
}

void SA1AudioProcessorEditor::toggle (const char* id)
{
    auto* par = proc.apvts.getParameter (id);
    par->beginChangeGesture();
    par->setValueNotifyingHost (par->getValue() > 0.5f ? 0.0f : 1.0f);
    par->endChangeGesture();
}

void SA1AudioProcessorEditor::layoutKnobs()
{
    const float sc = scale(), d = 2.0f * (kKnobR + kPad) * sc;
    for (auto& k : knobs)
        k.slider.setBounds (juce::Rectangle<float> (d, d).withCentre (knobCentre (k.panel, k.index, k.count) * sc).toNearestInt());
    for (int i = 0; i < 4; ++i)
        stageTarget[i].setBounds (pillRect (i).expanded (6.0f, 8.0f).transformedBy (juce::AffineTransform::scale (sc)).toNearestInt());
}

void SA1AudioProcessorEditor::resized() { layoutKnobs(); }

//==============================================================================
// SHAPE display: on the left the transfer curve of the chain (input on the x axis, output on the y axis, the grey diagonal is "no effect"), on the
// right a 440 Hz sine (grey) and what the chain makes of it (colour): the saturation flattens and skews it, the crush turns it into steps.
void SA1AudioProcessorEditor::drawShape (juce::Graphics& g)
{
    sa1::Params p;
    p.boostOn = isOn ("boostOn");  p.boostGain = value ("boostGain");  p.tubeOn = isOn ("tubeOn");  p.tubeDrive = value ("tubeDrive") * 0.01f;
    p.tubeBias = value ("tubeBias") * 0.01f;  p.tapeOn = isOn ("tapeOn");  p.tapeDrive = value ("tapeDrive") * 0.01f;  p.crushOn = isOn ("crushOn");
    p.bits = value ("bits");  p.rate = value ("rate");  p.crushMix = value ("crushMix") * 0.01f;

    const auto panel = shapePanelRect();
    const float side = 150.0f;
    const juce::Rectangle<float> curve (panel.getX() + 56.0f, panel.getY() + 34.0f, side, side);
    const juce::Rectangle<float> wave (curve.getRight() + 70.0f, curve.getY(), panel.getRight() - curve.getRight() - 70.0f - 40.0f, side);

    for (const auto& r : { curve, wave })
    {
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (r.expanded (8.0f), 10.0f);
        g.setColour (kInk.withAlpha (0.12f));
        g.drawLine (r.getX(), r.getCentreY(), r.getRight(), r.getCentreY(), 1.0f);
    }
    g.setColour (kInk.withAlpha (0.12f));
    g.drawLine (curve.getCentreX(), curve.getY(), curve.getCentreX(), curve.getBottom(), 1.0f);
    g.setColour (kInk.withAlpha (0.3f));
    g.drawLine (curve.getX(), curve.getBottom(), curve.getRight(), curve.getY(), 1.2f);            // the diagonal: no effect

    const auto clampY = [] (float v) { return std::clamp (v, -1.2f, 1.2f); };
    juce::Path pc;
    for (int i = 0; i <= 240; ++i)
    {
        const float x = -1.0f + 2.0f * (float) i / 240.0f, y = clampY (sa1::shape (x, p));
        const juce::Point<float> pt (curve.getCentreX() + x * side * 0.5f, curve.getCentreY() - y * side * 0.5f);
        if (i == 0) pc.startNewSubPath (pt); else pc.lineTo (pt);
    }
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (curve.expanded (4.0f).toNearestInt());
        g.setColour (kBody);
        g.strokePath (pc, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    drawText (g, "TRANSFER", curve.getCentreX(), curve.getBottom() + 26.0f, 12.0f, kInk.withAlpha (0.55f), true, 0.18f);
    drawText (g, "IN", curve.getRight() - 6.0f, curve.getCentreY() + 12.0f, 10.0f, kInk.withAlpha (0.45f), true, 0.1f);
    drawText (g, "OUT", curve.getCentreX() + 16.0f, curve.getY() + 8.0f, 10.0f, kInk.withAlpha (0.45f), true, 0.1f);

    // the waveform: two cycles of a 440 Hz sine at 48 kHz, 0.7 peak; the crush holds each sample for 48000 / rate samples
    const int n = 218, hold = std::max (1, (int) std::lround (48000.0f / std::max (500.0f, p.rate)));
    juce::Path in, out;
    float held = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float x = 0.7f * std::sin (juce::MathConstants<float>::twoPi * 440.0f * (float) i / 48000.0f);
        float y = sa1::shape (x, p);
        if (p.crushOn)
        {
            if (i % hold == 0) held = sa1::crushCurve (sa1::shape (x, [&] { auto q = p; q.crushOn = false; return q; }()), p.bits);
            y = (sa1::shape (x, [&] { auto q = p; q.crushOn = false; return q; }())) + (held - sa1::shape (x, [&] { auto q = p; q.crushOn = false; return q; }())) * p.crushMix;
        }
        const float px = wave.getX() + wave.getWidth() * (float) i / (float) (n - 1);
        const juce::Point<float> a (px, wave.getCentreY() - x * wave.getHeight() * 0.5f), b (px, wave.getCentreY() - clampY (y) * wave.getHeight() * 0.5f);
        if (i == 0) { in.startNewSubPath (a); out.startNewSubPath (b); } else { in.lineTo (a); out.lineTo (b); }
    }
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (wave.expanded (4.0f).toNearestInt());
        g.setColour (kInk.withAlpha (0.45f));
        g.strokePath (in, juce::PathStrokeType (1.6f));
        g.setColour (kBody);
        g.strokePath (out, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    drawText (g, "WAVEFORM  (440 Hz SINE)", wave.getCentreX(), wave.getBottom() + 26.0f, 12.0f, kInk.withAlpha (0.55f), true, 0.18f);
}

//==============================================================================
void SA1AudioProcessorEditor::paint (juce::Graphics& g)
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
    drawText (g, "SA-1", kMargin + 3.0f, 55.0f, 70.0f, kDark, true, 0.0f, juce::Justification::centredLeft, true);
    drawText (g, "LO-FI SATURATOR", kW - kMargin - 95.0f, 43.0f, 20.0f, kDark, false, 0.2f, juce::Justification::centredRight);
    drawText (g, "WARMTH AND GRIT", kW - kMargin - 95.0f, 67.0f, 17.0f, kDark, false, 0.2f, juce::Justification::centredRight);
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

    // panels: charcoal, light outline, title pill on the top edge (the pill of a stage is its on / off switch: lit = on, with a dark border)
    for (int i = 0; i < 5; ++i)
    {
        const auto r = panelRect (i);
        g.setColour (kPanel);
        g.fillRoundedRectangle (r, 20.0f);
        g.setColour (kInk.withAlpha (0.55f));
        g.drawRoundedRectangle (r.reduced (0.5f), 20.0f, 1.4f);
        const auto pill = pillRect (i);
        const bool lit = i < 4 && stageOn (i);
        g.setColour (lit ? kBody : kPanel);
        g.fillRoundedRectangle (pill, 11.0f);
        if (lit) { g.setColour (kDark); g.drawRoundedRectangle (pill.reduced (0.9f), 11.0f, 1.8f); }
        else if (i < 4) { g.setColour (kInk.withAlpha (0.35f)); g.drawRoundedRectangle (pill.reduced (0.5f), 11.0f, 1.2f); }
        drawText (g, kPanelName[i], pill.getCentreX(), pill.getCentreY(), 18.0f, lit ? kDark : (i < 4 ? kInk.withAlpha (0.45f) : kBody), true, 0.08f);
    }
    {
        const auto r = shapePanelRect();
        g.setColour (kPanel);
        g.fillRoundedRectangle (r, 20.0f);
        g.setColour (kInk.withAlpha (0.55f));
        g.drawRoundedRectangle (r.reduced (0.5f), 20.0f, 1.4f);
        const float w = textWidth (font (18.0f, true).withExtraKerningFactor (0.08f), "SHAPE") + 38.0f;
        const auto pill = juce::Rectangle<float> (w, 22.0f).withCentre ({ r.getCentreX(), r.getY() });
        g.setColour (kPanel);
        g.fillRoundedRectangle (pill, 11.0f);
        drawText (g, "SHAPE", pill.getCentreX(), pill.getCentreY(), 18.0f, kBody, true, 0.08f);
    }
    drawShape (g);

    // knobs: recessed ring, 11 ticks, legends, name and value
    for (auto& k : knobs)
    {
        const float dim = stageOn (k.panel) ? 1.0f : 0.4f;
        const auto c = knobCentre (k.panel, k.index, k.count);
        const auto ring = juce::Rectangle<float> (2.0f * (kKnobR + 5.0f), 2.0f * (kKnobR + 5.0f)).withCentre (c);
        g.setColour (juce::Colours::black.withAlpha (0.5f * dim));
        g.fillEllipse (ring);
        g.setColour (juce::Colours::white.withAlpha (0.1f * dim));
        g.drawEllipse (ring, 1.0f);
        for (int t = 0; t <= 10; ++t)
        {
            const float a = juce::degreesToRadians (-135.0f + 27.0f * (float) t), s = std::sin (a), co = std::cos (a);
            const bool major = t == 0 || t == 5 || t == 10;
            const float r0 = kKnobR + 10.0f, r1 = r0 + (major ? 8.0f : 5.0f);
            g.setColour (kInk.withAlpha (0.85f * dim));
            g.drawLine (c.x + s * r0, c.y - co * r0, c.x + s * r1, c.y - co * r1, major ? 2.0f : 1.2f);
        }
        static const char* lo[5] = { "0", "0", "1", "0.5k", "-24" }, * hi[5] = { "24", "10", "16", "48k", "+12" };
        const float lx = (kKnobR + 18.0f) * 0.7071f, ly = lx + 13.0f;
        drawText (g, lo[k.kind], c.x - lx, c.y + ly, 12.0f, kInk.withAlpha (0.85f * dim), false, 0.0f);
        drawText (g, hi[k.kind], c.x + lx, c.y + ly, 12.0f, kInk.withAlpha (0.85f * dim), false, 0.0f);

        drawText (g, k.name, c.x, kPanelY + nameOffset (k.panel) + (k.panel == 4 ? kRow2Y - kPanelY : 0.0f), 15.0f, kInk.withAlpha (0.95f * dim), true, 0.1f);
        const double v = k.slider.getValue();
        juce::String txt;
        switch (k.kind)
        {
            case 0: txt = juce::String::formatted ("+%.1f dB", v); break;
            case 1: txt = juce::String ((int) std::lround (v)) + " %"; break;
            case 2: txt = juce::String ((int) std::lround (v)) + " bit"; break;
            case 3: txt = v >= 1000.0 ? juce::String (v * 0.001, 1) + " kHz" : juce::String ((int) std::lround (v)) + " Hz"; break;
            default: txt = juce::String::formatted ("%+d dB", (int) std::lround (v)); break;
        }
        drawText (g, txt, c.x, kPanelY + valueOffset (k.panel) + (k.panel == 4 ? kRow2Y - kPanelY : 0.0f), 14.0f, kBody.withAlpha (dim), false, 0.04f);
    }

    drawText (g, "Made by Richard Leurs | VIBR", kW - kMargin - 20.0f, kRow2Y + kRow2H + 35.0f, 13.0f, kDark, false, 0.06f, juce::Justification::centredRight);
    drawVibrLogo (g, juce::Rectangle<float> (13.0f, 13.0f).withCentre ({ kW - kMargin - 6.5f, kRow2Y + kRow2H + 35.0f }), kDark);
}

void SA1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / scale();
    if (powerRect().contains (p)) { toggle ("power"); return; }
}
