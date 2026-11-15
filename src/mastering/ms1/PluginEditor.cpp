// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "VibrLogo.h"
#include "ui/VibrUI.h"

namespace ui = vibr::ui;

namespace
{
// layout units: the design is 1200 wide (style.md: 20 side margin, 10 gap, name centre 55, rule 104, panels 30 below the rule, 24 between the rows).
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

bool inRow3 (int i) { return i == 1 || i == 3 || i == 5 || i == 6; }
juce::Rectangle<float> panelRect (int i) { return { kPanelX[i], i == 7 ? kRow4Y : inRow3 (i) ? kRow3Y : kRow1Y, kPanelW[i], i == 7 ? kRow4H : inRow3 (i) ? kRow3H : kRow1H }; }
float knobDy (int panel) { return panel == 1 || panel == 5 ? 24.0f : 0.0f; }                  // the knob row of the taller row 3 panels sits 24 lower
juce::Rectangle<float> powerRect() { return juce::Rectangle<float> (50.0f, 50.0f).withCentre ({ kW - kMargin - 16.5f, 43.5f }); }
juce::Rectangle<float> targetChipRect() { return { vibr::ui::kBarX, 22.0f, vibr::ui::kChipW, 34.0f }; }          // as wide as the preset chip under it
juce::Rectangle<float> plotRect() { const auto r = panelRect (3); return { r.getX() + 57.0f, r.getY() + 58.0f, r.getWidth() - 106.0f, r.getHeight() - 133.0f }; }   // the dB labels (left) and EQ gain labels (right) run from x + 22 to x + 1140, the same as the buttons below; 12 px above the box (spectrum buttons) and 12 px below the Hz labels (bottom buttons)
// the selected EQ band (type, frequency, gain, Q): text on the top left of the panel, above the BEFORE label
juce::Rectangle<float> eqInfoRect() { const auto r = panelRect (3); return { r.getX() + 54.0f, r.getY() + 12.0f, 440.0f, 26.0f }; }
// the second strip: the dynamic EQ of the selected band: DYN, ABOVE / BELOW, and four fields (threshold, ratio, attack, release) to drag
juce::Rectangle<float> eqDynStripRect() { const auto r = panelRect (3); return { r.getX() + 14.0f, r.getBottom() - 44.0f, r.getWidth() - 28.0f, 34.0f }; }
juce::Rectangle<float> eqDynButtonRect (int i) { const auto s = eqDynStripRect(); return i == 0 ? juce::Rectangle<float> (s.getX() + 8.0f, s.getY() + 2.0f, 64.0f, 30.0f) : juce::Rectangle<float> (s.getX() + 78.0f, s.getY() + 2.0f, 84.0f, 30.0f); }
juce::Rectangle<float> eqDynFieldRect (int i) { const auto s = eqDynStripRect(); return { s.getX() + 172.0f + (float) i * 126.0f, s.getY() + 2.0f, 120.0f, 30.0f }; }
juce::Rectangle<float> eqPhaseRect() { const auto s = eqDynStripRect(); return { s.getX() + 680.0f + (s.getWidth() - 752.0f), s.getY() + 2.0f, 66.0f, 30.0f }; }      // MIN / LIN: the phase mode of the EQ
juce::Rectangle<float> eqButtonRect (int i)
{
    // EQ IN, BAND ON, STEREO, L, R, M, S: on the bottom strip between the REL field (ends at s.x + 670) and MIN PH (starts at s.x + 1060)
    static const float w[7] = { 58.0f, 66.0f, 70.0f, 32.0f, 32.0f, 32.0f, 32.0f };
    const auto s = eqDynStripRect();
    float x = s.getX() + 686.0f;
    for (int k = 0; k < i; ++k) x += w[k] + 6.0f;
    return { x, s.getY() + 2.0f, w[i], 30.0f };
}
constexpr float kEqRange = 18.0f;
juce::Rectangle<float> overRect() { const auto r = panelRect (2); return juce::Rectangle<float> (54.0f, 20.0f).withCentre ({ r.getX() + 41.0f, r.getY() + 22.0f }); }       // the OVER lamp of the OUTPUT panel (top left, clear of the meter labels)
// the OUTPUT L / R dB meters (shared ui::OutputMeter): right of the two knobs, bars 110 high from y + 46 (style.md), the values on the readout row
juce::Point<float> meterOrigin() { const auto r = panelRect (2); return { r.getX() + 226.0f, r.getY() + 46.0f }; }
juce::Rectangle<float> resetRect() { const auto r = panelRect (4); return { r.getRight() - 84.0f, r.getY() + 6.0f, 70.0f, 22.0f }; }
constexpr float kLoudMeterW = 380.0f;                                                       // the loudness bars take the left 380 px of the panel, the history the rest
juce::Point<float> knobCentre (int panel, int k, int n)
{
    const auto r = panelRect (panel);
    // the knobs sit 108 apart around the middle of the panel (ticks and legends reach 42 each side)
    if (panel == 2) return { r.getX() + 62.0f + 103.0f * (float) k, r.getY() + 92.0f };               // OUTPUT: knobs at x + 62 and x + 165 (tick rings 19 apart, and 19 from the right ring to the bars at x + 226), the meters on the right
    return { r.getCentreX() + kKnobPitch * ((float) k - 0.5f * (float) (n - 1)), r.getY() + 92.0f + knobDy (panel) };
}
// the buttons of rows 2 and 3: LIMITER: TRANSPARENT, PUNCHY, LOUD, AUTO GAIN; OUTPUT: DELTA, SHAPE (the dither noise shaping); BUS: BUS IN, SIDECHAIN HIGH-PASS
juce::Rectangle<float> buttonRect (int i)
{
    if (i < 4 || i >= 6) { const auto r = panelRect (1); const int pos = i < 4 ? i : i - 2; return { r.getX() + (r.getWidth() - 634.0f) * 0.5f + (float) pos * 107.0f, r.getY() + 248.0f, 99.0f, 30.0f }; }       // six buttons: TRANSPARENT, PUNCHY, LOUD, AUTO TO TARGET, AUTO REL., FREEZE
    const auto r = panelRect (2);
    return { r.getX() + 15.0f + (float) (i - 4) * 115.0f, r.getY() + 204.0f, 105.0f, 30.0f };
}
// the INPUT panel: polarity, rumble high-pass (cycles off / 20 / 30 / 40 / 60 Hz), mono check
juce::Rectangle<float> inputButtonRect (int i) { const auto r = panelRect (0); return { r.getX() + 12.0f + (float) i * 57.0f, r.getY() + 204.0f, 52.0f, 30.0f }; }
// the options of the spectrum: SMOOTH / FFT, slope, HOLD, FREEZE, source (mid, side, left, right); small buttons on the top right of the SPECTRUM panel
juce::Rectangle<float> specButtonRect (int i) { const auto r = panelRect (3); static const float w[5] = { 110.0f, 110.0f, 66.0f, 80.0f, 110.0f }; float x = r.getRight() - 14.0f; for (int k = 4; k >= i; --k) x -= w[k] + 4.0f; return { x + 4.0f, r.getY() + 12.0f, w[i], 26.0f }; }
// a button that cycles through several settings on each click: a row of dots along its bottom edge, the dot of the current setting lit
void paintCycleDots (juce::Graphics& g, juce::Rectangle<float> b, int steps, int cur, bool on)
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
juce::Rectangle<float> chainRect (int i)
{
    const float gap = 22.0f, x0 = kMargin + 12.0f + 62.0f + gap, w = (1032.0f - x0 - gap * (float) (kChainStages - 1)) / (float) kChainStages;       // after the AUTO button
    return { x0 + (float) i * (w + gap), kChainY + 6.0f, w, kChainH - 12.0f };
}
juce::Rectangle<float> refPillRect() { return { 1048.0f, kChainY + 6.0f, 120.0f, kChainH - 12.0f }; }          // the REFERENCE button at the right end of the workflow strip
juce::Rectangle<float> chainDotRect (int i) { const auto r = chainRect (i); return { r.getX() + 4.0f, r.getY(), 34.0f, r.getHeight() }; }
const char* const kChainName[kChainStages] = { "EQ", "MULTIBAND", "IMAGER", "COLOUR", "BUS COMP", "LIMITER" };
const char* const kChainOn[kChainStages] = { "eqOn", "mbOn", "imgOn", "colourOn", "busOn", nullptr };
juce::Rectangle<float> stageButtonRect (int i, int n)
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
juce::Rectangle<float> dispRect() { const auto r = panelRect (6); return { r.getX() + 18.0f, r.getY() + 36.0f, r.getWidth() - 36.0f, 188.0f }; }
juce::Rectangle<float> goniometerRect() { const auto r = dispRect(); return { r.getRight() - 140.0f, r.getY(), 140.0f, r.getHeight() }; }     // the imager's display: bands on the left, the goniometer on the right
// REFERENCE: LOAD, PLAY, A, REF, MATCH, FOLLOW HOST and the position bar
juce::Rectangle<float> refButtonRect (int i)
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
juce::Rectangle<float> refBarRect() { const auto r = panelRect (7); return { r.getX() + 140.0f, r.getY() + 80.0f, 400.0f, 14.0f }; }
// AUTO: the first button of the workflow strip and the panel over the stage area (the STAGES and display panels of row 3)
juce::Rectangle<float> autoButtonRect() { return { kMargin + 12.0f, kChainY + 6.0f, 62.0f, kChainH - 12.0f }; }       // the first button of the workflow strip
juce::Rectangle<float> autoRect() { return panelRect (5).getUnion (panelRect (6)); }
juce::Rectangle<float> autoCloseRect() { const auto r = autoRect(); return { r.getRight() - 40.0f, r.getY() + 8.0f, 24.0f, 24.0f }; }
juce::Rectangle<float> autoRunRect() { const auto r = autoRect(); return { r.getX() + 16.0f, r.getY() + 54.0f, 124.0f, 30.0f }; }       // ANALYZE / STOP
juce::Rectangle<float> autoClearRect() { const auto r = autoRect(); return { r.getX() + 148.0f, r.getY() + 54.0f, 90.0f, 30.0f }; }
juce::Rectangle<float> autoStyleRect (int i) { const auto r = autoRect(); return { r.getX() + 16.0f + (float) i * 92.0f, r.getY() + 92.0f, 88.0f, 28.0f }; }
juce::Rectangle<float> autoPlotRect() { const auto r = autoRect(); return { r.getX() + 16.0f, r.getY() + 142.0f, 364.0f, 108.0f }; }
juce::Rectangle<float> autoRowRect (int i) { const auto r = autoRect(); return { r.getX() + 410.0f, r.getY() + 50.0f + (float) i * 27.0f, 734.0f, 25.0f }; }
juce::Rectangle<float> autoCheckRect (int i) { const auto r = autoRowRect (i); return { r.getX() + 5.0f, r.getY() + 4.5f, 16.0f, 16.0f }; }
juce::Rectangle<float> autoHelpRect (int i) { const auto r = autoRowRect (i); return { r.getRight() - 27.0f, r.getY(), 25.0f, 25.0f }; }
juce::Rectangle<float> autoApplyRect (int i) { const auto r = autoRect(); static const float x[3] = { 0.0f, 132.0f, 294.0f }, w[3] = { 124.0f, 154.0f, 100.0f }; return { r.getX() + 410.0f + x[i], r.getY() + 248.0f, w[i], 30.0f }; }
const char* const kAutoRowName[7] = { "INPUT", "EQ", "MULTIBAND", "IMAGER", "BUS", "LIMITER", "OUTPUT" };
const char* const kAutoStyleName[4] = { "NEUTRAL", "WARM", "BRIGHT", "CLUB" };
juce::Rectangle<float> historyRect() { const auto r = panelRect (4); return { r.getX() + kLoudMeterW + 22.0f, r.getY() + 40.0f, r.getWidth() - kLoudMeterW - 22.0f - 18.0f, 150.0f }; }
} // namespace

//==============================================================================
MS1AudioProcessorEditor::MS1AudioProcessorEditor (MS1AudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      knobs { { { "inGain", 0, 0, 1, "TRIM", 0, {}, {} },
                { "limGain", 1, 0, 6, "GAIN", 1, {}, {} },       { "limCeiling", 1, 1, 6, "CEILING", 2, {}, {} },  { "limRelease", 1, 2, 6, "RELEASE", 3, {}, {} },
                { "limLook", 1, 3, 6, "LOOKAHEAD", 4, {}, {} },  { "limLink", 1, 4, 6, "LINK", 5, {}, {} },        { "limTarget", 1, 5, 6, "TARGET", 6, {}, {} },
                { "outGain", 2, 0, 2, "GAIN", 0, {}, {} },       { "ditherBits", 2, 1, 2, "DITHER", 7, {}, {} },
                { "busThresh", 5, 0, 6, "THRESHOLD", 8, {}, {}, 3 }, { "busRatio", 5, 1, 6, "RATIO", 9, {}, {}, 3 },    { "busAttack", 5, 2, 6, "ATTACK", 10, {}, {}, 3 },
                { "busRelease", 5, 3, 6, "RELEASE", 11, {}, {}, 3 }, { "busMakeup", 5, 4, 6, "MAKE-UP", 12, {}, {}, 3 }, { "busMix", 5, 5, 6, "MIX", 5, {}, {}, 3 },
                { "Thresh", 5, 0, 6, "THRESHOLD", 13, {}, {}, 0, true }, { "Ratio", 5, 1, 6, "RATIO", 14, {}, {}, 0, true },    { "Attack", 5, 2, 6, "ATTACK", 15, {}, {}, 0, true },
                { "Release", 5, 3, 6, "RELEASE", 16, {}, {}, 0, true },   { "Make", 5, 4, 6, "MAKE-UP", 17, {}, {}, 0, true },   { "mbMix", 5, 5, 6, "MIX", 5, {}, {}, 0 },
                { "width1", 5, 0, 5, "WIDTH LOW", 18, {}, {}, 1 },        { "width2", 5, 1, 5, "WIDTH MID", 18, {}, {}, 1 },    { "width3", 5, 2, 5, "WIDTH HIGH", 18, {}, {}, 1 },
                { "monoBelow", 5, 3, 5, "MONO BELOW", 19, {}, {}, 1 },     { "imgBalance", 5, 4, 5, "BALANCE", 20, {}, {}, 1 },
                { "colourDrive", 5, 0, 4, "DRIVE", 5, {}, {}, 2 },        { "colourTilt", 5, 1, 4, "TONE", 20, {}, {}, 2 },     { "colourMix", 5, 2, 4, "MIX", 5, {}, {}, 2 },
                { "colourOut", 5, 3, 4, "OUTPUT", 17, {}, {}, 2 } } },
      powerAtt  (*p.apvts.getParameter ("power"),       [this] (float) { repaint(); }, nullptr),
      autoAtt   (*p.apvts.getParameter ("limAuto"),     [this] (float) { repaint(); }, nullptr),
      deltaAtt  (*p.apvts.getParameter ("delta"),       [this] (float) { repaint(); }, nullptr),
      charAtt   (*p.apvts.getParameter ("limChar"),     [this] (float) { repaint(); }, nullptr),
      shapeAtt  (*p.apvts.getParameter ("ditherShape"), [this] (float) { repaint(); }, nullptr),
      targetAtt (*p.apvts.getParameter ("target"),      [this] (float) { repaint(); }, nullptr)
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
        s.onDragStart = [this] { proc.undoManager.beginNewTransaction(); };
        addAndMakeVisible (s);
        attachKnob (k);
        s.onValueChange = [this] { repaint(); };
    }
    powerAtt.sendInitialUpdate(); autoAtt.sendInitialUpdate(); deltaAtt.sendInitialUpdate(); charAtt.sendInitialUpdate(); shapeAtt.sendInitialUpdate(); targetAtt.sendInitialUpdate();

    juce::Random rng (11);                                                                          // satin grain tile
    grain = juce::Image (juce::Image::ARGB, 128, 128, true);
    for (int y = 0; y < 128; ++y)
        for (int x = 0; x < 128; ++x)
            grain.setPixelAt (x, y, (rng.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (rng.nextFloat() * 0.05f));

    constrainer.setSizeLimits (720, 300, 2400, 2400);
    constrainer.setFixedAspectRatio ((double) kW / (double) designH());
    setConstrainer (&constrainer);
    setResizable (true, true);
    setWantsKeyboardFocus (true);
    setSize ((int) kW, (int) designH());
    holdPeak.fill (-120.0f);
    setTab (0);
    startTimerHz (30);
}

MS1AudioProcessorEditor::~MS1AudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void MS1AudioProcessorEditor::timerCallback()
{
    if (autoOpen) { autoRefresh(); repaint (autoRect().expanded (4.0f).transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer()); }
    if (autoIsLearning != proc.autoAn.learning.load()) { autoIsLearning = ! autoIsLearning; repaint (autoButtonRect().expanded (4.0f).transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer()); }
    meter.update (proc.outMeter);                                                    // the OUTPUT L / R dB meters
    if (proc.overLatch.exchange (false)) overLitUntil = juce::Time::getMillisecondCounter() + 1500;     // each overshoot lights the OVER lamp for 1.5 s
    repaint (juce::Rectangle<float> (0.0f, kRow1Y - 6.0f, kW, designH() - kRow1Y + 6.0f).transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer());     // the meters, history and displays move
    if (matchNoteTicks > 0) --matchNoteTicks;
    if (barState.copiedAt != 0) { repaint (juce::Rectangle<float> (0.0f, 50.0f, 900.0f, 50.0f).transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer()); if (! barState.copiedRecently()) barState.copiedAt = 0; }
    grHist[(size_t) grPos] = proc.busGrDb(); limHist[(size_t) grPos] = -std::abs (proc.grDb.load()); grPos = (grPos + 1) % (int) grHist.size();
}

void MS1AudioProcessorEditor::toggle (const char* id)
{
    auto* par = proc.apvts.getParameter (id);
    par->beginChangeGesture();
    par->setValueNotifyingHost (par->getValue() > 0.5f ? 0.0f : 1.0f);
    par->endChangeGesture();
}

void MS1AudioProcessorEditor::setChoice (const char* id, int index)
{
    auto* par = proc.apvts.getParameter (id);
    par->beginChangeGesture();
    par->setValueNotifyingHost (par->convertTo0to1 ((float) index));
    par->endChangeGesture();
}

void MS1AudioProcessorEditor::setFloat (const char* id, float v)
{
    auto* par = proc.apvts.getParameter (id);
    par->beginChangeGesture();
    par->setValueNotifyingHost (par->convertTo0to1 (v));
    par->endChangeGesture();
}

void MS1AudioProcessorEditor::applyTarget (int index)
{
    const auto& t = ms1::target (index);
    setChoice ("target", index);
    if (index != ms1::kNumTargets - 1)          // CUSTOM keeps the current values
    {
        setFloat ("limTarget", t.lufs);
        setFloat ("limCeiling", t.ceiling);
        setChoice ("limChar", t.character);
    }
}

void MS1AudioProcessorEditor::showTargetMenu()
{
    static vibr::ui::PresetMenuLook look;                                          // the same dark menu as the PRESET dropdown
    juce::PopupMenu menu;
    menu.setLookAndFeel (&look);
    for (int i = 0; i < ms1::kNumTargets; ++i)
    {
        const auto& t = ms1::target (i);
        menu.addItem (i + 1, juce::String (t.name) + (i < ms1::kNumTargets - 1 ? juce::String::formatted ("   %.0f LUFS   %.1f dBTP", t.lufs, t.ceiling) : juce::String()), true, i == choice ("target"));
    }
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (targetChipRect().transformedBy (juce::AffineTransform::scale (scale())).toNearestInt().translated (getScreenX(), getScreenY())),
                        [this] (int result) { if (result > 0) applyTarget (result - 1); });
}

void MS1AudioProcessorEditor::attachKnob (Knob& k)          // (re)connects a knob to its parameter (the multiband knobs follow the selected band)
{
    auto* par = proc.apvts.getParameter (knobParam (k));
    k.att.reset();
    k.slider.setDoubleClickReturnValue (true, par->convertFrom0to1 (par->getDefaultValue()));
    k.att = std::make_unique<juce::SliderParameterAttachment> (*dynamic_cast<juce::RangedAudioParameter*> (par), k.slider, nullptr);
}

void MS1AudioProcessorEditor::drawChain (juce::Graphics& g)
{
    ui::paintPanel (g, kPal, juce::Rectangle<float> (kMargin - 0.0f, kChainY, 1160.0f, kChainH), {}, ui::Pill::Plain, 10.0f);       // small corners like its buttons
    {   // AUTO: lit while its panel is open; a dot while the analyser is learning
        const auto b = autoButtonRect();
        ui::paintFlatButton (g, kPal, b, "AUTO", autoOpen, 13.0f, 1.0f, 10.0f, 0.1f, 1.3f);
        if (proc.autoAn.learning.load()) { g.setColour (autoOpen ? kPal.dark : kPal.body); g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ b.getRight() - 9.0f, b.getY() + 9.0f })); }
    }
    for (int i = 0; i < kChainStages; ++i)
    {
        const auto r = chainRect (i);
        const bool on = kChainOn[i] == nullptr || isOn (kChainOn[i]), selected = i == step && ! autoOpen;                                              // while the AUTO panel is shown no step is selected
        ui::paintFlatButton (g, kPal, r, kChainName[i], selected, 13.0f, 1.0f, 10.0f, 0.1f, 1.3f);
        const auto dot = juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ r.getX() + 20.0f, r.getCentreY() });      // the power dot: filled = on
        const auto col = selected ? kPal.dark : kPal.body;
        g.setColour (col);
        if (on) g.fillEllipse (dot); else g.drawEllipse (dot.reduced (1.0f), 1.5f);
        float gr = 0.0f;                                                                                               // the gain reduction of the stage (dB)
        if (i == 1) for (auto& b : proc.mbGr) gr = std::min (gr, b.load (std::memory_order_relaxed));
        else if (i == 4) gr = proc.busGrDb();
        else if (i == 5) gr = -std::abs (proc.grDb.load());
        if (on && gr < -0.05f)
        {
            const auto bar = juce::Rectangle<float> (r.getRight() - 50.0f, r.getBottom() - 9.0f, 38.0f, 3.0f);
            g.setColour (col.withAlpha (0.3f)); g.fillRoundedRectangle (bar, 1.5f);
            g.setColour (col); g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * std::clamp (-gr / 12.0f, 0.0f, 1.0f)), 1.5f);
        }
        if (i < kChainStages - 1)
            drawText (g, ">", r.getRight() + 11.0f, r.getCentreY(), 13.0f, kInk.withAlpha (0.4f), true, 0.0f);
    }
    {   // REF: opens / closes the REFERENCE panel; a dot shows that the B (reference) monitor is on
        const auto r = refPillRect();
        ui::paintFlatButton (g, kPal, r, "REFERENCE", refOpen, 13.0f, 1.0f, 10.0f, 0.1f, 1.3f);
        if (isOn ("refAB") && proc.reference.hasData()) { g.setColour (refOpen ? kPal.dark : kPal.body); g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ r.getX() + 12.0f, r.getCentreY() })); }
    }
}

void MS1AudioProcessorEditor::setStep (int s)
{
    step = juce::jlimit (0, kChainStages - 1, s);
    if (step >= 1 && step <= 4) tab = step - 1;
    layoutKnobs();
    repaint();
}

void MS1AudioProcessorEditor::setRefOpen (bool open)
{
    if (refOpen == open) return;
    refOpen = open;
    constrainer.setFixedAspectRatio ((double) kW / (double) designH());
    setSize (getWidth(), juce::roundToInt ((float) getWidth() * designH() / kW));
    repaint();
}

void MS1AudioProcessorEditor::setTab (int t)
{
    setStep (juce::jlimit (0, 3, t) + 1);
}

void MS1AudioProcessorEditor::selectMbBand (int b)
{
    selMb = juce::jlimit (0, 3, b);
    for (auto& k : knobs) if (k.perBand) attachKnob (k);
    repaint();
}

void MS1AudioProcessorEditor::layoutKnobs()
{
    const float sc = scale(), d = 2.0f * (kKnobR + kPad) * sc;
    for (auto& k : knobs)
    {
        k.slider.setBounds (juce::Rectangle<float> (d, d).withCentre (knobCentre (k.panel, k.index, k.count) * sc).toNearestInt());
        k.slider.setVisible (knobVisible (k));
    }
}

void MS1AudioProcessorEditor::resized() { layoutKnobs(); }

//==============================================================================
// SPECTRUM: before (grey) and after (colour) the plugin, 20 Hz .. 20 kHz on a log axis, dBFS with +4.5 dB per octave slope compensation
void MS1AudioProcessorEditor::drawSpectrum (juce::Graphics& g)
{
    const auto r = plotRect();
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (r.expanded (8.0f), 10.0f);
    const auto xOf = [&] (float f) { return r.getX() + r.getWidth() * std::log (f / 20.0f) / std::log (1000.0f); };
    const auto yOf = [&] (float db) { return r.getBottom() - r.getHeight() * (std::clamp (db, -90.0f, 0.0f) + 90.0f) / 90.0f; };
    for (float db : { -80.0f, -60.0f, -40.0f, -20.0f, 0.0f })
    {
        g.setColour (kInk.withAlpha (0.1f));
        g.drawLine (r.getX(), yOf (db), r.getRight(), yOf (db), 1.0f);
        drawText (g, juce::String ((int) db), r.getX() - 22.0f, yOf (db), 13.0f, kInk.withAlpha (0.5f), false, 0.0f);
    }
    for (float f : { 20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f })
    {
        g.setColour (kInk.withAlpha (0.1f));
        g.drawLine (xOf (f), r.getY(), xOf (f), r.getBottom(), 1.0f);
        drawText (g, f >= 1000.0f ? juce::String ((int) (f / 1000.0f)) + "k" : juce::String ((int) f), xOf (f), r.getBottom() + 14.0f, 13.0f, kInk.withAlpha (0.5f), false, 0.0f);
    }
    const float slope = proc.specOption (MS1AudioProcessor::SpecSlope) == 0 ? 0.0f : proc.specOption (MS1AudioProcessor::SpecSlope) == 1 ? 3.0f : 4.5f;
    std::array<float, ms1::kBands> pre, post;
    for (int b = 0; b < ms1::kBands; ++b) { pre[(size_t) b] = proc.specPre.bands[b].load (std::memory_order_relaxed); post[(size_t) b] = proc.specPost.bands[b].load (std::memory_order_relaxed); }
    if (specFreeze) { if (! frozenValid) { frozenPre = pre; frozenPost = post; frozenValid = true; } pre = frozenPre; post = frozenPost; }
    else frozenValid = false;
    for (int b = 0; b < ms1::kBands; ++b) holdPeak[(size_t) b] = std::max (post[(size_t) b], holdPeak[(size_t) b] - 0.1f);          // the hold line falls 3 dB per second
    const auto draw = [&] (const std::array<float, ms1::kBands>& v, juce::Colour col, float fillAlpha, float thick, bool dashed = false)
    {
        juce::Path line, area;
        for (int b = 0; b < ms1::kBands; ++b)
        {
            const float f = 20.0f * std::pow (1000.0f, ((float) b + 0.5f) / (float) ms1::kBands);
            const float db = v[(size_t) b] + slope * std::log2 (f / 1000.0f);
            const juce::Point<float> pt (xOf (f), yOf (db));
            if (b == 0) { line.startNewSubPath (pt); area.startNewSubPath (pt.x, r.getBottom()); area.lineTo (pt); } else { line.lineTo (pt); area.lineTo (pt); }
        }
        area.lineTo (r.getRight(), r.getBottom());
        area.closeSubPath();
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (r.expanded (2.0f).toNearestInt());
        if (fillAlpha > 0.0f) { g.setColour (col.withAlpha (fillAlpha)); g.fillPath (area); }
        g.setColour (col);
        if (dashed) { juce::Path dashedPath; const float dash[2] = { 3.0f, 3.0f }; juce::PathStrokeType (thick).createDashedStroke (dashedPath, line, dash, 2); g.fillPath (dashedPath); }
        else g.strokePath (line, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };
    if (const float* rs = proc.reference.spectrum())                                                // the long-term spectrum of the reference, at the matched level
    {
        const float shift = isOn ("refMatch") ? proc.refMatchDb.load() : 0.0f;
        juce::Path line;
        for (int b = 0; b < ms1::kBands; ++b)
        {
            const float f = 20.0f * std::pow (1000.0f, ((float) b + 0.5f) / (float) ms1::kBands);
            const juce::Point<float> pt (xOf (f), yOf (rs[b] + shift + slope * std::log2 (f / 1000.0f)));
            if (b == 0) line.startNewSubPath (pt); else line.lineTo (pt);
        }
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (r.expanded (2.0f).toNearestInt());
        g.setColour (kRed.withAlpha (0.8f));
        g.strokePath (line, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        drawText (g, "REFERENCE", r.getX() + 168.0f, r.getY() + 12.0f, 13.0f, kRed, true, 0.14f);
    }
    draw (pre, kInk.withAlpha (0.55f), 0.0f, 1.4f);
    draw (post, kBody, 0.14f, 2.2f);
    if (specHold) draw (holdPeak, kBody.withAlpha (0.65f), 0.0f, 1.2f, true);
    drawText (g, "BEFORE", r.getX() + 36.0f, r.getY() + 12.0f, 13.0f, kInk.withAlpha (0.55f), true, 0.14f);
    drawText (g, "AFTER", r.getX() + 96.0f, r.getY() + 12.0f, 13.0f, kBody, true, 0.14f);
    {   // the option buttons
        static const char* srcName[4] = { "SRC: MID", "SRC: SIDE", "SRC: LEFT", "SRC: RIGHT" };
        const int mode = proc.specOption (MS1AudioProcessor::SpecMode), src = proc.specOption (MS1AudioProcessor::SpecSource), sl = proc.specOption (MS1AudioProcessor::SpecSlope);
        const juce::String label[5] = { mode == 0 ? "SMOOTH 1/6" : "FFT 8192", sl == 0 ? "SLOPE 0" : sl == 1 ? "SLOPE +3" : "SLOPE +4.5", "HOLD", "FREEZE", srcName[juce::jlimit (0, 3, src)] };
        const bool on[5] = { mode == 1, sl != 2, specHold, specFreeze, src != 0 };
        for (int i = 0; i < 5; ++i)
        {
            vibr::ui::paintFlatButton (g, kPal, specButtonRect (i), label[i], on[i], 13.0f, 1.0f, 7.0f, 0.04f, 1.0f);
            static const int steps[5] = { 2, 3, 0, 0, 4 };                                // SMOOTH / FFT, the slope and the source cycle through settings on each click
            if (steps[i] > 0) paintCycleDots (g, specButtonRect (i), steps[i], i == 0 ? mode : i == 1 ? sl : src, on[i]);
        }
    }
    drawEq (g);
}

//==============================================================================
// EQ: the summed curve (+-18 dB around the middle of the plot), the eight band nodes and the strip of the selected band
ms1::EqBand MS1AudioProcessorEditor::eqBand (int band) const
{
    const juce::String id = "eq" + juce::String (band + 1);
    return { value ((id + "Freq").toRawUTF8()), value ((id + "Gain").toRawUTF8()), value ((id + "Q").toRawUTF8()), choice ((id + "Mode").toRawUTF8()), isOn ((id + "On").toRawUTF8()) };
}

int MS1AudioProcessorEditor::nodeAt (juce::Point<float> p, float radius) const
{
    const auto r = plotRect();
    int best = -1; float bestD = radius;
    for (int b = 0; b < ms1::kEqBands; ++b)
    {
        const auto e = eqBand (b);
        const juce::Point<float> c (r.getX() + r.getWidth() * std::log (e.freq / 20.0f) / std::log (1000.0f), r.getCentreY() - r.getHeight() * 0.5f * (e.gainDb / kEqRange));
        const float d = c.getDistanceFrom (p);
        if (d < bestD) { bestD = d; best = b; }
    }
    return best;
}

void MS1AudioProcessorEditor::drawEq (juce::Graphics& g)
{
    const auto r = plotRect();
    const bool eqIn = isOn ("eqOn");
    const float fs = proc.getSampleRate() > 0.0 ? (float) proc.getSampleRate() : 48000.0f;
    const auto xOf = [&] (float f) { return r.getX() + r.getWidth() * std::log (f / 20.0f) / std::log (1000.0f); };
    const auto yOf = [&] (float db) { return r.getCentreY() - r.getHeight() * 0.5f * (std::clamp (db, -kEqRange, kEqRange) / kEqRange); };
    std::array<ms1::EqBand, ms1::kEqBands> bands;
    for (int b = 0; b < ms1::kEqBands; ++b) bands[(size_t) b] = eqBand (b);

    g.setColour (kInk.withAlpha (eqIn ? 0.35f : 0.15f));                                            // the 0 dB line and the scale on the right
    g.drawLine (r.getX(), yOf (0.0f), r.getRight(), yOf (0.0f), 1.2f);
    for (float db : { -12.0f, 0.0f, 12.0f })
        drawText (g, juce::String::formatted ("%+d", (int) db), r.getRight() + 16.0f, yOf (db), 13.0f, kInk.withAlpha (0.4f), false, 0.0f);

    juce::Path curve;
    for (int i = 0; i <= 200; ++i)
    {
        const float f = 20.0f * std::pow (1000.0f, (float) i / 200.0f);
        float db = 0.0f;
        for (int b = 0; b < ms1::kEqBands; ++b) db += ms1::eqResponseDb (b, bands[(size_t) b], f, fs);
        const juce::Point<float> pt (r.getX() + r.getWidth() * (float) i / 200.0f, yOf (db));
        if (i == 0) curve.startNewSubPath (pt); else curve.lineTo (pt);
    }
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (r.expanded (2.0f).toNearestInt());
        g.setColour ((eqIn ? kInk : kInk.withAlpha (0.5f)).withAlpha (eqIn ? 0.95f : 0.35f));
        g.strokePath (curve, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    for (int b = 0; b < ms1::kEqBands; ++b)
    {
        const auto& e = bands[(size_t) b];
        const juce::Point<float> c (xOf (e.freq), yOf (e.gainDb));
        const bool sel = b == selBand, live = eqIn && e.on;
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillEllipse (juce::Rectangle<float> (22.0f, 22.0f).withCentre (c));
        if (sel) { g.setColour (kBody.withAlpha (live ? 1.0f : 0.5f)); g.fillEllipse (juce::Rectangle<float> (22.0f, 22.0f).withCentre (c)); }
        g.setColour ((sel ? kBody : kInk).withAlpha (live ? 0.95f : 0.35f));
        g.drawEllipse (juce::Rectangle<float> (22.0f, 22.0f).withCentre (c), 1.6f);
        drawText (g, juce::String (b + 1), c.x, c.y, 13.0f, sel ? kDark.withAlpha (live ? 1.0f : 0.6f) : kInk.withAlpha (live ? 0.95f : 0.4f), true, 0.0f);
        if (live && isOn (("eq" + juce::String (b + 1) + "Dyn").toRawUTF8()))                      // a dynamic band: a ring that fills with how much of its gain applies now
        {
            const float amt = proc.eqDynAmount (b);
            g.setColour (kBody.withAlpha (0.25f + 0.75f * amt));
            g.drawEllipse (juce::Rectangle<float> (30.0f, 30.0f).withCentre (c), 1.0f + 2.0f * amt);
        }
    }

    // type, frequency, gain, Q of the selected band: top left of the panel, above BEFORE
    const auto s = eqInfoRect();
    const auto& e = bands[(size_t) selBand];
    const char* type = selBand == 0 ? "LOW SHELF" : selBand == ms1::kEqBands - 1 ? "HIGH SHELF" : "BELL";
    const juce::String freq = e.freq >= 1000.0f ? juce::String (e.freq * 0.001f, 2) + " kHz" : juce::String ((int) std::lround (e.freq)) + " Hz";
    drawText (g, juce::String (selBand + 1) + "  " + type + "   " + freq + "   " + juce::String::formatted ("%+.1f dB", (double) e.gainDb) + "   Q " + juce::String (e.q, 2),
              s.getX(), s.getCentreY(), 13.0f, kInk.withAlpha (0.95f), true, 0.06f, juce::Justification::centredLeft);
    {   // the dynamic strip of the selected band
        const juce::String id = "eq" + juce::String (selBand + 1);
        const auto s2 = eqDynStripRect();
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillRoundedRectangle (s2, 8.0f);
        const bool dyn = isOn ((id + "Dyn").toRawUTF8()), below = isOn ((id + "DynBelow").toRawUTF8());
        for (int i = 0; i < 2; ++i)
        {
            vibr::ui::paintFlatButton (g, kPal, eqDynButtonRect (i), i == 0 ? "DYNAMIC" : (below ? "BELOW" : "ABOVE"), i == 0 ? dyn : below, 13.0f, 1.0f, 8.0f, 0.08f, 1.2f);
        }
        {
            const bool lin = choice ("eqPhase") == 1;
            vibr::ui::paintFlatButton (g, kPal, eqPhaseRect(), lin ? "LINEAR" : "MIN PH", lin, 13.0f, 1.0f, 8.0f, 0.06f, 1.2f);
            paintCycleDots (g, eqPhaseRect(), 2, lin ? 1 : 0, lin);
        }
        const float thr = value ((id + "DynThr").toRawUTF8()), ratio = value ((id + "DynRatio").toRawUTF8()), atk = value ((id + "DynAtk").toRawUTF8()), rel = value ((id + "DynRel").toRawUTF8());
        const juce::String text[4] = { "THR " + juce::String (thr, 1) + " dB", "RATIO " + juce::String (ratio, 1) + ":1", "ATT " + juce::String (atk, atk < 10.0f ? 1 : 0) + " ms", "REL " + juce::String ((int) std::lround (rel)) + " ms" };
        for (int i = 0; i < 4; ++i)
        {
            static const char* suffix[4] = { "DynThr", "DynRatio", "DynAtk", "DynRel" };
            const auto f = eqDynFieldRect (i);
            vibr::ui::paintField (g, kPal, f, text[i], dyn, 13.0f);
            if (auto* par = proc.apvts.getParameter (id + suffix[i]))                       // the value as a bar along the bottom edge: the field is dragged up / down, a double click resets it
            {
                const auto track = juce::Rectangle<float> (f.getX() + 10.0f, f.getBottom() - 6.0f, f.getWidth() - 20.0f, 3.0f);
                g.setColour (kInk.withAlpha (dyn ? 0.18f : 0.08f)); g.fillRoundedRectangle (track, 1.5f);
                g.setColour (dyn ? kBody : kInk.withAlpha (0.3f)); g.fillRoundedRectangle (track.withWidth (track.getWidth() * par->getValue()), 1.5f);
            }
        }
    }
    const char* labels[7] = { "EQ IN", "BAND ON", "STEREO", "L", "R", "M", "S" };
    const bool on[7] = { eqIn, e.on, e.mode == ms1::EqStereo, e.mode == ms1::EqLeft, e.mode == ms1::EqRight, e.mode == ms1::EqMid, e.mode == ms1::EqSide };
    for (int i = 0; i < 7; ++i)
    {
        vibr::ui::paintFlatButton (g, kPal, eqButtonRect (i), labels[i], on[i], 13.0f, 1.0f, 8.0f, 0.08f, 1.2f);
    }
}

//==============================================================================
// LOUDNESS: momentary, short-term, integrated, range, true-peak hold and the limiter's gain reduction, and the short-term history
// OUTPUT panel: true-peak bars for the left and right channel (the tick is the limiter ceiling, red above it, the mark holds the highest peak for 1.5 s), their peak
// in dB under them, and the OVER lamp: it lights red when the output went above the ceiling and goes out 1.5 s after the last overshoot (RESET or a click on it clears it at once)
void MS1AudioProcessorEditor::drawOutputMeters (juce::Graphics& g)
{
    meter.paint (g, meterOrigin().x, meterOrigin().y, isOn ("power"), kBody, kInk, 110.0f, 26.0f, 13.0f);
    const bool over = (juce::int32) (overLitUntil - juce::Time::getMillisecondCounter()) > 0;
    const auto lamp = overRect();
    g.setColour (over ? kRed : juce::Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (lamp, 10.0f);
    g.setColour (over ? kRed : kInk.withAlpha (0.35f)); g.drawRoundedRectangle (lamp.reduced (0.5f), 10.0f, 1.2f);
    drawText (g, "OVER", lamp.getCentreX(), lamp.getCentreY(), 13.0f, over ? kInk : kInk.withAlpha (0.55f), true, 0.14f);
}

void MS1AudioProcessorEditor::drawLoudness (juce::Graphics& g)
{
    const auto& ld = proc.loudness;
    const auto lufs = [] (float v) { return v <= -119.0f ? juce::String ("--") : juce::String (v, 1); };
    const float tpMax = proc.tpMaxDb.load(), gr = proc.grDb.load();
    const float target = value ("limTarget");
    const float integrated = ld.integrated.load();
    const auto& lv = proc.levels;
    const float peakDb = lv.samplePeakDb.load(), rmsDb = lv.rmsDb.load(), winPeak = lv.windowPeakDb.load(), shortTerm = ld.shortTerm.load(), momentary = ld.momentary.load(), ceiling = value ("limCeiling");
    const auto diff = [] (float a, float b) { return a <= -70.0f || b <= -70.0f ? juce::String ("--") : juce::String (a - b, 1); };          // the ratios need real signal (above the -70 absolute gate)

    // the meters: a label, a bar that fills from the left over a scale, an optional tick (the target, the ceiling) and the value at the end
    const auto panel = panelRect (4);
    const float labelX = panel.getX() + 14.0f, barX = panel.getX() + 98.0f, barW = panel.getX() + kLoudMeterW - 54.0f - barX;
    const auto meter = [&] (float y, const char* label, float lo, float hi, float v, juce::Colour fill, float tick, const juce::String& text, juce::Colour textCol, float redFrom = 1.0e9f)
    {
        const juce::Rectangle<float> bar (barX, y + 4.0f, barW, 14.0f);
        g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (bar, 4.0f);
        const auto xOf = [&] (float x) { return bar.getX() + bar.getWidth() * (std::clamp (x, lo, hi) - lo) / (hi - lo); };
        if (v > lo)
        {
            juce::Graphics::ScopedSaveState ss (g);
            juce::Path clip; clip.addRoundedRectangle (bar, 4.0f); g.reduceClipRegion (clip);
            const float x1 = xOf (v), xr = xOf (redFrom);
            g.setColour (fill); g.fillRect (juce::Rectangle<float> (bar.getX(), bar.getY(), std::min (x1, xr) - bar.getX(), bar.getHeight()));
            if (v > redFrom) { g.setColour (kRed); g.fillRect (juce::Rectangle<float> (xr, bar.getY(), x1 - xr, bar.getHeight())); }
        }
        if (tick > lo) { g.setColour (kInk.withAlpha (0.95f)); g.fillRect (xOf (tick) - 0.8f, bar.getY() - 3.0f, 1.6f, bar.getHeight() + 6.0f); }
        drawText (g, label, labelX, bar.getCentreY(), 13.0f, kInk.withAlpha (0.6f), true, 0.12f, juce::Justification::centredLeft);
        drawText (g, text, panel.getX() + kLoudMeterW, bar.getCentreY(), 13.0f, textCol, true, 0.02f, juce::Justification::centredRight);
    };
    const bool onTarget = integrated > -119.0f && std::abs (integrated - target) < 1.0f;
    meter (panel.getY() + 36.0f,  "MOMENTARY",  -40.0f, 0.0f, momentary, kBody.withAlpha (0.55f), target, lufs (momentary), kInk.withAlpha (0.95f));
    meter (panel.getY() + 63.0f,  "SHORT-TERM", -40.0f, 0.0f, shortTerm, kBody.withAlpha (0.8f), target, lufs (shortTerm), kInk.withAlpha (0.95f));
    meter (panel.getY() + 90.0f,  "INTEGRATED", -40.0f, 0.0f, integrated, kBody, target, lufs (integrated), onTarget ? kBody : kInk.withAlpha (0.95f));
    meter (panel.getY() + 123.0f, "LEVEL",      -60.0f, 0.0f, rmsDb, kInk.withAlpha (0.55f), peakDb, peakDb <= -119.0f ? juce::String ("--") : juce::String (peakDb, 1), peakDb >= -0.05f ? kRed : kInk.withAlpha (0.95f));
    meter (panel.getY() + 150.0f, "TRUE PEAK",  -24.0f, 0.0f, tpMax, kInk.withAlpha (0.75f), ceiling, tpMax <= -119.0f ? juce::String ("--") : juce::String (tpMax, 1), tpMax > ceiling + 0.05f ? kRed : kInk.withAlpha (0.95f), ceiling + 0.05f);
    meter (panel.getY() + 177.0f, "GAIN RED.",   0.0f, 12.0f, -gr, kBody, -1.0e9f, juce::String (gr, 1), gr < -6.0f ? kRed : kInk.withAlpha (0.95f), 6.0f);
    // the rest, as one quiet line
    drawText (g, "RANGE " + (ld.range.load() > 0.05f ? juce::String (ld.range.load(), 1) : juce::String ("--")) + " LU    CREST " + diff (peakDb, rmsDb) + "    PLR " + diff (tpMax, integrated)
                 + "    PSR " + diff (winPeak, shortTerm) + "    DR " + (lv.drValid.load() && rmsDb > -70.0f ? juce::String ((int) std::lround (lv.dr.load())) : juce::String ("--")),
              panel.getCentreX(), panel.getY() + 222.0f, 13.0f, kInk.withAlpha (0.6f), true, 0.06f);
    // the short-term history
    const auto h = historyRect();
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (h.expanded (4.0f), 6.0f);
    const auto yOf = [&] (float lu) { return h.getBottom() - h.getHeight() * (std::clamp (lu, -40.0f, 0.0f) + 40.0f) / 40.0f; };
    const float dash[2] = { 5.0f, 4.0f };
    g.setColour (kBody.withAlpha (0.7f));
    g.drawDashedLine (juce::Line<float> (h.getX(), yOf (target), h.getRight(), yOf (target)), dash, 2, 1.2f);
    const int pos = ld.historyPos.load (std::memory_order_acquire);
    juce::Path line;
    bool started = false;
    for (int k = 0; k < ms1::Loudness::kHistory; ++k)
    {
        const float v = ld.history[(pos + k) % ms1::Loudness::kHistory].load (std::memory_order_relaxed);
        if (v <= -119.0f) continue;
        const juce::Point<float> pt (h.getX() + h.getWidth() * (float) k / (float) (ms1::Loudness::kHistory - 1), yOf (v));
        if (! started) { line.startNewSubPath (pt); started = true; } else line.lineTo (pt);
    }
    g.setColour (kBody);
    juce::Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (h.expanded (2.0f).toNearestInt());
    g.strokePath (line, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    drawText (g, "SHORT-TERM, LAST 5 MIN", h.getX() + 6.0f, h.getY() + 8.0f, 13.0f, kInk.withAlpha (0.45f), true, 0.12f, juce::Justification::centredLeft);
}

//==============================================================================
// GAIN REDUCTION of the bus compressor: the last 8 seconds hanging from the top (0 .. -12 dB), the value now and the largest in the window
void MS1AudioProcessorEditor::drawBusGr (juce::Graphics& g, const std::array<float, 240>& hist, bool on, float now, const char* maxLabel)
{
    const auto panel = panelRect (6), r = juce::Rectangle<float> (panel.getX() + 18.0f, panel.getY() + 36.0f, panel.getWidth() - 36.0f, 188.0f);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (r.expanded (6.0f), 10.0f);
    const auto yOf = [&] (float db) { return r.getY() + r.getHeight() * std::clamp (-db, 0.0f, 12.0f) / 12.0f; };
    for (float db : { 0.0f, -3.0f, -6.0f, -9.0f, -12.0f })
    {
        g.setColour (kInk.withAlpha (0.1f));
        g.drawLine (r.getX(), yOf (db), r.getRight(), yOf (db), 1.0f);
        drawText (g, juce::String ((int) db), r.getRight() - 12.0f, yOf (db) + (db == 0.0f ? 8.0f : -7.0f), 13.0f, kInk.withAlpha (0.45f), false, 0.0f);
    }
    juce::Path area;
    const int n = (int) hist.size();
    float worst = 0.0f;
    area.startNewSubPath (r.getX(), r.getY());
    for (int k = 0; k < n; ++k)
    {
        const float v = hist[(size_t) ((grPos + k) % n)];
        worst = std::min (worst, v);
        area.lineTo (r.getX() + r.getWidth() * (float) k / (float) (n - 1), yOf (v));
    }
    area.lineTo (r.getRight(), r.getY());
    area.closeSubPath();
    g.setColour (kBody.withAlpha (on ? 0.25f : 0.1f));
    g.fillPath (area);
    g.setColour (kBody.withAlpha (on ? 1.0f : 0.35f));
    g.strokePath (area, juce::PathStrokeType (1.6f));
    drawText (g, "NOW", panel.getX() + 70.0f, panel.getBottom() - 46.0f, 13.0f, kInk.withAlpha (0.5f), true, 0.16f);
    drawText (g, juce::String (now, 1) + " dB", panel.getX() + 70.0f, panel.getBottom() - 24.0f, 20.0f, on ? kBody : kInk.withAlpha (0.4f), true, 0.02f);
    drawText (g, maxLabel, panel.getRight() - 90.0f, panel.getBottom() - 46.0f, 13.0f, kInk.withAlpha (0.5f), true, 0.16f);
    drawText (g, juce::String (worst, 1) + " dB", panel.getRight() - 90.0f, panel.getBottom() - 24.0f, 20.0f, worst < -6.0f ? kRed : (on ? kInk.withAlpha (0.95f) : kInk.withAlpha (0.4f)), true, 0.02f);
}

//==============================================================================
// the buttons under the knobs of the selected stage tab
std::vector<MS1AudioProcessorEditor::StageButton> MS1AudioProcessorEditor::stageButtons()
{
    const auto flip = [this] (juce::String id) { return [this, id] { toggle (id.toRawUTF8()); }; };
    const auto pick = [this] (juce::String id, int v) { return [this, id, v] { setChoice (id.toRawUTF8(), v); }; };
    std::vector<StageButton> out;
    if (tab == 0)
    {
        const juce::String b = "mb" + juce::String (selMb + 1);
        out = { { "MB IN", isOn ("mbOn"), flip ("mbOn") }, { juce::String (kBandName[selMb]) + " ON", isOn ((b + "On").toRawUTF8()), flip (b + "On") },
                { "SOLO", isOn ((b + "Solo").toRawUTF8()), flip (b + "Solo") }, { "AUTO REL.", isOn ((b + "Auto").toRawUTF8()), flip (b + "Auto") } };
        out.push_back ({ choice ("mbPhase") == 1 ? "PHASE: LINEAR" : "PHASE: MIN", choice ("mbPhase") == 1, [this] { setChoice ("mbPhase", choice ("mbPhase") == 0 ? 1 : 0); }, 2, choice ("mbPhase") == 1 ? 1 : 0 });
        const float knee = value ("mbKnee"), det = value ("mbRms"), sc = value ("mbScHz");
        const float nextKnee = knee < 1.0f ? 3.0f : knee < 4.5f ? 6.0f : knee < 9.0f ? 12.0f : 0.0f, nextDet = det < 25.0f ? 50.0f : det < 75.0f ? 100.0f : 0.0f;
        const float nextSc = sc < 30.0f ? 60.0f : sc < 90.0f ? 120.0f : sc < 180.0f ? 250.0f : 20.0f;
        out.push_back ({ "KNEE " + juce::String ((int) std::lround (knee)) + " DB", knee > 0.5f, [this, nextKnee] { setFloat ("mbKnee", nextKnee); }, 4, knee < 1.0f ? 0 : knee < 4.5f ? 1 : knee < 9.0f ? 2 : 3 });
        out.push_back ({ det < 25.0f ? "DET: PEAK" : det < 75.0f ? "DET: BLEND" : "DET: RMS", det >= 25.0f, [this, nextDet] { setFloat ("mbRms", nextDet); }, 3, det < 25.0f ? 0 : det < 75.0f ? 1 : 2 });
        out.push_back ({ sc < 30.0f ? juce::String ("SC HPF: OFF") : "SC: " + juce::String ((int) std::lround (sc)) + " HZ", sc >= 30.0f, [this, nextSc] { setFloat ("mbScHz", nextSc); }, 4, sc < 30.0f ? 0 : sc < 90.0f ? 1 : sc < 180.0f ? 2 : 3 });
    }
    else if (tab == 1) out = { { "IMAGER IN", isOn ("imgOn"), flip ("imgOn") }, { "AUTO SAFE WIDTH", isOn ("imgSafe"), flip ("imgSafe") } };
    else if (tab == 2)
    {
        static const char* modes[3] = { "MODE: STEREO", "MODE: MID", "MODE: SIDE" };
        const int ch = choice ("colourChar"), md = choice ("colourMode");
        out = { { "COLOUR IN", isOn ("colourOn"), flip ("colourOn") }, { "TAPE", ch == 0, pick ("colourChar", 0) }, { "TUBE", ch == 1, pick ("colourChar", 1) },
                { "TRANSFORMER", ch == 2, pick ("colourChar", 2) }, { modes[juce::jlimit (0, 2, md)], md != 0, [this, md] { setChoice ("colourMode", (md + 1) % 3); }, 3, juce::jlimit (0, 2, md) } };
    }
    else
    {
        static const char* sc[4] = { "SC HPF: OFF", "SC HPF: 60 HZ", "SC HPF: 90 HZ", "SC HPF: 150 HZ" };
        const int c = choice ("busSc");
        out = { { "BUS IN", isOn ("busOn"), flip ("busOn") }, { sc[juce::jlimit (0, 3, c)], c != 0, [this, c] { setChoice ("busSc", (c + 1) % 4); }, 4, juce::jlimit (0, 3, c) } };
    }
    return out;
}

//==============================================================================
// the display next to the stage knobs: multiband (band columns on a frequency axis, gain reduction hanging from the top, draggable crossovers),
// imager (width per band, draggable crossovers, correlation), colour (the transfer curve), bus (the gain reduction history)
void MS1AudioProcessorEditor::drawStageDisplay (juce::Graphics& g)
{
    if (step == 5) { drawBusGr (g, limHist, true, -std::abs (proc.grDb.load()), "MAX (8 S)"); return; }
    if (tab == 3) { drawBusGr (g, grHist, isOn ("busOn"), proc.busGrDb(), "MAX (8 S)"); return; }
    const auto panel = panelRect (6), full = dispRect(), r = tab == 1 ? full.withTrimmedRight (172.0f) : full;
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (full.expanded (6.0f), 10.0f);
    const auto xOf = [&] (float f) { return r.getX() + r.getWidth() * std::log (f / 20.0f) / std::log (1000.0f); };

    if (tab == 2)
    {
        const int ch = choice ("colourChar");
        const float drive = value ("colourDrive") * 0.01f;
        const auto sq = juce::Rectangle<float> (r.getHeight(), r.getHeight()).withCentre ({ r.getCentreX(), r.getCentreY() });
        const auto px = [&] (float x) { return sq.getCentreX() + x * sq.getWidth() * 0.5f; };
        const auto py = [&] (float y) { return sq.getCentreY() - std::clamp (y, -1.15f, 1.15f) * sq.getHeight() * 0.5f; };
        g.setColour (kInk.withAlpha (0.12f));
        g.drawRect (sq, 1.0f);
        g.drawLine (sq.getX(), sq.getCentreY(), sq.getRight(), sq.getCentreY(), 1.0f);
        g.drawLine (sq.getCentreX(), sq.getY(), sq.getCentreX(), sq.getBottom(), 1.0f);
        const float dash[2] = { 4.0f, 4.0f };
        g.setColour (kInk.withAlpha (0.35f));
        g.drawDashedLine (juce::Line<float> (px (-1.0f), py (-1.0f), px (1.0f), py (1.0f)), dash, 2, 1.0f);
        juce::Path curve;
        for (int i = 0; i <= 120; ++i)
        {
            const float x = -1.0f + 2.0f * (float) i / 120.0f;
            const juce::Point<float> pt (px (x), py (ms1::Colour::curve (ch, x, drive)));
            if (i == 0) curve.startNewSubPath (pt); else curve.lineTo (pt);
        }
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (sq.expanded (1.0f).toNearestInt());
            g.setColour (isOn ("colourOn") ? kBody : kBody.withAlpha (0.35f));
            g.strokePath (curve, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        static const char* names[3] = { "TAPE", "TUBE", "TRANSFORMER" };
        drawText (g, "IN", sq.getRight() + 16.0f, sq.getCentreY() + 10.0f, 13.0f, kInk.withAlpha (0.45f), true, 0.12f);
        drawText (g, "OUT", sq.getCentreX() + 14.0f, sq.getY() + 8.0f, 13.0f, kInk.withAlpha (0.45f), true, 0.12f);
        drawText (g, names[juce::jlimit (0, 2, ch)], panel.getCentreX(), panel.getY() + 240.0f, 16.0f, kBody, true, 0.14f);
        drawText (g, "4X OVERSAMPLED  |  LATENCY " + juce::String (ms1::Colour::kLatency) + " SAMPLES", panel.getCentreX(), panel.getY() + 264.0f, 13.0f, kInk.withAlpha (0.5f), true, 0.12f);
        return;
    }

    // log frequency axis with the band regions (multiband 4, imager 3)
    const int nBands = tab == 0 ? 4 : 3;
    float edge[5];
    edge[0] = 20.0f; edge[nBands] = 20000.0f;
    if (tab == 0) { edge[1] = value ("xover1"); edge[2] = std::max (value ("xover2"), edge[1] * 1.2f); edge[3] = std::max (value ("xover3"), edge[2] * 1.2f); }
    else { edge[1] = value ("imgX1"); edge[2] = std::max (value ("imgX2"), edge[1] * 1.2f); }
    const bool stageOn = isOn (tab == 0 ? "mbOn" : "imgOn");
    for (int k = 0; k < nBands; ++k)
    {
        const float x0 = xOf (edge[k]), x1 = xOf (edge[k + 1]);
        const bool sel = tab == 0 && k == selMb;
        g.setColour (kBody.withAlpha (sel ? 0.16f : 0.07f));
        g.fillRect (juce::Rectangle<float> (x0, r.getY(), x1 - x0, r.getHeight()));
        const float cx = 0.5f * (x0 + x1), bw = std::min (34.0f, x1 - x0 - 10.0f);
        if (tab == 0)
        {
            const float gr = proc.mbGr[(size_t) k].load (std::memory_order_relaxed), h = r.getHeight() * std::clamp (-gr / 18.0f, 0.0f, 1.0f);
            const juce::String id = "mb" + juce::String (k + 1);
            const bool bandOn = isOn ((id + "On").toRawUTF8());
            g.setColour (kBody.withAlpha (stageOn && bandOn ? 0.85f : 0.3f));
            g.fillRoundedRectangle (juce::Rectangle<float> (bw, std::max (2.0f, h)).withX (cx - bw * 0.5f).withY (r.getY()), 3.0f);
            drawText (g, juce::String (gr, 1), cx, r.getY() + 14.0f + std::max (2.0f, h) , 13.0f, kInk.withAlpha (0.85f), true, 0.02f);
            drawText (g, kBandName[k], cx, r.getBottom() - 14.0f, 13.0f, isOn ((id + "Solo").toRawUTF8()) ? kBody : kInk.withAlpha (sel ? 1.0f : 0.6f), true, 0.1f);
        }
        else
        {
            const float w = value (("width" + juce::String (k + 1)).toRawUTF8()) * 0.01f, h = r.getHeight() * 0.5f * std::clamp (w, 0.0f, 2.0f);
            g.setColour (kBody.withAlpha (stageOn ? 0.85f : 0.3f));
            g.fillRoundedRectangle (juce::Rectangle<float> (bw, std::max (2.0f, h)).withX (cx - bw * 0.5f).withY (r.getBottom() - std::max (2.0f, h)), 3.0f);
            static const char* nm[3] = { "LOW", "MID", "HIGH" };
            drawText (g, juce::String ((int) std::lround (w * 100.0f)) + " %", cx, r.getBottom() - std::max (2.0f, h) - 12.0f, 13.0f, kInk.withAlpha (0.85f), true, 0.02f);
            drawText (g, nm[k], cx, r.getBottom() - 14.0f, 13.0f, kInk.withAlpha (0.6f), true, 0.1f);
            const float safe = proc.imager.safeScale[k].load (std::memory_order_relaxed);
            if (isOn ("imgSafe") && safe < 0.98f) drawText (g, "SAFE " + juce::String ((int) std::lround ((safe - 1.0f) * 100.0f)) + " %", cx, r.getY() + 26.0f, 13.0f, kRed, true, 0.06f);
            const float sm = proc.imager.sideMidDb[k].load (std::memory_order_relaxed);                  // the measured width: the side level against the mid level of the band
            drawText (g, sm <= -59.0f ? juce::String ("S/M --") : "S/M " + juce::String (sm, 0) + " dB", cx, r.getY() + 12.0f, 13.0f, sm > 0.0f ? kRed : kInk.withAlpha (0.6f), true, 0.04f);
        }
    }
    if (tab == 1)                                                                              // the 100 % line
    {
        const float dash[2] = { 4.0f, 4.0f };
        g.setColour (kInk.withAlpha (0.35f));
        g.drawDashedLine (juce::Line<float> (r.getX(), r.getCentreY(), r.getRight(), r.getCentreY()), dash, 2, 1.0f);
    }
    for (int j = 1; j < nBands; ++j)                                                           // the crossovers: grip and frequency
    {
        const float x = xOf (edge[j]);
        g.setColour (kInk.withAlpha (0.75f));
        g.drawLine (x, r.getY(), x, r.getBottom(), 1.4f);
        g.fillRoundedRectangle (juce::Rectangle<float> (8.0f, 22.0f).withCentre ({ x, r.getCentreY() }), 3.0f);
        drawText (g, edge[j] >= 1000.0f ? juce::String (edge[j] * 0.001f, 2) + "k" : juce::String ((int) std::lround (edge[j])), x, panel.getY() + 236.0f, 13.0f, kInk.withAlpha (0.7f), true, 0.04f);
    }
    if (tab == 1)                                                                              // the goniometer: side to the sides, mid up (L = left diagonal)
    {
        const auto gr = goniometerRect().reduced (4.0f);
        const auto sq = juce::Rectangle<float> (std::min (gr.getWidth(), gr.getHeight()), std::min (gr.getWidth(), gr.getHeight())).withCentre (gr.getCentre());
        g.setColour (kInk.withAlpha (0.14f));
        g.drawRect (sq, 1.0f);
        g.drawLine (sq.getX(), sq.getCentreY(), sq.getRight(), sq.getCentreY(), 1.0f);
        g.drawLine (sq.getCentreX(), sq.getY(), sq.getCentreX(), sq.getBottom(), 1.0f);
        const float half = sq.getWidth() * 0.5f;
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (sq.toNearestInt());
        g.setColour (kBody.withAlpha (isOn ("imgOn") ? 0.55f : 0.3f));
        const int pos = proc.stereoMeter.scopePos.load (std::memory_order_relaxed);
        for (int k = 0; k < ms1::StereoMeter::kScope; ++k)
        {
            const int idx = (pos + k) % ms1::StereoMeter::kScope;
            const float l = proc.stereoMeter.scopeL[idx], rr = proc.stereoMeter.scopeR[idx];
            const float sx = (l - rr) * 0.7071f, my = (l + rr) * 0.7071f;
            g.fillRect (sq.getCentreX() + std::clamp (sx, -1.0f, 1.0f) * half * 0.95f - 0.8f, sq.getCentreY() - std::clamp (my, -1.0f, 1.0f) * half * 0.95f - 0.8f, 1.6f, 1.6f);
        }
    }
    if (tab == 0) drawText (g, juce::String (kBandName[selMb]) + " BAND: CLICK A BAND, DRAG A CROSSOVER", panel.getCentreX(), panel.getY() + 264.0f, 13.0f, kInk.withAlpha (0.5f), true, 0.04f);
    else
    {
        const float c = proc.stereoMeter.corrShort.load (std::memory_order_relaxed), ci = proc.stereoMeter.corrInt.load (std::memory_order_relaxed);
        const auto bar = juce::Rectangle<float> (panel.getX() + 60.0f, panel.getY() + 254.0f, panel.getWidth() - 120.0f, 8.0f);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (bar, 4.0f);
        g.setColour (kInk.withAlpha (0.3f));
        g.fillRect (bar.getCentreX() - 0.7f, bar.getY() - 3.0f, 1.4f, bar.getHeight() + 6.0f);
        g.setColour (c < 0.0f ? kRed : kBody);
        g.fillEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre ({ bar.getX() + bar.getWidth() * 0.5f * (c + 1.0f), bar.getCentreY() }));
        drawText (g, "CORRELATION  SHORT-TERM " + juce::String::formatted ("%+.2f", (double) c) + "   INTEGRATED " + juce::String::formatted ("%+.2f", (double) ci), panel.getCentreX(), panel.getY() + 274.0f, 13.0f, kInk.withAlpha (0.6f), true, 0.12f);
    }
}

//==============================================================================
// REFERENCE: load, play, A / B, level match, follow the host, position
void MS1AudioProcessorEditor::showReferenceChooser()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose a reference track", proc.reference.file().getParentDirectory(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.m4a;*.ogg");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (f.existsAsFile()) proc.loadReference (f);
    });
}

void MS1AudioProcessorEditor::drawReference (juce::Graphics& g)
{
    const auto& ref = proc.reference;
    const bool has = ref.hasData();
    const auto button = [&] (int i, const juce::String& label, bool on, float alpha = 1.0f)
    {
        vibr::ui::paintFlatButton (g, kPal, refButtonRect (i), label, on, 13.0f, alpha);
    };
    button (0, ref.isLoading() ? "LOADING..." : "LOAD", false);
    button (1, isOn ("refPlay") ? "PAUSE" : "PLAY", isOn ("refPlay"), has ? 1.0f : 0.4f);
    button (2, "A  CHAIN", ! isOn ("refAB"));
    button (3, "B  REFERENCE", isOn ("refAB"), has ? 1.0f : 0.5f);
    button (4, isOn ("refMatch") ? juce::String::formatted ("LEVEL MATCH %+.1f dB", (double) proc.refMatchDb.load()) : juce::String ("LEVEL MATCH"), isOn ("refMatch"));
    button (5, "FOLLOW HOST", isOn ("refFollow"));
    button (6, "RESET", false);
    button (7, "MATCH EQ", false, has ? 1.0f : 0.4f);

    const auto r = panelRect (7);
    juce::String info = ref.isLoading() ? "Loading and analysing..." : ref.hasFailed() ? "This file could not be read" : has ? ref.name() : "No reference loaded: load a mastered track to compare";
    drawText (g, info, r.getX() + 140.0f, r.getY() + 43.0f, 15.0f, has ? kInk.withAlpha (0.95f) : kInk.withAlpha (0.5f), true, 0.04f, juce::Justification::centredLeft);
    if (has)
    {
        const int secs = (int) ref.seconds();
        drawText (g, juce::String::formatted ("%.1f LUFS   |   %d:%02d", (double) ref.integratedLufs(), secs / 60, secs % 60), r.getX() + 710.0f, r.getY() + 43.0f, 13.0f, kBody, true, 0.04f, juce::Justification::centredRight);
    }
    drawText (g, matchNoteTicks > 0 ? matchNote : "INPUT LEARNED: " + juce::String (proc.learnSeconds(), 0) + " S", r.getX() + 556.0f, r.getY() + 119.0f, 13.0f, matchNoteTicks > 0 ? kBody : kInk.withAlpha (0.5f), true, 0.1f, juce::Justification::centredLeft);
    const auto bar = refBarRect();
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (bar, 7.0f);
    if (has)
    {
        g.setColour (kBody.withAlpha (isOn ("refPlay") ? 0.9f : 0.5f));
        g.fillRoundedRectangle (bar.withWidth (std::max (8.0f, bar.getWidth() * ref.positionFraction())), 7.0f);
    }
}

//==============================================================================
void MS1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));

    vibr::ui::paintBody (g, kPal, kW, designH(), grain);
    vibr::ui::paintHeader (g, kPal, "MS-1", "MASTERING SUITE", "AUDIO PERFECTION", isOn ("power"));
    {   // the target chip
        const auto chip = targetChipRect();
        g.setColour (kPanel);
        g.fillRoundedRectangle (chip, 6.0f);                                      // the same small corners as the preset bar
        g.setColour (kDark);
        g.drawRoundedRectangle (chip.reduced (0.9f), 6.0f, 1.8f);
        const auto& t = ms1::target (choice ("target"));
        drawText (g, "TARGET", chip.getX() + 14.0f, chip.getCentreY(), 13.0f, kInk.withAlpha (0.5f), true, 0.2f, juce::Justification::centredLeft);
        drawText (g, t.name, chip.getX() + 78.0f, chip.getCentreY(), 13.0f, kBody, true, 0.06f, juce::Justification::centredLeft);
        drawText (g, juce::String::formatted ("%.1f LUFS  %.1f dBTP", value ("limTarget"), value ("limCeiling")), chip.getRight() - 12.0f, chip.getCentreY(), 13.0f, kInk.withAlpha (0.85f), false, 0.02f, juce::Justification::centredRight);
    }
    vibr::ui::paintPresetBar (g, kPal, proc.presets, proc.undoManager, 62.0f, barState, true, 6.0f);

    // panels: charcoal, light outline, title pill on the top edge (the stage panel has the tab pills instead)
    static const char* displayTitle[4] = { "BAND GAIN REDUCTION", "STEREO FIELD", "TRANSFER CURVE", "BUS GAIN REDUCTION" };
    for (int i = 0; i < 8; ++i)
        if (panelShown (i))
            vibr::ui::paintPanel (g, kPal, panelRect (i), i == 5 ? juce::String (kTabName[tab]) : i == 6 ? juce::String (step == 5 ? "LIMITER GAIN REDUCTION" : displayTitle[tab]) : juce::String (kPanelName[i]));
    if (step == 4 && ! autoOpen)
    {   // EXT lamp: lit while the host's sidechain input is the key of the bus compressor
        const auto r6 = panelRect (6);
        const bool ext = proc.extKey.load (std::memory_order_relaxed);
        const auto lamp = juce::Rectangle<float> (10.0f, 10.0f).withCentre ({ r6.getRight() - 52.0f, r6.getY() + 22.0f });
        g.setColour (ext ? kBody : kBody.withAlpha (0.25f));
        g.fillEllipse (lamp);
        drawText (g, "EXT", lamp.getRight() + 6.0f, lamp.getCentreY(), 13.0f, kInk.withAlpha (ext ? 0.9f : 0.5f), true, 0.1f, juce::Justification::centredLeft);
    }
    drawChain (g);
    if (step == 0 && ! autoOpen) drawSpectrum (g);
    drawLoudness (g);
    drawOutputMeters (g);
    if (autoOpen) drawAuto (g); else if (step != 0) drawStageDisplay (g);
    if (refOpen) drawReference (g);
    {   // RESET: integrated loudness, range and the true-peak hold start again
        vibr::ui::paintFlatButton (g, kPal, resetRect(), "RESET", false, 13.0f, 1.0f, 8.0f, 0.14f, 1.2f);
    }

    // the buttons of row 2
    const int ch = choice ("limChar"), shape = choice ("ditherShape");
    const bool autoOn = isOn ("limAuto");
    const bool btnOn[8] = { ch == 0, ch == 1, ch == 2, autoOn, isOn ("delta"), shape != 0, isOn ("limRelAuto"), isOn ("limFreeze") };
    const juce::String label[8] = { "TRANSPARENT", "PUNCHY", "LOUD", "AUTO TO TARGET", "DELTA", shape == 0 ? juce::String ("SHAPE: FLAT") : shape == 1 ? juce::String ("SHAPE: GENTLE") : juce::String ("SHAPE: AGGR."), "AUTO REL.", "FREEZE GAIN" };
    for (int i = 0; i < 8; ++i)
    {
        if ((i < 4 || i >= 6) && (step != 5 || autoOpen)) continue;                                // the limiter buttons belong to the LIMITER step
        vibr::ui::paintFlatButton (g, kPal, buttonRect (i), label[i], btnOn[i], 13.0f, 1.0f, 9.0f, 0.06f);
        if (i == 5) paintCycleDots (g, buttonRect (i), 3, shape, btnOn[i]);                    // the dither shape: flat, gentle, aggressive
    }

    {   // the INPUT panel buttons
        const float hpf = value ("trimHpf");
        const juce::String lab[3] = { "PHASE", hpf < 1.0f ? juce::String ("HPF") : "HPF " + juce::String ((int) std::lround (std::max (10.0f, hpf))), "MONO" };
        const bool on[3] = { isOn ("trimPolarity"), hpf >= 1.0f, isOn ("monoCheck") };
        for (int i = 0; i < 3; ++i)
        {
            vibr::ui::paintFlatButton (g, kPal, inputButtonRect (i), i == 0 && on[0] ? "INVERT" : lab[i], on[i], 13.0f, 1.0f, 9.0f, 0.04f);
            if (i == 0) paintCycleDots (g, inputButtonRect (i), 2, on[0] ? 1 : 0, on[0]);                                    // PHASE / INVERT
            else if (i == 1) paintCycleDots (g, inputButtonRect (i), 5, hpf < 1.0f ? 0 : hpf < 25.0f ? 1 : hpf < 35.0f ? 2 : hpf < 50.0f ? 3 : 4, on[1]);   // HPF: off, 20, 30, 40, 60 Hz
        }
    }
    if (! autoOpen && step >= 1 && step <= 4)
    {   // the buttons of the stage tab
        const auto sb = stageButtons();
        for (int i = 0; i < (int) sb.size(); ++i)
        {
            vibr::ui::paintFlatButton (g, kPal, stageButtonRect (i, (int) sb.size()), sb[(size_t) i].label, sb[(size_t) i].on, 13.0f);
            if (sb[(size_t) i].steps > 0) paintCycleDots (g, stageButtonRect (i, (int) sb.size()), sb[(size_t) i].steps, sb[(size_t) i].cur, sb[(size_t) i].on);
        }
    }

    // knobs: recessed ring, 11 ticks, legends, name and value
    for (auto& k : knobs)
    {
        if (! knobVisible (k)) continue;
        const auto c = knobCentre (k.panel, k.index, k.count);
        static const char* lo[21] = { "-24", "0", "-3", "10", "0.5", "0", "-24", "off", "-30", "1.5", ".1", ".1", "0", "-60", "1", ".1", "10", "-12", "0", "off", "-6" },
                         * hi[21] = { "+24", "+24", "0", "1k", "5", "100", "-6", "24", "0", "10", "30", "AUTO", "+12", "0", "20", "200", "2k", "+12", "200", "250", "+6" };
        const bool outPanel = k.panel == 2;
        const float dy = knobDy (k.panel);
        const char* loText = outPanel && k.kind == 0 ? "-12" : lo[k.kind], * hiText = outPanel && k.kind == 0 ? "+6" : hi[k.kind];

        const double v = k.slider.getValue();
        juce::String txt;
        switch (k.kind)
        {
            case 0: txt = juce::String::formatted ("%+.1f dB", v); break;
            case 1: txt = autoOn ? juce::String::formatted ("AUTO %+.1f dB", (double) proc.autoGainDb.load()) : juce::String::formatted ("+%.1f dB", v); break;
            case 2: txt = juce::String::formatted ("%.1f dBTP", v); break;
            case 3: txt = juce::String ((int) std::lround (v)) + " ms"; break;
            case 4: txt = juce::String (v, 1) + " ms"; break;
            case 5: txt = juce::String ((int) std::lround (v)) + " %"; break;
            case 6: txt = juce::String::formatted ("%.1f LUFS", v); break;
            case 7: { static const char* d[4] = { "OFF", "16 BIT", "20 BIT", "24 BIT" }; txt = d[juce::jlimit (0, 3, (int) std::lround (v))]; break; }
            case 8: txt = juce::String::formatted ("%.1f dB", v); break;
            case 9: txt = juce::String (ms1::busRatioValue ((int) std::lround (v)), 1) + " : 1"; break;
            case 10: { const float a = ms1::busAttackMs ((int) std::lround (v)); txt = juce::String (a, a < 1.0f ? 1 : 0) + " ms"; break; }
            case 11: { const float rl = ms1::busReleaseMs ((int) std::lround (v)); txt = rl <= 0.0f ? juce::String ("AUTO") : juce::String (rl * 0.001f, 1) + " s"; break; }
            case 12: txt = juce::String::formatted ("+%.1f dB", v); break;
            case 13: txt = juce::String::formatted ("%.1f dB", v); break;
            case 14: txt = juce::String (v, 1) + " : 1"; break;
            case 15: txt = v < 10.0 ? juce::String (v, 1) + " ms" : juce::String ((int) std::lround (v)) + " ms"; break;
            case 16: txt = isOn (("mb" + juce::String (selMb + 1) + "Auto").toRawUTF8()) ? juce::String ("AUTO") : v >= 1000.0 ? juce::String (v * 0.001, 2) + " s" : juce::String ((int) std::lround (v)) + " ms"; break;
            case 17: case 20: txt = juce::String::formatted ("%+.1f dB", v); break;
            case 18: txt = juce::String ((int) std::lround (v)) + " %"; break;
            default: txt = v < 1.0 ? juce::String ("OFF") : juce::String ((int) std::lround (std::max (20.0, v))) + " Hz"; break;      // 19: mono below
        }
        vibr::ui::paintKnob (g, kPal, c, k.name, txt, loText, hiText, panelRect (k.panel).getY() + 154.0f + dy, panelRect (k.panel).getY() + 182.0f + dy);
        ui::paintValueArc (g, c, vibr::ui::kKnobR, (float) k.slider.valueToProportionOfLength (k.slider.getValue()), kBody);
    }

    vibr::ui::paintFooter (g, kPal, (refOpen ? kRow4Y + kRow4H : kRow3Y + kRow3H) + 35.0f);
}

void MS1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    const auto p = e.position / scale();
    proc.undoManager.beginNewTransaction();                                                 // every click or drag is one undo step
    if (powerRect().contains (p)) { toggle ("power"); return; }
    if (targetChipRect().contains (p)) { showTargetMenu(); return; }
    if (autoButtonRect().contains (p)) { setAutoOpen (! autoOpen); return; }
    if (autoOpen && autoMouseDown (p)) return;
    if (vibr::ui::presetBarMouseDown (p, *this, proc.presets, proc.undoManager, 62.0f, scale(), barState, [this] { repaint(); })) return;
    const bool eqShown = step == 0 && ! autoOpen;
    for (int i = 0; i < 5 && eqShown; ++i)                                                  // the spectrum options
        if (specButtonRect (i).expanded (2.0f).contains (p))
        {
            using P = MS1AudioProcessor;
            if (i == 0) proc.setSpecOption (P::SpecMode, 1 - proc.specOption (P::SpecMode));
            else if (i == 1) proc.setSpecOption (P::SpecSlope, (proc.specOption (P::SpecSlope) + 1) % 3);
            else if (i == 2) { specHold = ! specHold; holdPeak.fill (-120.0f); }
            else if (i == 3) specFreeze = ! specFreeze;
            else proc.setSpecOption (P::SpecSource, (proc.specOption (P::SpecSource) + 1) % 4);
            repaint();
            return;
        }
    if (resetRect().expanded (3.0f).contains (p)) { proc.requestReset(); overLitUntil = 0; repaint(); return; }
    if (overRect().expanded (3.0f).contains (p)) { proc.overLatch.store (false); overLitUntil = 0; repaint(); return; }

    // the dynamic EQ strip, the EQ strip buttons, then the band nodes
    const juce::String id = "eq" + juce::String (selBand + 1);
    if (eqShown) {
    if (eqPhaseRect().expanded (2.0f).contains (p)) { setChoice ("eqPhase", choice ("eqPhase") == 0 ? 1 : 0); repaint(); return; }
    if (eqDynButtonRect (0).expanded (2.0f).contains (p)) { toggle ((id + "Dyn").toRawUTF8()); repaint(); return; }
    if (eqDynButtonRect (1).expanded (2.0f).contains (p)) { toggle ((id + "DynBelow").toRawUTF8()); repaint(); return; }
    for (int i = 0; i < 4; ++i)
        if (eqDynFieldRect (i).contains (p))
        {
            static const char* suffix[4] = { "DynThr", "DynRatio", "DynAtk", "DynRel" };
            auto* par = proc.apvts.getParameter (id + suffix[i]);
            par->beginChangeGesture();
            dragField = i; dragStartY = p.y; dragStartNorm = par->getValue();
            return;
        }
    for (int i = 0; i < 7; ++i)
        if (eqButtonRect (i).expanded (2.0f).contains (p))
        {
            if (i == 0) toggle ("eqOn");
            else if (i == 1) toggle ((id + "On").toRawUTF8());
            else setChoice ((id + "Mode").toRawUTF8(), i == 2 ? ms1::EqStereo : i == 3 ? ms1::EqLeft : i == 4 ? ms1::EqRight : i == 5 ? ms1::EqMid : ms1::EqSide);
            repaint();
            return;
        }
    if (const int n = nodeAt (p, 16.0f); n >= 0)
    {
        selBand = n; dragBand = n;
        const juce::String nid = "eq" + juce::String (n + 1);
        proc.apvts.getParameter (nid + "Freq")->beginChangeGesture();
        proc.apvts.getParameter (nid + "Gain")->beginChangeGesture();
        repaint();
        return;
    }
    }
    // stage tabs and buttons (row 3), the display (selects a multiband band, drags the crossovers), the reference (row 4)
    {
        for (int i = 0; i < kChainStages; ++i)
            if (chainRect (i).expanded (3.0f).contains (p))
            {
                if (kChainOn[i] != nullptr && chainDotRect (i).contains (p)) toggle (kChainOn[i]);
                else { if (autoOpen) setAutoOpen (false); setStep (i); }
                repaint();
                return;
            }
        if (refPillRect().expanded (3.0f).contains (p)) { setRefOpen (! refOpen); return; }
        auto sb = stageButtons();
        for (int i = 0; i < (int) sb.size() && step >= 1 && step <= 4 && ! autoOpen; ++i)
            if (stageButtonRect (i, (int) sb.size()).expanded (3.0f).contains (p)) { sb[(size_t) i].act(); repaint(); return; }

        if ((tab == 0 || tab == 1) && step >= 1 && step <= 4 && ! autoOpen && dispRect().expanded (8.0f).contains (p))
        {
            const auto r = tab == 1 ? dispRect().withTrimmedRight (172.0f) : dispRect();
            const int nDiv = tab == 0 ? 3 : 2;
            for (int j = 0; j < nDiv; ++j)
            {
                const juce::String did = tab == 0 ? "xover" + juce::String (j + 1) : "imgX" + juce::String (j + 1);
                const float dx = r.getX() + r.getWidth() * std::log (value (did.toRawUTF8()) / 20.0f) / std::log (1000.0f);
                if (std::abs (p.x - dx) < 10.0f) { dragDiv = j; proc.apvts.getParameter (did)->beginChangeGesture(); return; }
            }
            if (tab == 0)
            {
                float edge[5] = { 20.0f, value ("xover1"), value ("xover2"), value ("xover3"), 20000.0f };
                for (int k = 0; k < 4; ++k)
                {
                    const float x0 = r.getX() + r.getWidth() * std::log (edge[k] / 20.0f) / std::log (1000.0f), x1 = r.getX() + r.getWidth() * std::log (edge[k + 1] / 20.0f) / std::log (1000.0f);
                    if (p.x >= x0 && p.x < x1) { selectMbBand (k); return; }
                }
            }
        }
        for (int i = 0; i < 8 && refOpen; ++i)
            if (refButtonRect (i).expanded (3.0f).contains (p))
            {
                if (i == 6) { proc.resetLearn(); matchNote = "Learning the input again"; matchNoteTicks = 90; repaint(); return; }
                if (i == 7)
                {
                    matchNote = proc.matchEq() ? "EQ fitted to the reference (undo with UNDO)" : proc.reference.hasData() ? "Play the input for 3 seconds first" : "Load a reference first";
                    matchNoteTicks = 120; repaint(); return;
                }
                if (i == 0) showReferenceChooser();
                else if (i == 1) toggle ("refPlay");
                else if (i == 2) setChoice ("refAB", 0);
                else if (i == 3) setChoice ("refAB", 1);
                else if (i == 4) toggle ("refMatch");
                else toggle ("refFollow");
                repaint();
                return;
            }
        if (refOpen && refBarRect().expanded (4.0f).contains (p)) { proc.reference.seek ((p.x - refBarRect().getX()) / refBarRect().getWidth()); repaint(); return; }
    }
    for (int i = 0; i < 3; ++i)                                                              // INPUT: polarity, high-pass, mono check
        if (inputButtonRect (i).expanded (2.0f).contains (p))
        {
            if (i == 0) toggle ("trimPolarity");
            else if (i == 2) toggle ("monoCheck");
            else { const float h = value ("trimHpf"); setFloat ("trimHpf", h < 1.0f ? 20.0f : h < 25.0f ? 30.0f : h < 35.0f ? 40.0f : h < 50.0f ? 60.0f : 0.0f); }
            repaint();
            return;
        }
    for (int i = 0; i < 8; ++i)
        if (((i >= 4 && i < 6) || (step == 5 && ! autoOpen)) && buttonRect (i).expanded (3.0f).contains (p))
        {
            if (i == 6) { toggle ("limRelAuto"); return; }
            if (i == 7) { toggle ("limFreeze"); return; }
            if (i < 3) setChoice ("limChar", i);
            else if (i == 3) toggle ("limAuto");
            else if (i == 4) toggle ("delta");
            else setChoice ("ditherShape", (choice ("ditherShape") + 1) % 3);
            return;
        }
}

void MS1AudioProcessorEditor::setEqFreqGain (int band, float freq, float gain)
{
    const juce::String id = "eq" + juce::String (band + 1);
    for (auto [suffix, v] : { std::pair<const char*, float> { "Freq", freq }, { "Gain", gain } })
        if (auto* par = proc.apvts.getParameter (id + suffix)) par->setValueNotifyingHost (par->convertTo0to1 (v));
}

void MS1AudioProcessorEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (dragField >= 0)                                                                     // a field of the dynamic EQ strip
    {
        static const char* suffix[4] = { "DynThr", "DynRatio", "DynAtk", "DynRel" };
        if (auto* par = proc.apvts.getParameter ("eq" + juce::String (selBand + 1) + suffix[dragField]))
            par->setValueNotifyingHost (std::clamp (dragStartNorm + (dragStartY - e.position.y / scale()) / 160.0f, 0.0f, 1.0f));
        repaint();
        return;
    }
    if (dragDiv >= 0)                                                                       // a crossover of the multiband or the imager
    {
        const auto r = tab == 1 ? dispRect().withTrimmedRight (172.0f) : dispRect();
        const float f = 20.0f * std::pow (1000.0f, std::clamp ((e.position.x / scale() - r.getX()) / r.getWidth(), 0.0f, 1.0f));
        const juce::String id = tab == 0 ? "xover" + juce::String (dragDiv + 1) : "imgX" + juce::String (dragDiv + 1);
        float lo = 0.0f, hi = 1.0e9f;
        if (tab == 0) { if (dragDiv > 0) lo = value (("xover" + juce::String (dragDiv)).toRawUTF8()) * 1.2f; if (dragDiv < 2) hi = value (("xover" + juce::String (dragDiv + 2)).toRawUTF8()) / 1.2f; }
        else if (dragDiv == 1) lo = value ("imgX1") * 1.2f; else hi = value ("imgX2") / 1.2f;
        if (auto* par = proc.apvts.getParameter (id)) par->setValueNotifyingHost (par->convertTo0to1 (std::clamp (f, lo, hi)));
        repaint();
        return;
    }
    if (dragBand < 0) return;
    const auto p = e.position / scale();
    const auto r = plotRect();
    const float f = 20.0f * std::pow (1000.0f, std::clamp ((p.x - r.getX()) / r.getWidth(), 0.0f, 1.0f));
    const float gain = std::clamp ((r.getCentreY() - p.y) / (r.getHeight() * 0.5f) * kEqRange, -kEqRange, kEqRange);
    setEqFreqGain (dragBand, f, std::round (gain * 10.0f) / 10.0f);
    repaint();
}

void MS1AudioProcessorEditor::endEqDrag()
{
    if (dragBand < 0) return;
    const juce::String id = "eq" + juce::String (dragBand + 1);
    proc.apvts.getParameter (id + "Freq")->endChangeGesture();
    proc.apvts.getParameter (id + "Gain")->endChangeGesture();
    dragBand = -1;
}

void MS1AudioProcessorEditor::endDivDrag()
{
    if (dragDiv < 0) return;
    const juce::String id = tab == 0 ? "xover" + juce::String (dragDiv + 1) : "imgX" + juce::String (dragDiv + 1);
    proc.apvts.getParameter (id)->endChangeGesture();
    dragDiv = -1;
}

void MS1AudioProcessorEditor::mouseUp (const juce::MouseEvent&)
{
    if (dragField >= 0)
    {
        static const char* suffix[4] = { "DynThr", "DynRatio", "DynAtk", "DynRel" };
        proc.apvts.getParameter ("eq" + juce::String (selBand + 1) + suffix[dragField])->endChangeGesture();
        dragField = -1;
    }
    endEqDrag(); endDivDrag();
}

void MS1AudioProcessorEditor::mouseDoubleClick (const juce::MouseEvent& e)          // a node: back to 0 dB
{
    const auto p = e.position / scale();
    for (int i = 0; i < kChainStages; ++i)                                                  // a chain pill: bypass the stage
        if (kChainOn[i] != nullptr && chainRect (i).contains (p) && ! chainDotRect (i).contains (p)) { toggle (kChainOn[i]); repaint(); return; }
    for (int i = 0; i < 4 && step == 0 && ! autoOpen; ++i)                                  // a dynamic EQ field: back to its default
        if (eqDynFieldRect (i).contains (p))
        {
            static const char* suffix[4] = { "DynThr", "DynRatio", "DynAtk", "DynRel" };
            if (auto* par = proc.apvts.getParameter ("eq" + juce::String (selBand + 1) + suffix[i])) { par->beginChangeGesture(); par->setValueNotifyingHost (par->getDefaultValue()); par->endChangeGesture(); }
            repaint();
            return;
        }
    if (const int n = step == 0 && ! autoOpen ? nodeAt (p, 16.0f) : -1; n >= 0)
    {
        endEqDrag();
        setFloat (("eq" + juce::String (n + 1) + "Gain").toRawUTF8(), 0.0f);
        repaint();
    }
}

void MS1AudioProcessorEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)     // over the plot: the Q of the node under the mouse (else the selected band)
{
    const auto p = e.position / scale();
    if (step != 0 || autoOpen || ! plotRect().expanded (10.0f).contains (p)) return;
    const int n = nodeAt (p, 24.0f) >= 0 ? nodeAt (p, 24.0f) : selBand;
    selBand = n;
    const float dy = w.deltaY * (w.isReversed ? -1.0f : 1.0f);
    if (std::abs (dy) < 1.0e-4f) return;
    const juce::String id = "eq" + juce::String (n + 1) + "Q";
    setFloat (id.toRawUTF8(), std::clamp (value (id.toRawUTF8()) * std::exp (dy * 1.2f), 0.2f, 10.0f));
    repaint();
}

bool MS1AudioProcessorEditor::keyPressed (const juce::KeyPress& k)               // 0 .. 5: the workflow step
{
    if (autoOpen && k == juce::KeyPress::escapeKey) { if (autoReason >= 0) autoReason = -1; else setAutoOpen (false); repaint(); return true; }
    const int c = k.getTextCharacter() - '1';
    if (k.getModifiers().isAnyModifierKeyDown()) return false;
    if (c >= 0 && c < 4) { setStep (c + 1); return true; }                                   // 1 .. 4: MULTIBAND, IMAGER, COLOUR, BUS COMP
    if (c == 4 || k.getTextCharacter() == '0') { setStep (c == 4 ? 5 : 0); return true; }    // 5: LIMITER, 0: EQ
    return false;
}

//==============================================================================
// AUTO (stages 1 to 3): the measurements of the chain input and the proposal for every row (see AutoMaster.h)
void MS1AudioProcessorEditor::setAutoOpen (bool open)
{
    autoOpen = open; autoReason = -1;
    if (open) autoRefresh();
    layoutKnobs();                                                      // the knobs under the panel are hidden
    repaint();
}

void MS1AudioProcessorEditor::autoRefresh()
{
    proc.autoAn.read (autoSnap);                                        // false (the audio thread was writing for too long): keep the last one
    if (proc.autoAn.clearRequest.load()) autoSnap = {};                  // CLEAR is done at the next audio block: show it at once
    autoSnap.learning = proc.autoAn.learning.load() ? 1u : 0u;
    autoProp = ms1::autom::propose (autoSnap, proc.autoContext());
    if (! autoProp.valid) autoReason = -1;
}

void MS1AudioProcessorEditor::autoApplyNow (bool toB)
{
    const bool in = autoRowChecked (0), eq = autoRowChecked (1), mb = autoRowChecked (2), img = autoRowChecked (3), bus = autoRowChecked (4), lim = autoRowChecked (5), out = autoRowChecked (6);
    if (! (in || eq || mb || img || bus || lim || out)) return;
    const int n = toB ? proc.autoApplyToB (autoProp, in, eq, mb, img, bus, lim, out) : proc.autoApply (autoProp, in, eq, mb, img, bus, lim, out);
    matchNote = toB ? juce::String ("AUTO: slot A = before, slot B = with the proposal (") + juce::String (n) + " parameters)" : "AUTO applied: " + juce::String (n) + " parameters (UNDO reverts all of it)";
    matchNoteTicks = 150;
    setAutoOpen (false);
}

bool MS1AudioProcessorEditor::autoMouseDown (juce::Point<float> p)
{
    if (! autoRect().contains (p)) return false;
    if (autoReason >= 0) { autoReason = -1; repaint(); return true; }       // a click closes the '?' popup
    if (autoCloseRect().expanded (4.0f).contains (p)) { setAutoOpen (false); return true; }
    if (autoRunRect().contains (p))
    {
        if (proc.autoAn.learning.load()) proc.autoStop();
        else if (autoSnap.programmeSeconds < ms1::autom::kMaxProgrammeSeconds) proc.autoStart();
    }
    else if (autoClearRect().contains (p)) { proc.autoClear(); autoSnap = {}; }
    else for (int i = 0; i < 4; ++i) if (autoStyleRect (i).contains (p)) proc.autoStyle.store (i);
    for (int i = 0; i < 7; ++i)
    {
        if (autoHelpRect (i).expanded (2.0f).contains (p) && autoProp.valid && rowOf (autoProp, i).available && ! rowOf (autoProp, i).reason.empty()) autoReason = i;
        else if (autoRowRect (i).withWidth (150.0f).contains (p) && autoProp.valid && rowOf (autoProp, i).available && rowOf (autoProp, i).needed) autoOff[(size_t) i] = ! autoOff[(size_t) i];
    }
    for (int i = 0; i < 3; ++i)
        if (autoApplyRect (i).contains (p))
        {
            if (i == 2) setAutoOpen (false);
            else if (autoProp.valid) autoApplyNow (i == 1);
        }
    autoRefresh();
    repaint();
    return true;
}

void MS1AudioProcessorEditor::drawAuto (juce::Graphics& g)
{
    const auto r = autoRect();
    vibr::ui::paintPanel (g, kPal, r, "AUTO");
    const auto& s = autoSnap;
    const bool learning = autoIsLearning || s.learning != 0;
    const int secs = (int) std::floor (s.programmeSeconds);
    const bool full = s.programmeSeconds >= ms1::autom::kMaxProgrammeSeconds - 0.5f;

    // the cross
    {
        const auto c = autoCloseRect();
        g.setColour (kInk.withAlpha (0.6f));
        g.drawLine (c.getX() + 6.0f, c.getY() + 6.0f, c.getRight() - 6.0f, c.getBottom() - 6.0f, 1.6f);
        g.drawLine (c.getRight() - 6.0f, c.getY() + 6.0f, c.getX() + 6.0f, c.getBottom() - 6.0f, 1.6f);
    }
    // the state line
    {
        juce::String st;
        if (learning) st = "LEARNING  " + juce::String (secs) + " s of " + juce::String ((int) ms1::autom::kMinProgrammeSeconds) + " s minimum";
        else if (s.programmeSeconds <= 0.0f) st = "IDLE";
        else if (s.programmeSeconds < ms1::autom::kMinProgrammeSeconds) st = "STOPPED  " + juce::String (secs) + " s of " + juce::String ((int) ms1::autom::kMinProgrammeSeconds) + " s minimum";
        else st = "READY  " + juce::String (secs) + " s heard" + (full ? " (full)" : "");
        drawText (g, st, r.getX() + 16.0f, r.getY() + 38.0f, 17.0f, learning ? kBody : s.programmeSeconds >= ms1::autom::kMinProgrammeSeconds ? kBody : kInk.withAlpha (0.8f), true, 0.08f, juce::Justification::centredLeft);
        if (learning || s.programmeSeconds > 0.0f)                                                     // the progress towards the 10 s minimum
        {
            const auto bar = juce::Rectangle<float> (r.getX() + 296.0f, r.getY() + 35.0f, 84.0f, 6.0f);
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.fillRoundedRectangle (bar, 3.0f);
            g.setColour (kBody);
            g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * std::min (1.0f, s.programmeSeconds / ms1::autom::kMinProgrammeSeconds)), 3.0f);
        }
    }
    vibr::ui::paintFlatButton (g, kPal, autoRunRect(), learning ? "STOP" : "ANALYZE", learning, 14.0f, learning || ! full ? 1.0f : 0.4f);
    vibr::ui::paintFlatButton (g, kPal, autoClearRect(), "CLEAR", false, 14.0f, s.programmeSeconds > 0.0f ? 1.0f : 0.45f);
    for (int i = 0; i < 4; ++i) vibr::ui::paintFlatButton (g, kPal, autoStyleRect (i), kAutoStyleName[i], proc.autoStyle.load() == i, 13.0f, 1.0f, 9.0f, 0.08f);
    drawText (g, "STYLE (the EQ target; a loaded reference replaces it)", r.getX() + 16.0f, r.getY() + 132.0f, 13.0f, kInk.withAlpha (0.55f), true, 0.06f, juce::Justification::centredLeft);

    // the live picture: the long-term spectrum of the input and the short-term loudness distribution, as they build up
    {
        const auto plot = autoPlotRect();
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (plot.expanded (4.0f), 8.0f);
        const auto sp = plot.withWidth (236.0f).reduced (2.0f, 4.0f), hi = plot.withTrimmedLeft (252.0f).reduced (2.0f, 4.0f);
        drawText (g, "SPECTRUM", sp.getX() + 4.0f, sp.getY() + 6.0f, 13.0f, kInk.withAlpha (0.4f), true, 0.12f, juce::Justification::centredLeft);
        drawText (g, "SHORT-TERM LUFS", hi.getX() + 4.0f, hi.getY() + 6.0f, 13.0f, kInk.withAlpha (0.4f), true, 0.12f, juce::Justification::centredLeft);
        if (s.spectrumFrames > 2)
        {
            float peak = -200.0f;
            std::array<float, ms1::autom::kSpecBands> v {};
            for (int b = 0; b < ms1::autom::kSpecBands; ++b)
            {
                const float f = 20.0f * std::pow (1000.0f, ((float) b + 0.5f) / (float) ms1::autom::kSpecBands);
                v[(size_t) b] = s.spectrumDb[b] + 4.5f * std::log2 (f / 1000.0f);                    // the display's +4.5 dB per octave slope
                peak = std::max (peak, v[(size_t) b]);
            }
            juce::Path path;
            path.startNewSubPath (sp.getX(), sp.getBottom());
            for (int b = 0; b < ms1::autom::kSpecBands; ++b)
                path.lineTo (sp.getX() + sp.getWidth() * ((float) b + 0.5f) / (float) ms1::autom::kSpecBands, sp.getBottom() - sp.getHeight() * std::clamp ((v[(size_t) b] - (peak - 54.0f)) / 54.0f, 0.0f, 1.0f));
            path.lineTo (sp.getRight(), sp.getBottom());
            path.closeSubPath();
            g.setColour (kBody.withAlpha (0.35f));
            g.fillPath (path);
            g.setColour (kBody.withAlpha (0.9f));
            g.strokePath (path, juce::PathStrokeType (1.2f));
        }
        else drawText (g, learning ? "listening ..." : "no audio learned yet", sp.getCentreX(), sp.getCentreY() + 4.0f, 13.0f, kInk.withAlpha (0.35f), true, 0.1f);
        {
            uint32_t mx = 1;
            for (int b = 40; b < ms1::autom::kHistBins; ++b) mx = std::max (mx, s.shortTermHist[b]);        // -60 .. 0 LUFS
            const float bw = hi.getWidth() / 60.0f;
            g.setColour (kBody.withAlpha (0.8f));
            for (int b = 40; b < ms1::autom::kHistBins; ++b)
            {
                const float h = hi.getHeight() * (float) s.shortTermHist[b] / (float) mx;
                if (h > 0.4f) g.fillRect (hi.getX() + bw * (float) (b - 40), hi.getBottom() - h, std::max (1.0f, bw - 0.4f), h);
            }
        }
        const juce::String live = (s.momentaryNow > -100.0f ? "M " + juce::String (s.momentaryNow, 1) : juce::String ("M --")) + "   I " + (s.integratedLufs > -100.0f ? juce::String (s.integratedLufs, 1) : juce::String ("--"))
                                  + " LUFS   TP " + (s.truePeakDb > -100.0f ? juce::String (s.truePeakDb, 1) : juce::String ("--")) + "   CORR " + juce::String::formatted ("%+.2f", (double) s.correlationNow);
        drawText (g, live, plot.getX(), r.getY() + 271.0f, 13.0f, kInk.withAlpha (0.85f), true, 0.02f, juce::Justification::centredLeft);
    }

    // the proposal
    drawText (g, "PROPOSAL", r.getX() + 410.0f, r.getY() + 36.0f, 14.0f, kInk.withAlpha (0.65f), true, 0.16f, juce::Justification::centredLeft);
    if (! autoProp.valid) drawText (g, juce::String (autoProp.message), r.getX() + 530.0f, r.getY() + 36.0f, 13.0f, kInk.withAlpha (0.8f), true, 0.04f, juce::Justification::centredLeft);
    for (int i = 0; i < 7; ++i)
    {
        const auto& row = rowOf (autoProp, i);
        const auto rr = autoRowRect (i);
        const bool have = autoProp.valid && row.available;
        g.setColour (juce::Colours::black.withAlpha (0.3f));
        g.fillRoundedRectangle (rr, 8.0f);
        const auto cb = autoCheckRect (i);
        const bool checked = autoRowChecked (i), canCheck = have && row.needed;
        g.setColour (checked ? kBody : kInk.withAlpha (canCheck ? 0.5f : 0.2f));
        if (checked) g.fillRoundedRectangle (cb, 4.0f); else g.drawRoundedRectangle (cb.reduced (0.5f), 4.0f, 1.2f);
        if (checked)
        {
            juce::Path tick;
            tick.startNewSubPath (cb.getX() + 3.5f, cb.getCentreY()); tick.lineTo (cb.getCentreX() - 1.0f, cb.getBottom() - 4.5f); tick.lineTo (cb.getRight() - 3.0f, cb.getY() + 4.0f);
            g.setColour (kDark);
            g.strokePath (tick, juce::PathStrokeType (1.8f));
        }
        const bool warn = have && (i == 0 ? autoProp.clipWarning : i == 5 ? autoProp.heavyWarning : false);
        drawText (g, kAutoRowName[i], rr.getX() + 30.0f, rr.getCentreY(), 13.0f, have ? (warn ? kRed : kBody) : kInk.withAlpha (0.35f), true, 0.08f, juce::Justification::centredLeft);
        juce::String text = ! autoProp.valid ? juce::String() : juce::String::fromUTF8 (row.summary.c_str());
        float size = 13.5f;
        const float maxW = rr.getWidth() - 140.0f - 34.0f;
        while (size > 13.0f && textWidth (font (size, false).withExtraKerningFactor (0.02f), text) > maxW) size -= 0.5f;
        while (text.length() > 4 && textWidth (font (size, false).withExtraKerningFactor (0.02f), text) > maxW) text = text.dropLastCharacters (4) + "...";
        drawText (g, text, rr.getX() + 140.0f, rr.getCentreY(), size, have ? (row.needed ? kInk.withAlpha (0.95f) : kInk.withAlpha (0.55f)) : kInk.withAlpha (0.35f), false, 0.02f, juce::Justification::centredLeft);
        if (have && ! row.reason.empty())
        {
            const auto hq = autoHelpRect (i).withSizeKeepingCentre (18.0f, 18.0f);
            g.setColour (kInk.withAlpha (0.55f));
            g.drawEllipse (hq.reduced (0.5f), 1.2f);
            drawText (g, "?", hq.getCentreX(), hq.getCentreY(), 13.0f, kInk.withAlpha (0.8f), true, 0.0f);
        }
    }
    const bool can = autoProp.valid && (autoRowChecked (0) || autoRowChecked (1) || autoRowChecked (2) || autoRowChecked (3) || autoRowChecked (4) || autoRowChecked (5) || autoRowChecked (6));
    vibr::ui::paintFlatButton (g, kPal, autoApplyRect (0), "APPLY", can, 13.0f, can ? 1.0f : 0.4f);
    vibr::ui::paintFlatButton (g, kPal, autoApplyRect (1), "APPLY TO B", false, 13.0f, can ? 1.0f : 0.4f);
    vibr::ui::paintFlatButton (g, kPal, autoApplyRect (2), "CANCEL", false, 13.0f);
    drawText (g, "Nothing changes until you press APPLY.", r.getX() + 830.0f, r.getY() + 253.0f, 14.0f, kInk.withAlpha (0.8f), false, 0.02f, juce::Justification::centredLeft);
    drawText (g, "A measured starting point, not the best setting.", r.getX() + 830.0f, r.getY() + 274.0f, 14.0f, kInk.withAlpha (0.8f), false, 0.02f, juce::Justification::centredLeft);

    if (autoReason >= 0 && autoProp.valid)                                                              // the '?' popup: the reason and the numbers
    {
        const auto box = juce::Rectangle<float> (autoRowRect (0).getX() + 40.0f, autoRowRect (0).getY() - 2.0f, 650.0f, 222.0f);
        g.setColour (juce::Colour (14, 15, 17));
        g.fillRoundedRectangle (box, 12.0f);
        g.setColour (kBody.withAlpha (0.7f));
        g.drawRoundedRectangle (box.reduced (0.5f), 12.0f, 1.4f);
        drawText (g, kAutoRowName[autoReason], box.getX() + 16.0f, box.getY() + 18.0f, 14.0f, kBody, true, 0.14f, juce::Justification::centredLeft);
        g.setColour (kInk.withAlpha (0.9f));
        g.setFont (font (14.0f, false));
        g.drawFittedText (juce::String::fromUTF8 (rowOf (autoProp, autoReason).reason.c_str()), box.reduced (16.0f, 0.0f).withTrimmedTop (36.0f).withTrimmedBottom (10.0f).toNearestInt(), juce::Justification::topLeft, 11, 1.0f);
    }
}
