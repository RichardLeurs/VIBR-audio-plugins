// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

// GR-1 Grain: granular processing of the playing.
//   The input is recorded all the time; DENSITY grains per second are cut from it, each SIZE long, read from POSITION back in time
//   (SPRAY scatters the position), at PITCH semitones (RANDOM scatters the pitch in whole semitones), backwards with the chance REVERSE,
//   panned at random within SPREAD, and shaped by the ENVELOPE (soft, sharp, percussive, swell).  FREEZE stops recording: the grains keep
//   reading the captured audio, a texture that sustains for ever.
// Header-only, no JUCE dependency, no allocation after prepare().
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

namespace gr1
{
constexpr float kPi = 3.14159265358979f;
constexpr int kNumShapes = 4, kMaxGrains = 64, kVisEvents = 512;
enum Shape { Soft, Sharp, Percussive, Swell };

inline const char* shapeName (int s)
{
    static const char* n[kNumShapes] = { "SOFT", "SHARP", "PERCUSSIVE", "SWELL" };
    return n[std::clamp (s, 0, kNumShapes - 1)];
}

inline float sizeMs (float v) { return 10.0f * std::pow (50.0f, std::clamp (v, 0.0f, 1.0f)); }        // 10 ms .. 500 ms
// tempo sync: DENSITY picks one of these note lengths between grains (a beat = a quarter note)
constexpr int kNumDivisions = 8;
inline const char* divisionName (int i)
{
    static const char* n[kNumDivisions] = { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32", "1/32T", "1/64" };
    return n[std::clamp (i, 0, kNumDivisions - 1)];
}
inline int divisionIndex (float v) { return std::clamp ((int) (v * (float) kNumDivisions), 0, kNumDivisions - 1); }
inline float divisionHz (int i, float bpm)          // grains per second when one grain starts every note length
{
    static const float beats[kNumDivisions] = { 1.0f, 0.5f, 1.0f / 3.0f, 0.25f, 1.0f / 6.0f, 0.125f, 1.0f / 12.0f, 0.0625f };
    return std::max (20.0f, bpm) / 60.0f / beats[std::clamp (i, 0, kNumDivisions - 1)];
}
inline float densityHz (float v) { return 2.0f * std::pow (40.0f, std::clamp (v, 0.0f, 1.0f)); }       // 2 .. 80 grains per second
inline float positionMs (float v) { return 2000.0f * std::clamp (v, 0.0f, 1.0f); }                      // 0 .. 2 s back in time
inline int pitchSemis (float v) { return (int) std::lround ((std::clamp (v, 0.0f, 1.0f) - 0.5f) * 24.0f); }    // -12 .. +12
inline int randomSemis (float v) { return (int) std::lround (std::clamp (v, 0.0f, 1.0f) * 12.0f); }           // 0 .. 12 (+-)

struct Params
{
    int shape = 0;
    bool freeze = false, sync = false;               // sync: DENSITY follows the host tempo (grains start on the note grid, without the timing jitter)
    float bpm = 120.0f;
    float size = 0.5f, density = 0.55f, position = 0.2f, spray = 0.3f, pitch = 0.5f, random = 0.0f, reverse = 0.0f, spread = 0.6f;   // 0..1
};

class Engine
{
public:
    // what the editor shows: the last grains that were started (a ring of events: when, pitch, length), written by the audio thread and
    // read by the message thread. The editor places each one by its age, so the cloud scrolls to the left as time passes.
    std::atomic<int64_t> evT[kVisEvents];            // start time of the grain in samples (the engine's own clock)
    std::atomic<float> evY[kVisEvents], evS[kVisEvents];   // pitch (0 .. 1, 0.5 = unchanged) and length (0 .. 1 of 0.5 s)
    std::atomic<int64_t> clockNow { 0 };             // the engine's clock at the end of the last block
    float sampleRate() const { return fs; }

    void prepare (float sampleRate)
    {
        fs = sampleRate;
        size = 1; while ((float) size < 6.0f * fs) size <<= 1;
        mask = size - 1;
        for (auto& b : buf) b.assign ((size_t) size, 0.0f);
        reset();
    }

    void reset()
    {
        for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0f);
        for (auto& g : grains) g = Grain();
        for (int i = 0; i < kVisEvents; ++i) { evT[i] = -(int64_t) 1 << 40; evY[i] = 0.5f; evS[i] = 0.0f; }
        clockNow = 0; clock = 0; evHead = 0;
        write = 0; countdown = 0.0f; dryG = 0.7f; wetG = 0.7f; rng = 0x9e3779b9u;
    }

    void process (float* l, float* r, int n, const Params& p, float mix, float levelGain)
    {
        const float dryT = std::cos (mix * 0.5f * kPi), wetT = std::sin (mix * 0.5f * kPi) * levelGain;
        const float sizeS = std::clamp (sizeMs (p.size) * 0.001f * fs, 64.0f, 0.5f * fs);
        const float dens = p.sync ? divisionHz (divisionIndex (p.density), p.bpm) : densityHz (p.density);
        const float comp = 1.2f / std::sqrt (std::max (1.0f, dens * sizeS / fs));        // keeps the level steady for any size / density
        for (int i = 0; i < n; ++i)
        {
            if ((i & 31) == 0) { dryG += 0.2f * (dryT - dryG); wetG += 0.2f * (wetT - wetG); }
            const float inL = l[i], inR = r[i];
            if (! p.freeze) { buf[0][(size_t) (write & mask)] = inL; buf[1][(size_t) (write & mask)] = inR; ++write; }

            ++clock;
            countdown -= 1.0f;
            while (countdown <= 0.0f)
            {
                spawn (p, sizeS);
                countdown += fs / dens * (p.sync ? 1.0f : 0.5f + rand01());               // a little jitter in the timing (none when synced)
            }

            float wl = 0.0f, wr = 0.0f;
            for (int k = 0; k < kMaxGrains; ++k)
            {
                Grain& g = grains[k];
                if (! g.active) continue;
                const float u = g.age / g.len;
                const float w = window (p.shape, u);
                const double ri = g.pos;
                const int i0 = (int) std::floor (ri);
                const float f = (float) (ri - (double) i0);
                const float a0 = buf[0][(size_t) (i0 & mask)], a1 = buf[0][(size_t) ((i0 + 1) & mask)];
                const float b0 = buf[1][(size_t) (i0 & mask)], b1 = buf[1][(size_t) ((i0 + 1) & mask)];
                const float mono = 0.5f * ((a0 + (a1 - a0) * f) + (b0 + (b1 - b0) * f));
                wl += mono * w * g.gl;
                wr += mono * w * g.gr;
                g.pos += g.inc;
                g.age += 1.0f;
                if (g.age >= g.len) g.active = false;
            }
            wl = 1.5f * std::tanh (wl * comp / 1.5f);
            wr = 1.5f * std::tanh (wr * comp / 1.5f);
            l[i] = inL * dryG + wl * wetG;
            r[i] = inR * dryG + wr * wetG;
        }
        clockNow.store (clock, std::memory_order_relaxed);
    }

private:
    struct Grain
    {
        bool active = false;
        double pos = 0.0;
        float inc = 1.0f, age = 0.0f, len = 1.0f, gl = 0.7f, gr = 0.7f;
    };

    uint32_t rnd() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }
    float rand01() { return (float) (rnd() & 0xffffff) / 16777216.0f; }
    float rand11() { return rand01() * 2.0f - 1.0f; }

    static float window (int shape, float u)
    {
        switch (shape)
        {
            case Sharp:      return u < 0.5f ? u * 2.0f : (1.0f - u) * 2.0f;                                   // triangle
            case Percussive: return u < 0.04f ? u / 0.04f : std::exp (-4.5f * (u - 0.04f) / 0.96f);            // fast attack, exponential decay
            case Swell:      return u < 0.9f ? std::pow (u / 0.9f, 2.0f) : (1.0f - u) / 0.1f;                  // slow rise, abrupt end
            default:         { const float s = std::sin (kPi * u); return s * s; }                              // Hann
        }
    }

    void spawn (const Params& p, float sizeS)
    {
        int slot = -1;
        for (int k = 0; k < kMaxGrains; ++k) if (! grains[k].active) { slot = k; break; }
        if (slot < 0) return;

        const int baseSt = pitchSemis (p.pitch), rndSt = randomSemis (p.random);
        const int st = std::clamp (baseSt + (rndSt > 0 ? (int) std::lround (rand11() * (float) rndSt) : 0), -24, 24);
        const float ratio = std::pow (2.0f, (float) st / 12.0f);
        const bool back = rand01() < p.reverse;

        float off = positionMs (p.position) * 0.001f * fs + p.spray * 1.0f * fs * rand11();
        const float span = sizeS * ratio;                                              // source samples one grain covers
        const float minOff = (back || p.freeze) ? span + 64.0f : std::max (64.0f, span - sizeS + 64.0f);
        off = std::clamp (off, minOff, 4.5f * fs);

        Grain& g = grains[slot];
        g.active = true;
        g.len = sizeS;
        g.age = 0.0f;
        const double base = (double) write - (double) off;
        g.pos = back ? base + (double) span : base;
        g.inc = back ? -ratio : ratio;
        const float pan = rand11() * p.spread;
        g.gl = std::cos ((pan + 1.0f) * kPi * 0.25f);
        g.gr = std::sin ((pan + 1.0f) * kPi * 0.25f);
        const int e = evHead; evHead = (evHead + 1) % kVisEvents;
        evY[e].store (0.5f + (float) st / 48.0f, std::memory_order_relaxed);
        evS[e].store (std::clamp (sizeS / (0.5f * fs), 0.0f, 1.0f), std::memory_order_relaxed);
        evT[e].store (clock, std::memory_order_relaxed);
    }

    float fs = 48000.0f, countdown = 0.0f, dryG = 0.7f, wetG = 0.7f;
    int size = 1, mask = 0, evHead = 0;
    int64_t write = 0, clock = 0;
    uint32_t rng = 0x9e3779b9u;
    std::vector<float> buf[2];
    Grain grains[kMaxGrains];
};
} // namespace gr1
