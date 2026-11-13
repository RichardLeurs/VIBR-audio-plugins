// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

// VR-1 Vibe and Rotary: three swirling modulation effects in one plugin.
//   UNI-VIBE  four all-pass stages swept (unevenly, like a lamp and photocell) by an LFO; TYPE CHORUS adds the dry sound (throbbing phase),
//             TYPE VIBRATO is the phase-shifted sound alone. RATE can follow the host tempo (SYNC).
//   ROTARY    a rotary speaker: a crossover at 800 Hz sends the highs to a fast horn and the lows to a slower drum; each pulses in level through
//             two "microphones" 90 degrees apart and has a Doppler pitch wobble. The speeds ramp slowly (horn 0.6 s, drum 2.2 s) like the real thing.
//             TYPE SLOW / FAST is the speed range, SPEED fine-tunes it.
//   RING MOD  the input multiplied by an oscillator (TYPE SINE / TRIANGLE / SQUARE): metallic, bell-like tones; DEPTH blends from a plain
//             amplitude modulation (tremolo-like) to a full ring modulation.
// MIX blends the effect with the dry sound; LEVEL is the output gain. Header-only, no JUCE dependency, no allocation after prepare().
#include <algorithm>
#include <atomic>
#include <cmath>
#include <vector>

namespace vr1
{
constexpr float kPi = 3.14159265358979f, kTwoPi = 6.28318530717959f;
constexpr int kNumModes = 3, kNumDivisions = 6;
enum Mode { UniVibe, Rotary, RingMod };

inline const char* modeName (int m)
{
    static const char* n[kNumModes] = { "UNI-VIBE", "ROTARY", "RING MOD" };
    return n[std::clamp (m, 0, kNumModes - 1)];
}
inline int variantCount (int mode) { return mode == RingMod ? 3 : 2; }
inline const char* variantTitle (int mode)
{
    static const char* n[kNumModes] = { "TYPE", "SPEED", "WAVE" };
    return n[std::clamp (mode, 0, kNumModes - 1)];
}
inline const char* variantName (int mode, int v)
{
    static const char* n[kNumModes][3] = { { "CHORUS", "VIBRATO", "" }, { "SLOW", "FAST", "" }, { "SINE", "TRIANGLE", "SQUARE" } };
    return n[std::clamp (mode, 0, kNumModes - 1)][std::clamp (v, 0, 2)];
}
inline const char* rateName (int mode)  { static const char* n[kNumModes] = { "RATE", "SPEED", "FREQ" }; return n[std::clamp (mode, 0, kNumModes - 1)]; }
inline const char* depthName (int mode) { static const char* n[kNumModes] = { "INTENSITY", "DEPTH", "DEPTH" }; return n[std::clamp (mode, 0, kNumModes - 1)]; }

// tempo sync of the UNI-VIBE rate: one LFO cycle lasts this many beats (a beat = a quarter note)
inline const char* divisionName (int i)
{
    static const char* n[kNumDivisions] = { "1 BAR", "1/2", "1/4", "1/8", "1/8T", "1/16" };
    return n[std::clamp (i, 0, kNumDivisions - 1)];
}
inline int divisionIndex (float v) { return std::clamp ((int) (v * (float) kNumDivisions), 0, kNumDivisions - 1); }
inline float divisionHz (int i, float bpm)
{
    static const float beats[kNumDivisions] = { 4.0f, 2.0f, 1.0f, 0.5f, 1.0f / 3.0f, 0.25f };
    return std::max (20.0f, bpm) / 60.0f / beats[std::clamp (i, 0, kNumDivisions - 1)];
}

inline float vibeHz (float v) { return 0.1f * std::pow (100.0f, std::clamp (v, 0.0f, 1.0f)); }            // 0.1 .. 10 Hz
inline float ringHz (float v) { return 20.0f * std::pow (250.0f, std::clamp (v, 0.0f, 1.0f)); }           // 20 Hz .. 5 kHz
inline float hornHz (float v, bool fast)                                                                    // rotary horn speed
{
    v = std::clamp (v, 0.0f, 1.0f);
    return fast ? 3.0f + v * 4.5f : 0.35f + v * 2.4f;
}

struct Params
{
    int mode = UniVibe, variant = 0;
    bool sync = false;
    float bpm = 120.0f;
    float rate = 0.55f, depth = 0.7f, mix = 1.0f;       // 0 .. 1
};

class Engine
{
public:
    // what the editor shows (written by the audio thread, read by the message thread)
    std::atomic<float> visA { 0.0f }, visB { 0.0f };     // UNI-VIBE: lamp 0..1; ROTARY: horn / drum angle (radians); RING MOD: carrier phase 0..1
    std::atomic<float> visHz { 1.0f };                   // the speed of the effect in Hz (UNI-VIBE LFO, ROTARY horn, RING MOD carrier)

    void prepare (float sampleRate)
    {
        fs = sampleRate;
        size = 1; while ((float) size < 0.012f * fs + 8.0f) size <<= 1;
        mask = size - 1;
        for (auto& b : horn) b.assign ((size_t) size, 0.0f);
        for (auto& b : drum) b.assign ((size_t) size, 0.0f);
        reset();
    }

    void reset()
    {
        for (auto& b : horn) std::fill (b.begin(), b.end(), 0.0f);
        for (auto& b : drum) std::fill (b.begin(), b.end(), 0.0f);
        w = 0; lfo = 0.0f; carrier = 0.0f; thH = thD = 0.0f; fH = fD = 0.7f;
        for (auto& c : ap) for (auto& z : c) z = 0.0f;
        lp1 = lp2 = 0.0f;
        rateS = -1.0f; mixS = levelS = -1.0f; fade = 1.0f; lastMode = -1;
    }

    void process (float* l, float* r, int n, const Params& p, float levelGain)
    {
        if (p.mode != lastMode) { if (lastMode >= 0) fade = 0.0f; lastMode = p.mode; }
        const float fadeInc = 1.0f / (0.03f * fs), sm = 1.0f - std::exp (-1.0f / (0.02f * fs));
        const bool fast = p.variant == 1;
        const float lowFc = 1.0f - std::exp (-kTwoPi * 800.0f / fs);

        // the rate the effect should run at
        float hz = 1.0f;
        if (p.mode == UniVibe) hz = p.sync ? divisionHz (divisionIndex (p.rate), p.bpm) : vibeHz (p.rate);
        else if (p.mode == Rotary) hz = hornHz (p.rate, fast);
        else hz = ringHz (p.rate);
        if (rateS < 0.0f) { rateS = hz; mixS = p.mix; levelS = levelGain; }

        const float depthU = 0.15f + 0.85f * p.depth;
        const float cH = 1.0f - std::exp (-1.0f / (0.6f * fs)), cD = 1.0f - std::exp (-1.0f / (2.2f * fs));
        const float dopH = p.depth * 0.00045f * fs, dopD = p.depth * 0.0002f * fs, ampH = p.depth * 0.65f, ampD = p.depth * 0.4f, base = 0.0007f * fs;

        for (int i = 0; i < n; ++i)
        {
            rateS += (hz - rateS) * (p.mode == Rotary ? 0.0f : sm * 0.5f);
            mixS += (p.mix - mixS) * sm; levelS += (levelGain - levelS) * sm;
            fade = std::min (1.0f, fade + fadeInc);
            const float xl = l[i], xr = r[i];
            float wl = xl, wr = xr;

            if (p.mode == UniVibe)
            {
                static const float lo[4] = { 130.0f, 250.0f, 480.0f, 900.0f }, hi[4] = { 1100.0f, 2000.0f, 3300.0f, 5200.0f };
                lfo += rateS / fs; if (lfo >= 1.0f) lfo -= 1.0f;
                const bool vibrato = p.variant == 1;
                const float in[2] = { xl, xr };
                float out[2];
                float lampL = 0.0f;
                for (int c = 0; c < 2; ++c)
                {
                    const float s0 = 0.5f + 0.5f * std::sin (kTwoPi * (lfo + (c == 0 ? 0.0f : 0.08f)));
                    const float lamp = std::pow (s0, 1.6f) * depthU;                  // the lamp / photocell is not linear
                    if (c == 0) lampL = lamp;
                    float v = in[c];
                    for (int k = 0; k < 4; ++k)
                    {
                        const float fc = lo[k] * std::pow (hi[k] / lo[k], lamp), tn = std::tan (kPi * fc / fs), a = (tn - 1.0f) / (tn + 1.0f);
                        const float y = a * v + ap[c][k];
                        ap[c][k] = v - a * y;
                        v = y;
                    }
                    out[c] = vibrato ? v : 0.5f * (in[c] + v) * 1.35f;
                }
                wl = out[0]; wr = out[1];
                if ((i & 63) == 0) visA.store (lampL, std::memory_order_relaxed);
            }
            else if (p.mode == Rotary)
            {
                const float hT = hz, dT = hz * 0.68f;
                fH += (hT - fH) * cH; fD += (dT - fD) * cD;
                thH += kTwoPi * fH / fs; if (thH > kTwoPi) thH -= kTwoPi;
                thD += kTwoPi * fD / fs; if (thD > kTwoPi) thD -= kTwoPi;
                const float m = 0.5f * (xl + xr);
                lp1 += lowFc * (m - lp1); lp2 += lowFc * (lp1 - lp2);
                const float lo = lp2, hi = m - lo;
                const float hornTap = tap (horn[0], base + dopH * (1.0f + std::sin (thH)));
                horn[0][(size_t) w] = hi;
                const float drumTap = tap (drum[0], base + dopD * (1.0f + std::sin (thD)));
                drum[0][(size_t) w] = lo;
                w = (w + 1) & mask;
                const float gHL = 1.0f - ampH * (0.5f - 0.5f * std::cos (thH)), gHR = 1.0f - ampH * (0.5f - 0.5f * std::cos (thH + 1.6f));
                const float gDL = 1.0f - ampD * (0.5f - 0.5f * std::cos (thD)), gDR = 1.0f - ampD * (0.5f - 0.5f * std::cos (thD + 1.6f));
                wl = hornTap * gHL + drumTap * gDL;
                wr = hornTap * gHR + drumTap * gDR;
                if ((i & 63) == 0) { visA.store (thH, std::memory_order_relaxed); visB.store (thD, std::memory_order_relaxed); visHz.store (fH, std::memory_order_relaxed); }
            }
            else
            {
                carrier += rateS / fs; if (carrier >= 1.0f) carrier -= 1.0f;
                const float t = carrier;
                const float car = p.variant == 0 ? std::sin (kTwoPi * t) : p.variant == 1 ? 4.0f * std::abs (t - 0.5f) - 1.0f : (t < 0.5f ? 1.0f : -1.0f);
                const float g = (1.0f - p.depth) + p.depth * car;                   // depth 0 = no effect, 1 = full ring modulation
                wl = xl * g; wr = xr * g;
                if ((i & 63) == 0) visA.store (carrier, std::memory_order_relaxed);
            }

            const float m = mixS * fade;
            l[i] = (xl + (wl - xl) * m) * levelS;
            r[i] = (xr + (wr - xr) * m) * levelS;
        }
        if (p.mode != Rotary) visHz.store (rateS, std::memory_order_relaxed);
    }

private:
    float tap (const std::vector<float>& b, float d) const
    {
        d = std::clamp (d, 1.0f, (float) mask - 3.0f);
        const float fi = std::floor (d), fr = d - fi;
        const int i0 = (w - (int) fi) & mask, i1 = (i0 - 1) & mask;
        return b[(size_t) i0] + (b[(size_t) i1] - b[(size_t) i0]) * fr;
    }

    float fs = 48000.0f, lfo = 0.0f, carrier = 0.0f, thH = 0.0f, thD = 0.0f, fH = 0.7f, fD = 0.7f, lp1 = 0.0f, lp2 = 0.0f;
    float rateS = -1.0f, mixS = 1.0f, levelS = 1.0f, fade = 1.0f;
    float ap[2][4] {};
    int size = 1, mask = 0, w = 0, lastMode = -1;
    std::vector<float> horn[1], drum[1];
};
} // namespace vr1
