// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "VibrLogo.h"

namespace
{
// layout units: the design is 1200 x 690 (style.md: 20 side margin, 10 gap, name centre 55, rule 104, panels 30 below the rule, 24 between the rows)
constexpr float kW = 1200.0f, kH = 690.0f, kMargin = 20.0f, kRuleY = 104.0f;
constexpr float kPanelY = 134.0f, kPanelH = 236.0f, kKnobR = 24.0f, kPad = 5.0f, kActY = 394.0f, kActH = 227.0f, kKnobPitch = 116.0f;
constexpr float kPanelX[4] = { 20.0f, 290.0f, 650.0f, 920.0f }, kPanelW[4] = { 260.0f, 350.0f, 260.0f, 260.0f };
constexpr const char* kPanelName[4] = { "GATE", "EXPANDER", "TRANSIENT", "OUTPUT" };

const juce::Colour kBody (242, 211, 78), kDark (40, 33, 6), kPanel (26, 27, 30), kInk (238, 238, 232);

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
juce::Rectangle<float> actionRect() { return { kMargin, kActY, kW - 2.0f * kMargin, kActH }; }
juce::Rectangle<float> plotRect() { const auto r = actionRect(); return { r.getX() + 64.0f, r.getY() + 34.0f, r.getWidth() - 128.0f, 150.0f }; }
juce::Rectangle<float> pillRect (int i)           // the title pill of a panel: it is also the on / off switch of the stage
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
    return { r.getCentreX() + kKnobPitch * ((float) k - 0.5f * (float) (n - 1)), r.getY() + 100.0f };
}
} // namespace

//==============================================================================
void DY1LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider&)
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
DY1AudioProcessorEditor::DY1AudioProcessorEditor (DY1AudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      knobs { { { "gateThresh", 0, 0, 2, "THRESH", 0, {}, {} },  { "gateRelease", 0, 1, 2, "RELEASE", 1, {}, {} },
                { "expThresh", 1, 0, 3, "THRESH", 0, {}, {} },   { "expRatio", 1, 1, 3, "RATIO", 2, {}, {} },     { "expRelease", 1, 2, 3, "RELEASE", 1, {}, {} },
                { "attack", 2, 0, 2, "ATTACK", 3, {}, {} },      { "sustain", 2, 1, 2, "SUSTAIN", 3, {}, {} },
                { "mix", 3, 0, 2, "MIX", 4, {}, {} },            { "level", 3, 1, 2, "LEVEL", 5, {}, {} } } },
      powerAtt (*p.apvts.getParameter ("power"),  [this] (float) { repaint(); }, nullptr),
      gateAtt  (*p.apvts.getParameter ("gateOn"), [this] (float) { repaint(); }, nullptr),
      expAtt   (*p.apvts.getParameter ("expOn"),  [this] (float) { repaint(); }, nullptr),
      transAtt (*p.apvts.getParameter ("transOn"), [this] (float) { repaint(); }, nullptr)
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
    powerAtt.sendInitialUpdate(); gateAtt.sendInitialUpdate(); expAtt.sendInitialUpdate(); transAtt.sendInitialUpdate();

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

DY1AudioProcessorEditor::~DY1AudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void DY1AudioProcessorEditor::timerCallback()
{
    for (auto& k : knobs) k.slider.setAlpha (stageOn (k.panel) ? 1.0f : 0.4f);       // the knobs of a stage that is off are dimmed
    repaint (actionRect().expanded (4.0f).transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer());     // the display moves
}

void DY1AudioProcessorEditor::toggle (const char* id)
{
    auto* par = proc.apvts.getParameter (id);
    par->beginChangeGesture();
    par->setValueNotifyingHost (par->getValue() > 0.5f ? 0.0f : 1.0f);
    par->endChangeGesture();
}

void DY1AudioProcessorEditor::layoutKnobs()
{
    const float sc = scale(), d = 2.0f * (kKnobR + kPad) * sc;
    for (auto& k : knobs)
        k.slider.setBounds (juce::Rectangle<float> (d, d).withCentre (knobCentre (k.panel, k.index, k.count) * sc).toNearestInt());
}

void DY1AudioProcessorEditor::resized() { layoutKnobs(); }

//==============================================================================
// ACTION display: the input level (grey area, dB scale on the left) and the gain of the chain (orange line, scale on the right) over the last
// five seconds, left = past, right = now; the dashed lines are the gate and expander thresholds on the input scale.
void DY1AudioProcessorEditor::drawAction (juce::Graphics& g)
{
    const auto r = plotRect();
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (r.expanded (8.0f), 10.0f);
    const auto yIn = [&] (float db) { return r.getBottom() - r.getHeight() * (std::clamp (db, -80.0f, 0.0f) + 80.0f) / 80.0f; };
    const auto yGain = [&] (float db) { return r.getBottom() - r.getHeight() * (std::clamp (db, -60.0f, 12.0f) + 60.0f) / 72.0f; };
    for (float db : { -60.0f, -40.0f, -20.0f, 0.0f })
    {
        g.setColour (kInk.withAlpha (db == 0.0f ? 0.25f : 0.1f));
        g.drawLine (r.getX(), yIn (db), r.getRight(), yIn (db), 1.0f);
        drawText (g, juce::String ((int) db), r.getX() - 24.0f, yIn (db), 12.0f, kInk.withAlpha (0.55f), false, 0.0f);
    }
    for (float db : { -60.0f, -40.0f, -20.0f, 0.0f })
        drawText (g, juce::String::formatted ("%+d", (int) db), r.getRight() + 26.0f, yGain (db), 12.0f, kBody.withAlpha (0.7f), false, 0.0f);
    drawText (g, "INPUT dB", r.getX() - 24.0f, r.getY() - 14.0f, 10.0f, kInk.withAlpha (0.5f), true, 0.1f);
    drawText (g, "GAIN dB", r.getRight() + 26.0f, r.getY() - 14.0f, 10.0f, kBody.withAlpha (0.8f), true, 0.1f);
    drawText (g, "PAST", r.getX() + 24.0f, r.getBottom() + 22.0f, 12.0f, kInk.withAlpha (0.55f), true, 0.14f);
    drawText (g, "NOW", r.getRight() - 16.0f, r.getBottom() + 22.0f, 12.0f, kInk.withAlpha (0.55f), true, 0.14f);

    const auto& eng = proc.getEngine();
    const int pos = eng.histPos.load (std::memory_order_acquire);
    juce::Path area, line;
    area.startNewSubPath (r.getX(), r.getBottom());
    for (int k = 0; k < dy1::kHistory; ++k)
    {
        const int idx = (pos + k) % dy1::kHistory;
        const float x = r.getX() + r.getWidth() * (float) k / (float) (dy1::kHistory - 1);
        area.lineTo (x, yIn (eng.histIn[idx].load (std::memory_order_relaxed)));
        const float y = yGain (eng.histGain[idx].load (std::memory_order_relaxed));
        if (k == 0) line.startNewSubPath (x, y); else line.lineTo (x, y);
    }
    area.lineTo (r.getRight(), r.getBottom());
    area.closeSubPath();
    g.setColour (kInk.withAlpha (0.2f));
    g.fillPath (area);
    g.setColour (kInk.withAlpha (0.45f));
    g.strokePath (area, juce::PathStrokeType (1.2f));
    g.setColour (kBody);
    g.strokePath (line, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float dash[2] = { 6.0f, 5.0f };
    const auto threshold = [&] (const char* id, const char* label, bool on)
    {
        if (! on) return;
        const float y = yIn (proc.apvts.getRawParameterValue (id)->load());
        g.setColour (kBody.withAlpha (0.8f));
        g.drawDashedLine (juce::Line<float> (r.getX(), y, r.getRight(), y), dash, 2, 1.4f);
        drawText (g, label, r.getX() + 8.0f, y - 9.0f, 11.0f, kBody, true, 0.12f, juce::Justification::centredLeft);
    };
    threshold ("gateThresh", "GATE", isOn ("gateOn") && isOn ("power"));
    threshold ("expThresh", "EXPANDER", isOn ("expOn") && isOn ("power"));
}

//==============================================================================
void DY1AudioProcessorEditor::paint (juce::Graphics& g)
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
    drawText (g, "DY-1", kMargin + 3.0f, 55.0f, 70.0f, kDark, true, 0.0f, juce::Justification::centredLeft, true);
    drawText (g, "DYNAMIC SHAPER", kW - kMargin - 95.0f, 43.0f, 20.0f, kDark, false, 0.2f, juce::Justification::centredRight);
    drawText (g, "TRANSIENT SCULPTING", kW - kMargin - 95.0f, 67.0f, 17.0f, kDark, false, 0.2f, juce::Justification::centredRight);
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

    // panels: charcoal, light outline, title pill on the top edge (the pill of a stage is its on / off switch: lit = on)
    for (int i = 0; i < 4; ++i)
    {
        const auto r = panelRect (i);
        g.setColour (kPanel);
        g.fillRoundedRectangle (r, 20.0f);
        g.setColour (kInk.withAlpha (0.55f));
        g.drawRoundedRectangle (r.reduced (0.5f), 20.0f, 1.4f);
        const auto pill = pillRect (i);
        const bool lit = i < 3 && stageOn (i);
        g.setColour (lit ? kBody : kPanel);
        g.fillRoundedRectangle (pill, 11.0f);
        if (lit) { g.setColour (kDark); g.drawRoundedRectangle (pill.reduced (0.9f), 11.0f, 1.8f); }                    // a lit button pill has a dark border
        else if (i < 3) { g.setColour (kInk.withAlpha (0.35f)); g.drawRoundedRectangle (pill.reduced (0.5f), 11.0f, 1.2f); }    // an off one a thin light outline
        drawText (g, kPanelName[i], pill.getCentreX(), pill.getCentreY(), 18.0f, lit ? kDark : (i < 3 ? kInk.withAlpha (0.45f) : kBody), true, 0.08f);
    }
    {
        const auto r = actionRect();
        g.setColour (kPanel);
        g.fillRoundedRectangle (r, 20.0f);
        g.setColour (kInk.withAlpha (0.55f));
        g.drawRoundedRectangle (r.reduced (0.5f), 20.0f, 1.4f);
        const float w = textWidth (font (18.0f, true).withExtraKerningFactor (0.08f), "ACTION") + 38.0f;
        const auto pill = juce::Rectangle<float> (w, 22.0f).withCentre ({ r.getCentreX(), r.getY() });
        g.setColour (kPanel);
        g.fillRoundedRectangle (pill, 11.0f);
        drawText (g, "ACTION", pill.getCentreX(), pill.getCentreY(), 18.0f, kBody, true, 0.08f);
    }
    drawAction (g);

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
        static const char* lo[6] = { "-80", "10", "1", "-100", "0", "-24" }, * hi[6] = { "-10", "800", "10", "+100", "10", "+12" };
        const float lx = (kKnobR + 18.0f) * 0.7071f, ly = lx + 13.0f;
        drawText (g, lo[k.kind], c.x - lx, c.y + ly, 12.0f, kInk.withAlpha (0.85f * dim), false, 0.0f);
        drawText (g, hi[k.kind], c.x + lx, c.y + ly, 12.0f, kInk.withAlpha (0.85f * dim), false, 0.0f);

        drawText (g, k.name, c.x, kPanelY + 178.0f, 15.0f, kInk.withAlpha (0.95f * dim), true, 0.1f);
        const double v = k.slider.getValue();
        juce::String txt;
        switch (k.kind)
        {
            case 0: txt = juce::String::formatted ("%d dB", (int) std::lround (v)); break;
            case 1: txt = juce::String ((int) std::lround (v)) + " ms"; break;
            case 2: txt = juce::String (v, 1) + ":1"; break;
            case 3: txt = juce::String::formatted ("%+d %%", (int) std::lround (v)); break;
            case 4: txt = juce::String ((int) std::lround (v)) + " %"; break;
            default: txt = juce::String::formatted ("%+d dB", (int) std::lround (v)); break;
        }
        drawText (g, txt, c.x, kPanelY + 206.0f, 14.0f, kBody.withAlpha (dim), false, 0.04f);
    }

    drawText (g, "Made by Richard Leurs | VIBR", kW - kMargin - 20.0f, kActY + kActH + 35.0f, 13.0f, kDark, false, 0.06f, juce::Justification::centredRight);
    drawVibrLogo (g, juce::Rectangle<float> (13.0f, 13.0f).withCentre ({ kW - kMargin - 6.5f, kActY + kActH + 35.0f }), kDark);
}

void DY1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / scale();
    if (powerRect().contains (p)) { toggle ("power"); return; }
    static const char* ids[3] = { "gateOn", "expOn", "transOn" };
    for (int i = 0; i < 3; ++i)
        if (pillRect (i).expanded (6.0f, 8.0f).contains (p)) { toggle (ids[i]); return; }
}
