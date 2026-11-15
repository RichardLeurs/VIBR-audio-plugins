// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
// Offline check of the ML-1 engine: every mode / type at extreme and mid settings stays finite and bounded; the VIBRATO keeps the level of a tone
// while bending its pitch (the three VR-1 modes: UNI-VIBE notches move, ROTARY speeds ramp, RING MOD makes sidebands at f_in +- f_carrier;
// every mode: bit-exact at MIX 0, no allocation, mode switches without steps); the FLANGER / PHASER notch a noise-like input (level drops); MIX 0 passes the input untouched; the sync rate follows the tempo.
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <vector>
#include "ModEngine.h"

// counts every heap allocation, so the test can show that process() allocates nothing
static std::atomic<long> gAllocs { 0 };
void* operator new (std::size_t n) { gAllocs.fetch_add (1); if (void* p = std::malloc (n ? n : 1)) return p; throw std::bad_alloc(); }
void operator delete (void* p) noexcept { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }

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

    // 6. UNI-VIBE: the stages (and with them the notch of TYPE CHORUS) move; a steady tone swings in level as the notch passes it
    {
        check (ml1::vibeStageHz (0, 1.0f) > 4.0f * ml1::vibeStageHz (0, 0.0f) && ml1::vibeStageHz (3, 1.0f) > 4.0f * ml1::vibeStageHz (3, 0.0f), "UNI-VIBE: the four stages slide up with the lamp");
        ml1::Params p; p.mode = ml1::UniVibe; p.variant = 0; p.rate = 0.35f; p.depth = 1.0f; p.mix = 1.0f;     // about 0.55 Hz
        const auto o = run (p, 700.0f, 6.0f);
        double lo = 1e9, hi = 0;
        for (int a = 24000; a + 2400 < 288000; a += 2400) { const double v = rms (o, a, a + 2400); lo = std::min (lo, v); hi = std::max (hi, v); }
        std::printf ("UNI-VIBE\n  level of a 700 Hz tone over the sweep: %.3f .. %.3f\n", lo, hi);
        check (hi > 1.5 * lo, "CHORUS type: the notch sweeps through the tone");
        p.variant = 1; const auto v = run (p, 700.0f, 6.0f);
        double vlo = 1e9, vhi = 0;
        for (int a = 24000; a + 2400 < 288000; a += 2400) { const double x = rms (v, a, a + 2400); vlo = std::min (vlo, x); vhi = std::max (vhi, x); }
        check (vhi < 1.15 * vlo + 0.01 && std::abs (vhi - 0.5 / std::sqrt (2.0)) < 0.06, "VIBRATO type: all-pass only, the level of the tone stays");
    }

    // 7. ROTARY: the horn speed ramps (0.6 s time constant) from slow to fast, the drum is slower, the highs pulse in level
    {
        ml1::Engine e; e.prepare (48000.0f);
        ml1::Params p; p.mode = ml1::Rotary; p.variant = 0; p.rate = 0.0f; p.depth = 1.0f; p.mix = 1.0f;
        std::vector<float> l (512), r (512);
        float t = 0.0f, hzAt[4] = {}; int stage = 0; bool monotonic = true; float prev = 0.0f; double sum = 0;
        std::vector<float> env;
        for (int blk = 0; blk < 600; ++blk)                                      // 6.4 s
        {
            if (blk == 100) { p.variant = 1; p.rate = 1.0f; }                    // FAST at full SPEED: horn target 7.5 Hz
            for (int i = 0; i < 512; ++i) l[(size_t) i] = r[(size_t) i] = 0.4f * std::sin (6.2831853f * 3000.0f * (t += 1.0f / 48000.0f));
            e.process (l.data(), r.data(), 512, p, 1.0f);
            for (int i = 0; i < 512; ++i) sum += l[(size_t) i] * l[(size_t) i];
            env.push_back ((float) std::sqrt (sum / 512.0)); sum = 0;
            const float hz = e.visHz.load();
            if (blk == 99) hzAt[0] = hz; if (blk == 110) hzAt[1] = hz; if (blk == 140) hzAt[2] = hz; if (blk == 599) hzAt[3] = hz;
            if (blk >= 100 && hz < prev - 1.0e-4f) monotonic = false;
            prev = hz; (void) stage;
        }
        std::printf ("ROTARY\n  horn speed: %.2f Hz (slow) -> %.2f (0.1 s later) -> %.2f (0.5 s) -> %.2f Hz (fast, settled)\n", hzAt[0], hzAt[1], hzAt[2], hzAt[3]);
        check (std::abs (hzAt[0] - 0.35f) < 0.1f, "SLOW at SPEED 0: horn runs at 0.35 Hz (still settling from its start speed)");
        check (hzAt[1] < 2.5f && hzAt[2] > hzAt[1] + 0.5f && monotonic, "the speed ramps up slowly, no jump");
        check (std::abs (hzAt[3] - 7.5f) < 0.15f, "FAST at SPEED 100 %: horn settles at 7.5 Hz");
        float emin = 1e9f, emax = 0.0f; for (size_t b = 500; b < env.size(); ++b) { emin = std::min (emin, env[b]); emax = std::max (emax, env[b]); }
        check (emax > 1.2f * emin, "the horn pulses the level of the highs");
        // the drum (below the crossover) turns at 0.68 of the horn once both have settled: compare their unwrapped angles over the last 2 s of a long run
        ml1::Engine e2; e2.prepare (48000.0f);
        ml1::Params q = p; q.variant = 1; q.rate = 1.0f;
        float pa = 0, pb = 0, da = 0, db = 0;
        for (int blk = 0; blk < 1100; ++blk)
        {
            for (int i = 0; i < 512; ++i) l[(size_t) i] = r[(size_t) i] = 0.0f;
            e2.process (l.data(), r.data(), 512, q, 1.0f);
            const float a = e2.visA.load(), b = e2.visB.load();
            if (blk > 1004) { da += std::fmod (a - pa + 6.2831853f, 6.2831853f); db += std::fmod (b - pb + 6.2831853f, 6.2831853f); }
            pa = a; pb = b;
        }
        std::printf ("  drum / horn speed after 11 s: %.2f (0.68 expected)\n", db / da);
        check (std::abs (db / da - 0.68f) < 0.04f, "the drum turns slower than the horn (0.68)");
    }

    // 8. RING MOD: a 1 kHz tone times a 300 Hz sine carrier leaves 700 Hz and 1300 Hz and no 1000 Hz
    {
        const auto tone = [] (const std::vector<float>& v, double f, int a, int b)
        {
            double re = 0, im = 0;
            for (int i = a; i < b; ++i) { re += v[(size_t) i] * std::cos (6.283185307 * f * i / 48000.0); im += v[(size_t) i] * std::sin (6.283185307 * f * i / 48000.0); }
            return 2.0 * std::sqrt (re * re + im * im) / (b - a);
        };
        ml1::Params p; p.mode = ml1::RingMod; p.variant = 0; p.depth = 1.0f; p.mix = 1.0f;
        p.rate = std::log (300.0f / 20.0f) / std::log (250.0f);
        const auto o = run (p, 1000.0f, 2.0f);
        const double lowSide = tone (o, 700.0, 48000, 96000), highSide = tone (o, 1300.0, 48000, 96000), centre = tone (o, 1000.0, 48000, 96000);
        std::printf ("RING MOD\n  700 Hz %.3f, 1000 Hz %.3f, 1300 Hz %.3f (input 0.5, carrier 300 Hz)\n", lowSide, centre, highSide);
        check (std::abs (lowSide - 0.25) < 0.02 && std::abs (highSide - 0.25) < 0.02, "sidebands at f_in -+ f_carrier, half the input each");
        check (centre < 0.02, "depth 100 %: the input tone itself is gone");
        p.depth = 0.5f; const auto h = run (p, 1000.0f, 2.0f);
        check (std::abs (tone (h, 1000.0, 48000, 96000) - 0.25) < 0.03, "depth 50 %: half of the input remains (amplitude modulation)");
        p.depth = 0.0f; const auto z = run (p, 1000.0f, 2.0f);
        check (std::abs (tone (z, 1000.0, 48000, 96000) - 0.5) < 0.01, "depth 0: no effect");
        p.depth = 1.0f; p.variant = 2; const auto sq = run (p, 1000.0f, 2.0f);
        check (tone (sq, 700.0, 48000, 96000) > 0.3 && tone (sq, 1300.0, 48000, 96000) > 0.3, "square carrier: stronger sidebands");
    }

    // 9. bit-exact at MIX 0 and LEVEL 0 dB in every mode (also with a mode change and silence), no NaN, no allocation, no steps at a mode change
    {
        ml1::Params p; p.mix = 0.0f; p.depth = 1.0f; p.rate = 0.7f;
        bool exact = true;
        for (int mode = 0; mode < ml1::kNumModes; ++mode)
            for (int variant = 0; variant < ml1::variantCount (mode); ++variant)
            {
                p.mode = mode; p.variant = variant;
                const auto o = run (p, 0.0f, 0.5f, 0.5f, true);
                const auto in = run (ml1::Params { .mix = 0.0f }, 0.0f, 0.5f, 0.5f, true);       // chorus at MIX 0 is the same noise
                for (size_t i = 0; i < o.size(); ++i) if (o[i] != in[i]) exact = false;
            }
        check (exact, "MIX 0: bit-exact input in all 7 modes and all types");

        ml1::Engine e; e.prepare (48000.0f);
        std::vector<float> l (512), r (512);
        const long before = gAllocs.load();
        bool finite = true; float maxStep = 0.0f, last = 0.0f;
        long pos = 0;
        for (int blk = 0; blk < 7 * 8 * 4; ++blk)
        {
            p.mix = 1.0f; p.mode = (blk / 16) % ml1::kNumModes; p.variant = 0; p.rate = 0.5f; p.depth = 0.8f; p.sync = (blk & 8) != 0;
            for (int i = 0; i < 512; ++i, ++pos) l[(size_t) i] = r[(size_t) i] = 0.5f * std::sin (6.2831853f * 220.0f * (float) pos / 48000.0f);
            e.process (l.data(), r.data(), 512, p, 1.0f);
            for (int i = 0; i < 512; ++i)
            {
                if (! std::isfinite (l[(size_t) i]) || ! std::isfinite (r[(size_t) i])) finite = false;
                if (pos > 20000) maxStep = std::max (maxStep, std::abs (l[(size_t) i] - last));
                last = l[(size_t) i];
            }
        }
        std::printf ("mode switches (every 16 blocks through all modes): largest sample step %.3f (a 220 Hz tone of 0.5 steps by 0.015)\n", maxStep);
        check (finite, "no NaN or infinity through all mode changes");
        check (gAllocs.load() == before, "no heap allocation in process()");
        check (maxStep < 0.1f, "mode switches are free of clicks (no large step)");
    }

    std::printf (problems ? "\n%d PROBLEMS\n" : "\nall good\n", problems);
    return problems ? 1 : 0;
}
