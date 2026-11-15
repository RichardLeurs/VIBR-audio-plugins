// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include <JuceHeader.h>
#include "ui/VibrUI.h"

// Test look "Dark analog futurism" (.claude/skeuomorphic_ui_spec.md), used by the AX-1 only (the shared src/common/ui/VibrUI.h is not touched).
// Everything is drawn in code in the 1200-unit design space (the editor scales the Graphics once), except the knob, which is drawn by its LookAndFeel in pixels
// from the size of its component (88 units wide, see kKnobBox).
namespace skeuo
{
// colour tokens
const juce::Colour kPanel (0xff2c3242), kPanelDeep (0xff232836), kSection (0xff282d3b), kCapTop (0xff3a4155), kCapBottom (0xff2e3446), kTrack (0xff1b1f2a);
const juce::Colour kBlue (0xff5bb8e0), kBlueMuted (0xff7fa6c2), kCoral (0xffe2805f), kCoralMuted (0xffc99484);
const juce::Colour kText (0xffd3dae4), kText2 (0xff8992a6);
const juce::Colour kGreen (0xff3fa54a), kYellow (0xffd6cb2c), kOrange (0xffe5793a), kRed (0xffe2463c);

struct Accent { juce::Colour main, muted; };
const Accent kBlueAccent { kBlue, kBlueMuted }, kCoralAccent { kCoral, kCoralMuted };

constexpr float kKnobBox = 88.0f;        // the slider component, in layout units
constexpr float kWellR = 40.0f, kArcR = 35.0f, kHousingR = 31.0f, kCapR = 28.0f;

//==============================================================================
// the one face of the window: the squared Silom of macOS (Aldrich / Michroma are not installed and no font is bundled); a fallback keeps it readable elsewhere
inline juce::Font font (float h, bool = true)
{
    static const juce::String face = []
    {
        for (const char* n : { "Silom", "DIN Alternate", "Futura", "Avenir Next" })
            if (juce::Font::findAllTypefaceNames().contains (n)) return juce::String (n);
        return juce::Font::getDefaultSansSerifFontName();
    }();
    return juce::Font (juce::FontOptions (face, h, juce::Font::plain));
}

inline float textWidth (const juce::Font& f, const juce::String& s)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (f, s, 0.0f, 0.0f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}

inline void drawText (juce::Graphics& g, const juce::String& s, float cx, float cy, float size, juce::Colour c, bool = true, float kerning = 0.1f,
                      juce::Justification j = juce::Justification::centred)
{
    auto f = font (size).withExtraKerningFactor (kerning);
    g.setColour (c);
    g.setFont (f);
    const float w = textWidth (f, s) + 10.0f;
    juce::Rectangle<float> r (w, size * 1.7f);
    if (j == juce::Justification::centred) r = r.withCentre ({ cx, cy });
    else if (j == juce::Justification::centredRight) r = r.withCentre ({ cx - w * 0.5f, cy });
    else r = r.withCentre ({ cx + w * 0.5f, cy });
    g.drawText (s, r, j, false);
}

// live values glow: the text with two soft halos around it
inline void glowText (juce::Graphics& g, const juce::String& s, float cx, float cy, float size, juce::Colour c, float glow = 1.0f, float kerning = 0.06f)
{
    for (int ring = 0; ring < 2; ++ring)
    {
        const float rad = ring == 0 ? 2.2f : 1.1f, a = (ring == 0 ? 0.07f : 0.13f) * glow;
        for (int k = 0; k < 8; ++k)
        {
            const float ang = juce::MathConstants<float>::twoPi * (float) k / 8.0f;
            drawText (g, s, cx + std::cos (ang) * rad, cy + std::sin (ang) * rad, size, c.withAlpha (a), true, kerning);
        }
    }
    drawText (g, s, cx, cy, size, c, true, kerning);
}

//==============================================================================
// the faint star grain: 1 px specks at 4 to 8 % white, fixed (no animation)
inline juce::Image makeStars (int w, int h)
{
    juce::Random rng (2024);
    juce::Image img (juce::Image::ARGB, w, h, true);
    const int n = w * h / 520;
    for (int i = 0; i < n; ++i)
    {
        const int x = rng.nextInt (w), y = rng.nextInt (h);
        img.setPixelAt (x, y, juce::Colours::white.withAlpha (0.04f + 0.04f * rng.nextFloat()));
        if (rng.nextInt (9) == 0) img.setPixelAt (x + 1, y, juce::Colours::white.withAlpha (0.03f));
    }
    return img;
}

inline void paintFaceplate (juce::Graphics& g, float W, float H, const juce::Image& stars)
{
    g.setGradientFill (juce::ColourGradient (kPanel.brighter (0.07f), 0.0f, 0.0f, kPanel.darker (0.04f), 0.0f, H, false));
    g.fillRect (0.0f, 0.0f, W, H);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.035f), W * 0.38f, H * 0.42f, juce::Colours::transparentWhite, W * 0.38f, H * 1.05f, true));   // a wide, quiet light
    g.fillRect (0.0f, 0.0f, W, H);
    g.drawImage (stars, 0, 0, (int) W, (int) H, 0, 0, stars.getWidth(), stars.getHeight());
}

// the preset notch: a darker tab dropping from the top edge, its shoulders curving out into the faceplate (inverted corners, 36 units)
inline juce::Path notchPath (float x0, float x1, float h, float shoulder = 36.0f, float corner = 18.0f)
{
    juce::Path p;
    p.startNewSubPath (x0 - shoulder, -4.0f);
    p.lineTo (x0 - shoulder, 0.0f);
    p.quadraticTo (x0, 0.0f, x0, shoulder);
    p.lineTo (x0, h - corner);
    p.quadraticTo (x0, h, x0 + corner, h);
    p.lineTo (x1 - corner, h);
    p.quadraticTo (x1, h, x1, h - corner);
    p.lineTo (x1, shoulder);
    p.quadraticTo (x1, 0.0f, x1 + shoulder, 0.0f);
    p.lineTo (x1 + shoulder, -4.0f);
    p.closeSubPath();
    return p;
}

inline void paintNotch (juce::Graphics& g, float x0, float x1, float h)
{
    const auto p = notchPath (x0, x1, h);
    for (int i = 4; i >= 1; --i) { g.setColour (juce::Colours::black.withAlpha (0.09f)); g.strokePath (p, juce::PathStrokeType ((float) i * 3.0f), juce::AffineTransform::translation (0.0f, 3.0f)); }   // soft drop shadow
    g.setGradientFill (juce::ColourGradient (kPanelDeep.darker (0.15f), 0.0f, 0.0f, kPanelDeep, 0.0f, h, false));
    g.fillPath (p);
    g.setColour (juce::Colours::white.withAlpha (0.06f));                                         // light rim along the bottom edge
    g.drawLine (x0 + 14.0f, h - 0.5f, x1 - 14.0f, h - 0.5f, 1.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.38f), 0.0f, 0.0f, juce::Colours::transparentBlack, 0.0f, 22.0f, false));   // inner shadow at the top
    g.saveState(); g.reduceClipRegion (p); g.fillRect (x0 - 40.0f, 0.0f, x1 - x0 + 80.0f, 22.0f); g.restoreState();
}

//==============================================================================
// a raised module (drop shadow below, light edge on top) and an inset area
inline void paintRaised (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour fill, float radius = 20.0f)
{
    for (int i = 6; i >= 1; --i)
    {
        g.setColour (juce::Colours::black.withAlpha (0.07f));
        g.fillRoundedRectangle (r.expanded ((float) i * 1.6f).translated (0.0f, 6.0f), radius + (float) i * 1.6f);
    }
    g.setGradientFill (juce::ColourGradient (fill.brighter (0.07f), 0.0f, r.getY(), fill.darker (0.05f), 0.0f, r.getBottom(), false));
    g.fillRoundedRectangle (r, radius);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.10f), 0.0f, r.getY(), juce::Colours::transparentWhite, 0.0f, r.getY() + 10.0f, false));
    g.fillRoundedRectangle (r.withHeight (3.0f).reduced (radius * 0.6f, 0.0f), 1.5f);
}

inline void paintInset (juce::Graphics& g, juce::Rectangle<float> r, float radius = 20.0f, float depth = 1.0f)
{
    g.setColour (juce::Colours::black.withAlpha (0.16f * depth));
    g.fillRoundedRectangle (r, radius);
    g.saveState();
    juce::Path clip; clip.addRoundedRectangle (r, radius); g.reduceClipRegion (clip);
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.28f * depth), 0.0f, r.getY(), juce::Colours::transparentBlack, 0.0f, r.getY() + 16.0f, false));
    g.fillRect (r);
    g.restoreState();
    g.setColour (juce::Colours::white.withAlpha (0.045f));
    g.drawLine (r.getX() + radius * 0.7f, r.getBottom() - 0.5f, r.getRight() - radius * 0.7f, r.getBottom() - 0.5f, 1.0f);
}

// a recessed field for a graph or a meter
inline void paintWell (juce::Graphics& g, juce::Rectangle<float> r, float radius = 10.0f)
{
    g.setColour (kTrack.withAlpha (0.85f));
    g.fillRoundedRectangle (r, radius);
    g.saveState();
    juce::Path clip; clip.addRoundedRectangle (r, radius); g.reduceClipRegion (clip);
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.45f), 0.0f, r.getY(), juce::Colours::transparentBlack, 0.0f, r.getY() + 14.0f, false));
    g.fillRect (r);
    g.restoreState();
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawLine (r.getX() + radius * 0.6f, r.getBottom() - 0.5f, r.getRight() - radius * 0.6f, r.getBottom() - 0.5f, 1.0f);
}

//==============================================================================
// tab pills on the top edge of a panel (centred); the selected one is a raised cap with a glowing title
inline std::vector<juce::Rectangle<float>> tabRects (juce::Rectangle<float> panel, const std::vector<juce::String>& names, float size = 12.0f)
{
    std::vector<juce::Rectangle<float>> out;
    std::vector<float> w;
    float total = 8.0f * (float) (names.size() - 1);
    for (const auto& n : names) { w.push_back (textWidth (font (size).withExtraKerningFactor (0.1f), n) + 34.0f); total += w.back(); }
    float x = panel.getCentreX() - total * 0.5f;
    for (size_t i = 0; i < names.size(); ++i) { out.push_back (juce::Rectangle<float> (w[i], 26.0f).withCentre ({ x + w[i] * 0.5f, panel.getY() })); x += w[i] + 8.0f; }
    return out;
}

inline void paintTabs (juce::Graphics& g, juce::Rectangle<float> panel, const std::vector<juce::String>& names, int selected, const Accent& acc, float size = 12.0f)
{
    const auto rects = tabRects (panel, names, size);
    for (size_t i = 0; i < names.size(); ++i)
    {
        const auto r = rects[i];
        if ((int) i == selected)
        {
            for (int s = 3; s >= 1; --s) { g.setColour (juce::Colours::black.withAlpha (0.12f)); g.fillRoundedRectangle (r.expanded ((float) s).translated (0.0f, 2.5f), 13.0f + (float) s); }
            g.setGradientFill (juce::ColourGradient (kCapTop, 0.0f, r.getY(), kCapBottom, 0.0f, r.getBottom(), false));
            g.fillRoundedRectangle (r, 13.0f);
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.drawRoundedRectangle (r.reduced (0.5f), 13.0f, 1.0f);
            glowText (g, names[i], r.getCentreX(), r.getCentreY() + 0.5f, size, acc.main, 0.8f, 0.1f);
        }
        else
        {
            g.setColour (kPanelDeep);
            g.fillRoundedRectangle (r, 13.0f);
            g.setColour (juce::Colours::black.withAlpha (0.4f));
            g.drawRoundedRectangle (r.reduced (0.5f), 13.0f, 1.0f);
            drawText (g, names[i], r.getCentreX(), r.getCentreY() + 0.5f, size, kText2, true, 0.1f);
        }
    }
}

// a pill button: off = a shallow recess with grey text; on = a raised cap with the accent glowing in its title and a lamp on its left
inline void paintButton (juce::Graphics& g, juce::Rectangle<float> b, const juce::String& label, bool on, const Accent& acc, float size = 11.0f, float alpha = 1.0f, bool lamp = false)
{
    const float rad = 9.0f;
    if (on)
    {
        g.setColour (juce::Colours::black.withAlpha (0.25f * alpha)); g.fillRoundedRectangle (b.translated (0.0f, 2.5f).expanded (1.0f), rad + 1.0f);
        g.setGradientFill (juce::ColourGradient (kCapTop.withMultipliedAlpha (alpha), 0.0f, b.getY(), kCapBottom.withMultipliedAlpha (alpha), 0.0f, b.getBottom(), false));
        g.fillRoundedRectangle (b, rad);
        g.setColour (juce::Colours::white.withAlpha (0.08f * alpha)); g.drawRoundedRectangle (b.reduced (0.5f), rad, 1.0f);
    }
    else
    {
        g.setColour (kPanelDeep.withMultipliedAlpha (alpha)); g.fillRoundedRectangle (b, rad);
        g.setColour (juce::Colours::black.withAlpha (0.35f * alpha)); g.drawRoundedRectangle (b.reduced (0.5f), rad, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.04f * alpha)); g.drawLine (b.getX() + 6.0f, b.getBottom() - 0.5f, b.getRight() - 6.0f, b.getBottom() - 0.5f, 1.0f);
    }
    float cx = b.getCentreX();
    if (lamp)
    {
        const auto c = juce::Point<float> (b.getX() + 14.0f, b.getCentreY());
        if (on) { g.setColour (acc.main.withAlpha (0.25f * alpha)); g.fillEllipse (juce::Rectangle<float> (13.0f, 13.0f).withCentre (c)); }
        g.setColour ((on ? acc.main : kTrack).withMultipliedAlpha (alpha)); g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (c));
        cx += 7.0f;
    }
    if (on) glowText (g, label, cx, b.getCentreY() + 0.5f, size, acc.main.withMultipliedAlpha (alpha), 0.7f * alpha, 0.08f);
    else drawText (g, label, cx, b.getCentreY() + 0.5f, size, kText2.withMultipliedAlpha (alpha), true, 0.08f);
}

// a small status lamp: dark when off, the accent with a halo when lit
inline void paintLamp (juce::Graphics& g, juce::Point<float> c, bool on, const Accent& acc, float r = 5.0f)
{
    g.setColour (juce::Colours::black.withAlpha (0.4f)); g.fillEllipse (juce::Rectangle<float> (2.0f * r + 4.0f, 2.0f * r + 4.0f).withCentre (c));
    if (on)
    {
        g.setGradientFill (juce::ColourGradient (acc.main.withAlpha (0.45f), c.x, c.y, juce::Colours::transparentBlack, c.x + r * 3.2f, c.y, true));
        g.fillEllipse (juce::Rectangle<float> (r * 6.4f, r * 6.4f).withCentre (c));
    }
    g.setColour (on ? acc.main : kTrack); g.fillEllipse (juce::Rectangle<float> (2.0f * r, 2.0f * r).withCentre (c));
    g.setColour (juce::Colours::white.withAlpha (on ? 0.35f : 0.06f)); g.fillEllipse (juce::Rectangle<float> (r * 0.8f, r * 0.8f).withCentre (c.translated (-r * 0.3f, -r * 0.35f)));
}

// the power switch: a round cap in a well, its ring glowing blue when on
inline void paintPower (juce::Graphics& g, juce::Rectangle<float> r, bool on)
{
    const auto c = r.getCentre();
    const float R = r.getWidth() * 0.5f;
    g.setColour (kPanelDeep); g.fillEllipse (r);
    g.setColour (juce::Colours::black.withAlpha (0.45f)); g.drawEllipse (r.reduced (0.5f), 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.06f)); g.drawEllipse (r.expanded (0.5f), 1.0f);
    const auto capR = r.reduced (R * 0.22f);
    g.setGradientFill (juce::ColourGradient (kCapTop, c.x - R * 0.4f, c.y - R * 0.5f, kCapBottom, c.x + R * 0.5f, c.y + R * 0.6f, true));
    g.fillEllipse (capR);
    juce::Path ic;
    ic.addCentredArc (c.x, c.y + 0.5f, R * 0.30f, R * 0.30f, 0.0f, juce::degreesToRadians (40.0f), juce::degreesToRadians (320.0f), true);
    ic.startNewSubPath (c.x, c.y - R * 0.40f); ic.lineTo (c.x, c.y - R * 0.05f);
    const juce::PathStrokeType st (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    if (on) { g.setColour (kBlue.withAlpha (0.14f)); g.strokePath (ic, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)); g.setColour (kBlue.withAlpha (0.25f)); g.strokePath (ic, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)); }
    g.setColour (on ? kBlue : kText2); g.strokePath (ic, st);
}

// the wordmark (text.primary) with its single coral triangle
inline void paintLogo (juce::Graphics& g, float x, float cy, const juce::String& name)
{
    juce::Path tri;
    tri.addTriangle (x, cy + 9.0f, x + 17.0f, cy + 9.0f, x + 8.5f, cy - 9.0f);
    g.setColour (kCoral.withAlpha (0.18f)); g.strokePath (tri, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (kCoral); g.fillPath (tri);
    drawText (g, name, x + 28.0f, cy, 26.0f, kText, true, 0.1f, juce::Justification::centredLeft);
}

//==============================================================================
// Behind a knob: the soft spotlight around it and the recessed well (circle, inner shadow at the top, light rim at the bottom edge). Centre c, layout units.
inline void paintKnobBed (juce::Graphics& g, juce::Point<float> c)
{
    const float R = kWellR;
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.06f), c.x, c.y, juce::Colours::transparentWhite, c.x + R * 1.45f, c.y, true));
    g.fillEllipse (juce::Rectangle<float> (R * 2.9f, R * 2.9f).withCentre (c));
    const auto well = juce::Rectangle<float> (2.0f * R, 2.0f * R).withCentre (c);
    g.setColour (juce::Colours::white.withAlpha (0.13f)); g.drawEllipse (well.expanded (0.5f).translated (0.0f, 0.8f), 1.0f);     // the rim on the bottom edge
    g.setGradientFill (juce::ColourGradient (kTrack.darker (0.2f), c.x, c.y - R, kPanelDeep.darker (0.05f), c.x, c.y + R, false));
    g.fillEllipse (well);
    juce::Path clip; clip.addEllipse (well);
    g.saveState(); g.reduceClipRegion (clip);
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.55f), c.x, c.y - R, juce::Colours::transparentBlack, c.x, c.y - R + 16.0f, false));
    g.fillRect (well);
    g.restoreState();
}

// the readout above a knob and the range labels in the gap under it
inline void paintKnobText (juce::Graphics& g, juce::Point<float> c, const juce::String& value, const juce::String& lo, const juce::String& hi, const Accent& acc)
{
    glowText (g, value, c.x, c.y - kWellR - 11.0f, 12.0f, acc.main, 1.0f, 0.05f);
    drawText (g, lo, c.x - 29.0f, c.y + 38.0f, 10.0f, kText, true, 0.05f);
    drawText (g, hi, c.x + 29.0f, c.y + 38.0f, 10.0f, kText, true, 0.05f);
}

// The knob itself (inside its well): the unlit track and the lit arc (270 degrees, round caps, glow), a dark housing ring, the machined cap with its engraved label
// and the glowing indicator dot. Coral instead of blue when the slider has the property "coral".
class KnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
    {
        const auto c = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).getCentre();
        const float u = (float) juce::jmin (w, h) / kKnobBox;
        const auto& acc = (bool) s.getProperties()["coral"] ? kCoralAccent : kBlueAccent;
        const float hot = s.isMouseButtonDown() ? 1.6f : s.isMouseOver() ? 1.3f : 1.0f;
        const auto disc = [&c] (float r) { return juce::Rectangle<float> (2.0f * r, 2.0f * r).withCentre (c); };

        // arc: the track and the lit part
        const float ar = kArcR * u, stroke = 3.0f * u;
        juce::Path track, lit;
        track.addCentredArc (c.x, c.y, ar, ar, 0.0f, a0, a1, true);
        const float av = a0 + pos * (a1 - a0);
        if (pos > 0.002f) lit.addCentredArc (c.x, c.y, ar, ar, 0.0f, a0, av, true);
        g.setColour (kTrack);
        g.strokePath (track, juce::PathStrokeType (stroke + 1.0f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (pos > 0.002f)
        {
            for (int i = 3; i >= 1; --i)
            {
                g.setColour (acc.main.withAlpha (0.07f * (float) (4 - i) * hot));
                g.strokePath (lit, juce::PathStrokeType (stroke + (float) i * 3.2f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            g.setColour (acc.main);
            g.strokePath (lit, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // housing ring
        const float hr = kHousingR * u, cr = kCapR * u;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff10131a), c.x, c.y - hr, juce::Colour (0xff222735), c.x, c.y + hr, false));
        g.fillEllipse (disc (hr));
        g.setColour (juce::Colours::white.withAlpha (0.05f)); g.drawEllipse (disc (hr).translated (0.0f, 0.6f * u), 1.0f);

        // cap: shadow, radial body lit from the top left, machined rim
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.55f), c.x, c.y + 3.0f * u, juce::Colours::transparentBlack, c.x, c.y + 3.0f * u + cr * 1.35f, true));
        g.fillEllipse (disc (cr * 1.35f).translated (0.0f, 3.0f * u));
        g.setGradientFill (juce::ColourGradient (kCapTop.brighter (0.08f), c.x - cr * 0.45f, c.y - cr * 0.55f, kCapBottom.darker (0.1f), c.x + cr * 0.6f, c.y + cr * 0.8f, true));
        g.fillEllipse (disc (cr));
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.16f), c.x - cr * 0.7f, c.y - cr * 0.7f, juce::Colours::black.withAlpha (0.4f), c.x + cr * 0.7f, c.y + cr * 0.7f, false));
        g.drawEllipse (disc (cr - 0.5f * u), 1.0f * u);
        g.setColour (juce::Colours::black.withAlpha (0.22f)); g.drawEllipse (disc (cr * 0.86f), 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.05f)); g.drawEllipse (disc (cr * 0.86f).translated (0.0f, 0.8f), 1.0f);

        // engraved label (does not rotate): up to two lines
        {
            juce::String name = s.getName();
            const juce::StringArray lines = juce::StringArray::fromTokens (name, "\n", "");
            const float maxW = cr * 1.52f;
            float size = 10.0f * u;
            for (const auto& l : lines) { const float tw = textWidth (font (size).withExtraKerningFactor (0.04f), l); if (tw > maxW) size = juce::jmin (size, size * maxW / tw); }
            const float lh = size * 1.15f, y0 = c.y - 0.5f * lh * (float) (lines.size() - 1) + 0.5f * u;
            for (int i = 0; i < lines.size(); ++i)
            {
                const float cy = y0 + lh * (float) i;
                drawText (g, lines[i], c.x, cy + 0.9f * u, size, juce::Colours::black.withAlpha (0.6f), true, 0.04f);
                drawText (g, lines[i], c.x, cy - 0.4f * u, size, juce::Colours::white.withAlpha (0.04f), true, 0.04f);
                drawText (g, lines[i], c.x, cy, size, acc.muted.withMultipliedAlpha (hot > 1.0f ? 1.0f : 0.92f), true, 0.04f);
            }
        }

        // indicator dot near the rim
        const float dr = cr - 3.4f * u;
        const juce::Point<float> d (c.x + std::sin (av) * dr, c.y - std::cos (av) * dr);
        for (int i = 3; i >= 1; --i) { g.setColour (acc.main.withAlpha (0.12f * hot)); g.fillEllipse (juce::Rectangle<float> (2.0f * u * (1.6f + 0.9f * (float) i), 2.0f * u * (1.6f + 0.9f * (float) i)).withCentre (d)); }
        g.setColour (acc.main); g.fillEllipse (juce::Rectangle<float> (3.2f * u, 3.2f * u).withCentre (d));
        g.setColour (juce::Colours::white.withAlpha (0.55f)); g.fillEllipse (juce::Rectangle<float> (1.2f * u, 1.2f * u).withCentre (d.translated (-0.3f * u, -0.3f * u)));
    }
};

//==============================================================================
// a segmented level meter: a track, segments in the zone colours (green, yellow above -12 dB, orange above -3 dB, red at the clip cap), a peak-hold tick.
// level01 and hold01 run over `rangeDb` (-60 .. 0); the top segment is the clip cap.
inline void paintMeter (juce::Graphics& g, juce::Rectangle<float> r, float level01, float hold01, bool coralFill = false)
{
    paintWell (g, r.expanded (3.0f), 4.0f);
    constexpr int N = 36;
    const float gap = 1.4f, sh = (r.getHeight() - gap * (float) (N - 1)) / (float) N;
    const auto zone = [&] (int i)
    {
        const float db = -60.0f + 60.0f * (float) (i + 1) / (float) N;
        if (coralFill) return kCoral;
        return db > -0.8f ? kRed : db > -3.0f ? kOrange : db > -12.0f ? kYellow : kGreen;
    };
    for (int i = 0; i < N; ++i)
    {
        const auto seg = juce::Rectangle<float> (r.getX(), r.getBottom() - (float) (i + 1) * sh - (float) i * gap, r.getWidth(), sh);
        const bool lit = (float) (i + 1) / (float) N <= level01 + 0.001f && level01 > 0.001f;
        g.setColour (lit ? zone (i) : kTrack.brighter (0.05f));
        g.fillRoundedRectangle (seg, 1.0f);
    }
    if (hold01 > 0.01f)
    {
        const int i = juce::jlimit (0, N - 1, (int) std::ceil (hold01 * (float) N) - 1);
        g.setColour (zone (i));
        g.fillRect (juce::Rectangle<float> (r.getX() - 2.0f, r.getBottom() - (float) (i + 1) * (sh + gap) + gap * 0.5f, r.getWidth() + 4.0f, 1.8f));
    }
}
} // namespace skeuo
