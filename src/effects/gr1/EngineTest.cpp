// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
// Offline check of the GR-1 engine: every envelope at extreme / mid settings stays finite and bounded, +12 semitones moves a tone up an
// octave, and FREEZE keeps sounding after the input has stopped.
#include <cstdio>
#include <vector>
#include "GrainEngine.h"

static double tone (const std::vector<float>& x, float f, float fs)
{
    double re = 0, im = 0;
    for (size_t i = 0; i < x.size(); ++i) { const double a = 2.0 * 3.14159265358979 * f * (double) i / fs; re += x[i] * std::cos (a); im += x[i] * std::sin (a); }
    return std::sqrt (re * re + im * im) * 2.0 / (double) x.size();
}

static std::vector<float> run (const gr1::Params& p, float seconds, float toneEnd, float& peak, bool& finite)
{
    const float fs = 48000.0f;
    gr1::Engine e; e.prepare (fs);
    std::vector<float> out, l (512), r (512);
    peak = 0; finite = true;
    for (int pos = 0; pos < (int) (fs * seconds); pos += 512)
    {
        for (int i = 0; i < 512; ++i) { const float t = (float) (pos + i) / fs; l[(size_t) i] = r[(size_t) i] = t < toneEnd ? 0.4f * std::sin (6.2831853f * 220.0f * t) : 0.0f; }
        e.process (l.data(), r.data(), 512, p, 1.0f, 1.0f);
        for (int i = 0; i < 512; ++i)
        {
            if (! std::isfinite (l[(size_t) i]) || ! std::isfinite (r[(size_t) i])) finite = false;
            peak = std::max ({ peak, std::fabs (l[(size_t) i]), std::fabs (r[(size_t) i]) });
        }
        if (pos >= (int) (fs * 1.0f)) out.insert (out.end(), l.begin(), l.end());
    }
    return out;
}

int main()
{
    int problems = 0;
    for (int shape = 0; shape < gr1::kNumShapes; ++shape)
        for (int s = 0; s < 3; ++s)
        {
            gr1::Params p; p.shape = shape;
            const float v = s == 0 ? 0.0f : s == 1 ? 1.0f : 0.5f;
            p.size = p.density = p.position = p.spray = p.random = p.reverse = p.spread = v; p.pitch = v;
            float pk; bool fin;
            run (p, 3.0f, 3.0f, pk, fin);
            if (! fin || pk > 2.0f) { ++problems; std::printf ("shape %d setting %d  peak %.3f  <-- PROBLEM\n", shape, s, pk); }
        }

    float pk; bool fin;
    gr1::Params p; p.size = 0.6f; p.density = 0.8f; p.position = 0.1f; p.spray = 0.0f; p.random = 0.0f; p.reverse = 0.0f;
    p.pitch = 0.5f;
    auto y = run (p, 3.0f, 3.0f, pk, fin);
    const double base220 = tone (y, 220.0f, 48000.0f), base440 = tone (y, 440.0f, 48000.0f);
    p.pitch = 1.0f;
    y = run (p, 3.0f, 3.0f, pk, fin);
    const double up220 = tone (y, 220.0f, 48000.0f), up440 = tone (y, 440.0f, 48000.0f);
    std::printf ("pitch 0: 220 Hz %.3f  440 Hz %.3f\npitch +12: 220 Hz %.3f  440 Hz %.3f\n", base220, base440, up220, up440);
    if (! (base220 > 3.0 * base440)) { ++problems; std::printf ("  unshifted grains should keep 220 Hz\n"); }
    if (! (up440 > 3.0 * up220)) { ++problems; std::printf ("  +12 semitones should give 440 Hz\n"); }

    // freeze: the tone stops at 1 s, the freeze goes on at 0.9 s; the texture must still sound at 3-4 s
    gr1::Engine e; e.prepare (48000.0f);
    gr1::Params f; f.size = 0.6f; f.density = 0.8f; f.position = 0.0f; f.spray = 0.3f;
    std::vector<float> l (512), r (512);
    double late = 0, lateNoFreeze = 0;
    for (int run2 = 0; run2 < 2; ++run2)
    {
        gr1::Engine e2; e2.prepare (48000.0f);
        for (int pos = 0; pos < 48000 * 4; pos += 512)
        {
            const float t = (float) pos / 48000.0f;
            f.freeze = run2 == 0 && t > 0.9f;
            for (int i = 0; i < 512; ++i) { const float tt = (float) (pos + i) / 48000.0f; l[(size_t) i] = r[(size_t) i] = tt < 1.0f ? 0.4f * std::sin (6.2831853f * 220.0f * tt) : 0.0f; }
            e2.process (l.data(), r.data(), 512, f, 1.0f, 1.0f);
            if (t > 3.0f) for (int i = 0; i < 512; ++i) (run2 == 0 ? late : lateNoFreeze) += (double) l[(size_t) i] * l[(size_t) i];
        }
    }
    std::printf ("after the input stopped: freeze %.2f  no freeze %.4f\n", late, lateNoFreeze);
    if (! (late > 1.0 && late > 50.0 * lateNoFreeze)) { ++problems; std::printf ("  freeze does not sustain\n"); }
    std::printf ("%d problems\n", problems);
    return problems == 0 ? 0 : 1;
}
