// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

// MS-1 AUTO, stage 2: the STYLE CURVES, the target spectrum the EQ rule of AUTO aims at (see .claude/mastering_specs.md, "AUTO").
// A table per style, in dB against frequency, in the scale of the long-term spectrum that AutoAnalyzer.h learns (the 128 log bands of the
// spectrum display, raw: a typical finished mix falls about 4.5 dB per octave above 100 Hz, which the display compensates with its +4.5 dB / oct slope).
// Only the SHAPE counts: the level is removed by the fit (the loudness is the limiter's job).
//   Neutral  flat bass shelf up to 100 Hz, then -4.5 dB per octave
//   Warm     Neutral, +1 dB around 200 Hz (a bell, one octave each side), -1 dB above 8 kHz (a shelf, one octave transition)
//   Bright   Neutral, +1.5 dB above 6 kHz (a shelf)
//   Club     Neutral, +2 dB below 80 Hz (a shelf)
// Header-only, no JUCE, pure functions.
#include <algorithm>
#include <cmath>

namespace ms1::autom
{
constexpr int kStyles = 4;
inline const char* styleName (int s) { static const char* n[kStyles] = { "Neutral", "Warm", "Bright", "Club" }; return n[std::clamp (s, 0, kStyles - 1)]; }

// the deviation of a style from Neutral: a table of (Hz, dB) points, linear in dB against log frequency, flat outside the first and last point
struct StylePoint { float hz, db; };

inline float styleDeltaDb (int style, float hz)
{
    static const StylePoint warm[]   = { { 100.0f, 0.0f }, { 200.0f, 1.0f }, { 400.0f, 0.0f }, { 5700.0f, 0.0f }, { 11300.0f, -1.0f } };
    static const StylePoint bright[] = { { 4200.0f, 0.0f }, { 8500.0f, 1.5f } };
    static const StylePoint club[]   = { { 57.0f, 2.0f }, { 113.0f, 0.0f } };
    const StylePoint* t = nullptr; int n = 0;
    switch (style)
    {
        case 1: t = warm; n = (int) (sizeof (warm) / sizeof (warm[0])); break;
        case 2: t = bright; n = (int) (sizeof (bright) / sizeof (bright[0])); break;
        case 3: t = club; n = (int) (sizeof (club) / sizeof (club[0])); break;
        default: return 0.0f;
    }
    if (hz <= t[0].hz) return t[0].db;
    if (hz >= t[n - 1].hz) return t[n - 1].db;
    for (int i = 1; i < n; ++i)
        if (hz <= t[i].hz)
        {
            const float u = std::log (hz / t[i - 1].hz) / std::log (t[i].hz / t[i - 1].hz);
            return t[i - 1].db + u * (t[i].db - t[i - 1].db);
        }
    return 0.0f;
}

// the style curve in dB (relative level) at a frequency
inline float styleCurveDb (int style, float hz)
{
    const float neutral = hz <= 100.0f ? 0.0f : -4.5f * std::log2 (hz / 100.0f);
    return neutral + styleDeltaDb (style, hz);
}

// the curve on the 128 bands of the spectrum (20 Hz .. 20 kHz, log spaced; band centres)
template <int N>
inline void styleTarget (int style, float* out)
{
    for (int b = 0; b < N; ++b) out[b] = styleCurveDb (style, 20.0f * std::pow (1000.0f, ((float) b + 0.5f) / (float) N));
}
} // namespace ms1::autom
