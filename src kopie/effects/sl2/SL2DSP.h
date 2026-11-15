// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

// Slicer engine (SL-2 style): a rhythm pattern chops the input like a gate / tremolo, locked to the tempo.
//   Patterns: 11 one-bar patterns of 16 steps (16th notes), chosen by VARIATION.
//   Modes:    SINGLE    the pattern gates the signal
//             DUAL      output A plays the pattern, output B plays the gaps (ping-pong chop)
//             TREMOLO   the pattern as a smooth volume swell (never fully silent)
//             SFX       the gate also sweeps a low-pass filter
//             HARMONIC  low band follows the pattern, high band the gaps
//   TEMPO     40 .. 240 bpm (or the host tempo and position)   ATTACK  fade-in of each slice   DUTY  slice length
#include <JuceHeader.h>
#include <cmath>

namespace sl2
{
constexpr int kNumPatterns = 11, kSteps = 16;
enum Mode { Single = 0, Dual, Tremolo, Sfx, Harmonic, kNumModes };

inline const char* patternString (int index)
{
    static const char* const p[kNumPatterns] = {
        "x...x...x...x...",   // 1  quarter notes
        "x.x.x.x.x.x.x.x.",   // 2  eighth notes
        "xxxxxxxxxxxxxxxx",   // 3  sixteenth notes
        "x..x..x.x..x..x.",   // 4  3-3-2-3-3-2
        "x.xx.xx.x.xx.xx.",   // 5  broken eighths
        "xx..xx..xx..xx..",   // 6  pairs
        "x.x..x.x.x..x.x.",   // 7  syncopated
        "xxx.xxx.xxx.xxx.",   // 8  triplet feel gaps
        "x..xx..xx..xx..x",   // 9  push
        ".x.x.x.x.x.x.x.x",   // 10 off-beats
        "xx.xx.x.xx.xx.x."    // 11 shuffle
    };
    return p[juce::jlimit (0, kNumPatterns - 1, index)];
}

inline bool stepOn (int pattern, int step) { return patternString (pattern)[step & (kSteps - 1)] == 'x'; }

inline float bpmFromKnob (float tempo01) { return 40.0f + 200.0f * tempo01; }
inline float attackMs (float attack01)   { return 0.5f * std::pow (100.0f, attack01); }      // 0.5 .. 50 ms
inline float dutyFromKnob (float duty01) { return 0.1f + 0.9f * duty01; }                     // 10 % .. 100 % of a step

// one-pole smoothing coefficient for a time constant in milliseconds
inline float coefMs (float ms, float sampleRate) { return 1.0f - std::exp (-1.0f / (0.001f * juce::jmax (0.05f, ms) * sampleRate)); }

// state-variable low-pass (Chamberlin), used by the SFX mode
struct Svf
{
    float low = 0.0f, band = 0.0f;
    float process (float x, float cutoffHz, float sampleRate)
    {
        const float f = 2.0f * std::sin (juce::MathConstants<float>::pi * juce::jmin (cutoffHz, 0.2f * sampleRate) / sampleRate);
        low += f * band;
        const float high = x - low - 0.45f * band;
        band += f * high;
        return low;
    }
    void reset() { low = band = 0.0f; }
};
} // namespace sl2
