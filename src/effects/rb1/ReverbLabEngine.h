// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

// RB-1 Reverb Lab: the reverbs of the VIBR collection as modes of one plugin (see .claude/lab_specs.md):
//   CLASSIC    the BS-1 Reverb: 12 types (room, hall, plate, spring, swell, bloom, cloud, chorale, shimmer, magneto, nonlinear, reflections), the engine of src/common
//   SPACE      the SP-1 Space Engine: SHIMMER, INFINITE, SPECTRAL, MODULATED, ROOMS (DECAY, SIZE, DIFFUSION, TONE, MOTION, TEXTURE, FREEZE)
//   RESONATE   the SR-1 Sympathetic Resonator: up to 12 virtual strings tuned to a scale that ring along with the playing (WOOD, METAL, GLASS)
// Shared by the modes: PRE-DELAY (0 .. 250 ms), DUCK (the wet level dips while the input plays), FREEZE (SPACE holds its tail; CLASSIC and RESONATE mute the input and
// set the decay to its maximum, so the tail dies very slowly), MIX and LEVEL. Header-only, no JUCE dependency, no allocation after prepare(). No latency.
#include <algorithm>
#include <atomic>
#include <cmath>
#include <vector>
#include "../../common/ReverbEngine.h"
#include "../sp1/SpaceEngine.h"
#include "../sr1/ResonatorEngine.h"

namespace rb1
{
constexpr int kNumModes = 3, kHistory = 256;
enum Mode { Classic, Space, Resonate };

inline int clampMode (int m) { return std::clamp (m, 0, kNumModes - 1); }
inline const char* modeName (int m) { static const char* n[kNumModes] = { "CLASSIC", "SPACE", "RESONATE" }; return n[clampMode (m)]; }
inline const char* classicName (int t) { return bigsky::typeName (t); }
inline const char* classicLongName (int t)
{
    static const char* n[bigsky::kNumTypes] = { "ROOM", "HALL", "PLATE", "SPRING", "SWELL", "BLOOM", "CLOUD", "CHORALE", "SHIMMER", "MAGNETO", "NONLINEAR", "REFLECTIONS" };
    return n[std::clamp (t, 0, bigsky::kNumTypes - 1)];
}

struct Params
{
    int mode = Classic, variant = 0;                  // CLASSIC: the type 0 .. 11, SPACE: the space mode 0 .. 4, RESONATE: the texture 0 .. 2
    bool freeze = false;
    float decay = 0.5f, size = 0.6f, tone = 0.55f, motion = 0.35f;                           // 0 .. 1
    float preDelayMs = 15.0f, duck = 0.0f, mix = 0.35f;
    // CLASSIC
    float param2 = 0.5f, value = 0.5f;
    // SPACE
    float diffusion = 0.6f, texture = 0.5f;
    // RESONATE
    int root = 0, scale = 2;
    float damping = 0.4f, tension = 0.25f, fine = 0.5f, blend = 1.0f;
};

class Engine
{
public:
    std::atomic<float> histWet[kHistory];             // the level of the wet sound, dB, one entry per 20 ms
    std::atomic<int> histPos { 0 };
    std::atomic<float> duckNow { 0.0f };

    void prepare (float sampleRate)
    {
        fs = sampleRate;
        classic.prepare (fs); space.prepare (fs); res.prepare (fs);
        size_t n = 1; while ((float) n < 0.26f * fs + 8.0f) n <<= 1;
        for (auto& d : pre) d.assign (n, 0.0f);
        preMask = (int) n - 1;
        histWindow = std::max (1, (int) (0.02f * fs));
        reset();
    }

    void reset()
    {
        classic.resetState(); space.reset(); res.reset();
        for (auto& d : pre) std::fill (d.begin(), d.end(), 0.0f);
        wpos = 0; env = 0.0f; fade = 1.0f; lastMode = -1; winCount = 0; winPeak = 0.0f; preS = -1.0f;
        for (int i = 0; i < kHistory; ++i) histWet[i].store (-90.0f);
        histPos.store (0); duckNow.store (0.0f);
    }

    const sr1::Engine& resonator() const { return res; }

    void process (float* l, float* r, int n, const Params& p, float levelGain)
    {
        for (int pos = 0; pos < n; pos += kChunk) processChunk (l + pos, r + pos, std::min (kChunk, n - pos), p, levelGain);
    }

private:
    static constexpr int kChunk = 512;

    void processChunk (float* l, float* r, int n, const Params& p, float levelGain)
    {
        const int mode = clampMode (p.mode);
        if (mode != lastMode)
        {
            if (lastMode >= 0) { classic.resetState(); space.reset(); res.reset(); fade = 0.0f; }
            lastMode = mode;
        }
        float* io[2] = { l, r };
        for (int c = 0; c < 2; ++c) for (int i = 0; i < n; ++i) { if (! std::isfinite (io[c][i])) io[c][i] = 0.0f; dry[c][i] = io[c][i]; }

        // the input of the reverb: pre-delay, muted while frozen (SPACE does its own freeze)
        if (preS < 0.0f) preS = p.preDelayMs;
        const float preCoef = 1.0f - std::exp (-1.0f / (0.05f * fs));
        const bool muteIn = p.freeze && mode != Space;
        float inPeak = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            preS += (p.preDelayMs - preS) * preCoef;
            const float d = std::clamp (preS * 0.001f * fs, 0.0f, (float) preMask - 4.0f);
            const int di = (int) d; const float fr = d - (float) di;
            for (int c = 0; c < 2; ++c)
            {
                pre[c][(size_t) ((wpos + i) & preMask)] = dry[c][i];
                const float a = pre[c][(size_t) ((wpos + i - di) & preMask)], b = pre[c][(size_t) ((wpos + i - di - 1) & preMask)];
                float x = a + (b - a) * fr;
                inPeak = std::max (inPeak, std::abs (dry[c][i]));
                io[c][i] = muteIn ? 0.0f : x;
            }
        }
        wpos += n;

        // the reverb, wet only (mix 1)
        const float full = p.freeze && mode != Space ? 1.0f : p.decay;
        if (mode == Classic)
        {
            bigsky::Params q; q.type = std::clamp (p.variant, 0, bigsky::kNumTypes - 1); q.decay = full; q.predelay = 0.0f; q.mix = 1.0f; q.tone = p.tone; q.p1 = p.size; q.p2 = p.param2; q.mod = p.motion; q.value = p.value;
            classic.process (l, r, n, q);
        }
        else if (mode == Space)
        {
            sp1::Params q; q.mode = std::clamp (p.variant, 0, sp1::kNumModes - 1); q.freeze = p.freeze; q.size = p.size; q.decay = p.decay; q.diffusion = p.diffusion; q.tone = p.tone; q.motion = p.motion; q.texture = p.texture;
            space.process (l, r, n, q, 1.0f, 1.0f);
        }
        else
        {
            sr1::Params q; q.root = std::clamp (p.root, 0, 11); q.scale = std::clamp (p.scale, 0, sr1::kNumScales - 1); q.texture = std::clamp (p.variant, 0, sr1::kNumTextures - 1);
            q.strings = p.size; q.decay = full; q.damping = p.damping; q.tension = p.tension; q.motion = p.motion; q.fine = p.fine;
            res.process (l, r, n, q, std::clamp (p.blend, 0.0f, 1.0f), 1.0f);
        }

        // DUCK, MIX, LEVEL
        const float fadeInc = 1.0f / (0.03f * fs), atk = 1.0f - std::exp (-1.0f / (0.005f * fs)), rel = 1.0f - std::exp (-1.0f / (0.15f * fs));
        const float duck = std::clamp (p.duck, 0.0f, 1.0f), mixV = std::clamp (p.mix, 0.0f, 1.0f);
        for (int i = 0; i < n; ++i)
        {
            const float a = 0.5f * (std::abs (dry[0][i]) + std::abs (dry[1][i]));
            env += (a > env ? atk : rel) * (a - env);
            const float g = 1.0f - duck * std::clamp (env * 4.0f, 0.0f, 1.0f);
            fade = std::min (1.0f, fade + fadeInc);
            for (int c = 0; c < 2; ++c)
            {
                const float w = io[c][i] * g;
                winPeak = std::max (winPeak, std::abs (w));
                float o = (dry[c][i] + (w - dry[c][i]) * mixV) * levelGain;
                o = dry[c][i] * levelGain + (o - dry[c][i] * levelGain) * fade;                  // a mode change fades in from the dry sound
                io[c][i] = std::isfinite (o) ? o : 0.0f;
            }
            duckNow.store (g, std::memory_order_relaxed);
        }
        (void) inPeak;
        winCount += n;
        while (winCount >= histWindow)
        {
            const int hp = histPos.load (std::memory_order_relaxed);
            histWet[hp].store (20.0f * std::log10 (std::max (winPeak, 1.0e-5f)), std::memory_order_relaxed);
            histPos.store ((hp + 1) % kHistory, std::memory_order_release);
            winCount -= histWindow; winPeak = 0.0f;
        }
    }

    float fs = 48000.0f, env = 0.0f, fade = 1.0f, winPeak = 0.0f, preS = -1.0f;
    int lastMode = -1, histWindow = 960, winCount = 0, wpos = 0, preMask = 0;
    bigsky::Reverb classic;
    sp1::Engine space;
    sr1::Engine res;
    std::vector<float> pre[2];
    float dry[2][kChunk] {};
};
} // namespace rb1
