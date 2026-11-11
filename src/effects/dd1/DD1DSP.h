// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

// DD-1 dual delay + shimmer reverb.
//   Two identical delay engines (time free or tempo-synced, repeats, level, modulation depth + rate), each with an
//   analog-style feedback path (low-pass, DC block, soft saturation). The shimmer reverb is the Big Sky engine in
//   SHIMMER mode (octave / fifth pitch-shift in the feedback of an 8-line FDN).
// Header-only, no allocation after prepare().
#include <cmath>
#include "ReverbEngine.h"

namespace dd1
{
constexpr int kNumDivisions = 10;
inline const char* divisionName (int i)
{
    static const char* n[kNumDivisions] = { "FREE", "1/1", "1/2", "1/4.", "1/4", "1/4T", "1/8.", "1/8", "1/8T", "1/16" };
    return n[std::clamp (i, 0, kNumDivisions - 1)];
}
// length of each division in quarter-note beats (0 = free)
inline float divisionBeats (int i)
{
    static const float b[kNumDivisions] = { 0.0f, 4.0f, 2.0f, 1.5f, 1.0f, 2.0f / 3.0f, 0.75f, 0.5f, 1.0f / 3.0f, 0.25f };
    return b[std::clamp (i, 0, kNumDivisions - 1)];
}
// Delay B's selector is a ratio of Delay A's time: 1/1 = the same time as A, and B follows when A changes
inline const char* ratioName (int i)
{
    static const char* n[kNumDivisions] = { "2/1", "3/2", "1/1", "3/4", "2/3", "1/2", "3/8", "1/3", "1/4", "1/8" };
    return n[std::clamp (i, 0, kNumDivisions - 1)];
}
inline float ratioOfA (int i)
{
    static const float r[kNumDivisions] = { 2.0f, 1.5f, 1.0f, 0.75f, 2.0f / 3.0f, 0.5f, 0.375f, 1.0f / 3.0f, 0.25f, 0.125f };
    return r[std::clamp (i, 0, kNumDivisions - 1)];
}
inline float freeTimeMs (float knob01) { return 20.0f * std::pow (100.0f, knob01); }     // 20 ms .. 2 s
inline float modRateHz (float knob01) { return 0.1f * std::pow (60.0f, knob01); }          // 0.1 .. 6 Hz
inline float lowCutHz (float knob01) { return 20.0f * std::pow (40.0f, knob01); }          // 20 .. 800 Hz
inline float hiCutHz (float knob01) { return 700.0f * std::pow (24.0f, knob01); }          // 0.7 .. 16.8 kHz
// Boost / drive in front of everything: tightening high-pass, tanh push with a little asymmetry, tone low-pass, level.
inline float driveGain (float knob01) { return std::pow (10.0f, knob01 * 24.0f / 20.0f); }                // 0 .. +24 dB
inline float boostToneHz (float knob01) { return 1500.0f * std::pow (8.0f, knob01); }                      // 1.5 .. 12 kHz
inline float boostLevelGain (float knob01) { return std::pow (10.0f, (knob01 - 0.5f) * 2.0f * 12.0f / 20.0f); }   // +-12 dB, centre = 0 dB
inline float chorusRateHz (float knob01) { return 0.1f * std::pow (50.0f, knob01); }                    // 0.1 .. 5 Hz

// Stereo chorus: a short modulated delay per channel, the two LFOs a quarter turn apart for width.
class Chorus
{
public:
    void prepare (float sampleRate)
    {
        fs = sampleRate;
        for (auto& l : line) { l.init ((int) (0.05f * fs) + 16); l.clear(); }
        ph = 0.0f;
    }
    void clear() { for (auto& l : line) l.clear(); }
    // wet signal of the chorus voices (not mixed with the input); depth 0..1, rate in Hz
    void process (float inL, float inR, float depth, float rate, float& wetL, float& wetR) noexcept
    {
        ph += 2.0f * bigsky::kPi * rate / fs;
        if (ph > 2.0f * bigsky::kPi) ph -= 2.0f * bigsky::kPi;
        const float centre = 0.0095f * fs, swing = (0.0010f + 0.0035f * depth) * fs;     // 9.5 ms +- 1..4.5 ms
        line[0].push (inL);
        line[1].push (inR);
        wetL = line[0].read (centre + swing * std::sin (ph) - 1.0f);
        wetR = line[1].read (centre + swing * std::sin (ph + 0.5f * bigsky::kPi) - 1.0f);
    }
private:
    float fs = 48000.0f, ph = 0.0f;
    bigsky::Delay line[2];
};

inline float modDepthMs (float knob01) { return 6.0f * knob01; }
constexpr float kMaxDelaySeconds = 6.0f;

struct Boost
{
    float hpIn = 0.0f, hpOut = 0.0f, lp = 0.0f;
    float process (float x, float gain, float hpA, float lpA, float outGain) noexcept
    {
        const float hp = hpA * (hpOut + x - hpIn);                    // ~90 Hz high-pass keeps the push tight
        hpIn = x; hpOut = hp;
        const float y = std::tanh (gain * hp + 0.12f) - 0.11943f;     // slight asymmetry = some even harmonics
        lp += lpA * (y - lp);
        return lp * outGain;
    }
};

class Delay
{
public:
    void prepare (float sampleRate, float startPhase)
    {
        fs = sampleRate;
        line.init ((int) (kMaxDelaySeconds * fs) + 16);
        line2.init ((int) (kMaxDelaySeconds * fs) + 16);
        ph0 = startPhase;
        clear();
    }
    void clear() { line.clear(); line2.clear(); for (auto& f : fb) f = {}; ph = ph0; timeS = -1.0f; }

    // in: input sample. Mono mode: outL = outR = the echo. Ping-pong: the echoes alternate, first one on the L line
    // (outL), second on the R line (outR), and so on. repeats 0..1, mod depth in samples.
    void process (float in, float timeSamples, float repeats, float modDepthSamples, float modRate, bool pingPong,
                  float& outL, float& outR) noexcept
    {
        if (timeS < 0.0f) timeS = timeSamples;
        timeS += 0.0006f * (timeSamples - timeS);                      // tape-like glide when the time changes
        ph += 2.0f * bigsky::kPi * modRate / fs;
        if (ph > 2.0f * bigsky::kPi) ph -= 2.0f * bigsky::kPi;
        const float d = std::clamp (timeS + modDepthSamples * std::sin (ph), 4.0f, kMaxDelaySeconds * fs) - 1.0f;
        const float y1 = line.read (d);

        if (! pingPong)
        {
            line.push (in + std::tanh (fb[0].run (y1, lpA, hpA) * repeats * 1.02f));
            line2.push (0.0f);
            outL = outR = y1;
            return;
        }
        const float y2 = line2.read (d);
        line.push (in + std::tanh (fb[0].run (y2, lpA, hpA) * repeats * 1.02f));      // R feeds back into L ...
        line2.push (std::tanh (fb[1].run (y1, lpA, hpA) * repeats * 1.02f));          // ... and L into R
        outL = y1; outR = y2;
    }

    // repeat filter (in the feedback path, so every repeat is filtered again): low cut and hi cut, Hz
    void setFilter (float lowHz, float highHz) { hpA = bigsky::coef (lowHz, fs); lpA = 1.0f - bigsky::coef (highHz, fs); }

private:
    struct Feedback      // darker with every repeat, DC blocked
    {
        float lp = 0.0f, lp2 = 0.0f, hpIn = 0.0f, hpOut = 0.0f;
        float run (float y, float lc, float hc) noexcept
        {
            lp += lc * (y - lp);                     // hi cut: 2-pole low-pass
            lp2 += lc * (lp - lp2);
            const float hp = hc * (hpOut + lp2 - hpIn);   // low cut: 1-pole high-pass (also blocks DC)
            hpIn = lp2; hpOut = hp;
            return hp;
        }
    };
    float fs = 48000.0f, ph0 = 0.0f, ph = 0.0f, timeS = -1.0f, lpA = 0.5f, hpA = 0.995f;
    Feedback fb[2];
    bigsky::Delay line, line2;
};
} // namespace dd1
