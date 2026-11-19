// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once
#include <JuceHeader.h>
#include "VibrLogo.h"
#include "ui/Presets.h"            // vibr::PresetManager, the preset menu look and rows (shared, unchanged)
#include "ui/OutputMeter.h"        // vibr::ui::OutputMeter: the dB state of the meters (this file only draws it)
#include "VibrFontData.h"          // the embedded fonts (juce_add_binary_data VibrFonts, src/common/CMakeLists.txt)

// "Vintage hardware" look for the code-drawn VIBR windows: a cream front plate with corner screws, black bakelite knobs with a red value ring, outlined panels
// with their name in the edge, tab buttons inside the panel, a green phosphor screen for the preset, the division, the scope and the meters. Everything draws in a
// 1120 x 660 design space (the editor scales the Graphics once: g.addTransform (AffineTransform::scale (width / kW))).
//
// This is the default look of the VIBR windows (the RH-1 first: from the "RH-1 Rhythm Lab - UI redesign" canvas, direction A; the guide is .claude/style_guide.md). The old
// grey / satin look of the Labs stays as the backup in VibrUI.h and LabEditor.h (namespace vibr::ui). An editor draws only through these functions, which have the
// same names as the old ones (paintBody, paintHeader, paintPanel, tabRects / paintTabs, paintFlatButton, paintKnob, paintFooter, drawText, Palette, KnobLookAndFeel)
// plus the preset bar (paintPresetBar, presetBarMouseDown). To use it an editor derives from vibr::ui::vintage::LabEditor (VintageLabEditor.h, which installs the
// bakelite knob) and writes `namespace ui = vibr::ui::vintage;`. Pedals without the Lab base class only need the namespace and the knob look-and-feel.
//
// Fonts: Oswald (labels) and IBM Plex Mono (values) are embedded in every plugin that links rl_common (src/common/ui/fonts, SIL Open Font License 1.1, the texts next to
// the files; juce_add_binary_data VibrFonts in src/common/CMakeLists.txt), so nothing depends on the fonts of the system.
namespace vibr::ui::vintage
{
constexpr float kW = 1120.0f, kH = 660.0f, kMargin = 28.0f, kRuleY = 90.0f;
constexpr float kKnobR = 30.0f, kKnobPad = 5.0f;                         // the black body radius, and the margin the slider keeps around it

struct Palette
{
    Palette() = default;
    explicit Palette (juce::Colour plugin) : accent (plugin), accentSoft (plugin.interpolatedWith (juce::Colour (0xffE6DCC6), 0.45f)) {}      // the plugin's own colour

    juce::Colour accent { 0xffB8361F }, accentSoft { 0xffD98A74 };       // the plugin's colour: the value rings, the full steps, the values and the plugin code (default: red)
    juce::Colour paper { 0xffE6DCC6 }, paperLight { 0xffEFE7D5 }, cream { 0xffF3ECDC };
    juce::Colour ink { 0xff2A251F }, inkSoft { 0xff5E5447 }, track { 0xffC9BCA0 }, stepOff { 0xffCFC3A8 }, dashed { 0xffA89B7F };
    juce::Colour screen { 0xff0F140D }, phosphor { 0xff9BEA84 }, phosphorDim { 0xff6FAE60 };
    juce::Colour segGreen { 0xff7FD86A }, segYellow { 0xffE8C14A }, segRed { 0xffF0573A }, segOff { 0xff253020 };
    juce::Colour ledOn { 0xffFF4A2A }, ledOff { 0xff5A2A20 }, screw { 0xffB3A688 }, screwShade { 0xff6F6450 };
};

//==============================================================================
// Label = Oswald (Medium, or SemiBold when `bold`), Title = Oswald Bold (the plugin code), Mono = IBM Plex Mono (Regular, or SemiBold when `bold`)
enum class Face { Label, Mono, Title };

// the five typefaces, made once from the font data compiled into the plugin (look/fonts, see CMakeLists.txt): nothing is looked up on the system
struct Faces
{
    juce::Typeface::Ptr oswaldMedium   = juce::Typeface::createSystemTypefaceFor (VibrFontData::OswaldMedium_ttf, VibrFontData::OswaldMedium_ttfSize);
    juce::Typeface::Ptr oswaldSemiBold = juce::Typeface::createSystemTypefaceFor (VibrFontData::OswaldSemiBold_ttf, VibrFontData::OswaldSemiBold_ttfSize);
    juce::Typeface::Ptr oswaldBold     = juce::Typeface::createSystemTypefaceFor (VibrFontData::OswaldBold_ttf, VibrFontData::OswaldBold_ttfSize);
    juce::Typeface::Ptr plexRegular    = juce::Typeface::createSystemTypefaceFor (VibrFontData::PlexMonoRegular_ttf, VibrFontData::PlexMonoRegular_ttfSize);
    juce::Typeface::Ptr plexSemiBold   = juce::Typeface::createSystemTypefaceFor (VibrFontData::PlexMonoSemiBold_ttf, VibrFontData::PlexMonoSemiBold_ttfSize);
};
inline const Faces& faces() { static const Faces f; return f; }

inline juce::Font font (float h, bool bold = true, Face face = Face::Label)
{
    const auto& f = faces();
    const auto& tf = face == Face::Title ? f.oswaldBold : face == Face::Mono ? (bold ? f.plexSemiBold : f.plexRegular) : (bold ? f.oswaldSemiBold : f.oswaldMedium);
    return juce::Font (juce::FontOptions (tf).withHeight (h));
}

inline float textWidth (const juce::Font& f, const juce::String& s)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (f, s, 0.0f, 0.0f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}

// text centred on (cx, cy) (or left / right aligned from that point), `size` in layout units, `kerning` as a fraction of the font height
inline void drawText (juce::Graphics& g, const juce::String& s, float cx, float cy, float size, juce::Colour c, bool bold = true, float kerning = 0.08f,
                      juce::Justification j = juce::Justification::centred, Face face = Face::Label)
{
    auto f = font (size, bold, face).withExtraKerningFactor (kerning);
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
// The black bakelite knob: a dark domed body with a lower rim and a cream pointer bar. Drawn kKnobPad inside its component (in the same ratio when the window is
// scaled). The value ring around it is drawn by the editor (paintKnob / paintValueArc), so MIDI marks and popups work as on every other knob.
class KnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider&) override
    {
        const auto c = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).getCentre();
        const float half = (float) juce::jmin (w, h) * 0.5f, R = half * kKnobR / (kKnobR + kKnobPad);
        const auto circle = [&c] (float r) { return juce::Rectangle<float> (2.0f * r, 2.0f * r).withCentre (c); };

        g.setColour (juce::Colour (0xff8F846F));                                                    // the lower rim (a 3 unit drop shadow)
        g.fillEllipse (circle (R).translated (0.0f, R * 0.14f));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4A443D), c.x - R * 0.3f, c.y - R * 0.4f, juce::Colour (0xff15120F), c.x + R * 0.55f, c.y + R * 0.6f, true));
        g.fillEllipse (circle (R));
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.drawEllipse (circle (R - 0.5f), 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawEllipse (circle (R * 0.62f), 1.0f);                                                   // the soft step of the cap

        const float a = startAngle + pos * (endAngle - startAngle);
        juce::Path bar;
        bar.addRoundedRectangle (-R * 0.07f, -R * 0.82f, R * 0.14f, R * 0.62f, R * 0.07f);
        g.setColour (juce::Colour (0xffF3ECDC));
        g.fillPath (bar, juce::AffineTransform::rotation (a).translated (c));
    }
};

//==============================================================================
inline void paintBody (juce::Graphics& g, const Palette& pal, float W, float H)
{
    g.setColour (pal.paper);
    g.fillRect (0.0f, 0.0f, W, H);
    for (const auto& c : { juce::Point<float> (15.0f, 15.0f), { W - 15.0f, 15.0f }, { 15.0f, H - 15.0f }, { W - 15.0f, H - 15.0f } })      // four screws
    {
        const auto r = juce::Rectangle<float> (10.0f, 10.0f).withCentre (c);
        g.setColour (pal.screw);
        g.fillEllipse (r);
        g.setColour (pal.screwShade);
        g.drawEllipse (r.reduced (0.5f), 1.0f);
        g.drawLine (c.x - 2.8f, c.y + 2.8f, c.x + 2.8f, c.y - 2.8f, 1.2f);
    }
}

// the name, two description lines and the rule under the header; the preset bar sits on the right (see paintPresetBar)
inline void paintHeader (juce::Graphics& g, const Palette& pal, const juce::String& name, const juce::String& line1, const juce::String& line2, float limit = 326.0f)
{
    drawText (g, name, kMargin, 48.0f, 74.0f, pal.accent, true, -0.01f, juce::Justification::centredLeft, Face::Title);        // the plugin code in the plugin's colour (the red of the value rings)
    const float x = kMargin + textWidth (font (74.0f, true, Face::Title).withExtraKerningFactor (-0.01f), name) + 30.0f;
    const float room = limit - x;                                                                                       // the preset screen starts at 350 (limit 326): a longer line shrinks to fit
    const auto fit = [room] (const juce::String& t, float size, float kerning) { const float w = textWidth (font (size, true).withExtraKerningFactor (kerning), t); return w > room ? size * room / w : size; };
    drawText (g, line1, x, 37.0f, fit (line1, 31.0f, 0.16f), pal.ink, true, 0.16f, juce::Justification::centredLeft);
    drawText (g, line2, x, 64.0f, fit (line2, 20.0f, 0.2f), pal.inkSoft, true, 0.2f, juce::Justification::centredLeft);
}      // (no rule under the header)

// a flat outlined button: lit = ink with cream text; `alpha` dims a button that does not apply
inline void paintFlatButton (juce::Graphics& g, const Palette& pal, juce::Rectangle<float> b, const juce::String& label, bool on, float fontSize = 17.0f, float alpha = 1.0f,
                             float corner = 4.0f, float kerning = 0.08f, float outline = 2.0f)
{
    corner = juce::jmin (corner, 5.0f);
    if (const float w = textWidth (font (fontSize, true).withExtraKerningFactor (kerning), label), room = b.getWidth() - 12.0f; w > room) fontSize *= room / w;      // a label that does not fit (a wider font) shrinks
    g.setColour ((on ? pal.ink : pal.paperLight).withMultipliedAlpha (alpha));
    g.fillRoundedRectangle (b, corner);
    g.setColour (pal.ink.withMultipliedAlpha (alpha));
    g.drawRoundedRectangle (b.reduced (outline * 0.5f), corner, outline);
    drawText (g, label, b.getCentreX(), b.getCentreY(), fontSize, (on ? pal.cream : pal.ink).withMultipliedAlpha (alpha), true, kerning);
}

// a panel: outlined, with its name in the top edge (left)
inline void paintPanel (juce::Graphics& g, const Palette& pal, juce::Rectangle<float> r, const juce::String& title, float radius = 6.0f)
{
    g.setColour (pal.ink);
    g.drawRoundedRectangle (r.reduced (1.0f), radius, 2.0f);
    if (title.isEmpty()) return;
    const auto f = font (25.0f, true).withExtraKerningFactor (0.16f);
    const float w = textWidth (f, title.toUpperCase()) + 16.0f;
    g.setColour (pal.paper);
    g.fillRect (r.getX() + 14.0f, r.getY() - 11.0f, w, 24.0f);
    drawText (g, title.toUpperCase(), r.getX() + 22.0f, r.getY() + 1.0f, 25.0f, pal.ink, true, 0.16f, juce::Justification::centredLeft);
}

// tab buttons inside the top of a panel, from the left (`top` = their distance from the panel's top edge); returns their rectangles (for hit tests)
// where the tab buttons start when they sit on the top edge of a panel, right of the panel's name (see paintPanel)
inline float titleEnd (const juce::String& title) { return 14.0f + textWidth (font (25.0f, true).withExtraKerningFactor (0.16f), title.toUpperCase()) + 16.0f + 12.0f; }

// The tab buttons of a panel: in a row from `left` (distance from the panel's left edge), `top` = their distance from the panel's top edge (-15 = centred on the edge,
// the buttons are 30 high). A row that is wider than the room left in the panel shrinks its type to fit.
struct TabLayout { std::vector<juce::Rectangle<float>> rects; float size; };
inline TabLayout layoutTabs (juce::Rectangle<float> panel, const std::vector<juce::String>& names, float top, float fontSize, float left)
{
    const float pad = 20.0f, gap = 6.0f, room = panel.getWidth() - left - 16.0f;
    const auto totalAt = [&] (float size)
    {
        float t = gap * (float) (names.size() - 1);
        for (const auto& n : names) t += textWidth (font (size, true).withExtraKerningFactor (0.08f), n) + pad;
        return t;
    };
    float size = fontSize;
    for (int i = 0; i < 3; ++i) if (const float t = totalAt (size); t > room) size *= room / t * 0.98f;
    TabLayout out { {}, size };
    float x = panel.getX() + left;
    for (const auto& n : names)
    {
        const float w = textWidth (font (size, true).withExtraKerningFactor (0.08f), n) + pad;
        out.rects.push_back ({ x, panel.getY() + top + 1.0f, w, 30.0f });      // +1: the panel's line is 2 thick (y .. y + 2), its middle is 1 below the edge
        x += w + gap;
    }
    return out;
}
inline std::vector<juce::Rectangle<float>> tabRects (juce::Rectangle<float> panel, const std::vector<juce::String>& names, float top = -15.0f, float fontSize = 16.0f, float left = 20.0f)
{
    return layoutTabs (panel, names, top, fontSize, left).rects;
}
inline void paintTabs (juce::Graphics& g, const Palette& pal, juce::Rectangle<float> panel, const std::vector<juce::String>& names, int selected, float top = -15.0f, float fontSize = 16.0f, float left = 20.0f)
{
    const auto lay = layoutTabs (panel, names, top, fontSize, left);
    if (! lay.rects.empty())                                                                                         // the panel's line stops 6 before the first tab and 6 after the last
    {
        const float x0 = lay.rects.front().getX() - 6.0f, x1 = lay.rects.back().getRight() + 6.0f;
        g.setColour (pal.paper);
        g.fillRect (x0, panel.getY() - 1.0f, x1 - x0, 4.0f);
    }
    for (size_t i = 0; i < names.size(); ++i) paintFlatButton (g, pal, lay.rects[i], names[i], (int) i == selected, lay.size);
}

//==============================================================================
// The value ring of a knob: the track and, from the minimum (-135 deg) to the value (prop 0..1), the red arc. `knobRadius` = the black body's radius.
inline float ringRadius (float knobRadius) { return knobRadius * 1.371f; }
inline void paintValueArc (juce::Graphics& g, juce::Point<float> c, float knobRadius, float prop, juce::Colour accent, juce::Colour track)
{
    const float r = ringRadius (knobRadius), a0 = juce::degreesToRadians (-135.0f), w = knobRadius * 0.24f;
    juce::Path t, lit;
    t.addCentredArc (c.x, c.y, r, r, 0.0f, a0, -a0, true);
    g.setColour (track);
    g.strokePath (t, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (prop <= 0.002f) return;
    lit.addCentredArc (c.x, c.y, r, r, 0.0f, a0, a0 + prop * 2.0f * -a0, true);
    g.setColour (accent);
    g.strokePath (lit, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// the same ring for a value around a centre mark (prop -1 .. +1): from the top (0) to the value
inline void paintBipolarArc (juce::Graphics& g, juce::Point<float> c, float knobRadius, float prop, juce::Colour accent, juce::Colour track)
{
    const float r = ringRadius (knobRadius), a0 = juce::degreesToRadians (-135.0f), w = knobRadius * 0.24f;
    juce::Path t, lit;
    t.addCentredArc (c.x, c.y, r, r, 0.0f, a0, -a0, true);
    g.setColour (track);
    g.strokePath (t, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (std::abs (prop) < 0.01f) return;
    const float a1 = prop * -a0;
    lit.addCentredArc (c.x, c.y, r, r, 0.0f, std::min (0.0f, a1), std::max (0.0f, a1), true);
    g.setColour (accent);
    g.strokePath (lit, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// A knob's decoration around its centre: the value ring, the legends under the ends, the name and the value (red, mono) in two rows below.
inline void paintKnob (juce::Graphics& g, const Palette& pal, juce::Point<float> c, const juce::String& name, const juce::String& value, const juce::String& lo,
                       const juce::String& hi, float prop, float knobR = kKnobR)
{
    paintValueArc (g, c, knobR, prop, pal.accent, pal.track);
    const float rr = knobR * 1.71f;                                                 // half of the 72 unit cell the artwork is drawn in
    drawText (g, lo, c.x - knobR * 1.4f, c.y + ringRadius (knobR) * 0.707f + 15.0f, 13.0f, pal.inkSoft, false, 0.0f, juce::Justification::centredLeft, Face::Mono);
    drawText (g, hi, c.x + knobR * 1.4f, c.y + ringRadius (knobR) * 0.707f + 15.0f, 13.0f, pal.inkSoft, false, 0.0f, juce::Justification::centredRight, Face::Mono);
    drawText (g, name, c.x, c.y + rr + 22.0f, 21.0f, pal.ink, true, 0.1f);
    drawText (g, value, c.x, c.y + rr + 43.0f, 14.0f, pal.accent, true, 0.0f, juce::Justification::centred, Face::Mono);
}

// the dark recess and tick marks of a small knob (the matrix depth knobs): `knobR` = the black body's radius
inline void paintSmallKnobTicks (juce::Graphics& g, const Palette& pal, juce::Point<float> c, float knobR)
{
    for (int t = 0; t <= 10; ++t)
    {
        const float a = juce::degreesToRadians (-135.0f + 27.0f * (float) t), sn = std::sin (a), co = std::cos (a);
        const bool major = t == 0 || t == 5 || t == 10;
        const float r0 = ringRadius (knobR) + knobR * 0.18f, r1 = r0 + (major ? 4.0f : 2.5f);
        g.setColour (pal.inkSoft);
        g.drawLine (c.x + sn * r0, c.y - co * r0, c.x + sn * r1, c.y - co * r1, major ? 1.8f : 1.1f);
    }
}

//==============================================================================
// a phosphor screen: the small display for the preset name or the note length, and the frame of the scope and the meters
inline void paintScreen (juce::Graphics& g, const Palette& pal, juce::Rectangle<float> r, float corner = 5.0f)
{
    g.setColour (pal.screen);
    g.fillRoundedRectangle (r, corner);
    g.setColour (pal.phosphor.withAlpha (0.05f));
    g.fillRoundedRectangle (r.reduced (3.0f), corner * 0.6f);
    g.setColour (pal.ink);
    g.drawRoundedRectangle (r.reduced (1.5f), corner, 3.0f);
}

// the scope: the screen with its grid, a label top left and a hint top right; draw the traces in scopePlotRect() afterwards (clipped to the screen)
inline juce::Rectangle<float> scopePlotRect (juce::Rectangle<float> r) { return r.withTrimmedTop (30.0f).withTrimmedBottom (8.0f).reduced (4.0f, 0.0f); }      // the label and the hint have the strip above the plot
inline void paintScope (juce::Graphics& g, const Palette& pal, juce::Rectangle<float> r, const juce::String& label, const juce::String& hint)
{
    paintScreen (g, pal, r);
    g.saveState();
    g.reduceClipRegion (r.reduced (3.0f).toNearestInt());
    g.setColour (pal.phosphor.withAlpha (0.10f));
    for (float x = r.getX() + 40.0f; x < r.getRight(); x += 40.0f) g.drawLine (x, r.getY(), x, r.getBottom(), 1.0f);
    for (float y = r.getY() + 20.0f; y < r.getBottom(); y += 20.0f) g.drawLine (r.getX(), y, r.getRight(), y, 1.0f);
    g.restoreState();
    drawText (g, label.toUpperCase(), r.getX() + 12.0f, r.getY() + 16.0f, 15.0f, pal.phosphorDim, false, 0.08f, juce::Justification::centredLeft, Face::Mono);
    drawText (g, hint.toUpperCase(), r.getRight() - 12.0f, r.getY() + 16.0f, 15.0f, pal.phosphorDim.withAlpha (0.8f), false, 0.08f, juce::Justification::centredRight, Face::Mono);
}

// the 16-step lane: a box per step with a bar that grows from the bottom (full = red, lower = lighter, off = a stub), the playing step outlined in red, the beat
// numbers under every fourth step. `values` 0..1; `slicer`: a step below 0.5 is off (dimmed), as the slicer reads it.
inline void paintStepLane (juce::Graphics& g, const Palette& pal, juce::Rectangle<float> lane, const float* values, int n, int current, bool slicer)
{
    const float cw = lane.getWidth() / (float) n;
    for (int s = 0; s < n; ++s)
    {
        const float v = juce::jlimit (0.0f, 1.0f, values[s]);
        const auto box = juce::Rectangle<float> (cw - 6.0f, lane.getHeight()).withX (lane.getX() + (float) s * cw + 3.0f).withY (lane.getY());
        g.setColour (s == current ? pal.cream : pal.paperLight);
        g.fillRoundedRectangle (box, 4.0f);
        const auto inner = box.reduced (3.0f);
        const bool on = slicer ? v >= 0.5f : v > 0.0f;
        const float h = juce::jmax (3.0f, inner.getHeight() * v);
        g.setColour (v > 0.0f ? pal.accentSoft.interpolatedWith (pal.accent, v).withAlpha (on ? 1.0f : 0.45f) : pal.stepOff);
        g.fillRoundedRectangle (inner.withTop (inner.getBottom() - h), 2.0f);
        g.setColour (s == current ? pal.accent : pal.ink);
        g.drawRoundedRectangle (box.reduced (1.0f), 4.0f, 2.0f);
        if (s % 4 == 0) drawText (g, juce::String (s / 4 + 1), box.getCentreX(), lane.getBottom() + 13.0f, 16.0f, pal.inkSoft, true, 0.0f, juce::Justification::centred, Face::Mono);
    }
}

//==============================================================================
// The output meters: two columns of 16 segments (3 dB each from -48 dB) with the L / R names on top and the peak hold under them, on a phosphor screen.
constexpr float kMeterW = 96.0f, kMeterH = 195.0f;
inline void paintMeter (juce::Graphics& g, const Palette& pal, const vibr::ui::OutputMeter& m, float x, float y, bool on)
{
    paintScreen (g, pal, { x, y, kMeterW, kMeterH }, 6.0f);
    for (int ch = 0; ch < 2; ++ch)
    {
        const float left = x + 20.0f + (float) ch * 40.0f, cx = left + 8.0f;                                        // two 16 wide columns, 24 apart, centred in the 96 wide screen (20 left and right)
        const float level = on ? m.bar[ch] : -90.0f, hold = on ? m.hold[ch] : -90.0f;
        drawText (g, ch == 0 ? "L" : "R", cx, y + 17.0f, 15.0f, pal.phosphorDim, true, 0.1f, juce::Justification::centred, Face::Mono);
        for (int i = 0; i < 16; ++i)
        {
            const float db = -48.0f + (float) i * 3.0f;
            const juce::Rectangle<float> seg (left, y + 28.0f + (float) (15 - i) * 9.0f, 16.0f, 6.0f);
            const auto lit = i >= 14 ? pal.segRed : i >= 11 ? pal.segYellow : pal.segGreen;
            const bool isHold = hold > -60.0f && hold >= db && hold < db + 3.0f;
            g.setColour (level >= db ? lit : isHold ? lit.withAlpha (0.55f) : pal.segOff);
            g.fillRoundedRectangle (seg, 1.0f);
        }
        const juce::String txt = on && hold > -60.0f ? (hold <= -10.0f ? juce::String ((int) std::lround (hold)) : juce::String (hold, 1)) : juce::String ("-inf");
        drawText (g, txt, cx, y + kMeterH - 14.0f, 14.0f, on && hold > -1.0f ? pal.segRed : pal.phosphor, true, 0.0f, juce::Justification::centred, Face::Mono);
    }
}

// "Made by Richard Leurs | VIBR" and the small logo in the bottom right corner; `y` = the centre line
inline void paintFooter (juce::Graphics& g, const Palette& pal, float y)
{
    drawText (g, "Made by Richard Leurs | VIBR", kW - kMargin - 22.0f, y, 14.0f, pal.inkSoft, false, 0.05f, juce::Justification::centredRight);
    drawVibrLogo (g, juce::Rectangle<float> (16.0f, 16.0f).withCentre ({ kW - kMargin - 8.0f, y }), pal.ink);
}

//==============================================================================
// The header bar: the preset screen, UNDO / REDO, A / B and the power switch, right aligned on the header line.
inline juce::Rectangle<float> presetChipRect() { return { 350.0f, 28.0f, 400.0f, 44.0f }; }
inline juce::Rectangle<float> undoRect (bool redo) { return { redo ? 846.0f : 778.0f, 33.0f, 62.0f, 34.0f }; }
inline juce::Rectangle<float> abRect (int i) { return { 926.0f + 36.0f * (float) i, 33.0f, 38.0f, 34.0f }; }          // the two buttons share their middle border
inline juce::Rectangle<float> powerRect() { return { 1006.0f, 33.0f, 86.0f, 34.0f }; }

// the power switch of the header (a dark button with a lamp): `powerRect()`; also used alone by the plugins without a preset bar (the pedals)
inline void paintPowerButton (juce::Graphics& g, const Palette& pal, bool powerOn)
{
    const auto pr = powerRect();
    g.setColour (pal.ink);
    g.fillRoundedRectangle (pr, 4.0f);
    const auto led = juce::Rectangle<float> (10.0f, 10.0f).withCentre ({ pr.getX() + 24.0f, pr.getCentreY() });
    const auto ledCol = powerOn ? pal.ledOn : pal.ledOff;
    if (powerOn) { g.setColour (ledCol.withAlpha (0.25f)); g.fillEllipse (led.expanded (4.0f)); }
    g.setColour (ledCol);
    g.fillEllipse (led);
    drawText (g, powerOn ? "ON" : "OFF", pr.getX() + 40.0f, pr.getCentreY(), 20.0f, pal.cream, true, 0.14f, juce::Justification::centredLeft);
}

// the header of a plugin without presets (the pedals): the code, the name and the description, and the power switch; the text may use the room up to the switch
inline void paintHeaderNoPresets (juce::Graphics& g, const Palette& pal, const juce::String& name, const juce::String& line1, const juce::String& line2, bool powerOn)
{
    paintHeader (g, pal, name, line1, line2, 980.0f);
    paintPowerButton (g, pal, powerOn);
}

inline void paintPresetBar (juce::Graphics& g, const Palette& pal, const vibr::PresetManager& mgr, const juce::UndoManager& undo, bool powerOn)
{
    const auto chip = presetChipRect();
    paintScreen (g, pal, chip);
    drawText (g, "PRESET", chip.getX() + 16.0f, chip.getCentreY(), 14.0f, pal.phosphorDim, false, 0.14f, juce::Justification::centredLeft, Face::Mono);
    const float nameX = chip.getX() + 16.0f + textWidth (font (14.0f, false, Face::Mono).withExtraKerningFactor (0.14f), "PRESET") + 14.0f;       // 14 units after the label, so the name has the rest of the screen
    g.saveState();
    g.reduceClipRegion (chip.reduced (4.0f).withTrimmedRight (26.0f).toNearestInt());
    const auto pname = mgr.name().toUpperCase();
    const float avail = chip.getRight() - 30.0f - nameX, wName = textWidth (font (25.0f, true, Face::Mono).withExtraKerningFactor (0.05f), pname);
    const float nameSize = wName > avail ? juce::jmax (16.0f, 25.0f * avail / wName) : 25.0f;                       // a long name shrinks (to 16 at most) before it is clipped
    drawText (g, pname, nameX, chip.getCentreY(), nameSize, pal.phosphor, true, 0.05f, juce::Justification::centredLeft, Face::Mono);
    g.restoreState();
    for (int up = 0; up < 2; ++up)                                                                                  // the up / down marks of the preset list
    {
        const float cx = chip.getRight() - 18.0f, cy = chip.getCentreY() + (up ? -5.0f : 5.0f);
        juce::Path tri;
        tri.addTriangle (cx - 4.0f, cy + (up ? 3.0f : -3.0f), cx + 4.0f, cy + (up ? 3.0f : -3.0f), cx, cy + (up ? -3.0f : 3.0f));
        g.setColour (pal.phosphorDim);
        g.fillPath (tri);
    }
    for (int redo = 0; redo < 2; ++redo)
        paintFlatButton (g, pal, undoRect (redo != 0), redo ? "REDO" : "UNDO", false, 17.0f, (redo ? undo.canRedo() : undo.canUndo()) ? 1.0f : 0.4f, 4.0f, 0.1f);
    const int slot = mgr.abSlot();
    for (const int i : { 1 - slot, slot }) paintFlatButton (g, pal, abRect (i), i == 0 ? "A" : "B", i == slot, 23.0f, 1.0f, 4.0f, 0.0f);     // the lit one last: it keeps its whole border

    paintPowerButton (g, pal, powerOn);
}

// the preset menu under the screen: factory presets, my presets, save as, show the folder (the shared menu rows and look, anchored on this look's screen)
inline void showPresetMenu (juce::Component& owner, vibr::PresetManager& mgr, float scale, std::function<void()> done)
{
    static vibr::ui::PresetMenuLook look;
    juce::PopupMenu menu, factory, user;
    const auto& list = mgr.factory();
    int nameW = 0;
    for (const auto& pr : list) nameW = juce::jmax (nameW, (int) std::ceil (juce::GlyphArrangement::getStringWidth (juce::Font (juce::FontOptions (14.0f)), pr.name)));
    for (int i = 0; i < (int) list.size(); ++i)
        factory.addCustomItem (1000 + i, std::make_unique<vibr::ui::PresetMenuItem> (list[(size_t) i].name, list[(size_t) i].note, nameW, mgr.name() == list[(size_t) i].name), nullptr);
    factory.setLookAndFeel (&look); user.setLookAndFeel (&look); menu.setLookAndFeel (&look);
    auto files = mgr.folderFile().findChildFiles (juce::File::findFiles, false, "*.xml");
    files.sort();
    for (int i = 0; i < files.size(); ++i) user.addItem (2000 + i, files[i].getFileNameWithoutExtension(), true, mgr.name() == files[i].getFileNameWithoutExtension());
    menu.addSubMenu ("Factory presets", factory);
    menu.addSubMenu ("My presets", user, files.size() > 0);
    menu.addSeparator();
    menu.addItem (1, "Save preset as...");
    menu.addItem (2, "Show the presets folder");
    juce::Component::SafePointer<juce::Component> safe (&owner);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&owner).withTargetScreenArea (presetChipRect().transformedBy (juce::AffineTransform::scale (scale)).toNearestInt().translated (owner.getScreenX(), owner.getScreenY())),
                        [safe, &mgr, files, done] (int result)
                        {
                            if (safe == nullptr) return;
                            if (result >= 2000) mgr.loadUser (files[result - 2000]);
                            else if (result >= 1000) mgr.applyFactory (result - 1000);
                            else if (result == 1)
                            {
                                auto* w = new juce::AlertWindow ("Save preset", "Name of the preset (saved in Documents / VIBR / " + mgr.folderFile().getFileName() + "):", juce::MessageBoxIconType::NoIcon, safe.getComponent());
                                w->addTextEditor ("name", mgr.name(), "Name");
                                w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
                                w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
                                w->enterModalState (true, juce::ModalCallbackFunction::create ([&mgr, w, done] (int r) { if (r == 1) mgr.saveUser (w->getTextEditorContents ("name")); done(); }), true);
                            }
                            else if (result == 2) { auto f = mgr.folderFile(); f.createDirectory(); f.revealToUser(); }
                            done();
                        });
}

// a click at `p` (layout units) on the preset screen, UNDO, REDO or A / B: true when the bar took it (the power switch is the editor's: powerRect())
inline bool presetBarMouseDown (juce::Point<float> p, juce::Component& owner, vibr::PresetManager& mgr, juce::UndoManager& undo, float scale, std::function<void()> done)
{
    if (presetChipRect().contains (p)) { showPresetMenu (owner, mgr, scale, done); return true; }
    for (int i = 0; i < 2; ++i)
        if (abRect (i).contains (p)) { mgr.switchAB (i); done(); return true; }
    if (undoRect (false).contains (p)) { undo.undo(); done(); return true; }
    if (undoRect (true).contains (p)) { undo.redo(); done(); return true; }
    return false;
}
} // namespace vibr::ui::vintage
