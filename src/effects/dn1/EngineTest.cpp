// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
// Offline check of the DN-1 engine: SHAPER equals the DY-1, SUSTAIN the CS-3 and PEAK the LM-2 (their engines are run next to it and the outputs compared,
// after the constant latency); VCA settles to the right gain reduction; OPTO releases slower after a long reduction; TRUE PEAK holds its ceiling; the latency is
// the same in every mode; MIX 0 and the stages off pass the input unchanged (delayed); every setting stays finite; AUTO LEVEL keeps the level.
#include <cstdio>
#include <random>
#include <vector>
#include "DynamicsEngine.h"
#include "../cs3/CS3DSP.h"
#include "../lm2/LM2DSP.h"

namespace
{
int problems = 0;
void check (bool ok, const char* what) { std::printf ("  %-92s %s\n", what, ok ? "ok" : "FAILED"); if (! ok) ++problems; }
float fs = 48000.0f;

std::vector<float> noise (int n, float amp, unsigned seed = 1)
{
    std::mt19937 rng (seed); std::uniform_real_distribution<float> u (-1.0f, 1.0f);
    std::vector<float> v ((size_t) n); for (auto& x : v) x = amp * u (rng);
    return v;
}
std::vector<float> sine (int n, float hz, float amp) { std::vector<float> v ((size_t) n); for (int i = 0; i < n; ++i) v[(size_t) i] = amp * std::sin (6.2831853f * hz * (float) i / fs); return v; }
std::vector<float> run (const dn1::Params& p, const std::vector<float>& inL, const std::vector<float>& inR, float level = 1.0f, int* latency = nullptr, std::vector<float>* outR = nullptr)
{
    dn1::Engine e; e.prepare (fs);
    if (latency) *latency = e.latency();
    auto l = inL, r = inR;
    for (size_t pos = 0; pos < l.size(); pos += 480) { const int n = (int) std::min<size_t> (480, l.size() - pos); e.process (l.data() + pos, r.data() + pos, n, p, level); }
    if (outR) *outR = r;
    return l;
}
double rms (const std::vector<float>& v, int a, int b) { double s = 0; for (int i = a; i < b; ++i) s += (double) v[(size_t) i] * v[(size_t) i]; return std::sqrt (s / (b - a)); }
double diffDb (const std::vector<float>& a, const std::vector<float>& b, int from, int shiftA)       // a is delayed by shiftA samples against b
{
    double e = 0, r = 0; for (size_t i = (size_t) from; i + (size_t) shiftA < a.size() && i < b.size(); ++i) { const double d = (double) a[i + (size_t) shiftA] - b[i]; e += d * d; r += (double) b[i] * b[i]; }
    return 10.0 * std::log10 (std::max (1e-30, e / std::max (1e-30, r)));
}
}

int main()
{
    int lat = 0; { dn1::Engine e; e.prepare (fs); lat = e.latency(); }
    std::printf ("DN-1 engine, latency %d samples\n", lat);
    const auto in = noise ((int) (3.0f * fs), 0.3f, 3);
    auto music = in; for (size_t i = 0; i < music.size(); ++i) music[i] *= 0.3f + 0.7f * std::abs (std::sin (6.2831853f * 1.5f * (float) i / fs));      // a level that moves

    std::printf ("parity with the source plugins\n");
    {
        dn1::Params p; p.mode = dn1::Shaper; p.shaper.gateOn = true; p.shaper.expOn = true; p.shaper.transOn = true; p.shaper.gateThresh = -30.0f; p.shaper.expThresh = -22.0f; p.shaper.expRatio = 3.0f; p.shaper.attack = 40.0f; p.shaper.sustain = -30.0f;
        const auto o = run (p, music, music);
        dy1::Engine d; d.prepare (fs); auto l = music, r = music; for (size_t pos = 0; pos < l.size(); pos += 480) { const int n = (int) std::min<size_t> (480, l.size() - pos); d.process (l.data() + pos, r.data() + pos, n, p.shaper, 1.0f); }
        const double e = diffDb (o, l, lat + 100, lat);
        std::printf ("  SHAPER against the DY-1: %.1f dB\n", e);
        check (e < -80.0, "SHAPER equals the DY-1 (gate, expander and transient shaper on)");
    }
    {
        dn1::Params p; p.mode = dn1::Compressor; p.variant = dn1::Sustain; p.sustain = 0.6f; p.attack01 = 0.4f; p.tone = 0.5f; p.link = 1.0f;
        const auto o = run (p, music, music);
        // the CS-3 loop, as in its processor (level knob 6 = unity, tone 5 = flat)
        cs3::Shelf low[2], high[2]; for (int c = 0; c < 2; ++c) { low[c].set (fs, 350.0f, 0.0f, false); high[c].set (fs, 2500.0f, 0.0f, true); }
        float envDb = -120.0f; std::vector<float> ref (music.size());
        for (size_t i = 0; i < music.size(); ++i)
        {
            const float inDb = 20.0f * std::log10 (std::fabs (music[i]) + 1.0e-6f);
            const float aC = std::exp (-1.0f / (0.001f * cs3::attackMs (0.4f) * fs)), rC = std::exp (-1.0f / (0.001f * cs3::releaseMs (0.6f) * fs)), coef = inDb > envDb ? aC : rC;
            envDb = coef * envDb + (1.0f - coef) * inDb;
            const float thr = cs3::thresholdDb (0.6f), ratio = cs3::ratioFor (0.6f), gr = cs3::gainReductionDb (envDb, thr, ratio);
            float y = high[0].process (low[0].process (music[i] * cs3::dbToGain (gr + cs3::makeupDb (thr, ratio))));
            ref[i] = std::tanh (y * 0.9f) / 0.9f;
        }
        const double e = diffDb (o, ref, lat + 100, lat);
        std::printf ("  SUSTAIN against the CS-3: %.1f dB\n", e);
        check (e < -80.0, "SUSTAIN equals the CS-3");
    }
    {
        dn1::Params p; p.mode = dn1::Limiter; p.variant = dn1::Peak; p.limThresh = lm2::thresholdDb (0.4f); p.limRelease = lm2::releaseMs (0.5f); p.limCeiling = 0.0f; p.tone = 0.5f; p.link = 1.0f;
        const auto o = run (p, music, music);
        lm2::Shelf low, high; low.set (fs, 350.0f, 0.0f, false); high.set (fs, 2500.0f, 0.0f, true);
        float envDb = -120.0f; std::vector<float> ref (music.size());
        for (size_t i = 0; i < music.size(); ++i)
        {
            const float inDb = 20.0f * std::log10 (std::fabs (music[i]) + 1.0e-6f);
            const float aC = std::exp (-1.0f / (0.0002f * fs)), rC = std::exp (-1.0f / (0.001f * lm2::releaseMs (0.5f) * fs)), coef = inDb > envDb ? aC : rC;
            envDb = coef * envDb + (1.0f - coef) * inDb;
            const float thr = lm2::thresholdDb (0.4f), gr = lm2::gainReductionDb (envDb, thr);
            ref[i] = std::tanh (high.process (low.process (music[i] * lm2::dbToGain (gr + lm2::makeupDb (thr)))) * 0.9f) / 0.9f;
        }
        const double e = diffDb (o, ref, lat + 100, lat);
        std::printf ("  PEAK against the LM-2: %.1f dB\n", e);
        check (e < -80.0, "PEAK equals the LM-2");
    }

    std::printf ("compressor, limiter, latency\n");
    {
        const auto tone = sine ((int) (3.0f * fs), 1000.0f, 0.316f);                      // -10 dBFS peak
        dn1::Params p; p.mode = dn1::Compressor; p.variant = dn1::Vca; p.compThresh = -20.0f; p.compRatio = 4.0f; p.compAttackMs = 1.0f; p.compReleaseMs = 100.0f; p.compMakeup = 0.0f;
        const auto o = run (p, tone, tone);
        const double gr = 20.0 * std::log10 (rms (o, (int) (2.0f * fs), (int) (3.0f * fs)) / rms (tone, (int) (2.0f * fs), (int) (3.0f * fs)));
        std::printf ("  VCA: 10 dB over a -20 dB threshold at 4:1 -> %.2f dB (expected -7.5)\n", gr);
        check (std::abs (gr + 7.5) < 1.0, "VCA settles to the gain reduction of its ratio (-7.5 dB)");
        p.compMakeup = 6.0f; const auto o2 = run (p, tone, tone);
        check (std::abs (20.0 * std::log10 (rms (o2, (int) (2.0f * fs), (int) (3.0f * fs)) / rms (o, (int) (2.0f * fs), (int) (3.0f * fs))) - 6.0) < 0.1, "MAKE-UP adds its gain");
        p.compMakeup = 0.0f; p.compMix = 0.0f; const auto dry = run (p, tone, tone);
        check (diffDb (dry, tone, 2000, lat) < -100.0, "MIX 0 (parallel) is the dry sound");

        // OPTO: a long loud passage is released more slowly than a short one
        const auto recovery = [&] (dn1::Params q, float loudSec)
        {
            std::vector<float> sig ((int) (4.0f * fs)); const int loudN = (int) (loudSec * fs);
            for (int i = 0; i < (int) sig.size(); ++i) sig[(size_t) i] = (i < loudN ? 0.8f : 0.01f) * std::sin (6.2831853f * 1000.0f * (float) i / fs);
            const auto out = run (q, sig, sig);
            const int a = loudN + lat + (int) (0.05f * fs), b = (int) sig.size() - 100;
            const double target = rms (sig, loudN + 2000, (int) sig.size() - 100);                // the input level of the quiet part
            for (int i = a; i + 960 < b; i += 480) if (rms (out, i, i + 960) > 0.89 * target) return (double) (i - a) / fs;
            return 4.0;
        };
        dn1::Params q; q.mode = dn1::Compressor; q.variant = dn1::Opto; q.compThresh = -30.0f; q.compRatio = 6.0f; q.compAttackMs = 5.0f; q.compReleaseMs = 150.0f;
        const double shortT = recovery (q, 0.05f), longT = recovery (q, 1.5f);
        std::printf ("  OPTO recovery: after 50 ms %.3f s, after 1.5 s %.3f s\n", shortT, longT);
        check (longT > 1.5 * shortT, "OPTO: the release is slower after a long reduction (program dependent)");
    }
    for (float sr : { 44100.0f, 96000.0f })
    {
        fs = sr;
        const auto loud = noise ((int) (2.0f * fs), 0.9f, 5);
        dn1::Params p; p.mode = dn1::Limiter; p.variant = dn1::TruePeak; p.limThresh = -12.0f; p.limCeiling = -1.0f; p.limRelease = 100.0f;
        std::vector<float> oR; const auto o = run (p, loud, loud, 1.0f, nullptr, &oR);
        ms1::TruePeak tp; float mx = 0; for (size_t i = (size_t) (0.5f * fs); i < o.size(); ++i) mx = std::max (mx, tp.push (o[i]));
        const double db = 20.0 * std::log10 (mx);
        std::printf ("  TRUE PEAK at %.0f Hz: %.2f dBTP (ceiling -1)\n", (double) fs, db);
        check (db < -0.9, "TRUE PEAK holds the ceiling (within 0.1 dB)");
    }
    fs = 48000.0f;
    {
        int l0 = 0, l1 = 0, l2 = 0;
        dn1::Params a; a.mode = dn1::Shaper; dn1::Params b; b.mode = dn1::Compressor; dn1::Params c; c.mode = dn1::Limiter; c.variant = dn1::TruePeak;
        std::vector<float> click ((int) fs, 0.0f); click[500] = 0.5f;
        a.shaper.gateOn = false; a.shaper.expOn = false; a.shaper.transOn = false; b.variant = dn1::Vca; b.compThresh = 0.0f; b.compRatio = 1.0f;
        const auto oa = run (a, click, click, 1.0f, &l0), ob = run (b, click, click, 1.0f, &l1), oc = run (c, click, click, 1.0f, &l2);
        check (l0 == l1 && l1 == l2, "the latency is the same in every mode");
        check (oa[(size_t) (500 + lat)] == 0.5f, "SHAPER with its stages off passes a click bit-exact, delayed by the latency");
        check (std::abs (ob[(size_t) (500 + lat)] - 0.5f) < 0.01f, "a compressor with nothing to compress delays the click by the latency");
        check (std::abs (oc[(size_t) (500 + lat)]) > 0.2f, "TRUE PEAK delays the click by the same latency");
        dn1::Params m; m.mode = dn1::Compressor; m.variant = dn1::Vca; m.compThresh = -40.0f; m.compRatio = 10.0f; m.mix = 0.0f;
        const auto om = run (m, in, in);
        check (diffDb (om, in, 1000, lat) < -120.0, "MIX 0 passes the input unchanged (delayed by the latency)");
    }
    {
        // AUTO LEVEL: a hard-driven compressor keeps the level of the input
        const auto tone = noise ((int) (6.0f * fs), 0.2f, 9);
        dn1::Params p; p.mode = dn1::Compressor; p.variant = dn1::Vca; p.compThresh = -30.0f; p.compRatio = 4.0f; p.compMakeup = 0.0f; p.autoLevel = true;
        const auto o = run (p, tone, tone);
        const double d = 20.0 * std::log10 (rms (o, (int) (4.0f * fs), (int) (6.0f * fs)) / rms (tone, (int) (4.0f * fs), (int) (6.0f * fs)));
        std::printf ("  AUTO LEVEL: output %.2f dB against the input\n", d);
        check (std::abs (d) < 2.0, "AUTO LEVEL keeps the output level within 2 dB of the input");
    }
    {
        bool ok = true;
        const auto x = noise ((int) fs, 0.8f, 11);
        for (int mode = 0; mode < dn1::kNumModes; ++mode) for (int v = 0; v < std::max (1, dn1::variantCount (mode)); ++v) for (int s = 0; s < 4; ++s)
        {
            dn1::Params p; p.mode = mode; p.variant = v;
            p.shaper.gateOn = p.shaper.expOn = p.shaper.transOn = true; p.shaper.attack = s == 1 ? 100.0f : -100.0f; p.shaper.sustain = s == 1 ? 100.0f : -100.0f; p.shaper.gateThresh = s == 2 ? -10.0f : -80.0f;
            p.compThresh = s == 2 ? -60.0f : 0.0f; p.compRatio = s == 1 ? 20.0f : 1.0f; p.compAttackMs = 0.05f; p.compReleaseMs = s == 3 ? 10.0f : 2000.0f; p.compMakeup = 24.0f; p.sustain = s / 3.0f;
            p.limThresh = s == 2 ? -30.0f : 0.0f; p.limRelease = 10.0f; p.limCeiling = s == 3 ? -3.0f : 0.0f; p.tone = s == 1 ? 1.0f : 0.0f; p.scHpHz = 150.0f; p.link = s == 3 ? 0.0f : 1.0f; p.autoLevel = s != 0;
            const auto o = run (p, x, x, 4.0f); float peak = 0; bool fin = true; for (float y : o) { if (! std::isfinite (y)) fin = false; peak = std::max (peak, std::abs (y)); }
            if (! fin || peak > 400.0f) { ok = false; std::printf ("    %s type %d setting %d: peak %.2f%s\n", dn1::modeName (mode), v, s, peak, fin ? "" : " NOT FINITE"); }
        }
        check (ok, "every mode, type and extreme setting stays finite and bounded");
        std::vector<float> silent ((int) fs, 0.0f); dn1::Params p; p.mode = dn1::Compressor; const auto so = run (p, silent, silent); double e = 0; for (float y : so) e += (double) y * y;
        check (e == 0.0, "silence in, silence out");
    }
    std::printf ("%d problems\n", problems);
    return problems == 0 ? 0 : 1;
}
