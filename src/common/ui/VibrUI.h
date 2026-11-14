// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once
#include <JuceHeader.h>
#include "../VibrLogo.h"

// The shared look of the code-drawn VIBR windows (SA-1, SU-1, MS-1, DL-1 ...), see "Interface" in .claude/lab_specs.md and style.md:
// fonts, the black knob, the satin body with its rim, the header (name, two description lines, rule, power switch), panels with a pill on the top edge, tab
// pills, flat buttons, value cards and the knob decoration (ring, ticks, legends, name, value). Everything draws in the 1200-unit design space: the editor
// scales the Graphics once (`g.addTransform (AffineTransform::scale (width / 1200))`) and passes layout units.
namespace vibr::ui
{
constexpr float kW = 1200.0f, kMargin = 20.0f, kRuleY = 104.0f, kKnobR = 24.0f, kKnobPad = 5.0f;

// the colours of one plugin: body (the Lab's colour) and the dark print on it; the rest is the same everywhere
struct Palette
{
    juce::Colour body, dark;
    juce::Colour panel { 26, 27, 30 }, ink { 238, 238, 232 }, red { 229, 67, 44 };
};

//==============================================================================
inline juce::Font font (float h, bool bold = false, bool italic = false)
{
    if (italic) return juce::Font (juce::FontOptions ("Avenir Next", "Heavy Italic", h));
    if (! bold) return juce::Font (juce::FontOptions ("Avenir Next", "Medium", h));
    return juce::Font (juce::FontOptions ("Avenir Next", h, bold ? juce::Font::bold : juce::Font::plain));
}

inline float textWidth (const juce::Font& f, const juce::String& s)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (f, s, 0.0f, 0.0f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}

// text centred on (cx, cy) (or left / right aligned from that point), `size` in layout units, `kerning` as a fraction of the font height
inline void drawText (juce::Graphics& g, const juce::String& s, float cx, float cy, float size, juce::Colour c, bool bold = true, float kerning = 0.08f,
                      juce::Justification j = juce::Justification::centred, bool italic = false)
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

//==============================================================================
// The black knob: matt body with a knurled edge, a raised cap and a white pointer bar. Drawn 5 units inside its component so the shadow is not clipped.
class KnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider&) override
    {
        const auto c = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).getCentre();
        const float R = (float) juce::jmin (w, h) * 0.5f - 5.0f;
        const auto circle = [&c] (float r) { return juce::Rectangle<float> (2.0f * r, 2.0f * r).withCentre (c); };

        g.setColour (juce::Colours::black.withAlpha (0.5f));                                        // shadow
        g.fillEllipse (circle (R).translated (1.0f, R * 0.14f));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff35373d), c.x - R * 0.6f, c.y - R * 0.8f, juce::Colour (0xff08090a), c.x + R * 0.6f, c.y + R * 0.9f, false));
        g.fillEllipse (circle (R));                                                                 // matt black body
        for (int k = 0; k < 60; ++k)                                                                // knurled grip edge
        {
            const float a = (float) k * juce::MathConstants<float>::twoPi / 60.0f, s = std::sin (a), co = std::cos (a);
            g.setColour ((k & 1) ? juce::Colours::black.withAlpha (0.45f) : juce::Colours::white.withAlpha (0.13f));
            g.drawLine (c.x + s * R * 0.86f, c.y - co * R * 0.86f, c.x + s * R * 0.995f, c.y - co * R * 0.995f, 1.1f);
        }
        g.setColour (juce::Colours::white.withAlpha (0.16f));
        g.drawEllipse (circle (R - 0.5f), 1.0f);

        const float cr = R * 0.74f;                                                                 // raised cap
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3f4147), c.x - cr * 0.7f, c.y - cr * 0.8f, juce::Colour (0xff121315), c.x + cr * 0.7f, c.y + cr * 0.9f, false));
        g.fillEllipse (circle (cr));
        g.setColour (juce::Colours::white.withAlpha (0.22f));
        g.drawEllipse (circle (cr), 1.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.16f), c.x - cr * 0.4f, c.y - cr * 0.5f, juce::Colours::transparentWhite, c.x, c.y, true));
        g.fillEllipse (circle (cr * 0.95f));

        const float a = startAngle + pos * (endAngle - startAngle);                                 // white pointer bar
        juce::Path bar;
        bar.addRoundedRectangle (-R * 0.05f, -R * 0.84f, R * 0.10f, R * 0.5f, R * 0.05f);
        g.setColour (juce::Colour (0xfff2f2ee));
        g.fillPath (bar, juce::AffineTransform::rotation (a).translated (c));
    }
};

// the rotary parameters every VIBR knob has: 270 degrees, vertical drag, no text box, a popup while dragging, wheel off
inline void setupKnob (juce::Slider& s, juce::Component& popupParent)
{
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    s.setRotaryParameters (juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
    s.setMouseDragSensitivity (160);
    s.setScrollWheelEnabled (false);
    s.setPopupDisplayEnabled (true, true, &popupParent);
}

//==============================================================================
// the satin grain tile (random speckle, seeded so it is the same every time)
inline juce::Image makeGrain()
{
    juce::Random rng (11);
    juce::Image grain (juce::Image::ARGB, 128, 128, true);
    for (int y = 0; y < 128; ++y)
        for (int x = 0; x < 128; ++x)
            grain.setPixelAt (x, y, (rng.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (rng.nextFloat() * 0.05f));
    return grain;
}

// the satin body of the window with its rim (bright top and left, soft bottom and right)
inline void paintBody (juce::Graphics& g, const Palette& pal, float W, float H, const juce::Image& grain)
{
    g.setGradientFill (juce::ColourGradient (pal.body.brighter (0.22f), 0.0f, 0.0f, pal.body.darker (0.18f), W, H, false));
    g.fillRect (0.0f, 0.0f, W, H);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.14f), W * 0.3f, H * 0.1f, juce::Colours::transparentWhite, W * 0.3f, H * 0.75f, true));
    g.fillRect (0.0f, 0.0f, W, H);
    g.setTiledImageFill (grain, 0, 0, 1.0f);
    g.fillRect (0.0f, 0.0f, W, H);
    g.setColour (juce::Colours::white.withAlpha (0.28f));
    g.drawLine (0.0f, 1.5f, W, 1.5f, 3.0f);
    g.drawLine (1.5f, 0.0f, 1.5f, H, 3.0f);
    g.setColour (juce::Colours::black.withAlpha (0.12f));
    g.drawLine (0.0f, H - 1.5f, W, H - 1.5f, 3.0f);
    g.drawLine (W - 1.5f, 0.0f, W - 1.5f, H, 3.0f);
}

inline juce::Rectangle<float> powerRect() { return juce::Rectangle<float> (50.0f, 50.0f).withCentre ({ kW - kMargin - 16.5f, 43.5f }); }

// the header: the name (70 pt italic) on the left, two description lines on the right, the power switch and the rule under them
inline void paintHeader (juce::Graphics& g, const Palette& pal, const juce::String& name, const juce::String& line1, const juce::String& line2, bool powerOn)
{
    drawText (g, name, kMargin + 3.0f, 55.0f, 70.0f, pal.dark, true, 0.0f, juce::Justification::centredLeft, true);
    drawText (g, line1, kW - kMargin - 95.0f, 43.0f, 20.0f, pal.dark, false, 0.2f, juce::Justification::centredRight);
    drawText (g, line2, kW - kMargin - 95.0f, 67.0f, 17.0f, pal.dark, false, 0.2f, juce::Justification::centredRight);
    g.setColour (pal.dark.withAlpha (0.85f));
    g.fillRect (kMargin, kRuleY, kW - 2.0f * kMargin, 1.6f);

    const auto sr = juce::Rectangle<float> (33.0f, 17.0f).withCentre ({ kW - kMargin - 16.5f, 43.5f });          // power switch: slot, chrome ball, OFF / ON legend
    g.setGradientFill (powerOn ? juce::ColourGradient (juce::Colour (0xff0e3a1e), sr.getX(), sr.getY(), juce::Colour (0xff1a7a3e), sr.getX(), sr.getBottom(), false)
                               : juce::ColourGradient (juce::Colour (0xff111214), sr.getX(), sr.getY(), juce::Colour (0xff25272b), sr.getX(), sr.getBottom(), false));
    g.fillRoundedRectangle (sr, 8.5f);
    g.setColour (juce::Colours::white.withAlpha (0.22f));
    g.drawRoundedRectangle (sr, 8.5f, 1.0f);
    const auto ball = juce::Rectangle<float> (14.0f, 14.0f).withCentre ({ powerOn ? sr.getRight() - 8.5f : sr.getX() + 8.5f, sr.getCentreY() });
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillEllipse (ball.translated (0.5f, 1.2f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffafbfd), ball.getX() + 3.0f, ball.getY() + 2.0f, juce::Colour (0xff6e727e), ball.getRight(), ball.getBottom(), false));
    g.fillEllipse (ball);
    drawText (g, "OFF / ON", kW - kMargin, 68.5f, 13.0f, pal.dark, false, 0.12f, juce::Justification::centredRight);
}

//==============================================================================
// The look of a pill (a panel title or a tab), from style.md "Panel title as a button":
//   Plain  a panel without a switch: a charcoal pill cutting the outline, the title in the body colour
//   On     a switch that is on (a stage, the selected tab): filled with the body colour, the title in the dark print colour, and a dark border: the print colour,
//          1.8 units, drawn 0.9 inside the pill edge
//   Off    a switch that is off: a charcoal pill with a thin light outline (ink at 35 %, 1.2 units, no dark border), the title in ink at 45 %; the knobs of the
//          panel are dimmed to 40 % by the editor
enum class Pill { Plain, On, Off };

inline void paintPill (juce::Graphics& g, const Palette& pal, juce::Rectangle<float> pill, const juce::String& title, Pill style, float fontSize, float kerning = 0.08f)
{
    const float corner = pill.getHeight() * 0.5f;
    if (style == Pill::On)
    {
        g.setColour (pal.body);
        g.fillRoundedRectangle (pill, corner);
        g.setColour (pal.dark);
        g.drawRoundedRectangle (pill.reduced (0.9f), corner, 1.8f);
    }
    else
    {
        g.setColour (pal.panel);
        g.fillRoundedRectangle (pill, corner);
        if (style == Pill::Off) { g.setColour (pal.ink.withAlpha (0.35f)); g.drawRoundedRectangle (pill.reduced (0.6f), corner, 1.2f); }
    }
    drawText (g, title, pill.getCentreX(), pill.getCentreY(), fontSize, style == Pill::On ? pal.dark : style == Pill::Off ? pal.ink.withAlpha (0.45f) : pal.body, true, kerning);
}

// a panel: charcoal, light outline, and (when `title` is not empty) the title pill on the top edge (Pill::On / Off when the panel is a switchable stage)
inline void paintPanel (juce::Graphics& g, const Palette& pal, juce::Rectangle<float> r, const juce::String& title, Pill style = Pill::Plain)
{
    g.setColour (pal.panel);
    g.fillRoundedRectangle (r, 20.0f);
    g.setColour (pal.ink.withAlpha (0.55f));
    g.drawRoundedRectangle (r.reduced (0.5f), 20.0f, 1.4f);
    if (title.isEmpty()) return;
    const float w = textWidth (font (18.0f, true).withExtraKerningFactor (0.08f), title) + 38.0f;
    paintPill (g, pal, juce::Rectangle<float> (w, 22.0f).withCentre ({ r.getCentreX(), r.getY() }), title, style, 18.0f);
}

// tab pills on the top edge of a panel, centred; returns their rectangles (for hit tests) and paints them
inline std::vector<juce::Rectangle<float>> tabRects (juce::Rectangle<float> panel, const std::vector<juce::String>& names, float fontSize = 14.0f)
{
    std::vector<juce::Rectangle<float>> out;
    std::vector<float> w;
    float total = 6.0f * (float) (names.size() - 1);
    for (const auto& n : names) { w.push_back (textWidth (font (fontSize, true).withExtraKerningFactor (0.08f), n) + 30.0f); total += w.back(); }
    float x = panel.getCentreX() - total * 0.5f;
    for (size_t i = 0; i < names.size(); ++i) { out.push_back (juce::Rectangle<float> (w[i], 24.0f).withCentre ({ x + w[i] * 0.5f, panel.getY() })); x += w[i] + 6.0f; }
    return out;
}

inline void paintTabs (juce::Graphics& g, const Palette& pal, juce::Rectangle<float> panel, const std::vector<juce::String>& names, int selected, float fontSize = 14.0f)
{
    const auto rects = tabRects (panel, names, fontSize);
    for (size_t i = 0; i < names.size(); ++i)
    {
        paintPill (g, pal, rects[i], names[i], (int) i == selected ? Pill::On : Pill::Off, fontSize);                  // the selected tab is a lit pill, with its dark border
    }
}

// a flat outlined button: lit = body colour with dark text; `alpha` dims a button that does not apply
inline void paintFlatButton (juce::Graphics& g, const Palette& pal, juce::Rectangle<float> b, const juce::String& label, bool on, float fontSize = 11.0f, float alpha = 1.0f,
                             float corner = 9.0f, float kerning = 0.1f, float outline = 1.3f)
{
    if (on) { g.setColour (pal.body.withMultipliedAlpha (alpha)); g.fillRoundedRectangle (b, corner); }
    g.setColour ((on ? pal.body : pal.ink.withAlpha (0.4f)).withMultipliedAlpha (alpha));
    g.drawRoundedRectangle (b.reduced (0.5f), corner, outline);
    drawText (g, label, b.getCentreX(), b.getCentreY(), fontSize, on ? pal.dark.withMultipliedAlpha (alpha) : pal.ink.withAlpha (0.9f * alpha), true, kerning);
}

// a small box with a value that is changed by dragging up / down (the dynamic EQ fields of the MS-1); `active` = the thing it belongs to is on
inline void paintField (juce::Graphics& g, const Palette& pal, juce::Rectangle<float> b, const juce::String& text, bool active, float fontSize = 11.0f)
{
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (b, 8.0f);
    g.setColour (pal.ink.withAlpha (active ? 0.5f : 0.2f));
    g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.0f);
    drawText (g, text, b.getCentreX(), b.getCentreY(), fontSize, active ? pal.body : pal.ink.withAlpha (0.4f), true, 0.04f);
}

// the credit in the bottom right corner: "Made by Richard Leurs | VIBR" and the small logo, `y` = the centre line (35 below the last panel)
inline void paintFooter (juce::Graphics& g, const Palette& pal, float y)
{
    drawText (g, "Made by Richard Leurs | VIBR", kW - kMargin - 20.0f, y, 13.0f, pal.dark, false, 0.06f, juce::Justification::centredRight);
    drawVibrLogo (g, juce::Rectangle<float> (13.0f, 13.0f).withCentre ({ kW - kMargin - 6.5f, y }), pal.dark);
}

// a value card: a small black box with a tiny label and a large value (red past its limit)
inline void paintValueCard (juce::Graphics& g, const Palette& pal, juce::Rectangle<float> c, const juce::String& label, const juce::String& text, juce::Colour colour,
                            float valueSize = 17.0f)
{
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillRoundedRectangle (c, 8.0f);
    drawText (g, label, c.getCentreX(), c.getY() + 9.0f, 9.0f, pal.ink.withAlpha (0.55f), true, 0.14f);
    drawText (g, text, c.getCentreX(), c.getY() + 26.0f, valueSize, colour, true, 0.02f);
}

//==============================================================================
// A knob's decoration around its centre: the recessed ring, 11 ticks, the legends under the ends, the name and the value (in the body colour) in two rows at
// nameY / valueY. `dim` fades a knob that does not apply (a stage that is off).
inline void paintKnob (juce::Graphics& g, const Palette& pal, juce::Point<float> c, const juce::String& name, const juce::String& value, const juce::String& lo,
                       const juce::String& hi, float nameY, float valueY, float dim = 1.0f)
{
    const auto ring = juce::Rectangle<float> (2.0f * (kKnobR + 5.0f), 2.0f * (kKnobR + 5.0f)).withCentre (c);
    g.setColour (juce::Colours::black.withAlpha (0.5f * dim + 0.2f * (1.0f - dim)));
    g.fillEllipse (ring);
    g.setColour (juce::Colours::white.withAlpha (0.1f));
    g.drawEllipse (ring, 1.0f);
    for (int t = 0; t <= 10; ++t)
    {
        const float a = juce::degreesToRadians (-135.0f + 27.0f * (float) t), s = std::sin (a), co = std::cos (a);
        const bool major = t == 0 || t == 5 || t == 10;
        const float r0 = kKnobR + 10.0f, r1 = r0 + (major ? 8.0f : 5.0f);
        g.setColour (pal.ink.withAlpha (0.85f * dim));
        g.drawLine (c.x + s * r0, c.y - co * r0, c.x + s * r1, c.y - co * r1, major ? 2.0f : 1.2f);
    }
    const float lx = (kKnobR + 18.0f) * 0.7071f, ly = lx + 13.0f;
    drawText (g, lo, c.x - lx, c.y + ly, 12.0f, pal.ink.withAlpha (0.85f * dim), false, 0.0f);
    drawText (g, hi, c.x + lx, c.y + ly, 12.0f, pal.ink.withAlpha (0.85f * dim), false, 0.0f);
    drawText (g, name, c.x, nameY, 15.0f, pal.ink.withAlpha (0.95f * dim), true, 0.1f);
    drawText (g, value, c.x, valueY, 14.0f, pal.body.withAlpha (dim), false, 0.04f);
}
} // namespace vibr::ui
