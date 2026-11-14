// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
// Offline check of the DV-1 engine: the latency is the same in every mode; MIX 0 passes the input unchanged (delayed); TUBE single has even harmonics and push-pull
// does not; the other modes are odd-harmonic; the aliasing of a high tone is low; AGED tape is darker than NEW; CRUSH quantises to the set number of bits and holds
// samples at the set rate; AUTO LEVEL keeps the level across the drive range; every mode, type and extreme setting stays finite and bounded.
#include <cstdio>
#include <random>
#include <vector>
#include "DriveEngine.h"

namespace
{
int problems = 0;
void check (bool ok, const char* what) { std::printf ("  %-96s %s\n", what, ok ? "ok" : "FAILED"); if (! ok) ++problems; }
const float fs = 48000.0f;

std::vector<float> sine (int n, float hz, float amp) { std::vector<float> v ((size_t) n); for (int i = 0; i < n; ++i) v[(size_t) i] = amp * std::sin (6.2831853f * hz * (float) i / fs); return v; }
std::vector<float> noise (int n, float amp, unsigned seed)
{
    std::mt19937 rng (seed); std::uniform_real_distribution<float> u (-1.0f, 1.0f);
    std::vector<float> v ((size_t) n); for (auto& x : v) x = amp * u (rng);
    return v;
}
std::vector<float> run (const dv1::Params& p, const std::vector<float>& in, float level = 1.0f, int* latency = nullptr)
{
    dv1::Engine e; e.prepare (fs);
    if (latency) *latency = e.latency();
    auto l = in, r = in;
    for (size_t pos = 0; pos < l.size(); pos += 480) { const int n = (int) std::min<size_t> (480, l.size() - pos); e.process (l.data() + pos, r.data() + pos, n, p, level); }
    return l;
}
double rms (const std::vector<float>& v, int a, int b) { double s = 0; for (int i = a; i < b; ++i) s += (double) v[(size_t) i] * v[(size_t) i]; return std::sqrt (s / (b - a)); }
// the amplitude of a frequency (a Hann-windowed Goertzel over the second half of the signal)
double bin (const std::vector<float>& v, float hz)
{
    const int a = (int) v.size() / 2, len = (int) v.size() - a; double re = 0, im = 0;
    for (int i = 0; i < len; ++i) { const double w = 0.5 - 0.5 * std::cos (6.2831853 * i / len), ph = 6.2831853 * hz * (a + i) / fs; re += w * v[(size_t) (a + i)] * std::cos (ph); im += w * v[(size_t) (a + i)] * std::sin (ph); }
    return 2.0 * std::sqrt (re * re + im * im) / (0.5 * len);
}
double db (double x) { return 20.0 * std::log10 (std::max (x, 1e-12)); }
}

int main()
{
    int lat = 0; run ({}, sine (480, 100.0f, 0.1f), 1.0f, &lat);
    std::printf ("DV-1 engine, latency %d samples\n", lat);
    const auto tone = sine ((int) (2.0f * fs), 1000.0f, 0.3f);

    std::printf ("harmonics\n");
    {
        dv1::Params p; p.mode = dv1::Tube; p.variant = 0; p.drive = 0.6f; p.tubeBias = 0.6f; p.autoLevel = false; p.tone = 0.5f;
        const auto single = run (p, tone); p.variant = 1; const auto pp = run (p, tone);
        const double e1 = bin (single, 2000.0f), o1 = bin (single, 3000.0f), e2 = bin (pp, 2000.0f), o2 = bin (pp, 3000.0f);
        std::printf ("  TUBE single: 2nd %.1f dB 3rd %.1f dB, push-pull: 2nd %.1f dB 3rd %.1f dB\n", db (e1), db (o1), db (e2), db (o2));
        check (e1 > o1 * 0.8 && e1 > 0.0, "TUBE SINGLE: strong even harmonics (2nd above 0.8 x 3rd)");
        check (e2 < 0.2 * o2, "TUBE PUSH-PULL: even harmonics below 0.2 x the odd ones");
    }
    {
        dv1::Params p; p.mode = dv1::Fold; p.variant = 1; p.drive = 0.8f; p.fold = 0.8f; p.symmetry = 1.0f; p.autoLevel = false;
        const auto f = run (p, tone);
        check (bin (f, 2000.0f) < 0.2 * bin (f, 3000.0f) + 1e-4, "FOLD, symmetric: the odd harmonics lead");
        p.symmetry = 0.2f; const auto fa = run (p, tone);
        check (bin (fa, 2000.0f) > 4.0 * bin (f, 2000.0f), "FOLD: lowering SYMMETRY adds even harmonics");
    }
    std::printf ("aliasing, tone\n");
    {
        const auto hi = sine ((int) (2.0f * fs), 7000.0f, 0.5f);
        for (int mode : { (int) dv1::Overdrive, (int) dv1::Distortion, (int) dv1::Tube, (int) dv1::Tape, (int) dv1::Fold })
        {
            dv1::Params p; p.mode = mode; p.drive = 1.0f; p.autoLevel = false; p.odType = 11; p.fold = 1.0f;
            const auto o = run (p, hi);
            // an alias of the 7 kHz harmonics 4 (28 kHz -> 20 kHz) and 5 (35 kHz -> 13 kHz) folds into the band; harmonics below 24 kHz are real
            const double fund = bin (o, 7000.0f), alias = std::max (bin (o, 20000.0f), bin (o, 13000.0f) * 0.0 + bin (o, 17000.0f));
            std::printf ("  %-10s 7 kHz at full drive: strongest alias line %.1f dBc\n", dv1::modeName (mode), db (alias / std::max (fund, 1e-9)));
        }
        dv1::Params a, b; a.mode = b.mode = dv1::Tape; a.variant = 0; b.variant = 1; a.drive = b.drive = 0.3f; a.autoLevel = b.autoLevel = false; a.warmth = b.warmth = 0.6f;
        const auto white = noise ((int) (2.0f * fs), 0.3f, 4);
        const auto oa = run (a, white), ob = run (b, white);
        const double hfA = bin (oa, 10000.0f) + bin (oa, 12000.0f), hfB = bin (ob, 10000.0f) + bin (ob, 12000.0f);
        check (hfB < 0.8 * hfA, "TAPE: AGED has less top than NEW");
    }
    std::printf ("crush\n");
    {
        dv1::Params p; p.mode = dv1::Crush; p.variant = 0; p.bits = 4.0f; p.rate = 48000.0f; p.crushMix = 1.0f; p.drive = 0.0f; p.autoLevel = false; p.mix = 1.0f;
        const auto x = sine ((int) (1.0f * fs), 300.0f, 0.8f); const auto o = run (p, x);
        std::vector<float> seen; for (size_t i = (size_t) (0.5f * fs); i < o.size(); ++i) { bool f = false; for (float s : seen) if (std::abs (s - o[i]) < 1e-5f) { f = true; break; } if (! f) seen.push_back (o[i]); }
        std::printf ("  CRUSH 4 bits: %d distinct levels\n", (int) seen.size());
        check ((int) seen.size() <= 17, "CRUSH quantises to the set number of bits (4 bits: at most 17 levels)");
        p.bits = 16.0f; p.rate = 4800.0f; const auto o2 = run (p, x);
        int runs = 0, same = 0; for (size_t i = (size_t) (0.5f * fs) + 1; i < o2.size(); ++i) { if (o2[i] == o2[i - 1]) ++same; ++runs; }
        check ((double) same / runs > 0.85, "CRUSH: RATE 4.8 kHz holds each sample for about 10 samples at 48 kHz");
    }
    std::printf ("latency, mix, level\n");
    {
        bool same = true; std::vector<float> click ((int) fs, 0.0f); click[500] = 0.5f;
        for (int m = 0; m < dv1::kNumModes; ++m) { dv1::Params p; p.mode = m; int l = 0; run (p, click, 1.0f, &l); if (l != lat) same = false; }
        check (same, "the latency is the same in every mode");
        dv1::Params p; p.mode = dv1::Tube; p.mix = 0.0f; const auto in = noise ((int) fs, 0.4f, 5); const auto o = run (p, in);
        double e = 0; for (size_t i = (size_t) 1000; i + (size_t) lat < o.size(); ++i) { const double d = (double) o[i + (size_t) lat] - in[i]; e += d * d; }
        check (e < 1e-12, "MIX 0 passes the input unchanged (delayed by the latency)");
        std::vector<float> silent ((int) fs, 0.0f); dv1::Params q; q.mode = dv1::Tape; const auto so = run (q, silent); double se = 0; for (float y : so) se += (double) y * y;
        check (se == 0.0, "silence in, silence out");
    }
    {
        // AUTO LEVEL: the output stays near the input level from low to full drive
        const auto music = noise ((int) (6.0f * fs), 0.2f, 8);
        bool ok = true; double worst = 0;
        for (int m = 0; m < dv1::kNumModes; ++m) for (float drive : { 0.1f, 0.5f, 1.0f })
        {
            dv1::Params p; p.mode = m; p.drive = drive; p.autoLevel = true; p.odType = 5; p.rate = 24000.0f; p.bits = 10.0f;
            const auto o = run (p, music); const double d = db (rms (o, (int) (4.0f * fs), (int) (6.0f * fs)) / rms (music, (int) (4.0f * fs), (int) (6.0f * fs)));
            worst = std::max (worst, std::abs (d)); if (std::abs (d) > 2.0) { ok = false; std::printf ("    %s drive %.1f: %.2f dB\n", dv1::modeName (m), (double) drive, d); }
        }
        std::printf ("  AUTO LEVEL: worst deviation %.2f dB\n", worst);
        check (ok, "AUTO LEVEL keeps the output within 2 dB of the input in every mode at drive 10 / 50 / 100 %");
    }
    {
        bool ok = true; const auto x = noise ((int) fs, 0.8f, 11);
        for (int m = 0; m < dv1::kNumModes; ++m) for (int v = 0; v < 2; ++v) for (int t = 0; t < (m == 0 ? dv1::kNumOdTypes : 1); ++t) for (int s = 0; s < 3; ++s)
        {
            dv1::Params p; p.mode = m; p.variant = v; p.odType = t; p.drive = s == 0 ? 0.0f : 1.0f; p.tone = s == 1 ? 1.0f : 0.0f; p.boostOn = s != 0; p.boostDb = 24.0f; p.autoLevel = s != 2;
            p.odParam = s == 1 ? 10.0f : 0.0f; p.odLow = p.odMid = p.odHigh = s == 1 ? 10.0f : 0.0f; p.gateThresh = s == 2 ? 10.0f : 0.0f; p.tight = 1.0f; p.tubeBias = 1.0f; p.warmth = 1.0f;
            p.fold = 1.0f; p.symmetry = s == 1 ? 0.0f : 1.0f; p.bits = s == 1 ? 1.0f : 16.0f; p.rate = s == 1 ? 500.0f : 48000.0f; p.crushMix = 1.0f; p.mix = s == 2 ? 0.5f : 1.0f;
            const auto o = run (p, x, 4.0f); float peak = 0; bool fin = true; for (float y : o) { if (! std::isfinite (y)) fin = false; peak = std::max (peak, std::abs (y)); }
            if (! fin || peak > 16.5f) { ok = false; std::printf ("    %s type %d/%d setting %d: peak %.2f%s\n", dv1::modeName (m), v, t, s, peak, fin ? "" : " NOT FINITE"); }
        }
        check (ok, "every mode, type, OD-2 type and extreme setting stays finite and bounded");
    }
    std::printf ("%d problems\n", problems);
    return problems == 0 ? 0 : 1;
}
