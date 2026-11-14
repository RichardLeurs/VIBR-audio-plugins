// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

// Phaser engine (PH-2 style): a cascade of first-order all-pass stages swept by a sine LFO, with feedback, mixed with the dry signal.
//   RATE   LFO speed (0.1 .. 10 Hz)
//   DEPTH  sweep width (in octaves round the centre)
//   RES    feedback (resonance)
//   MODE   I: 4 stages (classic), II: 8 stages (deeper notches, stronger resonance)
//   SYNC   (plugin extension, not on the hardware) one LFO cycle per host-tempo DIVISION instead of the RATE knob,
//          clamped to the same 0.1 .. 10 Hz range the knob covers
#include <JuceHeader.h>
#include <cmath>
#include "TempoSync.h"

namespace ph2
{
constexpr int kModeI = 0, kModeII = 1, kMaxStages = 8;

constexpr float kMinHz = 0.1f, kMaxHz = 10.0f;                                                  // LFO range of the RATE knob
inline float rateHz (float rate01) { return 0.1f * std::pow (100.0f, rate01); }                 // RATE 0..10 (as 0..1): 0.1 .. 10 Hz, exponential taper

// tempo-synced LFO frequency: one cycle per division at `bpm`, kept inside the knob's range (e.g. 1/32 at 180 bpm = 12 Hz -> 10 Hz)
inline float syncedHz (double bpm, int division) { return juce::jlimit (kMinHz, kMaxHz, (float) vibr::sync::hz (bpm, division)); }
inline float feedbackAmount (float res01, int mode) { return res01 * (mode == kModeII ? 0.92f : 0.85f); }

class PhaserChannel
{
public:
    void reset() { for (auto& z : zs) z = 0.0f; fb = 0.0f; }

    // lfo -1..1; centreHz / octaves set the sweep; returns the wet (all-pass chain) signal
    float process (float x, float lfo, float centreHz, float octaves, float feedback, int stages, double sr)
    {
        const float fc = juce::jlimit (30.0f, 0.45f * (float) sr, centreHz * std::exp2 (octaves * lfo));
        const float t = std::tan (juce::MathConstants<float>::pi * fc / (float) sr);
        const float a = (t - 1.0f) / (t + 1.0f);
        float y = x + std::tanh (feedback * fb);
        for (int i = 0; i < stages; ++i)
        {
            const float o = a * y + zs[(size_t) i];
            zs[(size_t) i] = y - a * o;
            y = o;
        }
        fb = y;
        return y;
    }

private:
    float zs[kMaxStages] = {};
    float fb = 0.0f;
};
} // namespace ph2
