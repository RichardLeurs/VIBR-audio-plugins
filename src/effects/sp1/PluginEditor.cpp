// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "VibrLogo.h"

namespace
{
// layout units: the design is 1200 x 574 (style.md: 20 side margin, 10 gap, name centre 55, rule 104, panels 30 below the rule, 69 above the bottom)
constexpr float kW = 1200.0f, kH = 574.0f, kMargin = 20.0f, kRuleY = 104.0f;
constexpr float kPanelY = 134.0f, kPanelH = 371.0f, kKnobR = 28.0f, kPad = 5.0f;
constexpr float kPanelX[4] = { 20.0f, 250.0f, 580.0f, 910.0f }, kPanelW[4] = { 220.0f, 320.0f, 320.0f, 270.0f };
constexpr const char* kPanelName[4] = { "MODE", "SPACE", "CHARACTER", "OUTPUT" };

const juce::Colour kBody (177, 92, 214), kDark (36, 12, 42), kPanel (26, 27, 30), kInk (238, 238, 232), kIce (143, 211, 244);

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
juce::Rectangle<float> modeRect (int m) { const auto r = panelRect (0); return { r.getX() + 16.0f, r.getY() + 46.0f + (float) m * 46.0f, r.getWidth() - 32.0f, 38.0f }; }
juce::Rectangle<float> freezeRect() { const auto r = panelRect (0); return { r.getX() + 16.0f, r.getBottom() - 78.0f, r.getWidth() - 32.0f, 50.0f }; }
juce::Rectangle<float> powerRect() { return juce::Rectangle<float> (50.0f, 50.0f).withCentre ({ kW - kMargin - 16.5f, 43.5f }); }
juce::Point<float> knobCentre (int panel, int k, int n)
{
    const auto r = panelRect (panel);
    // the knobs (ticks and legends reach 46 each side) keep 16 clear of the panel edge: first and last centre 62 inside it
    const float inset = 62.0f, x = n > 1 ? r.getX() + inset + (r.getWidth() - 2.0f * inset) * (float) k / (float) (n - 1) : r.getCentreX();
    return { x, r.getY() + 150.0f };
}
} // namespace

//==============================================================================
void SP1LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider&)
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
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        s.setRotaryParameters (juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
        s.setMouseDragSensitivity (160);
        s.setScrollWheelEnabled (false);
        s.setPopupDisplayEnabled (true, true, this);
        s.setTextValueSuffix (k.dB ? " dB" : " %");
        auto* par = proc.apvts.getParameter (k.id);
        s.setDoubleClickReturnValue (true, par->convertFrom0to1 (par->getDefaultValue()));
        addAndMakeVisible (s);
        k.att = std::make_unique<juce::SliderParameterAttachment> (*dynamic_cast<juce::RangedAudioParameter*> (par), s, nullptr);
        midiUI.add (s, k.id);
        s.onValueChange = [this] { repaint(); };
    }
    powerAtt.sendInitialUpdate();
    modeAtt.sendInitialUpdate();
    freezeAtt.sendInitialUpdate();

    juce::Random rng (11);                                                                          // satin grain tile
    grain = juce::Image (juce::Image::ARGB, 128, 128, true);
    for (int y = 0; y < 128; ++y)
        for (int x = 0; x < 128; ++x)
            grain.setPixelAt (x, y, (rng.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (rng.nextFloat() * 0.05f));

    constrainer.setSizeLimits (720, 344, 2400, 1148);
    constrainer.setFixedAspectRatio ((double) kW / (double) kH);
    setConstrainer (&constrainer);
    setResizable (true, true);
    setSize ((int) kW, (int) kH);
    layoutKnobs();
    startTimerHz (10);
}

SP1AudioProcessorEditor::~SP1AudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void SP1AudioProcessorEditor::timerCallback()
{
    if (midiUI.needsRepaint()) { midiUI.tick(); repaint(); }
    if (currentMode() != lastMode) { lastMode = currentMode(); repaint(); }      // the TEXTURE name follows the mode
}

void SP1AudioProcessorEditor::toggle (const char* id)
{
    auto* par = proc.apvts.getParameter (id);
    par->beginChangeGesture();
    par->setValueNotifyingHost (par->getValue() > 0.5f ? 0.0f : 1.0f);
    par->endChangeGesture();
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
    midiUI.paintMarks (g, kBody);
}

void SP1AudioProcessorEditor::paint (juce::Graphics& g)
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
    drawText (g, "SP-1", kMargin + 3.0f, 55.0f, 70.0f, kDark, true, 0.0f, juce::Justification::centredLeft, true);
    drawText (g, "SPACE ENGINE", kW - kMargin - 95.0f, 43.0f, 20.0f, kDark, false, 0.2f, juce::Justification::centredRight);
    drawText (g, "EVOLVING SPACES", kW - kMargin - 95.0f, 67.0f, 17.0f, kDark, false, 0.2f, juce::Justification::centredRight);
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
    for (int i = 0; i < 4; ++i)
    {
        const auto r = panelRect (i);
        g.setColour (kPanel);
        g.fillRoundedRectangle (r, 20.0f);
        g.setColour (kInk.withAlpha (0.55f));
        g.drawRoundedRectangle (r.reduced (0.5f), 20.0f, 1.4f);
        const float w = textWidth (font (18.0f, true).withExtraKerningFactor (0.08f), kPanelName[i]) + 38.0f;
        const auto pill = juce::Rectangle<float> (w, 22.0f).withCentre ({ r.getCentreX(), r.getY() });
        g.setColour (kPanel);
        g.fillRoundedRectangle (pill, 11.0f);
        drawText (g, kPanelName[i], pill.getCentreX(), pill.getCentreY(), 18.0f, kBody, true, 0.08f);
    }

    // MODE panel: five mode buttons and the freeze button
    const int mode = currentMode();
    for (int m = 0; m < sp1::kNumModes; ++m)
    {
        const auto b = modeRect (m);
        const bool sel = m == mode;
        if (sel) { g.setColour (kBody); g.fillRoundedRectangle (b, 10.0f); }
        g.setColour (sel ? kBody : kInk.withAlpha (0.35f));
        g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, 1.2f);
        drawText (g, sp1::modeName (m), b.getCentreX(), b.getCentreY(), 16.0f, sel ? kDark : kInk.withAlpha (0.85f), true, 0.12f);
    }
    {
        const bool on = isOn ("freeze");
        const auto b = freezeRect();
        if (on) { g.setColour (kIce.withAlpha (0.22f)); g.fillRoundedRectangle (b.expanded (4.0f), 14.0f); g.setColour (kIce); g.fillRoundedRectangle (b, 10.0f); }
        g.setColour (on ? kIce : kInk.withAlpha (0.5f));
        g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, 1.4f);
        drawText (g, "FREEZE", b.getCentreX(), b.getCentreY(), 20.0f, on ? kDark : kInk.withAlpha (0.9f), true, 0.2f);
    }

    // knobs: recessed ring, 11 ticks, 0 / 10 legends, name and value
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
        if (k.dB) { drawText (g, "-24", c.x - lx, c.y + ly, 13.0f, kInk.withAlpha (0.85f), false, 0.0f); drawText (g, "+12", c.x + lx, c.y + ly, 13.0f, kInk.withAlpha (0.85f), false, 0.0f); }
        else      { drawText (g, "0", c.x - lx, c.y + ly, 13.0f, kInk.withAlpha (0.85f), false, 0.0f); drawText (g, "10", c.x + lx, c.y + ly, 13.0f, kInk.withAlpha (0.85f), false, 0.0f); }

        drawText (g, k.name != nullptr ? k.name : sp1::textureName (mode), c.x, kPanelY + 238.0f, 17.0f, kInk.withAlpha (0.95f), true, 0.14f);
        const double v = k.slider.getValue();
        drawText (g, k.dB ? juce::String::formatted ("%+d dB", (int) std::lround (v)) : juce::String ((int) std::lround (v)) + " %", c.x, kPanelY + 266.0f, 15.0f, kBody, false, 0.04f);
    }

    drawText (g, "Made by Richard Leurs | VIBR", kW - kMargin - 20.0f, kPanelY + kPanelH + 35.0f, 13.0f, kDark, false, 0.06f, juce::Justification::centredRight);
    drawVibrLogo (g, juce::Rectangle<float> (13.0f, 13.0f).withCentre ({ kW - kMargin - 6.5f, kPanelY + kPanelH + 35.0f }), kDark);
}

void SP1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / scale();
    if (powerRect().contains (p)) { toggle ("power"); return; }
    if (freezeRect().expanded (4.0f).contains (p)) { toggle ("freeze"); return; }
    for (int m = 0; m < sp1::kNumModes; ++m)
        if (modeRect (m).expanded (3.0f).contains (p))
        {
            auto* par = proc.apvts.getParameter ("mode");
            par->beginChangeGesture();
            par->setValueNotifyingHost (par->convertTo0to1 ((float) m));
            par->endChangeGesture();
            return;
        }
}
