// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
// Offline check of the HM-2 engine: every character at extreme and mid settings stays finite and bounded, ODD gives the 3rd harmonic
// and EVEN the 2nd, OCTAVE adds energy at 2f and SUB at f/2.
#include <cstdio>
#include <vector>
#include "HarmonicEngine.h"

static double tone (const std::vector<float>& x, float f, float fs)          // Goertzel-style magnitude at f
{
    double re = 0, im = 0;
    for (size_t i = 0; i < x.size(); ++i) { const double a = 2.0 * 3.14159265358979 * f * (double) i / fs; re += x[i] * std::cos (a); im += x[i] * std::sin (a); }
    return std::sqrt (re * re + im * im) * 2.0 / (double) x.size();
}

static std::vector<float> run (const hm2::Params& p, float f, float amp, float mix = 1.0f, int seconds = 1)
{
    const float fs = 48000.0f;
    hm2::Engine e; e.prepare (fs);
    std::vector<float> out;
    std::vector<float> l (512), r (512);
    for (int pos = 0; pos < (int) (fs * (float) seconds); pos += 512)
    {
        for (int i = 0; i < 512; ++i) l[(size_t) i] = r[(size_t) i] = amp * std::sin (6.2831853f * f * (float) (pos + i) / fs);
        e.process (l.data(), r.data(), 512, p, mix, 1.0f);
        if (pos >= (int) (fs * 0.5f)) out.insert (out.end(), l.begin(), l.end());          // skip the first half second
    }
    return out;
}

int main()
{
    int problems = 0;
    for (int c = 0; c < hm2::kNumCharacters; ++c)
        for (int s = 0; s < 3; ++s)
        {
            hm2::Params p; p.character = c;
            const float v = s == 0 ? 0.0f : s == 1 ? 1.0f : 0.5f;
            p.drive = p.oddEven = p.octave = p.sub = p.focus = p.tilt = p.tone = v;
            const auto y = run (p, 220.0f, 0.5f);
            float pk = 0; bool fin = true;
            for (float v2 : y) { if (! std::isfinite (v2)) fin = false; pk = std::max (pk, std::fabs (v2)); }
            if (! fin || pk > 2.0f) { ++problems; std::printf ("character %d setting %d  peak %.3f  <-- PROBLEM\n", c, s, pk); }
        }

    hm2::Params p; p.drive = 0.5f; p.octave = 0.0f; p.sub = 0.0f; p.tone = 1.0f; p.focus = 0.5f; p.tilt = 0.5f;
    p.oddEven = 1.0f;
    auto y = run (p, 200.0f, 0.4f);
    const double o2 = tone (y, 400.0f, 48000.0f), o3 = tone (y, 600.0f, 48000.0f);
    p.oddEven = 0.0f;
    y = run (p, 200.0f, 0.4f);
    const double e2 = tone (y, 400.0f, 48000.0f), e3 = tone (y, 600.0f, 48000.0f);
    std::printf ("ODD : H2 %.4f  H3 %.4f\nEVEN: H2 %.4f  H3 %.4f\n", o2, o3, e2, e3);
    if (! (o3 > 4.0 * o2)) { ++problems; std::printf ("  odd setting should give mostly the 3rd harmonic\n"); }
    if (! (e2 > 2.0 * o2)) { ++problems; std::printf ("  even setting should give more 2nd harmonic\n"); }

    p.oddEven = 1.0f; p.drive = 0.0f; p.octave = 1.0f;
    y = run (p, 200.0f, 0.4f);
    const double up = tone (y, 400.0f, 48000.0f);
    p.octave = 0.0f; p.sub = 1.0f;
    y = run (p, 200.0f, 0.4f, 1.0f, 2);
    const double down = tone (y, 100.0f, 48000.0f);
    p.sub = 0.0f;
    const double none = tone (run (p, 200.0f, 0.4f, 1.0f, 2), 100.0f, 48000.0f);
    std::printf ("OCTAVE: energy at 2f %.4f (without: %.4f)\nSUB: energy at f/2 %.4f (without: %.4f)\n", up, o2 * 0.0 + tone (run (hm2::Params{ 0, 0.0f, 1.0f, 0.0f, 0.0f, 0.5f, 0.5f, 1.0f }, 200.0f, 0.4f), 400.0f, 48000.0f), down, none);
    if (! (up > 0.02)) { ++problems; std::printf ("  octave up has no energy at 2f\n"); }
    if (! (down > 0.02 && down > 5.0 * none)) { ++problems; std::printf ("  sub octave has no energy at f/2\n"); }

    float db[8]; hm2::Params q; q.oddEven = 1.0f; hm2::harmonicSpectrum (q, db);
    std::printf ("spectrum odd: H2 %.1f dB  H3 %.1f dB\n", db[0], db[1]);
    std::printf ("%d problems\n", problems);
    return problems == 0 ? 0 : 1;
}
