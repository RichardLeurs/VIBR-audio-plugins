// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
// Offline check of the VR-1 engine: every mode / type at extreme and mid settings stays finite and bounded; the UNI-VIBE VIBRATO keeps the level
// (all-pass) while CHORUS notches the sound; the ROTARY modulates the level at the horn speed (more with more DEPTH); the RING MOD removes the
// input frequency and adds the sum / difference frequencies; MIX 0 passes the input untouched; SYNC follows the host tempo.
#include <cstdio>
#include <vector>
#include "VibeEngine.h"

static std::vector<float> run (vr1::Params p, float hz, float seconds, float gain = 0.5f, float fs = 48000.0f)
{
    vr1::Engine e;
    e.prepare (fs);
    const int total = (int) (fs * seconds);
    std::vector<float> out ((size_t) total);
    std::vector<float> l (512), r (512);
    for (int pos = 0; pos < total; pos += 512)
    {
        const int n = std::min (512, total - pos);
        for (int i = 0; i < n; ++i) l[(size_t) i] = r[(size_t) i] = gain * std::sin (6.2831853f * hz * (float) (pos + i) / fs);
        e.process (l.data(), r.data(), n, p, 1.0f);
        for (int i = 0; i < n; ++i) out[(size_t) (pos + i)] = l[(size_t) i];
    }
    return out;
}

static double rms (const std::vector<float>& v, int a, int b) { double s = 0; for (int i = a; i < b; ++i) s += (double) v[(size_t) i] * v[(size_t) i]; return std::sqrt (s / (b - a)); }

static double power (const std::vector<float>& v, float hz, int a, int b, float fs = 48000.0f)       // Goertzel-style single bin
{
    double re = 0, im = 0;
    for (int i = a; i < b; ++i) { const double ph = 6.283185307 * hz * i / fs; re += v[(size_t) i] * std::cos (ph); im += v[(size_t) i] * std::sin (ph); }
    return std::sqrt (re * re + im * im) / (b - a);
}

int main()
{
    const float fs = 48000.0f;
    int problems = 0;

    // 1. every mode / type / setting: finite and bounded
    for (int mode = 0; mode < vr1::kNumModes; ++mode)
        for (int variant = 0; variant < vr1::variantCount (mode); ++variant)
            for (int setting = 0; setting < 4; ++setting)
            {
                vr1::Params p;
                p.mode = mode; p.variant = variant;
                p.rate = p.depth = setting == 0 ? 0.0f : setting == 1 ? 1.0f : 0.5f;
                p.mix = setting == 3 ? 0.5f : 1.0f;
                p.sync = setting == 2;
                const auto o = run (p, 440.0f, 2.0f);
                float peak = 0; bool finite = true;
                for (float x : o) { if (! std::isfinite (x)) finite = false; peak = std::max (peak, std::abs (x)); }
                std::printf ("%-9s type %d setting %d: peak %.3f%s\n", vr1::modeName (mode), variant, setting, peak, finite ? "" : "  NOT FINITE");
                if (! finite || peak > 2.5f) { ++problems; std::printf ("  problem\n"); }
            }

    // 2. UNI-VIBE: VIBRATO keeps the level, CHORUS notches it (level moves)
    {
        vr1::Params p; p.mode = vr1::UniVibe; p.rate = 0.6f; p.depth = 1.0f; p.mix = 1.0f;
        p.variant = 1; const auto vib = run (p, 1500.0f, 4.0f);
        p.variant = 0; const auto cho = run (p, 1500.0f, 4.0f);
        auto spread = [] (const std::vector<float>& v) { double lo = 1e9, hi = 0; for (int s = 48000; s + 2400 < (int) v.size(); s += 2400) { const double x = rms (v, s, s + 2400); lo = std::min (lo, x); hi = std::max (hi, x); } return hi / std::max (lo, 1e-9); };
        std::printf ("uni-vibe level spread: vibrato %.2f  chorus %.2f\n", spread (vib), spread (cho));
        if (! (spread (vib) < 1.2)) { ++problems; std::printf ("  the vibrato should keep the level (all-pass)\n"); }
        if (! (spread (cho) > 1.5)) { ++problems; std::printf ("  the chorus should notch the level\n"); }
    }

    // 3. ROTARY: level modulation, stronger with more depth
    {
        vr1::Params p; p.mode = vr1::Rotary; p.variant = 1; p.rate = 0.5f; p.mix = 1.0f;
        auto spread = [&] (float depth)
        {
            p.depth = depth; const auto o = run (p, 3000.0f, 6.0f);        // the 3 kHz tone goes to the horn
            double lo = 1e9, hi = 0; for (int s = 3 * 48000; s + 960 < (int) o.size(); s += 960) { const double x = rms (o, s, s + 960); lo = std::min (lo, x); hi = std::max (hi, x); }
            return hi / std::max (lo, 1e-9);
        };
        const double d0 = spread (0.05f), d1 = spread (1.0f);
        std::printf ("rotary level spread: depth 5%% %.2f  depth 100%% %.2f\n", d0, d1);
        if (! (d1 > 1.5 && d1 > d0 * 1.3)) { ++problems; std::printf ("  the rotary should modulate the level more with more depth\n"); }
    }

    // 4. RING MOD: 440 Hz x 400 Hz -> 40 Hz and 840 Hz, almost no 440 Hz
    {
        vr1::Params p; p.mode = vr1::RingMod; p.variant = 0; p.depth = 1.0f; p.mix = 1.0f;
        p.rate = std::log (400.0f / 20.0f) / std::log (250.0f);
        const auto o = run (p, 440.0f, 2.0f);
        const int a = 24000, b = 96000;
        const double in440 = power (o, 440.0f, a, b), sum840 = power (o, 840.0f, a, b);
        std::printf ("ring mod: 440 Hz %.4f  840 Hz %.4f\n", in440, sum840);
        if (! (sum840 > 8.0 * in440)) { ++problems; std::printf ("  the ring mod should remove the input frequency and add 840 Hz\n"); }
    }

    // 5. MIX 0 passes the input untouched
    {
        vr1::Params p; p.mode = vr1::RingMod; p.rate = 0.5f; p.depth = 1.0f; p.mix = 0.0f;
        const auto o = run (p, 440.0f, 1.0f);
        const auto ref = run (p, 440.0f, 1.0f, 0.5f);
        double err = 0; for (size_t i = 24000; i < o.size(); ++i) err = std::max (err, (double) std::abs (o[i] - 0.5f * std::sin (6.2831853f * 440.0f * (float) i / fs)));
        std::printf ("mix 0 error %.5f\n", err);
        if (err > 0.01) { ++problems; std::printf ("  mix 0 should pass the input untouched\n"); }
        (void) ref;
    }

    // 6. SYNC: 1/4 at 120 bpm = 2 Hz
    {
        const float hz = vr1::divisionHz (2, 120.0f);
        std::printf ("sync 1/4 @120 bpm: %.2f Hz (expect 2)\n", hz);
        if (std::abs (hz - 2.0f) > 0.01f) { ++problems; std::printf ("  wrong sync rate\n"); }
    }

    std::printf ("%d problems\n", problems);
    return problems == 0 ? 0 : 1;
}
