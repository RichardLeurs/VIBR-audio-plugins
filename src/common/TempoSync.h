// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include <JuceHeader.h>

// Shared tempo-sync helper for the effects that follow the host tempo (see .claude/automation.md, section 4).
// A plugin adds two parameters: "sync" (bool, default off: the knob works as before) and "division" (choice, names from
// vibr::sync::divisionNames()). With sync on, the time of a delay or the period of an LFO comes from the host tempo and the division.
// The host tempo is read once per block with vibr::sync::hostBpm(); with no host (standalone) it falls back to the given tempo.
namespace vibr::sync
{
constexpr int kNumDivisions = 15;

inline juce::StringArray divisionNames()
{
    return { "2 BARS", "1 BAR", "1/2", "1/4", "1/8", "1/16", "1/32", "1/2 DOT", "1/4 DOT", "1/8 DOT", "1/16 DOT", "1/2 TRIP", "1/4 TRIP", "1/8 TRIP", "1/16 TRIP" };
}

// length of a division in quarter notes (beats)
inline double beats (int division)
{
    static const double b[kNumDivisions] = { 8.0, 4.0, 2.0, 1.0, 0.5, 0.25, 0.125, 3.0, 1.5, 0.75, 0.375, 4.0 / 3.0, 2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0 };
    return b[juce::jlimit (0, kNumDivisions - 1, division)];
}

inline double bpmClamp (double bpm) { return juce::jlimit (20.0, 400.0, bpm); }

// time of one division in milliseconds / seconds, and the frequency of an LFO that completes one cycle per division
inline double ms (double bpm, int division)    { return 60000.0 / bpmClamp (bpm) * beats (division); }
inline double seconds (double bpm, int division) { return ms (bpm, division) * 0.001; }
inline double hz (double bpm, int division)    { return 1000.0 / ms (bpm, division); }

// the host tempo (or `fallback` when the host gives none, e.g. the standalone app); call from processBlock
inline double hostBpm (juce::AudioProcessor& p, double fallback = 120.0)
{
    if (auto* ph = p.getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) return bpmClamp (*b);
    return bpmClamp (fallback);
}
} // namespace vibr::sync
