// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once
// The layout of the MS-1 window (design units, 1200 wide): the panel rectangles, the knob and button positions, the colours. Shared by the three parts of the
// editor (PluginEditor.cpp: window, paint and mouse; EditorDisplays.cpp: the spectrum, EQ, meters and stage displays; EditorAuto.cpp: the AUTO and REFERENCE panels).
#include <JuceHeader.h>
#include "ui/VibrUI.h"

namespace ms1ui
{
namespace ui = vibr::ui;
// layout units: the design is 1200 wide (style_guide.md: 20 side margin, 10 gap, name centre 55, rule 104, panels 30 below the rule, 24 between the rows).
// Row 1: INPUT, LOUDNESS, OUTPUT. Row 2: the workflow strip (EQ > MULTIBAND > IMAGER > COLOUR > BUS COMP > LIMITER, and the REF button). Row 3: the panels of the current
// step (EQ: SPECTRUM; MULTIBAND / IMAGER / COLOUR / BUS: STAGES + display; LIMITER: LIMITER + its gain reduction). Row 4: REFERENCE, only while it is open.
constexpr float kW = 1200.0f, kMargin = 20.0f, kRuleY = 104.0f;
constexpr float kRow1Y = 134.0f, kRow1H = 247.0f, kChainY = 405.0f, kChainH = 44.0f, kRow3Y = 473.0f, kRow3H = 296.0f, kRow4Y = 793.0f, kRow4H = 130.0f, kKnobR = 24.0f, kPad = 5.0f, kKnobPitch = 108.0f;
// panels: 0 INPUT, 2 OUTPUT, 4 LOUDNESS (row 1); 3 SPECTRUM (step EQ), 5 STAGES + 6 their display (steps 1 to 4), 1 LIMITER + 6 its gain reduction (step LIMITER) (row 3); 7 REFERENCE (row 4)
constexpr float kPanelX[8] = { 20.0f, 20.0f, 860.0f, 20.0f, 220.0f, 20.0f, 810.0f, 20.0f }, kPanelW[8] = { 190.0f, 780.0f, 320.0f, 1160.0f, 630.0f, 780.0f, 370.0f, 1160.0f };
constexpr const char* kPanelName[8] = { "INPUT", "LIMITER", "OUTPUT", "SPECTRUM", "LOUDNESS", "STAGES", "GAIN REDUCTION", "REFERENCE" };
constexpr const char* kTabName[4] = { "MULTIBAND", "IMAGER", "COLOUR", "BUS" };
constexpr const char* kBandName[4] = { "LOW", "LOW-MID", "HIGH-MID", "HIGH" };

const juce::Colour kBody (185, 196, 106), kDark (28, 33, 6), kPanel (26, 27, 30), kInk (238, 238, 232), kRed (229, 67, 44);

const vibr::ui::Palette kPal { kBody, kDark };
using vibr::ui::font;                                                    // the shared look (src/common/ui/VibrUI.h)
using vibr::ui::textWidth;
using vibr::ui::drawText;

inline bool inRow3 (int i) { return i == 1 || i == 3 || i == 5 || i == 6; }
inline juce::Rectangle<float> panelRect (int i) { return { kPanelX[i], i == 7 ? kRow4Y : inRow3 (i) ? kRow3Y : kRow1Y, kPanelW[i], i == 7 ? kRow4H : inRow3 (i) ? kRow3H : kRow1H }; }
inline float knobDy (int panel) { return panel == 1 || panel == 5 ? 24.0f : 0.0f; }                  // the knob row of the taller row 3 panels sits 24 lower
inline juce::Rectangle<float> powerRect() { return juce::Rectangle<float> (50.0f, 50.0f).withCentre ({ kW - kMargin - 16.5f, 43.5f }); }
inline juce::Rectangle<float> targetChipRect() { return { vibr::ui::kBarX, 22.0f, vibr::ui::kChipW, 34.0f }; }          // as wide as the preset chip under it
inline juce::Rectangle<float> plotRect() { const auto r = panelRect (3); return { r.getX() + 57.0f, r.getY() + 58.0f, r.getWidth() - 106.0f, r.getHeight() - 133.0f }; }   // the dB labels (left) and EQ gain labels (right) run from x + 22 to x + 1140, the same as the buttons below; 12 px above the box (spectrum buttons) and 12 px below the Hz labels (bottom buttons)
// the selected EQ band (type, frequency, gain, Q): text on the top left of the panel, above the BEFORE label
inline juce::Rectangle<float> eqInfoRect() { const auto r = panelRect (3); return { r.getX() + 54.0f, r.getY() + 12.0f, 440.0f, 26.0f }; }
// the second strip: the dynamic EQ of the selected band: DYN, ABOVE / BELOW, and four fields (threshold, ratio, attack, release) to drag
inline juce::Rectangle<float> eqDynStripRect() { const auto r = panelRect (3); return { r.getX() + 14.0f, r.getBottom() - 44.0f, r.getWidth() - 28.0f, 34.0f }; }
inline juce::Rectangle<float> eqDynButtonRect (int i) { const auto s = eqDynStripRect(); return i == 0 ? juce::Rectangle<float> (s.getX() + 8.0f, s.getY() + 2.0f, 64.0f, 30.0f) : juce::Rectangle<float> (s.getX() + 78.0f, s.getY() + 2.0f, 84.0f, 30.0f); }
inline juce::Rectangle<float> eqDynFieldRect (int i) { const auto s = eqDynStripRect(); return { s.getX() + 172.0f + (float) i * 126.0f, s.getY() + 2.0f, 120.0f, 30.0f }; }
inline juce::Rectangle<float> eqPhaseRect() { const auto s = eqDynStripRect(); return { s.getX() + 680.0f + (s.getWidth() - 752.0f), s.getY() + 2.0f, 66.0f, 30.0f }; }      // MIN / LIN: the phase mode of the EQ
inline juce::Rectangle<float> eqButtonRect (int i)
{
    // EQ IN, BAND ON, STEREO, L, R, M, S: on the bottom strip between the REL field (ends at s.x + 670) and MIN PH (starts at s.x + 1060)
    static const float w[7] = { 58.0f, 66.0f, 70.0f, 32.0f, 32.0f, 32.0f, 32.0f };
    const auto s = eqDynStripRect();
    float x = s.getX() + 686.0f;
    for (int k = 0; k < i; ++k) x += w[k] + 6.0f;
    return { x, s.getY() + 2.0f, w[i], 30.0f };
}
constexpr float kEqRange = 18.0f;
inline juce::Rectangle<float> overRect() { const auto r = panelRect (2); return juce::Rectangle<float> (54.0f, 20.0f).withCentre ({ r.getX() + 41.0f, r.getY() + 22.0f }); }       // the OVER lamp of the OUTPUT panel (top left, clear of the meter labels)
// the OUTPUT L / R dB meters (shared ui::OutputMeter): right of the two knobs, bars 110 high from y + 46 (style_guide.md), the values on the readout row
inline juce::Point<float> meterOrigin() { const auto r = panelRect (2); return { r.getX() + 226.0f, r.getY() + 46.0f }; }
inline juce::Rectangle<float> resetRect() { const auto r = panelRect (4); return { r.getRight() - 84.0f, r.getY() + 6.0f, 70.0f, 22.0f }; }
constexpr float kLoudMeterW = 380.0f;                                                       // the loudness bars take the left 380 px of the panel, the history the rest
inline juce::Point<float> knobCentre (int panel, int k, int n)
{
    const auto r = panelRect (panel);
    // the knobs sit 108 apart around the middle of the panel (ticks and legends reach 42 each side)
    if (panel == 2) return { r.getX() + 62.0f + 103.0f * (float) k, r.getY() + 92.0f };               // OUTPUT: knobs at x + 62 and x + 165 (tick rings 19 apart, and 19 from the right ring to the bars at x + 226), the meters on the right
    return { r.getCentreX() + kKnobPitch * ((float) k - 0.5f * (float) (n - 1)), r.getY() + 92.0f + knobDy (panel) };
}
// the buttons of rows 2 and 3: LIMITER: TRANSPARENT, PUNCHY, LOUD, AUTO GAIN; OUTPUT: DELTA, SHAPE (the dither noise shaping); BUS: BUS IN, SIDECHAIN HIGH-PASS
inline juce::Rectangle<float> buttonRect (int i)
{
    if (i < 4 || i >= 6) { const auto r = panelRect (1); const int pos = i < 4 ? i : i - 2; return { r.getX() + (r.getWidth() - 634.0f) * 0.5f + (float) pos * 107.0f, r.getY() + 248.0f, 99.0f, 30.0f }; }       // six buttons: TRANSPARENT, PUNCHY, LOUD, AUTO TO TARGET, AUTO REL., FREEZE
    const auto r = panelRect (2);
    return { r.getX() + 15.0f + (float) (i - 4) * 115.0f, r.getY() + 204.0f, 105.0f, 30.0f };
}
// the INPUT panel: polarity, rumble high-pass (cycles off / 20 / 30 / 40 / 60 Hz), mono check
inline juce::Rectangle<float> inputButtonRect (int i) { const auto r = panelRect (0); return { r.getX() + 12.0f + (float) i * 57.0f, r.getY() + 204.0f, 52.0f, 30.0f }; }
// the options of the spectrum: SMOOTH / FFT, slope, HOLD, FREEZE, source (mid, side, left, right); small buttons on the top right of the SPECTRUM panel
inline juce::Rectangle<float> specButtonRect (int i) { const auto r = panelRect (3); static const float w[5] = { 110.0f, 110.0f, 66.0f, 80.0f, 110.0f }; float x = r.getRight() - 14.0f; for (int k = 4; k >= i; --k) x -= w[k] + 4.0f; return { x + 4.0f, r.getY() + 12.0f, w[i], 26.0f }; }
// a button that cycles through several settings on each click: a row of dots along its bottom edge, the dot of the current setting lit
inline void paintCycleDots (juce::Graphics& g, juce::Rectangle<float> b, int steps, int cur, bool on)
{
    for (int d = 0; d < steps; ++d)
    {
        g.setColour (d == cur ? (on ? kDark : kBody) : (on ? kDark.withAlpha (0.35f) : kInk.withAlpha (0.3f)));
        g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre ({ b.getCentreX() + ((float) d - 0.5f * (float) (steps - 1)) * 8.0f, b.getBottom() - 4.5f }));
    }
}
// the CHAIN strip between the spectrum and the workbench: the stages in signal order (EQ, MULTIBAND, IMAGER, COLOUR, BUS, LIMITER) as pills with a power dot on the
// left and a little gain reduction bar; a click selects the stage in the workbench below, the dot (or a double click) switches it on and off
constexpr int kChainStages = 6;
inline juce::Rectangle<float> chainRect (int i)
{
    const float gap = 22.0f, x0 = kMargin + 12.0f + 62.0f + gap, w = (1032.0f - x0 - gap * (float) (kChainStages - 1)) / (float) kChainStages;       // after the AUTO button
    return { x0 + (float) i * (w + gap), kChainY + 6.0f, w, kChainH - 12.0f };
}
inline juce::Rectangle<float> refPillRect() { return { 1048.0f, kChainY + 6.0f, 120.0f, kChainH - 12.0f }; }          // the REFERENCE button at the right end of the workflow strip
inline juce::Rectangle<float> chainDotRect (int i) { const auto r = chainRect (i); return { r.getX() + 4.0f, r.getY(), 34.0f, r.getHeight() }; }
const char* const kChainName[kChainStages] = { "EQ", "MULTIBAND", "IMAGER", "COLOUR", "BUS COMP", "LIMITER" };
const char* const kChainOn[kChainStages] = { "eqOn", "mbOn", "imgOn", "colourOn", "busOn", nullptr };
inline juce::Rectangle<float> stageButtonRect (int i, int n)
{
    const auto r = panelRect (5);
    const float gap = n <= 4 ? 10.0f : n <= 5 ? 8.0f : 6.0f;
    if (n == 8)                                                                              // the multiband row: widths follow the longest label of each button (13 px text), scaled to fill the panel
    {
        static const float chars[8] = { 6.0f, 11.0f, 5.0f, 9.0f, 13.0f, 10.0f, 10.0f, 11.0f };
        float sum = 0.0f; for (float c : chars) sum += c * 6.8f + 14.0f;
        const float k = (r.getWidth() - 70.0f - gap * 7.0f) / sum;
        float x = r.getX() + 35.0f;
        for (int j = 0; j < i; ++j) x += (chars[j] * 6.8f + 14.0f) * k + gap;
        return { x, r.getY() + 248.0f, (chars[i] * 6.8f + 14.0f) * k, 30.0f };
    }
    const float w = n <= 4 ? 150.0f : n <= 5 ? 118.0f : (r.getWidth() - 70.0f - gap * (float) (n - 1)) / (float) n;
    return { r.getX() + 35.0f + (float) i * (w + gap), r.getY() + 248.0f, w, 30.0f };
}
inline juce::Rectangle<float> dispRect() { const auto r = panelRect (6); return { r.getX() + 18.0f, r.getY() + 36.0f, r.getWidth() - 36.0f, 188.0f }; }
inline juce::Rectangle<float> goniometerRect() { const auto r = dispRect(); return { r.getRight() - 140.0f, r.getY(), 140.0f, r.getHeight() }; }     // the imager's display: bands on the left, the goniometer on the right
// REFERENCE: LOAD, PLAY, A, REF, MATCH, FOLLOW HOST and the position bar
inline juce::Rectangle<float> refButtonRect (int i)
{
    const auto r = panelRect (7);
    switch (i)
    {
        case 0: return { r.getX() + 20.0f, r.getY() + 26.0f, 100.0f, 34.0f };
        case 1: return { r.getX() + 20.0f, r.getY() + 70.0f, 100.0f, 34.0f };
        case 2: return { r.getX() + 760.0f, r.getY() + 26.0f, 170.0f, 34.0f };
        case 3: return { r.getX() + 940.0f, r.getY() + 26.0f, 200.0f, 34.0f };
        case 4: return { r.getX() + 760.0f, r.getY() + 70.0f, 170.0f, 34.0f };
        case 5: return { r.getX() + 940.0f, r.getY() + 70.0f, 200.0f, 34.0f };
        case 6: return { r.getX() + 556.0f, r.getY() + 70.0f, 80.0f, 34.0f };        // RESET: learn the input again
        default: return { r.getX() + 644.0f, r.getY() + 70.0f, 108.0f, 34.0f };      // MATCH EQ
    }
}
inline juce::Rectangle<float> refBarRect() { const auto r = panelRect (7); return { r.getX() + 140.0f, r.getY() + 80.0f, 400.0f, 14.0f }; }
// AUTO: the first button of the workflow strip and the panel over the stage area (the STAGES and display panels of row 3)
inline juce::Rectangle<float> autoButtonRect() { return { kMargin + 12.0f, kChainY + 6.0f, 62.0f, kChainH - 12.0f }; }       // the first button of the workflow strip
inline juce::Rectangle<float> autoRect() { return panelRect (5).getUnion (panelRect (6)); }
inline juce::Rectangle<float> autoCloseRect() { const auto r = autoRect(); return { r.getRight() - 40.0f, r.getY() + 8.0f, 24.0f, 24.0f }; }
inline juce::Rectangle<float> autoRunRect() { const auto r = autoRect(); return { r.getX() + 16.0f, r.getY() + 54.0f, 124.0f, 30.0f }; }       // ANALYZE / STOP
inline juce::Rectangle<float> autoClearRect() { const auto r = autoRect(); return { r.getX() + 148.0f, r.getY() + 54.0f, 90.0f, 30.0f }; }
inline juce::Rectangle<float> autoStyleRect (int i) { const auto r = autoRect(); return { r.getX() + 16.0f + (float) i * 92.0f, r.getY() + 92.0f, 88.0f, 28.0f }; }
inline juce::Rectangle<float> autoPlotRect() { const auto r = autoRect(); return { r.getX() + 16.0f, r.getY() + 142.0f, 364.0f, 108.0f }; }
inline juce::Rectangle<float> autoRowRect (int i) { const auto r = autoRect(); return { r.getX() + 410.0f, r.getY() + 50.0f + (float) i * 27.0f, 734.0f, 25.0f }; }
inline juce::Rectangle<float> autoCheckRect (int i) { const auto r = autoRowRect (i); return { r.getX() + 5.0f, r.getY() + 4.5f, 16.0f, 16.0f }; }
inline juce::Rectangle<float> autoHelpRect (int i) { const auto r = autoRowRect (i); return { r.getRight() - 27.0f, r.getY(), 25.0f, 25.0f }; }
inline juce::Rectangle<float> autoApplyRect (int i) { const auto r = autoRect(); static const float x[3] = { 0.0f, 132.0f, 294.0f }, w[3] = { 124.0f, 154.0f, 100.0f }; return { r.getX() + 410.0f + x[i], r.getY() + 248.0f, w[i], 30.0f }; }
const char* const kAutoRowName[7] = { "INPUT", "EQ", "MULTIBAND", "IMAGER", "BUS", "LIMITER", "OUTPUT" };
const char* const kAutoStyleName[4] = { "NEUTRAL", "WARM", "BRIGHT", "CLUB" };
inline juce::Rectangle<float> historyRect() { const auto r = panelRect (4); return { r.getX() + kLoudMeterW + 22.0f, r.getY() + 40.0f, r.getWidth() - kLoudMeterW - 22.0f - 18.0f, 150.0f }; }
}
