// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
// Offline check of the PT-1 engine: the pitch detector finds sine tones; FIXED +12 semitones doubles the frequency; INTELLIGENT mode puts a
// third above C on E and a third above D on F (the notes of C major); extreme settings stay finite and bounded.
#include <cstdio>
#include <vector>
#include "PhantomEngine.h"

static double tone (const std::vector<float>& x, double f, double fs)
{
    double re = 0, im = 0;
    for (size_t i = 0; i < x.size(); ++i) { const double a = 2.0 * 3.14159265358979 * f * (double) i / fs; re += x[i] * std::cos (a); im += x[i] * std::sin (a); }
    return std::sqrt (re * re + im * im) * 2.0 / (double) x.size();
}

// the energy of the tone near f (+- 10 Hz): the shifter's crossfades spread a pure sine a little, so the test measures a band
static double band (const std::vector<float>& x, double f, double fs, double half = 10.0)
{
    double e = 0;
    for (double g = f - half; g <= f + half; g += 1.0) { const double a = tone (x, g, fs); e += a * a; }
    return std::sqrt (e);
}

// plays a sine of f; returns what the voices sound (mix = 100 %: no dry) after 1 s; also the detected note
static std::vector<float> run (pt1::Params p, double f, float& detected, float mix = 1.0f)
{
    const float fs = 48000.0f;
    pt1::Engine e; e.prepare (fs);
    std::vector<float> out, l (512), r (512);
    for (int pos = 0; pos < (int) (fs * 2.0f); pos += 512)
    {
        for (int i = 0; i < 512; ++i) l[(size_t) i] = r[(size_t) i] = 0.4f * (float) std::sin (2.0 * 3.14159265358979 * f * (double) (pos + i) / fs);
        e.process (l.data(), r.data(), 512, p, mix, 1.0f);
        if (pos >= (int) (fs * 1.0f)) for (int i = 0; i < 512; ++i) out.push_back (0.5f * (l[(size_t) i] + r[(size_t) i]));
    }
    detected = e.detected.load();
    return out;
}

int main()
{
    int problems = 0;
    // pitch detection
    for (double f : { 110.0, 220.0, 261.63, 440.0, 659.26 })
    {
        float det; pt1::Params p; run (p, f, det);
        const float expect = 69.0f + 12.0f * (float) std::log2 (f / 440.0);
        std::printf ("detect %.1f Hz -> note %.2f (expect %.2f)\n", f, det, expect);
        if (std::fabs (det - expect) > 0.3f) { ++problems; std::printf ("  pitch detection is off\n"); }
    }
    // fixed: only voice 1 at +12, no detune
    {
        pt1::Params p; p.intelligent = false;
        for (int v = 0; v < 4; ++v) { p.level[v] = v == 0 ? 1.0f : 0.0f; p.detune[v] = 0.0f; }
        p.interval[0] = 12; p.spread = 0.0f;
        float det; const auto y = run (p, 220.0, det);
        const double a = band (y, 220.0, 48000.0, 30.0), b = band (y, 440.0, 48000.0, 30.0);   // wider: at +12 the crossfades make sidebands 25 Hz away
        std::printf ("fixed +12: 220 Hz %.3f  440 Hz %.3f\n", a, b);
        if (! (b > 4.0 * a)) { ++problems; std::printf ("  +12 should give the octave\n"); }
    }
    // intelligent: third above C -> E (329.63), third above D -> F (349.23), in C major
    for (const auto& c : { std::pair<double, double> { 261.63, 329.63 }, std::pair<double, double> { 293.66, 349.23 } })
    {
        pt1::Params p; p.intelligent = true; p.root = 0; p.scale = 0;
        for (int v = 0; v < 4; ++v) { p.level[v] = v == 0 ? 1.0f : 0.0f; p.detune[v] = 0.0f; }
        p.interval[0] = 4; p.spread = 0.0f;
        float det; const auto y = run (p, c.first, det);
        const double got = band (y, c.second, 48000.0), wrong = band (y, c.second * std::pow (2.0, 1.0 / 12.0), 48000.0), below = band (y, c.second / std::pow (2.0, 1.0 / 12.0), 48000.0);
        (void) below;
        std::printf ("intelligent: %.1f Hz -> %.1f Hz: %.3f (a semitone higher: %.3f)\n", c.first, c.second, got, wrong);
        if (! (got > 3.0 * wrong)) { ++problems; std::printf ("  the harmony is not on the scale note\n"); }
    }
    // extreme settings
    for (int s = 0; s < 3; ++s)
    {
        pt1::Params p; p.intelligent = s != 1; p.root = s * 4; p.scale = s * 2;
        for (int v = 0; v < 4; ++v) { p.interval[v] = s == 0 ? -24 : s == 1 ? 24 : (v - 2) * 7; p.detune[v] = s == 0 ? -50.0f : 50.0f; p.level[v] = 1.0f; }
        p.spread = 1.0f;
        pt1::Engine e; e.prepare (48000.0f);
        std::vector<float> l (512), r (512);
        float peak = 0; bool fin = true;
        for (int pos = 0; pos < 48000 * 2; pos += 512)
        {
            for (int i = 0; i < 512; ++i) l[(size_t) i] = r[(size_t) i] = 0.5f * (float) std::sin (2.0 * 3.14159265358979 * 196.0 * (double) (pos + i) / 48000.0) + 0.05f * ((i * 37 % 11) - 5.0f) * 0.1f;
            e.process (l.data(), r.data(), 512, p, 0.5f, 1.0f);
            for (int i = 0; i < 512; ++i) { if (! std::isfinite (l[(size_t) i]) || ! std::isfinite (r[(size_t) i])) fin = false; peak = std::max ({ peak, std::fabs (l[(size_t) i]), std::fabs (r[(size_t) i]) }); }
        }
        if (! fin || peak > 2.0f) { ++problems; std::printf ("extreme setting %d peak %.3f <-- PROBLEM\n", s, peak); }
    }
    std::printf ("%d problems\n", problems);
    return problems == 0 ? 0 : 1;
}
