// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
// Offline check of the SA-1 engine: finite and bounded at extreme settings; BOOST adds its gain; the TUBE with BIAS adds a second harmonic that the
// unbiased TUBE does not; the TAPE adds a third harmonic and WARMTH rolls the highs off; the CRUSH really quantises (few distinct levels) and holds
// samples (runs of equal values); switched-off stages and MIX 0 leave the sound alone.
#include <cstdio>
#include <set>
#include <vector>
#include "SaturatorEngine.h"

static std::vector<float> run (const sa1::Params& p, float hz, float amp, float seconds = 1.0f, float level = 1.0f, float fs = 48000.0f)
{
    sa1::Engine e;
    e.prepare (fs);
    const int total = (int) (fs * seconds);
    std::vector<float> out ((size_t) total), l (512), r (512);
    for (int pos = 0; pos < total; pos += 512)
    {
        const int n = std::min (512, total - pos);
        for (int i = 0; i < n; ++i) l[(size_t) i] = r[(size_t) i] = amp * std::sin (6.2831853f * hz * (float) (pos + i) / fs);
        e.process (l.data(), r.data(), n, p, level);
        for (int i = 0; i < n; ++i) out[(size_t) (pos + i)] = l[(size_t) i];
    }
    return out;
}
static double bin (const std::vector<float>& v, float hz, int a, int b, float fs = 48000.0f)
{
    double re = 0, im = 0;
    for (int i = a; i < b; ++i) { const double ph = 6.283185307 * hz * i / fs; re += v[(size_t) i] * std::cos (ph); im += v[(size_t) i] * std::sin (ph); }
    return std::sqrt (re * re + im * im) / (b - a);
}

int main()
{
    int problems = 0;
    sa1::Params off; off.boostOn = off.tubeOn = off.tapeOn = off.crushOn = false;

    // 1. extreme settings: finite and bounded
    for (int s = 0; s < 4; ++s)
    {
        sa1::Params p; p.boostOn = p.tubeOn = p.tapeOn = p.crushOn = true;
        p.boostGain = s == 0 ? 0.0f : 24.0f; p.boostTone = s == 1 ? 0.0f : 1.0f;
        p.tubeDrive = s == 2 ? 0.0f : 1.0f; p.tubeBias = s == 3 ? 0.0f : 1.0f;
        p.tapeDrive = s == 1 ? 0.0f : 1.0f; p.tapeWarmth = s == 2 ? 0.0f : 1.0f;
        p.bits = s == 0 ? 1.0f : 16.0f; p.rate = s == 3 ? 500.0f : 48000.0f; p.crushMix = s == 2 ? 0.5f : 1.0f;
        const auto o = run (p, 440.0f, 0.9f, 2.0f);
        float peak = 0; bool finite = true;
        for (float x : o) { if (! std::isfinite (x)) finite = false; peak = std::max (peak, std::abs (x)); }
        std::printf ("setting %d: peak %.3f%s\n", s, peak, finite ? "" : "  NOT FINITE");
        if (! finite || peak > 4.0f) { ++problems; std::printf ("  problem\n"); }
    }

    // 2. BOOST: +12 dB at 220 Hz (tone flat) is about x4
    {
        sa1::Params p = off; p.boostOn = true; p.boostGain = 12.0f; p.boostTone = 0.5f;
        const auto o = run (p, 220.0f, 0.05f);
        const double g = bin (o, 220.0f, 24000, 48000) / (0.05 * 0.5);
        std::printf ("boost +12 dB: gain %.2f (expect about 3.98)\n", g);
        if (std::abs (g - 3.98) > 0.2) { ++problems; std::printf ("  wrong boost gain\n"); }
    }

    // 3. TUBE: bias adds a 2nd harmonic
    {
        sa1::Params p = off; p.tubeOn = true; p.tubeDrive = 0.8f;
        p.tubeBias = 0.0f; const auto a = run (p, 220.0f, 0.4f);
        p.tubeBias = 1.0f; const auto b = run (p, 220.0f, 0.4f);
        const double h2a = bin (a, 440.0f, 24000, 48000) / bin (a, 220.0f, 24000, 48000), h2b = bin (b, 440.0f, 24000, 48000) / bin (b, 220.0f, 24000, 48000);
        std::printf ("tube 2nd harmonic: bias 0 %.4f, bias 100%% %.4f\n", h2a, h2b);
        if (! (h2b > 5.0 * h2a && h2b > 0.01)) { ++problems; std::printf ("  the bias should add a 2nd harmonic\n"); }
    }

    // 4. TAPE: 3rd harmonic, and WARMTH rolls the highs off
    {
        sa1::Params p = off; p.tapeOn = true; p.tapeDrive = 1.0f; p.tapeWarmth = 0.0f;
        const auto a = run (p, 220.0f, 0.5f);
        const double h3 = bin (a, 660.0f, 24000, 48000) / bin (a, 220.0f, 24000, 48000);
        p.tapeDrive = 0.2f; p.tapeWarmth = 0.0f; const auto hiOpen = run (p, 9000.0f, 0.2f);
        p.tapeWarmth = 1.0f; const auto hiWarm = run (p, 9000.0f, 0.2f);
        const double ratio = bin (hiWarm, 9000.0f, 24000, 48000) / bin (hiOpen, 9000.0f, 24000, 48000);
        std::printf ("tape: 3rd harmonic %.4f; 9 kHz with warmth 100%% is %.2f of warmth 0\n", h3, ratio);
        if (! (h3 > 0.02)) { ++problems; std::printf ("  the tape should add a 3rd harmonic\n"); }
        if (! (ratio < 0.6)) { ++problems; std::printf ("  warmth should roll the highs off\n"); }
    }

    // 5. CRUSH: 3 bits = at most 8 distinct levels; 6 kHz hold = runs of 8 equal samples
    {
        sa1::Params p = off; p.crushOn = true; p.bits = 3.0f; p.rate = 48000.0f; p.crushMix = 1.0f;
        const auto a = run (p, 220.0f, 0.9f);
        std::set<int> levels; for (size_t i = 24000; i < a.size(); ++i) levels.insert ((int) std::lround (a[i] * 4.0f * 1000.0f));
        p.bits = 16.0f; p.rate = 6000.0f;
        const auto b = run (p, 220.0f, 0.9f);
        int maxRun = 1, runLen = 1; for (size_t i = 24001; i < b.size(); ++i) { if (b[i] == b[i - 1]) ++runLen; else runLen = 1; maxRun = std::max (maxRun, runLen); }
        std::printf ("crush: 3 bits -> %zu levels, 6 kHz -> longest run %d samples\n", levels.size(), maxRun);
        if (levels.size() > 9) { ++problems; std::printf ("  3 bits should give at most 8 or 9 levels\n"); }
        if (maxRun < 6) { ++problems; std::printf ("  a 6 kHz sample rate should hold samples\n"); }
    }

    // 6. all stages off and MIX 0 leave the sound alone
    {
        const auto a = run (off, 440.0f, 0.5f);
        double err = 0; for (size_t i = 4800; i < a.size(); ++i) err = std::max (err, (double) std::abs (a[i] - 0.5f * std::sin (6.2831853f * 440.0f * (float) i / 48000.0f)));
        sa1::Params p; p.boostOn = p.tubeOn = p.tapeOn = p.crushOn = true; p.mix = 0.0f;
        const auto m = run (p, 440.0f, 0.5f);
        double err2 = 0; for (size_t i = 4800; i < m.size(); ++i) err2 = std::max (err2, (double) std::abs (m[i] - 0.5f * std::sin (6.2831853f * 440.0f * (float) i / 48000.0f)));
        std::printf ("stages off error %.5f, mix 0 error %.5f\n", err, err2);
        if (err > 0.001 || err2 > 0.001) { ++problems; std::printf ("  off / mix 0 should leave the sound alone\n"); }
    }

    std::printf ("%d problems\n", problems);
    return problems == 0 ? 0 : 1;
}
