// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once
#include "VibrUI.h"
#include <atomic>

// Shared by the Labs: the knob value ring and the L / R output dB meters (first built for ML-1, then AL-1).
namespace vibr::ui
{
// a thin glowing ring in the knob's recess, from the minimum (-135 deg) to the value (prop 0..1), in the plugin's body colour
inline void paintValueArc (juce::Graphics& g, juce::Point<float> c, float knobRadius, float prop, juce::Colour body)
{
    const float r = knobRadius + 2.5f, a0 = juce::degreesToRadians (-135.0f);
    juce::Path track, lit;
    track.addCentredArc (c.x, c.y, r, r, 0.0f, a0, -a0, true);
    g.setColour (juce::Colours::white.withAlpha (0.08f)); g.strokePath (track, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (prop <= 0.002f) return;
    lit.addCentredArc (c.x, c.y, r, r, 0.0f, a0, a0 + prop * 2.0f * -a0, true);
    for (const auto& [w, al] : { std::pair { 6.0f, 0.12f }, std::pair { 4.0f, 0.22f } })      // glow
    { g.setColour (body.withAlpha (al)); g.strokePath (lit, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)); }
    g.setColour (body); g.strokePath (lit, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// audio thread: the sample peak of the output per channel since the editor last read it. Call measure() at the end of processBlock
// (and on every early return that still passes audio out).
struct PeakTap
{
    std::atomic<float> peak[2] { 0.0f, 0.0f };
    void measure (const juce::AudioBuffer<float>& b, int n) noexcept
    {
        for (int c = 0; c < 2; ++c)
        {
            const float pk = b.getNumChannels() > 0 ? b.getMagnitude (juce::jmin (c, b.getNumChannels() - 1), 0, n) : 0.0f;
            if (pk > peak[c].load (std::memory_order_relaxed)) peak[c].store (pk, std::memory_order_relaxed);
        }
    }
};

// message thread: bar (instant attack, 24 dB / s release) and peak hold (1.5 s) per channel; call update() from the 30 Hz timer
struct OutputMeter
{
    static constexpr float kLo = -48.0f;
    float bar[2] { -90.0f, -90.0f }, hold[2] { -90.0f, -90.0f }, age[2] { 0.0f, 0.0f };

    void update (PeakTap& tap, float dt = 1.0f / 30.0f) noexcept
    {
        for (int c = 0; c < 2; ++c)
        {
            const float pk = tap.peak[c].exchange (0.0f, std::memory_order_relaxed), db = pk > 1.0e-5f ? juce::Decibels::gainToDecibels (pk) : -90.0f;
            bar[c] = db > bar[c] ? db : juce::jmax (db, bar[c] - 24.0f * dt);
            if (db >= hold[c]) { hold[c] = db; age[c] = 0.0f; }
            else if ((age[c] += dt) > 1.5f) hold[c] = juce::jmax (db, hold[c] - 24.0f * dt);
        }
    }

    // the standard geometry: two 16 px bars 26 apart, 150 high, left edge x, top y (the bar rectangle of channel ch)
    static juce::Rectangle<float> barRect (float x, float y, int ch, float height = 150.0f) { return { x + (float) ch * 26.0f, y, 16.0f, height }; }

    // valueGap: distance from the bar bottom to the peak value text. L / R bars with hold ticks, the peak value under each, red above -1 dB, and the dB scale on the right. x, y = top left of the first bar
    void paint (juce::Graphics& g, float x, float y, bool on, juce::Colour body, juce::Colour ink, float height = 150.0f, float valueGap = 18.0f, float textSize = 10.0f) const
    {
        const juce::Colour red (0xffe5432c);
        const auto yOf = [&] (const juce::Rectangle<float>& b, float db) { return b.getBottom() - b.getHeight() * juce::jlimit (0.0f, 1.0f, (db - kLo) / -kLo); };
        for (int ch = 0; ch < 2; ++ch)
        {
            const auto b = barRect (x, y, ch, height);
            g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (b, 2.0f);
            const float db = on ? bar[ch] : -90.0f;
            if (db > kLo) { g.setColour (db > -1.0f ? red : body); g.fillRect (b.withTop (yOf (b, db)).reduced (2.0f, 0.0f).withBottom (b.getBottom() - 2.0f)); }
            if (on && hold[ch] > kLo) { g.setColour (hold[ch] > -1.0f ? red : ink.withAlpha (0.9f)); g.fillRect (b.getX() + 1.0f, yOf (b, hold[ch]) - 1.0f, b.getWidth() - 2.0f, 2.0f); }
            drawText (g, ch == 0 ? "L" : "R", b.getCentreX(), b.getY() - 12.0f, textSize + 1.0f, ink.withAlpha (0.85f), true, 0.1f);
            drawText (g, on && hold[ch] > -60.0f ? (hold[ch] <= -10.0f ? juce::String ((int) std::lround (hold[ch])) : juce::String (hold[ch], 1)) : juce::String ("-inf"), b.getCentreX(), b.getBottom() + valueGap, textSize, on && hold[ch] > -1.0f ? red : body, true, 0.02f);
        }
        const auto b1 = barRect (x, y, 1, height);
        for (const float t : { 0.0f, -6.0f, -12.0f, -24.0f, -48.0f })
            drawText (g, t == 0.0f ? "0" : juce::String ((int) t), b1.getRight() + 16.0f, yOf (b1, t), textSize, ink.withAlpha (0.7f), false, 0.0f);
        drawText (g, "dB", b1.getRight() + 16.0f, b1.getY() - 12.0f, textSize, ink.withAlpha (0.7f), false, 0.0f);
    }
};
}
