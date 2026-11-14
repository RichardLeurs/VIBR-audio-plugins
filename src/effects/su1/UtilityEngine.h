// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

// SU-1 Stereo Utility: the small tools of a stereo signal in one chain.
//   CHANNELS  INVERT L / R (polarity), SWAP L / R, MONO (sum to mono), and the listening aids MID / SIDE (only the middle or only the sides).
//   WIDTH     a mid / side stereo widener: WIDTH 0 .. 200 % scales the sides (100 % = unchanged), SPREAD adds a short delayed copy of the sides
//             (a wider, airier image), BASS MONO keeps everything below the chosen frequency in the middle (off .. 300 Hz): the sides go through
//             a 24 dB per octave (Linkwitz-Riley) high-pass at that frequency.
//   PAN       POSITION -100 .. +100 and an auto-pan (RATE, DEPTH): an equal-loudness balance (the louder side stays at full level).
//   LEVEL     GAIN -60 .. +12 dB (-60 = off).
// The order is CHANNELS -> WIDTH -> PAN -> LEVEL. The engine also measures what comes out (peak L / R, correlation, a goniometer trace) for the
// editor. Header-only, no JUCE dependency, no allocation after prepare().
#include <algorithm>
#include <atomic>
#include <cmath>
#include <vector>

namespace su1
{
constexpr float kPi = 3.14159265358979f, kTwoPi = 6.28318530717959f;
constexpr int kScope = 512;                          // the goniometer trace: the last 512 stored points

inline float dbToGain (float db) { return std::exp (db * 0.11512925f); }

struct Params
{
    bool invL = false, invR = false, swap = false, mono = false;
    int solo = 0;                                    // 0 stereo, 1 only the middle, 2 only the sides
    float width = 1.0f, spread = 0.0f, bassMono = 0.0f;     // 0 .. 2, 0 .. 1, Hz (0 = off)
    float pan = 0.0f, autoRate = 1.0f, autoDepth = 0.0f;    // -1 .. 1, Hz, 0 .. 1
    float gainDb = 0.0f;                                    // -60 .. 12 (-60 = off)
};

class Engine
{
public:
    // what the editor shows (written by the audio thread, read by the message thread)
    std::atomic<float> peakL { 0.0f }, peakR { 0.0f }, corr { 1.0f };
    std::atomic<float> scopeL[kScope], scopeR[kScope];
    std::atomic<int> scopePos { 0 };

    void prepare (float sampleRate)
    {
        fs = sampleRate;
        size = 1; while ((float) size < 0.03f * fs + 8.0f) size <<= 1;
        mask = size - 1;
        line.assign ((size_t) size, 0.0f);
        reset();
    }

    void reset()
    {
        std::fill (line.begin(), line.end(), 0.0f);
        w = 0; for (auto& b : hp) b = Bq(); lfo = 0.0f; sll = srr = slr = 0.0f; pkL = pkR = 0.0f; scopeCount = 0;
        widthS = spreadS = gainS = panS = -1.0f;
        for (int i = 0; i < kScope; ++i) { scopeL[i].store (0.0f); scopeR[i].store (0.0f); }
        scopePos.store (0); peakL.store (0.0f); peakR.store (0.0f); corr.store (1.0f);
    }

    void process (float* l, float* r, int n, const Params& p, float /*unused*/ = 1.0f)
    {
        const float sm = 1.0f - std::exp (-1.0f / (0.015f * fs));
        const float gTarget = p.gainDb <= -59.9f ? 0.0f : dbToGain (p.gainDb);
        if (widthS < 0.0f) { widthS = p.width; spreadS = p.spread; gainS = gTarget; panS = p.pan; }
        const bool bass = p.bassMono > 20.0f;
        if (bass && std::abs (p.bassMono - lastBass) > 0.01f) { lastBass = p.bassMono; for (auto& b : hp) b.highpass (p.bassMono, 0.70710678f, fs); }
        const float cLeak = std::exp (-1.0f / (0.3f * fs)), pkDecay = std::exp (-1.0f / (0.4f * fs));

        for (int i = 0; i < n; ++i)
        {
            widthS += (p.width - widthS) * sm; spreadS += (p.spread - spreadS) * sm; gainS += (gTarget - gainS) * sm;

            // CHANNELS
            float a = l[i], b = r[i];
            if (p.swap) std::swap (a, b);
            if (p.invL) a = -a;
            if (p.invR) b = -b;

            // WIDTH: mid / side
            const float m = 0.5f * (a + b);
            float s = 0.5f * (a - b);
            if (bass) s = hp[1].tick (hp[0].tick (s));                                                                  // the bass stays in the middle
            s *= widthS;
            const float sd = tap (0.0006f * fs + spreadS * 0.018f * fs);
            line[(size_t) w] = s;
            w = (w + 1) & mask;
            s = s * (1.0f - 0.55f * spreadS) + sd * (0.55f * spreadS) * 1.2f;
            float ol = m + s, orr = m - s;
            if (p.mono) ol = orr = m;
            if (p.solo == 1) ol = orr = m;
            else if (p.solo == 2) ol = orr = s;

            // PAN: an equal-loudness balance plus an auto-pan
            lfo += p.autoRate / fs; if (lfo >= 1.0f) lfo -= 1.0f;
            const float pos = std::clamp (p.pan + p.autoDepth * std::sin (kTwoPi * lfo), -1.0f, 1.0f);
            panS += (pos - panS) * sm;
            const float theta = (panS + 1.0f) * kPi * 0.25f;
            ol *= std::min (1.0f, 1.4142f * std::cos (theta));
            orr *= std::min (1.0f, 1.4142f * std::sin (theta));

            // LEVEL
            ol *= gainS; orr *= gainS;
            l[i] = ol; r[i] = orr;

            // measurements
            pkL = std::max (std::abs (ol), pkL * pkDecay); pkR = std::max (std::abs (orr), pkR * pkDecay);
            sll = sll * cLeak + ol * ol * (1.0f - cLeak); srr = srr * cLeak + orr * orr * (1.0f - cLeak); slr = slr * cLeak + ol * orr * (1.0f - cLeak);
            if (++scopeCount >= 6)
            {
                scopeCount = 0;
                const int pos2 = scopePos.load (std::memory_order_relaxed);
                scopeL[pos2].store (ol, std::memory_order_relaxed); scopeR[pos2].store (orr, std::memory_order_relaxed);
                scopePos.store ((pos2 + 1) % kScope, std::memory_order_release);
            }
        }
        peakL.store (pkL, std::memory_order_relaxed); peakR.store (pkR, std::memory_order_relaxed);
        const float den = std::sqrt (sll * srr);
        corr.store (den > 1.0e-9f ? std::clamp (slr / den, -1.0f, 1.0f) : 1.0f, std::memory_order_relaxed);
    }

private:
    struct Bq       // RBJ biquad, direct form II transposed
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        float tick (float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
        void highpass (float fc, float q, float fs)
        {
            const float w0 = kTwoPi * std::min (fc, 0.45f * fs) / fs, c = std::cos (w0), al = std::sin (w0) / (2.0f * q), i = 1.0f / (1.0f + al);
            b0 = (1 + c) * 0.5f * i; b1 = -(1 + c) * i; b2 = b0; a1 = -2 * c * i; a2 = (1 - al) * i;
        }
    };

    float tap (float d) const
    {
        d = std::clamp (d, 1.0f, (float) mask - 3.0f);
        const float fi = std::floor (d), fr = d - fi;
        const int i0 = (w - (int) fi) & mask, i1 = (i0 - 1) & mask;
        return line[(size_t) i0] + (line[(size_t) i1] - line[(size_t) i0]) * fr;
    }

    Bq hp[2];
    float fs = 48000.0f, lastBass = -1.0f, lfo = 0.0f, sll = 0.0f, srr = 0.0f, slr = 0.0f, pkL = 0.0f, pkR = 0.0f;
    float widthS = -1.0f, spreadS = -1.0f, gainS = -1.0f, panS = -1.0f;
    int size = 1, mask = 0, w = 0, scopeCount = 0;
    std::vector<float> line;
};
} // namespace su1
