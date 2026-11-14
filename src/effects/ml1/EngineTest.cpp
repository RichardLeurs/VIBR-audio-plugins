// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
// Offline check of the ML-1 engine: every mode / type at extreme and mid settings stays finite and bounded; the VIBRATO keeps the level of a tone
// while bending its pitch; the FLANGER / PHASER notch a noise-like input (level drops); MIX 0 passes the input untouched; the sync rate follows the tempo.
#include <cstdio>
#include <vector>
#include "ModEngine.h"

static std::vector<float> run (ml1::Params p, float hz, float seconds, float gain = 0.5f, bool noise = false, float fs = 48000.0f)
{
    ml1::Engine e;
    e.prepare (fs);
    const int total = (int) (fs * seconds);
    std::vector<float> out ((size_t) total);
    std::vector<float> l (512), r (512);
    unsigned seed = 12345;
    for (int pos = 0; pos < total; pos += 512)
    {
        const int n = std::min (512, total - pos);
        for (int i = 0; i < n; ++i)
        {
            seed = seed * 1664525u + 1013904223u;
            const float x = noise ? gain * ((float) (seed >> 8) / 8388608.0f - 1.0f) : gain * std::sin (6.2831853f * hz * (float) (pos + i) / fs);
            l[(size_t) i] = r[(size_t) i] = x;
        }
        e.process (l.data(), r.data(), n, p, 1.0f);
        for (int i = 0; i < n; ++i) out[(size_t) (pos + i)] = l[(size_t) i];
    }
    return out;
}

static double rms (const std::vector<float>& v, int a, int b) { double s = 0; for (int i = a; i < b; ++i) s += (double) v[(size_t) i] * v[(size_t) i]; return std::sqrt (s / (b - a)); }

static int zeroCrossings (const std::vector<float>& v, int a, int b) { int c = 0; for (int i = a + 1; i < b; ++i) if ((v[(size_t) i - 1] < 0) != (v[(size_t) i] < 0)) ++c; return c; }

int main()
{
    int problems = 0;
    const auto check = [&problems] (bool ok, const char* what) { std::printf ("  %-60s %s\n", what, ok ? "ok" : "FAILED"); if (! ok) ++problems; };

    // 1. every mode / type / setting: finite and bounded (sine and noise input)
    for (int mode = 0; mode < ml1::kNumModes; ++mode)
        for (int variant = 0; variant < ml1::variantCount (mode); ++variant)
            for (int setting = 0; setting < 4; ++setting)
                for (int noise = 0; noise < 2; ++noise)
                {
                    ml1::Params p;
                    p.mode = mode; p.variant = variant;
                    p.rate = p.depth = p.extra = setting == 0 ? 0.0f : setting == 1 ? 1.0f : 0.5f;
                    p.mix = setting == 3 ? 0.5f : 1.0f;
                    p.sync = setting == 2;
                    const auto o = run (p, 440.0f, 2.0f, 0.5f, noise != 0);
                    float peak = 0; bool finite = true;
                    for (float x : o) { if (! std::isfinite (x)) finite = false; peak = std::max (peak, std::abs (x)); }
                    std::printf ("%-8s type %d setting %d %s: peak %.3f%s\n", ml1::modeName (mode), variant, setting, noise ? "noise" : "sine ", peak, finite ? "" : "  NOT FINITE");
                    if (! finite || peak > 2.0f) { ++problems; std::printf ("  problem\n"); }
                }

    // 2. VIBRATO keeps the level and bends the pitch (zero-crossing count moves around the 1 kHz average)
    {
        ml1::Params p; p.mode = ml1::Vibrato; p.rate = 0.5f; p.depth = 1.0f; p.mix = 1.0f;
        const auto o = run (p, 1000.0f, 4.0f);
        std::printf ("VIBRATO\n");
        check (std::abs (rms (o, 48000, 192000) - 0.5 / std::sqrt (2.0)) < 0.05, "keeps the level of a tone");
        int lo = 1 << 30, hi = 0;
        for (int a = 48000; a + 2400 < 192000; a += 2400) { const int c = zeroCrossings (o, a, a + 2400); lo = std::min (lo, c); hi = std::max (hi, c); }
        std::printf ("  zero crossings per 50 ms: %d .. %d (plain 1 kHz = 100)\n", lo, hi);
        check (hi - lo >= 6, "the pitch wobbles");
        p.depth = 0.0f; const auto flat = run (p, 1000.0f, 2.0f);
        check (std::abs (zeroCrossings (flat, 48000, 96000) - 2000) <= 2, "depth 0: no pitch change");
    }

    // 3. FLANGER / PHASER with a noise input: the comb / notches take energy out, feedback adds resonance
    {
        ml1::Params p; p.mode = ml1::Flanger; p.rate = 0.0f; p.depth = 0.5f; p.extra = 0.0f; p.mix = 1.0f;
        const auto in = run (p, 0.0f, 2.0f, 0.5f, true);
        std::printf ("FLANGER\n");
        const double dry = 0.5 / std::sqrt (3.0);
        check (std::abs (rms (in, 24000, 96000) - 0.7071 * dry) < 0.08 * dry, "no feedback: 0.5 * (dry + delayed) = -3 dB on noise");
        p.extra = 1.0f; const auto fb = run (p, 0.0f, 2.0f, 0.5f, true);
        check (rms (fb, 24000, 96000) > 0.7 * rms (in, 24000, 96000), "feedback stays bounded and audible");
        p.mode = ml1::Phaser; p.extra = 0.5f; const auto ph = run (p, 0.0f, 2.0f, 0.5f, true);
        std::printf ("PHASER\n");
        check (rms (ph, 24000, 96000) < 0.9 * dry && rms (ph, 24000, 96000) > 0.3 * dry, "notches take some energy out of noise");
    }

    // 4. MIX 0 passes the input; mode change is click-free-ish and silent input stays silent
    {
        ml1::Params p; p.mode = ml1::Chorus; p.mix = 0.0f;
        const auto o = run (p, 440.0f, 1.0f);
        double err = 0; for (int i = 20000; i < 40000; ++i) err = std::max (err, (double) std::abs (o[(size_t) i] - 0.5f * std::sin (6.2831853f * 440.0f * (float) i / 48000.0f)));
        std::printf ("MIX / silence\n");
        check (err < 1.0e-4, "mix 0 passes the input untouched");
        for (int mode = 0; mode < ml1::kNumModes; ++mode) { p.mode = mode; p.mix = 1.0f; const auto s = run (p, 440.0f, 1.0f, 0.0f); check (rms (s, 0, 48000) < 1.0e-6, "silence in, silence out"); }
    }

    // 5. sync: 120 bpm, 1/4 note = 2 Hz
    {
        check (std::abs (ml1::divisionHz (2, 120.0f) - 2.0f) < 1.0e-4f, "SYNC: 1/4 at 120 bpm = 2 Hz");
        check (std::abs (ml1::divisionHz (0, 120.0f) - 0.5f) < 1.0e-4f, "SYNC: 1 bar at 120 bpm = 0.5 Hz");
    }

    std::printf (problems ? "\n%d PROBLEMS\n" : "\nall good\n", problems);
    return problems ? 1 : 0;
}
