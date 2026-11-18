// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
// MS-1 editor, the displays: chain strip, spectrum and EQ, meters, loudness, gain reduction history, stage displays.
#include "PluginEditor.h"
#include "EditorLayout.h"
#include "VibrLogo.h"
#include "ui/VibrUI.h"

using namespace ms1ui;

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

