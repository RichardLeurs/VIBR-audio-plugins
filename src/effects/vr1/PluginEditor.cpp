// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "VibrLogo.h"

namespace
{
// layout units: the design is 1200 x 574 (style.md: 20 side margin, 10 gap, name centre 55, rule 104, panels 30 below the rule, 69 above the bottom)
constexpr float kW = 1200.0f, kH = 574.0f, kMargin = 20.0f, kRuleY = 104.0f;
constexpr float kPanelY = 134.0f, kPanelH = 371.0f, kKnobR = 28.0f, kPad = 5.0f;
constexpr float kPanelX[4] = { 20.0f, 270.0f, 530.0f, 940.0f }, kPanelW[4] = { 240.0f, 250.0f, 400.0f, 240.0f };
constexpr const char* kPanelName[4] = { "MODE", "CONTROLS", "MOTION", "OUTPUT" };

const juce::Colour kBody (71, 198, 154), kDark (9, 34, 26), kPanel (26, 27, 30), kInk (238, 238, 232);

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
juce::Rectangle<float> variantRect (int v) { const auto r = panelRect (0); return { r.getX() + 16.0f, r.getY() + 238.0f + (float) v * 40.0f, r.getWidth() - 32.0f, 32.0f }; }
juce::Rectangle<float> syncRect() { const auto r = panelRect (1); return juce::Rectangle<float> (120.0f, 36.0f).withCentre ({ r.getCentreX(), r.getY() + 318.0f }); }
juce::Rectangle<float> motionRect() { return panelRect (2).reduced (22.0f, 30.0f).withTrimmedTop (4.0f); }
juce::Rectangle<float> powerRect() { return juce::Rectangle<float> (50.0f, 50.0f).withCentre ({ kW - kMargin - 16.5f, 43.5f }); }
juce::Point<float> knobCentre (int panel, int k, int n)
{
    const auto r = panelRect (panel);
    // the knobs sit 120 apart around the middle of the panel (ticks and legends reach 46 each side)
    const float x = r.getCentreX() + 120.0f * ((float) k - 0.5f * (float) (n - 1));
    return { x, r.getY() + 150.0f };
}
} // namespace

//==============================================================================
void VR1LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider&)
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
VR1AudioProcessorEditor::VR1AudioProcessorEditor (VR1AudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      knobs { { { "rate", 1, 0, 2, nullptr, 0, {}, {} },    { "depth", 1, 1, 2, nullptr, 1, {}, {} },
                { "mix", 3, 0, 2, "MIX", 2, {}, {} },       { "level", 3, 1, 2, "LEVEL", 3, {}, {} } } },
      powerAtt  (*p.apvts.getParameter ("power"),  [this] (float) { repaint(); }, nullptr),
      modeAtt   (*p.apvts.getParameter ("mode"),   [this] (float) { repaint(); }, nullptr),
      variantAtt (*p.apvts.getParameter ("variant"), [this] (float) { repaint(); }, nullptr),
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
        s.setTextValueSuffix (k.kind == 3 ? " dB" : " %");
        auto* par = proc.apvts.getParameter (k.id);
        s.setDoubleClickReturnValue (true, par->convertFrom0to1 (par->getDefaultValue()));
        addAndMakeVisible (s);
        k.att = std::make_unique<juce::SliderParameterAttachment> (*dynamic_cast<juce::RangedAudioParameter*> (par), s, nullptr);
        s.onValueChange = [this] { repaint(); };
    }
    powerAtt.sendInitialUpdate();
    modeAtt.sendInitialUpdate();
    variantAtt.sendInitialUpdate();
    syncAtt.sendInitialUpdate();

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
    startTimerHz (30);
}

VR1AudioProcessorEditor::~VR1AudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void VR1AudioProcessorEditor::timerCallback()
{
    lastMode = currentMode();
    repaint (motionRect().expanded (4.0f).transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer());     // the picture moves
}

void VR1AudioProcessorEditor::toggle (const char* id)
{
    auto* par = proc.apvts.getParameter (id);
    par->beginChangeGesture();
    par->setValueNotifyingHost (par->getValue() > 0.5f ? 0.0f : 1.0f);
    par->endChangeGesture();
}

void VR1AudioProcessorEditor::layoutKnobs()
{
    const float sc = scale(), d = 2.0f * (kKnobR + kPad) * sc;
    for (auto& k : knobs)
        k.slider.setBounds (juce::Rectangle<float> (d, d).withCentre (knobCentre (k.panel, k.index, k.count) * sc).toNearestInt());
}

void VR1AudioProcessorEditor::resized() { layoutKnobs(); }

//==============================================================================
// MOTION panel: a live picture of the effect.
//   UNI-VIBE  the four all-pass stages sliding along a frequency axis (100 Hz .. 8 kHz) with the lamp
//   ROTARY    the drum and the horn turning at their real speeds
//   RING MOD  the input, the carrier and the resulting product
void VR1AudioProcessorEditor::drawMotion (juce::Graphics& g, int mode, int variant)
{
    const auto r = motionRect();
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (r, 12.0f);
    juce::Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (r.toNearestInt());

    const auto& eng = proc.getEngine();
    const float a = eng.visA.load (std::memory_order_relaxed), b = eng.visB.load (std::memory_order_relaxed), hz = eng.visHz.load (std::memory_order_relaxed);
    const float rate = proc.apvts.getRawParameterValue ("rate")->load() * 0.01f, depth = proc.apvts.getRawParameterValue ("depth")->load() * 0.01f;
    const bool power = isOn ("power");

    if (mode == vr1::UniVibe)
    {
        static const float lo[4] = { 130.0f, 250.0f, 480.0f, 900.0f }, hi[4] = { 1100.0f, 2000.0f, 3300.0f, 5200.0f };
        const float x0 = r.getX() + 24.0f, x1 = r.getRight() - 24.0f, yTop = r.getY() + 34.0f, yAxis = r.getBottom() - 58.0f;
        const auto xOf = [&] (float f) { return x0 + (x1 - x0) * std::log (f / 100.0f) / std::log (80.0f); };
        g.setColour (kInk.withAlpha (0.3f));
        g.drawLine (x0, yAxis, x1, yAxis, 1.4f);
        for (float f : { 100.0f, 1000.0f, 8000.0f })
        {
            g.drawLine (xOf (f), yAxis, xOf (f), yAxis + 8.0f, 1.4f);
            drawText (g, f >= 1000.0f ? juce::String ((int) (f / 1000.0f)) + " kHz" : juce::String ((int) f) + " Hz", xOf (f), yAxis + 22.0f, 12.0f, kInk.withAlpha (0.55f), false, 0.04f);
        }
        const float lamp = power ? a : 0.0f;
        for (int k = 0; k < 4; ++k)
        {
            const float fc = lo[k] * std::pow (hi[k] / lo[k], lamp), x = xOf (fc);
            const auto bar = juce::Rectangle<float> (8.0f, yAxis - yTop - 20.0f * (float) (3 - k)).withCentre ({ x, 0.0f }).withBottom (yAxis);
            g.setColour (kBody.withAlpha (0.18f));
            g.fillRoundedRectangle (bar.expanded (6.0f, 0.0f), 7.0f);
            g.setColour (kBody.withAlpha (0.55f + 0.45f * lamp));
            g.fillRoundedRectangle (bar, 4.0f);
        }
        const auto lampC = juce::Point<float> (r.getCentreX(), r.getY() + 18.0f);
        g.setColour (kBody.withAlpha (0.12f + 0.3f * lamp));
        g.fillEllipse (juce::Rectangle<float> (30.0f, 30.0f).withCentre (lampC));
        g.setColour (kBody.withAlpha (0.35f + 0.65f * lamp));
        g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre (lampC));
        drawText (g, variant == 1 ? "PHASE VIBRATO" : "PHASE CHORUS", r.getCentreX(), r.getBottom() - 12.0f, 12.0f, kInk.withAlpha (0.5f), true, 0.2f);
    }
    else if (mode == vr1::Rotary)
    {
        const auto disc = [&] (juce::Point<float> c, float rad, float th, const char* label)
        {
            g.setColour (juce::Colour (0xff141518));
            g.fillEllipse (juce::Rectangle<float> (2.0f * rad, 2.0f * rad).withCentre (c));
            g.setColour (kInk.withAlpha (0.4f));
            g.drawEllipse (juce::Rectangle<float> (2.0f * rad, 2.0f * rad).withCentre (c), 1.6f);
            for (int t = 0; t < 12; ++t)                                                   // grille ticks round the rim
            {
                const float an = juce::MathConstants<float>::twoPi * (float) t / 12.0f;
                g.drawLine (c.x + std::sin (an) * (rad - 8.0f), c.y - std::cos (an) * (rad - 8.0f), c.x + std::sin (an) * (rad - 2.0f), c.y - std::cos (an) * (rad - 2.0f), 1.2f);
            }
            const juce::Point<float> tip (c.x + std::sin (th) * (rad - 10.0f), c.y - std::cos (th) * (rad - 10.0f));
            g.setColour (kBody);
            g.drawLine (c.x, c.y, tip.x, tip.y, 3.5f);
            g.fillEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre (tip));
            g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (c));
            drawText (g, label, c.x, c.y + rad + 20.0f, 13.0f, kInk.withAlpha (0.6f), true, 0.2f);
        };
        disc ({ r.getX() + 108.0f, r.getCentreY() - 8.0f }, 72.0f, power ? b : 0.0f, "DRUM");
        disc ({ r.getRight() - 92.0f, r.getCentreY() - 8.0f }, 46.0f, power ? a : 0.0f, "HORN");
        drawText (g, juce::String (power ? hz : 0.0f, 1) + " Hz", r.getRight() - 92.0f, r.getY() + 28.0f, 16.0f, kBody, true, 0.06f);
    }
    else
    {
        const float x0 = r.getX() + 16.0f, w = r.getWidth() - 32.0f, h = (r.getHeight() - 60.0f) / 3.0f;
        const float cyc = 1.0f + 5.0f * rate;
        const auto carrierAt = [&] (float u)
        {
            const float t = u * cyc - a - std::floor (u * cyc - a);
            return variant == 0 ? std::sin (juce::MathConstants<float>::twoPi * t) : variant == 1 ? 4.0f * std::abs (t - 0.5f) - 1.0f : (t < 0.5f ? 1.0f : -1.0f);
        };
        const auto trace = [&] (int row, float thick, juce::Colour col, const std::function<float (float)>& fn, const char* label)
        {
            const float yc = r.getY() + 22.0f + h * ((float) row + 0.5f);
            juce::Path path;
            for (int i = 0; i <= 200; ++i)
            {
                const float u = (float) i / 200.0f, y = yc - fn (u) * h * 0.42f;
                if (i == 0) path.startNewSubPath (x0, y); else path.lineTo (x0 + w * u, y);
            }
            g.setColour (col);
            g.strokePath (path, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            drawText (g, label, x0 + 4.0f, yc - h * 0.5f + 6.0f, 11.0f, kInk.withAlpha (0.45f), true, 0.2f, juce::Justification::centredLeft);
        };
        trace (0, 2.0f, kInk.withAlpha (0.55f), [] (float u) { return std::sin (juce::MathConstants<float>::twoPi * 3.0f * u); }, "INPUT");
        trace (1, 2.0f, kInk.withAlpha (0.55f), carrierAt, "CARRIER");
        trace (2, 3.2f, kBody, [&] (float u) { return std::sin (juce::MathConstants<float>::twoPi * 3.0f * u) * ((1.0f - depth) + depth * carrierAt (u)); }, "RESULT");
    }
}

//==============================================================================
void VR1AudioProcessorEditor::paint (juce::Graphics& g)
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
    drawText (g, "VR-1", kMargin + 3.0f, 55.0f, 70.0f, kDark, true, 0.0f, juce::Justification::centredLeft, true);
    drawText (g, "VIBE & ROTARY", kW - kMargin - 95.0f, 43.0f, 20.0f, kDark, false, 0.2f, juce::Justification::centredRight);
    drawText (g, "SWIRLING MOTION", kW - kMargin - 95.0f, 67.0f, 17.0f, kDark, false, 0.2f, juce::Justification::centredRight);
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

    // MODE panel: the three modes, and under them the TYPE buttons of the selected mode (CHORUS / VIBRATO, SLOW / FAST, SINE / TRIANGLE / SQUARE)
    const int mode = currentMode(), variant = currentVariant();
    for (int m = 0; m < vr1::kNumModes; ++m)
    {
        const auto b = modeRect (m);
        const bool sel = m == mode;
        if (sel) { g.setColour (kBody); g.fillRoundedRectangle (b, 10.0f); }
        g.setColour (sel ? kBody : kInk.withAlpha (0.35f));
        g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, 1.2f);
        drawText (g, vr1::modeName (m), b.getCentreX(), b.getCentreY(), 16.0f, sel ? kDark : kInk.withAlpha (0.85f), true, 0.12f);
    }
    {
        const auto r = panelRect (0);
        g.setColour (kInk.withAlpha (0.35f));
        g.fillRect (r.getX() + 22.0f, r.getY() + 194.0f, r.getWidth() - 44.0f, 1.4f);
        drawText (g, vr1::variantTitle (mode), r.getCentreX(), r.getY() + 218.0f, 15.0f, kInk.withAlpha (0.6f), true, 0.2f);
        for (int v = 0; v < vr1::variantCount (mode); ++v)
        {
            const auto b = variantRect (v);
            const bool sel = v == variant;
            if (sel) { g.setColour (kBody); g.fillRoundedRectangle (b, 9.0f); }
            g.setColour (sel ? kBody : kInk.withAlpha (0.35f));
            g.drawRoundedRectangle (b.reduced (0.5f), 9.0f, 1.2f);
            drawText (g, vr1::variantName (mode, v), b.getCentreX(), b.getCentreY(), 14.0f, sel ? kDark : kInk.withAlpha (0.85f), true, 0.12f);
        }
    }

    {   // SYNC: the UNI-VIBE rate becomes a note length of the host tempo (the button is dimmed in the other modes)
        const bool avail = mode == vr1::UniVibe, on = avail && isOn ("sync");
        const auto b = syncRect();
        if (on) { g.setColour (kBody); g.fillRoundedRectangle (b, 10.0f); }
        g.setColour ((on ? kBody : kInk).withAlpha (on ? 1.0f : (avail ? 0.5f : 0.2f)));
        g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, 1.4f);
        drawText (g, "SYNC", b.getCentreX(), b.getCentreY(), 16.0f, on ? kDark : kInk.withAlpha (avail ? 0.9f : 0.3f), true, 0.16f);
    }

    drawMotion (g, mode, variant);

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
        if (k.kind == 3) { drawText (g, "-24", c.x - lx, c.y + ly, 13.0f, kInk.withAlpha (0.85f), false, 0.0f); drawText (g, "+12", c.x + lx, c.y + ly, 13.0f, kInk.withAlpha (0.85f), false, 0.0f); }
        else      { drawText (g, "0", c.x - lx, c.y + ly, 13.0f, kInk.withAlpha (0.85f), false, 0.0f); drawText (g, "10", c.x + lx, c.y + ly, 13.0f, kInk.withAlpha (0.85f), false, 0.0f); }

        drawText (g, k.name != nullptr ? k.name : (k.kind == 0 ? vr1::rateName (mode) : vr1::depthName (mode)), c.x, kPanelY + 238.0f, 17.0f, kInk.withAlpha (0.95f), true, 0.14f);
        const double v = k.slider.getValue();
        juce::String txt;
        if (k.kind == 3) txt = juce::String::formatted ("%+d dB", (int) std::lround (v));
        else if (k.kind == 0)
        {
            const float f = (float) v * 0.01f;
            if (mode == vr1::UniVibe) txt = isOn ("sync") ? juce::String (vr1::divisionName (vr1::divisionIndex (f))) : juce::String (vr1::vibeHz (f), 2) + " Hz";
            else if (mode == vr1::Rotary) txt = juce::String (vr1::hornHz (f, variant == 1), 2) + " Hz";
            else { const float hz = vr1::ringHz (f); txt = hz >= 1000.0f ? juce::String (hz * 0.001f, 2) + " kHz" : juce::String ((int) std::lround (hz)) + " Hz"; }
        }
        else txt = juce::String ((int) std::lround (v)) + " %";
        drawText (g, txt, c.x, kPanelY + 266.0f, 15.0f, kBody, false, 0.04f);
    }

    drawText (g, "Made by Richard Leurs | VIBR", kW - kMargin - 20.0f, kPanelY + kPanelH + 35.0f, 13.0f, kDark, false, 0.06f, juce::Justification::centredRight);
    drawVibrLogo (g, juce::Rectangle<float> (13.0f, 13.0f).withCentre ({ kW - kMargin - 6.5f, kPanelY + kPanelH + 35.0f }), kDark);
}

void VR1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / scale();
    if (powerRect().contains (p)) { toggle ("power"); return; }
    if (currentMode() == vr1::UniVibe && syncRect().expanded (3.0f).contains (p)) { toggle ("sync"); return; }
    const auto setChoice = [this] (const char* id, int index)
    {
        auto* par = proc.apvts.getParameter (id);
        par->beginChangeGesture();
        par->setValueNotifyingHost (par->convertTo0to1 ((float) index));
        par->endChangeGesture();
    };
    for (int m = 0; m < vr1::kNumModes; ++m)
        if (modeRect (m).expanded (3.0f).contains (p)) { setChoice ("mode", m); return; }
    for (int v = 0; v < vr1::variantCount (currentMode()); ++v)
        if (variantRect (v).expanded (3.0f).contains (p)) { setChoice ("variant", v); return; }
}
