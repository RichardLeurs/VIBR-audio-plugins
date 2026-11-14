// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "VibrLogo.h"

namespace
{
// layout units: the design is 1200 x 690 (style.md: 20 side margin, 10 gap, name centre 55, rule 104, panels 30 below the rule, 24 between the rows)
constexpr float kW = 1200.0f, kH = 690.0f, kMargin = 20.0f, kRuleY = 104.0f;
constexpr float kPanelY = 134.0f, kPanelH = 236.0f, kKnobR = 23.0f, kPad = 5.0f, kRow2Y = 394.0f, kRow2H = 227.0f, kKnobPitch = 110.0f;
constexpr float kPanelX[4] = { 20.0f, 230.0f, 570.0f, 910.0f }, kPanelW[4] = { 200.0f, 330.0f, 330.0f, 270.0f };
constexpr const char* kPanelName[4] = { "LEVEL", "PAN", "WIDTH", "CHANNELS" };

const juce::Colour kBody (177, 139, 240), kDark (30, 16, 56), kPanel (26, 27, 30), kInk (238, 238, 232);

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
juce::Rectangle<float> fieldRect() { return { kMargin, kRow2Y, kW - 2.0f * kMargin, kRow2H }; }
juce::Rectangle<float> powerRect() { return juce::Rectangle<float> (50.0f, 50.0f).withCentre ({ kW - kMargin - 16.5f, 43.5f }); }
juce::Point<float> knobCentre (int panel, int k, int n)
{
    const auto r = panelRect (panel);
    // the knobs sit 110 apart around the middle of the panel (ticks and legends reach 41 each side)
    return { r.getCentreX() + kKnobPitch * ((float) k - 0.5f * (float) (n - 1)), r.getY() + 100.0f };
}
// the six buttons of the CHANNELS panel: INVERT L, INVERT R, SWAP L / R, MONO, MID, SIDE (two columns, three rows)
juce::Rectangle<float> buttonRect (int i)
{
    const auto r = panelRect (3);
    return { r.getX() + 18.0f + (float) (i % 2) * 118.0f, r.getY() + 44.0f + (float) (i / 2) * 56.0f, 108.0f, 40.0f };
}
constexpr const char* kButtonName[6] = { "INVERT L", "INVERT R", "SWAP L / R", "MONO", "MID", "SIDE" };
} // namespace

//==============================================================================
void SU1LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider&)
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
SU1AudioProcessorEditor::SU1AudioProcessorEditor (SU1AudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      knobs { { { "gain", 0, 0, 1, "GAIN", 0, {}, {} },
                { "pan", 1, 0, 3, "POSITION", 1, {}, {} },     { "autoRate", 1, 1, 3, "RATE", 2, {}, {} },     { "autoDepth", 1, 2, 3, "DEPTH", 3, {}, {} },
                { "width", 2, 0, 3, "WIDTH", 4, {}, {} },      { "spread", 2, 1, 3, "SPREAD", 3, {}, {} },     { "bassMono", 2, 2, 3, "BASS MONO", 5, {}, {} } } },
      powerAtt (*p.apvts.getParameter ("power"), [this] (float) { repaint(); }, nullptr),
      invLAtt  (*p.apvts.getParameter ("invL"),  [this] (float) { repaint(); }, nullptr),
      invRAtt  (*p.apvts.getParameter ("invR"),  [this] (float) { repaint(); }, nullptr),
      swapAtt  (*p.apvts.getParameter ("swap"),  [this] (float) { repaint(); }, nullptr),
      monoAtt  (*p.apvts.getParameter ("mono"),  [this] (float) { repaint(); }, nullptr),
      soloAtt  (*p.apvts.getParameter ("solo"),  [this] (float) { repaint(); }, nullptr)
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
        k.att = std::make_unique<juce::SliderParameterAttachment> (*dynamic_cast<juce::RangedAudioParameter*> (par), s, nullptr);
        s.onValueChange = [this] { repaint(); };
    }
    powerAtt.sendInitialUpdate(); invLAtt.sendInitialUpdate(); invRAtt.sendInitialUpdate(); swapAtt.sendInitialUpdate(); monoAtt.sendInitialUpdate(); soloAtt.sendInitialUpdate();

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

SU1AudioProcessorEditor::~SU1AudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void SU1AudioProcessorEditor::timerCallback()
{
    repaint (fieldRect().expanded (4.0f).transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer());     // the meters move
}

void SU1AudioProcessorEditor::toggle (const char* id)
{
    auto* par = proc.apvts.getParameter (id);
    par->beginChangeGesture();
    par->setValueNotifyingHost (par->getValue() > 0.5f ? 0.0f : 1.0f);
    par->endChangeGesture();
}

void SU1AudioProcessorEditor::setChoice (const char* id, int index)
{
    auto* par = proc.apvts.getParameter (id);
    par->beginChangeGesture();
    par->setValueNotifyingHost (par->convertTo0to1 ((float) index));
    par->endChangeGesture();
}

void SU1AudioProcessorEditor::layoutKnobs()
{
    const float sc = scale(), d = 2.0f * (kKnobR + kPad) * sc;
    for (auto& k : knobs)
        k.slider.setBounds (juce::Rectangle<float> (d, d).withCentre (knobCentre (k.panel, k.index, k.count) * sc).toNearestInt());
}

void SU1AudioProcessorEditor::resized() { layoutKnobs(); }

//==============================================================================
// FIELD display: left the peak level of the left and right output (dB), in the middle a goniometer (vertical = mid, horizontal = side: a mono
// signal is a vertical line, left only leans to the left, opposite channels make a horizontal line), right the phase correlation (-1 .. +1).
void SU1AudioProcessorEditor::drawField (juce::Graphics& g)
{
    const auto r = fieldRect();
    const auto& eng = proc.getEngine();
    const bool power = isOn ("power");
    const auto toDb = [] (float v) { return 8.68588964f * std::log (std::max (v, 1.0e-5f)); };

    // level bars
    const float barTop = r.getY() + 34.0f, barH = 150.0f;
    for (int c = 0; c < 2; ++c)
    {
        const float db = power ? toDb (c == 0 ? eng.peakL.load (std::memory_order_relaxed) : eng.peakR.load (std::memory_order_relaxed)) : -90.0f;
        const juce::Rectangle<float> bar (r.getX() + 90.0f + (float) c * 46.0f, barTop, 30.0f, barH);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (bar.expanded (4.0f), 8.0f);
        const float fill = barH * (std::clamp (db, -60.0f, 0.0f) + 60.0f) / 60.0f;
        g.setColour (db > -3.0f ? juce::Colour (0xffe5432c) : kBody);
        g.fillRoundedRectangle (juce::Rectangle<float> (bar.getX(), bar.getBottom() - fill, bar.getWidth(), fill), 5.0f);
        drawText (g, c == 0 ? "L" : "R", bar.getCentreX(), bar.getBottom() + 24.0f, 13.0f, kInk.withAlpha (0.7f), true, 0.1f);
    }
    for (float db : { -48.0f, -36.0f, -24.0f, -12.0f, 0.0f })
    {
        const float y = barTop + barH * (1.0f - (db + 60.0f) / 60.0f);
        g.setColour (kInk.withAlpha (0.15f));
        g.drawLine (r.getX() + 86.0f, y, r.getX() + 90.0f + 46.0f + 34.0f, y, 1.0f);
        drawText (g, juce::String ((int) db), r.getX() + 62.0f, y, 11.0f, kInk.withAlpha (0.5f), false, 0.0f);
    }

    // goniometer
    const float side = 160.0f;
    const juce::Rectangle<float> gon (r.getCentreX() - side * 0.5f - 120.0f, r.getY() + 24.0f, side, side);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (gon.expanded (8.0f), 10.0f);
    g.setColour (kInk.withAlpha (0.14f));
    g.drawLine (gon.getCentreX(), gon.getY(), gon.getCentreX(), gon.getBottom(), 1.0f);
    g.drawLine (gon.getX(), gon.getCentreY(), gon.getRight(), gon.getCentreY(), 1.0f);
    g.drawLine (gon.getX(), gon.getY(), gon.getRight(), gon.getBottom(), 1.0f);
    g.drawLine (gon.getX(), gon.getBottom(), gon.getRight(), gon.getY(), 1.0f);
    drawText (g, "M", gon.getCentreX(), gon.getY() + 9.0f, 11.0f, kInk.withAlpha (0.5f), true, 0.1f);
    drawText (g, "S", gon.getRight() - 9.0f, gon.getCentreY() - 9.0f, 11.0f, kInk.withAlpha (0.5f), true, 0.1f);
    drawText (g, "L", gon.getX() + 10.0f, gon.getY() + 9.0f, 11.0f, kInk.withAlpha (0.5f), true, 0.1f);
    drawText (g, "R", gon.getRight() - 10.0f, gon.getY() + 9.0f, 11.0f, kInk.withAlpha (0.5f), true, 0.1f);
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (gon.expanded (6.0f).toNearestInt());
        const int pos = eng.scopePos.load (std::memory_order_acquire);
        for (int k = 0; k < su1::kScope; ++k)
        {
            const int idx = (pos + k) % su1::kScope;
            const float l = eng.scopeL[idx].load (std::memory_order_relaxed), rr = eng.scopeR[idx].load (std::memory_order_relaxed);
            const float x = gon.getCentreX() + (rr - l) * side * 0.5f * 0.7071f * 1.4f, y = gon.getCentreY() - (l + rr) * side * 0.5f * 0.7071f * 1.4f;
            const float age = (float) k / (float) su1::kScope;
            g.setColour (kBody.withAlpha (0.1f + 0.7f * age * age));
            g.fillEllipse (juce::Rectangle<float> (3.0f, 3.0f).withCentre ({ x, y }));
        }
    }
    drawText (g, "GONIOMETER", gon.getCentreX(), gon.getBottom() + 22.0f, 12.0f, kInk.withAlpha (0.55f), true, 0.18f);

    // correlation
    const float corr = power ? eng.corr.load (std::memory_order_relaxed) : 1.0f;
    const juce::Rectangle<float> cm (r.getCentreX() + 90.0f, r.getY() + 70.0f, 400.0f, 22.0f);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (cm.expanded (8.0f, 10.0f), 10.0f);
    g.setColour (kInk.withAlpha (0.14f));
    g.drawLine (cm.getCentreX(), cm.getY() - 4.0f, cm.getCentreX(), cm.getBottom() + 4.0f, 1.0f);
    const float xc = cm.getCentreX() + corr * cm.getWidth() * 0.5f;
    g.setColour (corr < 0.0f ? juce::Colour (0xffe5432c) : kBody);
    g.fillRoundedRectangle (juce::Rectangle<float> (std::min (xc, cm.getCentreX()), cm.getY(), std::abs (xc - cm.getCentreX()) + 2.0f, cm.getHeight()), 4.0f);
    g.setColour (kInk.withAlpha (0.95f));
    g.fillRoundedRectangle (juce::Rectangle<float> (4.0f, cm.getHeight() + 14.0f).withCentre ({ xc, cm.getCentreY() }), 2.0f);
    drawText (g, "-1", cm.getX(), cm.getBottom() + 22.0f, 12.0f, kInk.withAlpha (0.6f), false, 0.0f);
    drawText (g, "0", cm.getCentreX(), cm.getBottom() + 22.0f, 12.0f, kInk.withAlpha (0.6f), false, 0.0f);
    drawText (g, "+1", cm.getRight(), cm.getBottom() + 22.0f, 12.0f, kInk.withAlpha (0.6f), false, 0.0f);
    drawText (g, "CORRELATION", cm.getCentreX(), cm.getY() - 30.0f, 12.0f, kInk.withAlpha (0.55f), true, 0.18f);
    drawText (g, juce::String::formatted ("%+.2f", corr), cm.getCentreX(), cm.getBottom() + 58.0f, 22.0f, corr < 0.0f ? juce::Colour (0xffe5432c) : kBody, true, 0.06f);
    drawText (g, corr > 0.9f ? "MONO COMPATIBLE" : corr > 0.3f ? "GOOD STEREO" : corr > -0.1f ? "VERY WIDE" : "OUT OF PHASE", cm.getCentreX(), cm.getBottom() + 84.0f, 11.0f, kInk.withAlpha (0.5f), true, 0.2f);
}

//==============================================================================
void SU1AudioProcessorEditor::paint (juce::Graphics& g)
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
    drawText (g, "SU-1", kMargin + 3.0f, 55.0f, 70.0f, kDark, true, 0.0f, juce::Justification::centredLeft, true);
    drawText (g, "STEREO UTILITY", kW - kMargin - 95.0f, 43.0f, 20.0f, kDark, false, 0.2f, juce::Justification::centredRight);
    drawText (g, "SPATIAL CONTROL", kW - kMargin - 95.0f, 67.0f, 17.0f, kDark, false, 0.2f, juce::Justification::centredRight);
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
    for (int i = 0; i < 4; ++i) drawPanel (panelRect (i), kPanelName[i]);
    drawPanel (fieldRect(), "FIELD");
    drawField (g);

    // the buttons of the CHANNELS panel
    const bool btnOn[6] = { isOn ("invL"), isOn ("invR"), isOn ("swap"), isOn ("mono"), solo() == 1, solo() == 2 };
    for (int i = 0; i < 6; ++i)
    {
        const auto b = buttonRect (i);
        if (btnOn[i]) { g.setColour (kBody); g.fillRoundedRectangle (b, 10.0f); }
        g.setColour (btnOn[i] ? kBody : kInk.withAlpha (0.4f));
        g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, 1.3f);
        drawText (g, kButtonName[i], b.getCentreX(), b.getCentreY(), 14.0f, btnOn[i] ? kDark : kInk.withAlpha (0.9f), true, 0.1f);
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
        static const char* lo[6] = { "-60", "L", "0.1", "0", "0", "off" }, * hi[6] = { "+12", "R", "10", "10", "200", "300" };
        const float lx = (kKnobR + 18.0f) * 0.7071f, ly = lx + 13.0f;
        drawText (g, lo[k.kind], c.x - lx, c.y + ly, 12.0f, kInk.withAlpha (0.85f), false, 0.0f);
        drawText (g, hi[k.kind], c.x + lx, c.y + ly, 12.0f, kInk.withAlpha (0.85f), false, 0.0f);

        drawText (g, k.name, c.x, kPanelY + 178.0f, 15.0f, kInk.withAlpha (0.95f), true, 0.1f);
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
        drawText (g, txt, c.x, kPanelY + 206.0f, 14.0f, kBody, false, 0.04f);
    }

    drawText (g, "Made by Richard Leurs | VIBR", kW - kMargin - 20.0f, kRow2Y + kRow2H + 35.0f, 13.0f, kDark, false, 0.06f, juce::Justification::centredRight);
    drawVibrLogo (g, juce::Rectangle<float> (13.0f, 13.0f).withCentre ({ kW - kMargin - 6.5f, kRow2Y + kRow2H + 35.0f }), kDark);
}

void SU1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / scale();
    if (powerRect().contains (p)) { toggle ("power"); return; }
    static const char* ids[4] = { "invL", "invR", "swap", "mono" };
    for (int i = 0; i < 6; ++i)
        if (buttonRect (i).expanded (3.0f).contains (p))
        {
            if (i < 4) toggle (ids[i]);
            else setChoice ("solo", solo() == i - 3 ? 0 : i - 3);          // MID and SIDE: click again to go back to stereo
            return;
        }
}
