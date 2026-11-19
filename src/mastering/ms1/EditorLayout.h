// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once
// The layout of the MS-1 window in the Vintage Hardware look (design units, 1120 wide, src/common/ui/VintageUI.h): the panel rectangles, the knob and button positions,
// the colours. Shared by the three parts of the editor (PluginEditor.cpp: window, paint and mouse; EditorDisplays.cpp: the spectrum, EQ, meters and stage displays;
// EditorAuto.cpp: the AUTO and REFERENCE panels). Margin 28, gap 18 between the panels, header as in every plugin (content from y 114); MS-1 grows in height only:
//   row 1  (y 114, 254 high)  INPUT, LOUDNESS, OUTPUT
//   row 2  (y 386,  62 high)  the workflow strip: AUTO, EQ > MULTIBAND > IMAGER > COLOUR > BUS COMP > LIMITER, REFERENCE
//   row 3  (y 466, 288 high)  the panels of the current step (EQ: SPECTRUM; MULTIBAND / IMAGER / COLOUR / BUS: STAGES + display; LIMITER: LIMITER + its gain reduction)
//   row 4  (y 772, 132 high)  REFERENCE, only while it is open
#include <JuceHeader.h>
#include "ui/VintageUI.h"

namespace ms1ui
{
namespace ui = vibr::ui::vintage;
constexpr float kW = ui::kW, kMargin = ui::kMargin, kKnobR = ui::kKnobR, kKnobPitch = 108.0f;
constexpr float kRow1Y = 114.0f, kRow1H = 254.0f, kChainY = 386.0f, kChainH = 62.0f, kRow3Y = 466.0f, kRow3H = 288.0f, kRow4Y = 772.0f, kRow4H = 132.0f;
constexpr float kDesignH = 784.0f, kDesignHRef = 934.0f;      // the window height without / with the REFERENCE panel (the content ends 30 above the bottom)

const ui::Palette kPal { juce::Colour (185, 196, 106) };                         // the Mastering colour #B9C46A
const juce::Colour kWarn (0xffB8361F), kScreenGrey (0xff8A9387);                 // red text on the plate (4.5 : 1), the 'before' grey on a screen
using ui::font;
using ui::textWidth;
using ui::drawText;

// panels: 0 INPUT, 2 OUTPUT, 4 LOUDNESS (row 1); 3 SPECTRUM (step EQ), 5 STAGES + 6 their display (steps 1 to 4), 1 LIMITER + 6 its gain reduction (step LIMITER) (row 3); 7 REFERENCE (row 4)
constexpr float kPanelX[8] = { 28.0f, 28.0f, 749.0f, 28.0f, 246.0f, 28.0f, 722.0f, 28.0f }, kPanelW[8] = { 200.0f, 676.0f, 343.0f, 1064.0f, 485.0f, 676.0f, 370.0f, 1064.0f };
constexpr const char* kPanelName[8] = { "INPUT", "LIMITER", "OUTPUT", "SPECTRUM", "LOUDNESS", "STAGES", "GAIN REDUCTION", "REFERENCE" };
constexpr const char* kTabName[4] = { "MULTIBAND", "IMAGER", "COLOUR", "BUS" };
constexpr const char* kBandName[4] = { "LOW", "LOW-MID", "HIGH-MID", "HIGH" };

inline bool inRow3 (int i) { return i == 1 || i == 3 || i == 5 || i == 6; }
inline juce::Rectangle<float> panelRect (int i) { return { kPanelX[i], i == 7 ? kRow4Y : inRow3 (i) ? kRow3Y : kRow1Y, kPanelW[i], i == 7 ? kRow4H : inRow3 (i) ? kRow3H : kRow1H }; }
inline juce::Rectangle<float> targetChipRect() { return { 350.0f, 75.0f, 400.0f, 24.0f }; }          // the loudness target: a phosphor chip under the preset screen
inline juce::Rectangle<float> edgeButton (juce::Rectangle<float> panel, float width, float right = 16.0f) { return { panel.getRight() - right - width, panel.getY() - 14.0f, width, 30.0f }; }   // on the top edge, like tabs

// the knobs: INPUT one, OUTPUT two (left of the meter), the stage panels up to six, 108 apart round the middle of the panel
inline juce::Point<float> knobCentre (int panel, int k, int n)
{
    const auto r = panelRect (panel);
    if (panel == 0) return { r.getCentreX(), r.getY() + 86.0f };
    if (panel == 2) return { r.getX() + 66.0f + 106.0f * (float) k, r.getY() + 86.0f };               // as the RH-1: MIX and LEVEL left of the meters
    return { r.getCentreX() + kKnobPitch * ((float) k - 0.5f * (float) (n - 1)), r.getY() + 100.0f };
}
inline juce::Rectangle<float> overRect() { return edgeButton (panelRect (2), 78.0f); }                // the OVER lamp on the top edge of OUTPUT
inline juce::Point<float> meterOrigin() { const auto r = panelRect (2); return { r.getRight() - 20.0f - ui::kMeterW, r.getY() + (r.getHeight() - ui::kMeterH) * 0.5f }; }
inline juce::Rectangle<float> resetRect() { const auto r = panelRect (4); return { r.getRight() - 16.0f - 76.0f, r.getY() + 200.0f, 76.0f, 30.0f }; }       // RESET under the screens of LOUDNESS

// the buttons: LIMITER: TRANSPARENT, PUNCHY, LOUD, AUTO TO TARGET (i 0 .. 3), DELTA, SHAPE (4, 5, in OUTPUT), AUTO REL., FREEZE GAIN (6, 7)
inline juce::Rectangle<float> buttonRect (int i)
{
    if (i < 4 || i >= 6) { const auto r = panelRect (1); const int pos = i < 4 ? i : i - 2; return { r.getX() + (r.getWidth() - 642.0f) * 0.5f + (float) pos * 108.0f, r.getY() + 234.0f, 102.0f, 30.0f }; }
    const auto r = panelRect (2);
    return { r.getX() + 16.0f + (float) (i - 4) * 102.0f, r.getY() + 200.0f, 96.0f, 30.0f };
}
// the INPUT panel: polarity, rumble high-pass (cycles off / 20 / 30 / 40 / 60 Hz), mono check
inline juce::Rectangle<float> inputButtonRect (int i) { const auto r = panelRect (0); return { r.getX() + 16.0f + (float) i * 58.0f, r.getY() + 200.0f, 52.0f, 30.0f }; }

// SPECTRUM: the options (SMOOTH / FFT, slope, HOLD, FREEZE, source) on the top edge, the info line of the selected band, the screen with the plot, the strip of buttons
inline juce::Rectangle<float> specButtonRect (int i)
{
    static const float w[5] = { 104.0f, 104.0f, 64.0f, 76.0f, 104.0f };
    const auto r = panelRect (3);
    float x = r.getRight() - 16.0f;
    for (int k = 4; k >= i; --k) x -= w[k] + 6.0f;
    return { x + 6.0f, r.getY() - 14.0f, w[i], 30.0f };
}
inline juce::Rectangle<float> specScreenRect() { const auto r = panelRect (3); return { r.getX() + 16.0f, r.getY() + 46.0f, r.getWidth() - 32.0f, 184.0f }; }
inline juce::Rectangle<float> plotRect() { const auto s = specScreenRect(); return { s.getX() + 40.0f, s.getY() + 10.0f, s.getWidth() - 84.0f, s.getHeight() - 36.0f }; }       // dB labels on the left, EQ gain labels on the right, Hz labels under it
inline juce::Rectangle<float> eqInfoRect() { const auto r = panelRect (3); return { r.getX() + 16.0f, r.getY() + 23.0f, 460.0f, 14.0f }; }
// the strip under the screen: DYNAMIC, ABOVE / BELOW, four fields (threshold, ratio, attack, release) to drag, EQ IN, BAND ON, STEREO, L, R, M, S, MIN PH / LINEAR
inline juce::Rectangle<float> eqStripRect() { const auto r = panelRect (3); return { r.getX() + 16.0f, r.getY() + 242.0f, r.getWidth() - 32.0f, 30.0f }; }
inline juce::Rectangle<float> eqDynButtonRect (int i) { const auto s = eqStripRect(); return { s.getX() + (i == 0 ? 0.0f : 82.0f), s.getY(), 76.0f, 30.0f }; }
inline juce::Rectangle<float> eqDynFieldRect (int i) { const auto s = eqStripRect(); return { s.getX() + 172.0f + (float) i * 106.0f, s.getY(), 100.0f, 30.0f }; }
inline juce::Rectangle<float> eqPhaseRect() { const auto s = eqStripRect(); return { s.getRight() - 66.0f, s.getY(), 66.0f, 30.0f }; }      // MIN / LIN: the phase mode of the EQ
inline juce::Rectangle<float> eqButtonRect (int i)
{
    static const float w[7] = { 58.0f, 70.0f, 66.0f, 30.0f, 30.0f, 30.0f, 30.0f };
    const auto s = eqStripRect();
    float x = s.getX() + 604.0f;
    for (int k = 0; k < i; ++k) x += w[k] + 6.0f;
    return { x, s.getY(), w[i], 30.0f };
}
constexpr float kEqRange = 18.0f;

// the panel's line is cut behind a button that sits on the top edge (6 before and after it, as for tabs)
inline void cutEdge (juce::Graphics& g, juce::Rectangle<float> b, float panelY)
{
    g.setColour (kPal.paper);
    g.fillRect (b.getX() - 6.0f, panelY - 1.0f, b.getWidth() + 12.0f, 4.0f);
}

// a lamp: a 14 disc with a 2 ink ring, lit = its colour with a soft glow
inline void paintLamp (juce::Graphics& g, juce::Point<float> c, bool lit, juce::Colour on, juce::Colour off)
{
    const auto d = juce::Rectangle<float> (14.0f, 14.0f).withCentre (c);
    if (lit) { g.setColour (on.withAlpha (0.25f)); g.fillEllipse (d.expanded (4.0f)); }
    g.setColour (lit ? on : off);
    g.fillEllipse (d);
    g.setColour (kPal.ink);
    g.drawEllipse (d.reduced (1.0f), 2.0f);
}

// a button that cycles through several settings on each click: a row of dots along its bottom edge, the dot of the current setting lit
inline void paintCycleDots (juce::Graphics& g, juce::Rectangle<float> b, int steps, int cur, bool on)
{
    for (int d = 0; d < steps; ++d)
    {
        g.setColour (d == cur ? (on ? kPal.cream : kPal.ink) : (on ? kPal.cream.withAlpha (0.35f) : kPal.ink.withAlpha (0.25f)));
        g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre ({ b.getCentreX() + ((float) d - 0.5f * (float) (steps - 1)) * 8.0f, b.getBottom() - 5.5f }));
    }
}

// the workflow strip (row 2): AUTO, the stages in signal order (EQ, MULTIBAND, IMAGER, COLOUR, BUS, LIMITER) with a power dot and a little gain reduction bar, REFERENCE.
// A click selects the stage in row 3, the dot (or a double click) switches it on and off.
constexpr int kChainStages = 6;
inline juce::Rectangle<float> chainRect (int i) { return { 128.0f + (float) i * 138.0f, kChainY + 16.0f, 124.0f, 30.0f }; }
inline juce::Rectangle<float> autoButtonRect() { return { kMargin + 16.0f, kChainY + 16.0f, 70.0f, 30.0f }; }
inline juce::Rectangle<float> refPillRect() { return { kMargin + kPanelW[3] - 16.0f - 120.0f, kChainY + 16.0f, 120.0f, 30.0f }; }          // the REFERENCE button at the right end
inline juce::Rectangle<float> chainDotRect (int i) { const auto r = chainRect (i); return { r.getX() + 4.0f, r.getY(), 34.0f, r.getHeight() }; }
const char* const kChainName[kChainStages] = { "EQ", "MULTIBAND", "IMAGER", "COLOUR", "BUS COMP", "LIMITER" };
const char* const kChainOn[kChainStages] = { "eqOn", "mbOn", "imgOn", "colourOn", "busOn", nullptr };

// the buttons of a stage panel: one row under the knobs
inline juce::Rectangle<float> stageButtonRect (int i, int n)
{
    const auto r = panelRect (5);
    const float gap = n == 8 ? 6.0f : 8.0f, y = r.getY() + 234.0f;
    if (n == 8)                                                                              // the multiband row: widths follow the longest label of each button, scaled to fill the panel
    {
        static const float chars[8] = { 6.0f, 11.0f, 5.0f, 9.0f, 13.0f, 10.0f, 10.0f, 11.0f };
        float sum = 0.0f; for (float c : chars) sum += c * 6.8f + 14.0f;
        const float k = (r.getWidth() - 32.0f - gap * 7.0f) / sum;
        float x = r.getX() + 16.0f;
        for (int j = 0; j < i; ++j) x += (chars[j] * 6.8f + 14.0f) * k + gap;
        return { x, y, (chars[i] * 6.8f + 14.0f) * k, 30.0f };
    }
    const float w = n <= 4 ? 160.0f : 120.0f, total = w * (float) n + gap * (float) (n - 1);
    return { r.getCentreX() - total * 0.5f + (float) i * (w + gap), y, w, 30.0f };
}
inline juce::Rectangle<float> dispRect() { const auto r = panelRect (6); return { r.getX() + 16.0f, r.getY() + 22.0f, r.getWidth() - 32.0f, 188.0f }; }     // the screen of the stage display
inline juce::Rectangle<float> goniometerRect() { const auto r = dispRect(); return { r.getRight() - 140.0f, r.getY(), 140.0f, r.getHeight() }; }     // the imager's display: bands on the left, the goniometer on the right

// REFERENCE: LOAD, PLAY, info and position bar, RESET, MATCH EQ, A, REF, LEVEL MATCH, FOLLOW HOST
inline juce::Rectangle<float> refButtonRect (int i)
{
    const auto r = panelRect (7);
    switch (i)
    {
        case 0: return { r.getX() + 16.0f, r.getY() + 22.0f, 100.0f, 30.0f };
        case 1: return { r.getX() + 16.0f, r.getY() + 64.0f, 100.0f, 30.0f };
        case 2: return { r.getX() + 712.0f, r.getY() + 22.0f, 150.0f, 30.0f };
        case 3: return { r.getX() + 868.0f, r.getY() + 22.0f, 180.0f, 30.0f };
        case 4: return { r.getX() + 712.0f, r.getY() + 64.0f, 150.0f, 30.0f };
        case 5: return { r.getX() + 868.0f, r.getY() + 64.0f, 180.0f, 30.0f };
        case 6: return { r.getX() + 528.0f, r.getY() + 64.0f, 70.0f, 30.0f };         // RESET: learn the input again
        default: return { r.getX() + 604.0f, r.getY() + 64.0f, 96.0f, 30.0f };       // MATCH EQ
    }
}
inline juce::Rectangle<float> refBarRect() { const auto r = panelRect (7); return { r.getX() + 132.0f, r.getY() + 72.0f, 380.0f, 14.0f }; }

// AUTO: the first button of the workflow strip and the panel over the stage area (the STAGES and display panels of row 3)
inline juce::Rectangle<float> autoRect() { return panelRect (5).getUnion (panelRect (6)); }
inline juce::Rectangle<float> autoCloseRect() { return edgeButton (autoRect(), 70.0f); }
inline juce::Rectangle<float> autoRunRect() { const auto r = autoRect(); return { r.getX() + 16.0f, r.getY() + 50.0f, 124.0f, 30.0f }; }       // ANALYZE / STOP
inline juce::Rectangle<float> autoClearRect() { const auto r = autoRect(); return { r.getX() + 148.0f, r.getY() + 50.0f, 90.0f, 30.0f }; }
inline juce::Rectangle<float> autoStyleRect (int i) { const auto r = autoRect(); return { r.getX() + 16.0f + (float) i * 92.0f, r.getY() + 88.0f, 88.0f, 28.0f }; }
inline juce::Rectangle<float> autoPlotRect() { const auto r = autoRect(); return { r.getX() + 16.0f, r.getY() + 146.0f, 364.0f, 98.0f }; }
inline juce::Rectangle<float> autoRowRect (int i) { const auto r = autoRect(); return { r.getX() + 398.0f, r.getY() + 44.0f + (float) i * 26.0f, 650.0f, 24.0f }; }
inline juce::Rectangle<float> autoCheckRect (int i) { const auto r = autoRowRect (i); return { r.getX() + 5.0f, r.getY() + 4.0f, 16.0f, 16.0f }; }
inline juce::Rectangle<float> autoHelpRect (int i) { const auto r = autoRowRect (i); return { r.getRight() - 27.0f, r.getY(), 24.0f, 24.0f }; }
inline juce::Rectangle<float> autoApplyRect (int i) { const auto r = autoRect(); static const float x[3] = { 696.0f, 812.0f, 958.0f }, w[3] = { 110.0f, 140.0f, 90.0f }; return { r.getX() + x[i], r.getY() + 242.0f, w[i], 30.0f }; }
const char* const kAutoRowName[7] = { "INPUT", "EQ", "MULTIBAND", "IMAGER", "BUS", "LIMITER", "OUTPUT" };
const char* const kAutoStyleName[4] = { "NEUTRAL", "WARM", "BRIGHT", "CLUB" };

// LOUDNESS: the bars on one phosphor screen, the five-minute history on another
inline juce::Rectangle<float> loudBarsRect() { const auto r = panelRect (4); return { r.getX() + 16.0f, r.getY() + 20.0f, 232.0f, 172.0f }; }
inline juce::Rectangle<float> loudHistScreenRect() { const auto r = panelRect (4), b = loudBarsRect(); return { b.getRight() + 12.0f, r.getY() + 20.0f, r.getRight() - 16.0f - (b.getRight() + 12.0f), 172.0f }; }
inline juce::Rectangle<float> historyRect() { return ui::scopePlotRect (loudHistScreenRect()); }
}
