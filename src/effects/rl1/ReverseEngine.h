// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

// RL-1 Reverse Lab: creative reverse and time manipulation on one engine with five modes.
//   The input is recorded all the time in a ring buffer.  Two playback heads (cross-faded) play slices of it:
//   REVERSE   the last LENGTH of audio, played backwards, slice after slice
//   SCRUB     a tape head that runs at SPEED behind the live signal; RANDOM scrubs it back and forth by hand (WANDER = how fast),
//             SMOOTH is the inertia of the tape motor
//   STUTTER   captures the last LENGTH and repeats it REPEATS times, then captures again; RANDOM varies speed and length
//   GLITCH    random slices: length, position, direction, speed and drop-outs (RANDOM = amount)
//   REPEAT    freeze-and-repeat loop of the last LENGTH (JITTER smears the loop position); the FREEZE button holds the buffer
//   FREEZE    (any mode) stops recording: the slices keep playing the captured audio
// Header-only, no JUCE dependency, no allocation after prepare().
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace rl1
{
constexpr int kNumModes = 5;
enum Mode { Reverse, Scrub, Stutter, Glitch, Repeat };

inline const char* modeName (int m)
{
    static const char* n[kNumModes] = { "REVERSE", "SCRUB", "STUTTER", "GLITCH", "REPEAT" };
    return n[std::clamp (m, 0, kNumModes - 1)];
}
inline const char* chaosName (int m, int which)     // the two CHAOS knobs are renamed per mode
{
    static const char* a[kNumModes] = { "RANDOM", "SCRUB", "RANDOM", "CHAOS", "JITTER" };
    static const char* b[kNumModes] = { "REPEATS", "WANDER", "REPEATS", "REPEATS", "REPEATS" };
    return (which == 0 ? a : b)[std::clamp (m, 0, kNumModes - 1)];
}

struct Params
{
    int mode = 0;
    bool freeze = false, sync = false;                                                  // sync: LENGTH follows the host tempo
    float bpm = 120.0f;
    float length = 0.5f, speed = 0.5f, smooth = 0.4f, random = 0.3f, repeats = 0.5f;     // 0..1
};

inline float lengthMs (float v) { return 20.0f * std::pow (100.0f, std::clamp (v, 0.0f, 1.0f)); }      // 20 ms .. 2 s
// tempo sync: LENGTH picks one of these note lengths (a beat = a quarter note)
constexpr int kNumDivisions = 8;
inline const char* divisionName (int i)
{
    static const char* n[kNumDivisions] = { "1/32", "1/16", "1/8T", "1/8", "1/4T", "1/4", "1/2", "1 BAR" };
    return n[std::clamp (i, 0, kNumDivisions - 1)];
}
inline int divisionIndex (float v) { return std::clamp ((int) (v * (float) kNumDivisions), 0, kNumDivisions - 1); }
inline float divisionBeats (int i)
{
    static const float b[kNumDivisions] = { 0.125f, 0.25f, 1.0f / 3.0f, 0.5f, 2.0f / 3.0f, 1.0f, 2.0f, 4.0f };
    return b[std::clamp (i, 0, kNumDivisions - 1)];
}
inline float speedRatio (float v) { return std::pow (2.0f, (std::clamp (v, 0.0f, 1.0f) - 0.5f) * 4.0f); }   // 0.25x .. 4x, 1x in the middle

class Engine
{
public:
    void prepare (float sampleRate)
    {
        fs = sampleRate;
        size = 1; while ((float) size < 4.5f * fs) size <<= 1;
        mask = size - 1;
        for (auto& b : buf) b.assign ((size_t) size, 0.0f);
        snapSize = (int) (3.0f * fs) + 16;
        for (auto& s : snap) for (auto& c : s) c.assign ((size_t) snapSize, 0.0f);
        reset();
    }

    void reset()
    {
        for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0f);
        write = 0;
        for (auto& h : heads) h = Head();
        nextSnap = 0; rate = 1.0f; walk = walkTarget = 0.0f; walkClock = 0; duck = 1.0f; dryG = 0.7f; wetG = 0.7f; mode = 0;
        rng = 0x1234567u;
    }

    void process (float* l, float* r, int n, const Params& p, float mix, float levelGain)
    {
        const float dryT = std::cos (mix * 0.5f * 3.14159265f), wetT = std::sin (mix * 0.5f * 3.14159265f) * levelGain;
        for (int i = 0; i < n; ++i)
        {
            const bool want = p.mode == mode;                    // mode change: fade out, drop the heads, fade in
            duck = std::clamp (duck + (want ? 1.0f : -1.0f) / (0.03f * fs), 0.0f, 1.0f);
            if (! want && duck <= 0.0f) { for (auto& h : heads) h = Head(); mode = p.mode; rate = 1.0f; walk = walkTarget = 0.0f; }
            if ((i & 31) == 0) { dryG += 0.2f * (dryT - dryG); wetG += 0.2f * (wetT - wetG); }

            const float inL = l[i], inR = r[i];
            if (! p.freeze) { buf[0][(size_t) (write & mask)] = inL; buf[1][(size_t) (write & mask)] = inR; ++write; }

            float wl = 0.0f, wr = 0.0f;
            if (mode == Scrub) tickScrub (p, wl, wr); else tickSlices (p, wl, wr);

            wl = 1.5f * std::tanh (wl / 1.5f);                  // safety
            wr = 1.5f * std::tanh (wr / 1.5f);
            const float w = duck * wetG;
            l[i] = inL * dryG + wl * w;
            r[i] = inR * dryG + wr * w;
        }
    }

private:
    struct Head
    {
        bool active = false, spawned = false;
        int snapId = -1;                    // slice heads: index of the snapshot; -1: a live tape head
        double pos = 0.0;                   // live head: absolute index in the ring
        float age = 0.0f, total = 1.0f, fade = 1.0f, len = 1.0f, rate = 1.0f, gain = 1.0f;
        int dir = 1, passes = 1;
        float jitter = 0.0f;
    };

    uint32_t rnd() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }
    float rand01() { return (float) (rnd() & 0xffffff) / 16777216.0f; }

    float lengthSamples (const Params& p) const
    {
        const float ms = p.sync ? divisionBeats (divisionIndex (p.length)) * 60000.0f / std::max (20.0f, p.bpm) : lengthMs (p.length);
        return std::clamp (ms * 0.001f * fs, 64.0f, 2.0f * fs);                          // the buffer holds 2 s: slower tempos clamp
    }

    static float interp (const std::vector<float>& b, double idx, int n)
    {
        const int i = (int) idx;
        const float f = (float) (idx - (double) i);
        const float a = b[(size_t) std::clamp (i, 0, n - 1)], c = b[(size_t) std::clamp (i + 1, 0, n - 1)];
        return a + (c - a) * f;
    }

    float copySlice (int64_t start, int len, int id)       // returns the peak of the copy
    {
        len = std::min (len, snapSize - 2);
        float pk = 0.0f;
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < len; ++k)
            {
                const float v = buf[c][(size_t) ((start + k) & mask)];
                snap[id][c][(size_t) k] = v;
                pk = std::max (pk, std::fabs (v));
            }
        return pk;
    }

    //---------------------------------------------------------------- slice modes
    void startHead (Head& h, int id, float len, int dir, float r, int passes, float gain, const Params& p)
    {
        h = Head();
        h.active = true; h.snapId = id; h.len = len; h.dir = dir; h.rate = r; h.passes = passes; h.gain = gain;
        h.total = std::max (32.0f, len / r);
        h.fade = std::clamp (h.total * (0.02f + 0.48f * p.smooth), 48.0f, h.total * 0.5f);
    }

    void chooseSlice (Head& h, const Params& p)
    {
        const float Ls = lengthSamples (p);
        const float R = speedRatio (p.speed);
        const int id = nextSnap; nextSnap ^= 1;
        float len = Ls, r = R, gain = 1.0f;
        int dir = 1, passes = 1;
        int64_t start = write - (int64_t) len;
        switch (mode)
        {
            case Reverse:
                dir = -1;
                r = R * std::pow (2.0f, (rand01() - 0.5f) * 2.0f * p.random);
                passes = 1 + (int) std::lround (p.repeats * 7.0f);
                break;
            case Stutter:
                len = Ls * (1.0f - 0.5f * p.random * rand01());
                start = write - (int64_t) len;
                r = R * std::pow (2.0f, (rand01() - 0.5f) * 2.0f * p.random);
                passes = 1 + (int) std::lround (p.repeats * 15.0f);
                break;
            case Glitch:
            {
                len = std::clamp (Ls * (1.0f + (rand01() - 0.5f) * 1.5f * p.random), 64.0f, 2.5f * fs);
                const float off = rand01() * p.random * std::min (1.0f * fs, 4.0f * Ls);
                start = write - (int64_t) len - (int64_t) off;
                if (rand01() < p.random * 0.6f) dir = -1;
                if (rand01() < p.random) { static const float m[6] = { 0.5f, 2.0f, 0.75f, 1.5f, 1.0f, 4.0f }; r = R * m[rnd() % 6]; }
                if (rand01() < p.random * 0.2f) gain = 0.0f;
                passes = 1 + (int) std::lround (p.repeats * 5.0f * rand01());
                break;
            }
            case Repeat:
            default:
                passes = 1 + (int) std::lround (p.repeats * p.repeats * 60.0f);
                break;
        }
        r = std::clamp (r, 0.1f, 6.0f);
        const float level = copySlice (start, (int) len, id);
        if (level < 1.0e-4f) passes = 1;                               // an empty capture is not worth repeating
        startHead (h, id, len, dir, r, passes, gain, p);
        h.jitter = mode == Repeat ? p.random : 0.0f;
    }

    void spawnNext (const Head& cur, Head& n, const Params& p)
    {
        if (cur.passes > 1)                                           // another pass of the same slice
        {
            startHead (n, cur.snapId, cur.len, cur.dir, cur.rate, cur.passes - 1, cur.gain, p);
            n.jitter = cur.jitter;
            if (cur.jitter > 0.0f) n.total *= 1.0f + (rand01() - 0.5f) * 0.3f * cur.jitter;   // loop length smear
            n.total = std::max (32.0f, n.total);
            n.fade = std::clamp (n.fade, 48.0f, n.total * 0.5f);
        }
        else chooseSlice (n, p);
    }

    void tickSlices (const Params& p, float& wl, float& wr)
    {
        if (! heads[0].active && ! heads[1].active) chooseSlice (heads[0], p);
        for (int k = 0; k < 2; ++k)
        {
            Head& h = heads[k];
            if (! h.active) continue;
            const float env = std::min (1.0f, std::min (h.age / h.fade, (h.total - h.age) / h.fade)) * h.gain;
            const float u = h.age * h.rate;                                     // source samples into the slice
            const double idx = h.dir > 0 ? (double) u : (double) (h.len - u);
            const float e = std::max (0.0f, env);
            wl += e * interp (snap[h.snapId][0], idx, (int) h.len);
            wr += e * interp (snap[h.snapId][1], idx, (int) h.len);
            h.age += 1.0f;
            if (! h.spawned && h.age >= h.total - h.fade)
            {
                h.spawned = true;
                spawnNext (h, heads[k ^ 1], p);
            }
            if (h.age >= h.total) h.active = false;
        }
    }

    //---------------------------------------------------------------- scrub (live tape head)
    void tickScrub (const Params& p, float& wl, float& wr)
    {
        const float Ls = lengthSamples (p), minD = 512.0f;
        if (--walkClock <= 0)                                            // the hand moves the tape: a new target now and then
        {
            walkTarget = (rand01() * 2.0f - 1.0f) * 3.0f * p.random;
            walkClock = (int) ((0.03f + (1.0f - p.repeats) * 0.6f * (0.3f + rand01())) * fs);
        }
        walk += (walkTarget - walk) * (1.0f / (0.04f * fs));
        const float target = speedRatio (p.speed) + walk;
        const float inertia = 1.0f / ((0.002f + p.smooth * p.smooth * 1.2f) * fs);
        rate += (target - rate) * std::min(1.0f, inertia);

        const double hi = (double) write - (double) minD, lo = (double) write - (double) Ls;
        const float Fs = std::clamp (Ls * (0.05f + 0.4f * p.smooth), 256.0f, Ls * 0.5f);
        if (! heads[0].active && ! heads[1].active) { heads[0] = Head(); heads[0].active = true; heads[0].pos = hi; heads[0].age = 1.0e9f; }
        const float vrel = rate - (p.freeze ? 0.0f : 1.0f);
        for (int k = 0; k < 2; ++k)
        {
            Head& h = heads[k];
            if (! h.active) continue;
            float edge = 1.0f;
            if (vrel < -1.0e-4f) edge = (float) ((h.pos - lo) / Fs);
            else if (vrel > 1.0e-4f) edge = (float) ((hi - h.pos) / Fs);
            const float e = std::clamp (std::min (edge, h.age / Fs), 0.0f, 1.0f);
            const double ri = h.pos;
            const int i0 = (int) std::floor (ri);
            const float f = (float) (ri - (double) i0);
            for (int c = 0; c < 2; ++c)
            {
                const float a = buf[c][(size_t) (i0 & mask)], b = buf[c][(size_t) ((i0 + 1) & mask)];
                (c == 0 ? wl : wr) += e * (a + (b - a) * f);
            }
            h.pos += rate;
            h.age += 1.0f;
            if (! h.spawned && edge < 1.0f && h.age > Fs)                // leaving the window: a new head enters on the other side
            {
                h.spawned = true;
                Head& n = heads[k ^ 1];
                n = Head(); n.active = true; n.age = 0.0f;
                n.pos = vrel < 0.0f ? hi : lo;
            }
            if (h.pos < lo - 1.0 || h.pos > hi + 1.0 || (edge <= 0.0f && h.spawned)) h.active = false;
        }
    }

    float fs = 48000.0f;
    int size = 1, mask = 0, snapSize = 0, mode = 0, nextSnap = 0, walkClock = 0;
    int64_t write = 0;
    float rate = 1.0f, walk = 0.0f, walkTarget = 0.0f, duck = 1.0f, dryG = 0.7f, wetG = 0.7f;
    uint32_t rng = 0x1234567u;
    std::vector<float> buf[2];
    std::vector<float> snap[2][2];       // [snapshot][channel]
    Head heads[2];
};
} // namespace rl1
