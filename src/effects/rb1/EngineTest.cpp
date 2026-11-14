// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
// Offline check of the RB-1 engine: CLASSIC equals the BS-1 engine, SPACE the SP-1 and RESONATE the SR-1 (their engines are run next to it); the RT60 of the classic
// types against DECAY; PRE-DELAY delays the tail by the set time; DUCK dips the wet sound while the input plays; FREEZE holds the SPACE tail and stops the input of the
// other modes; RESONATE strings are tuned to the scale; MIX 0 is bit-exact; every setting stays finite; silence in, silence out.
#include <cstdio>
#include <random>
#include <vector>
#include "ReverbLabEngine.h"

namespace
{
int problems = 0;
void check (bool ok, const char* what) { std::printf ("  %-96s %s\n", what, ok ? "ok" : "FAILED"); if (! ok) ++problems; }
const float fs = 48000.0f;

std::vector<float> noise (int n, float amp, unsigned seed = 1)
{
    std::mt19937 rng (seed); std::uniform_real_distribution<float> u (-1.0f, 1.0f);
    std::vector<float> v ((size_t) n); for (auto& x : v) x = amp * u (rng);
    return v;
}
std::vector<float> run (const rb1::Params& p, const std::vector<float>& inL, const std::vector<float>& inR, std::vector<float>* outR = nullptr, float level = 1.0f)
{
    rb1::Engine e; e.prepare (fs);
    auto l = inL, r = inR;
    for (size_t pos = 0; pos < l.size(); pos += 480) { const int n = (int) std::min<size_t> (480, l.size() - pos); e.process (l.data() + pos, r.data() + pos, n, p, level); }
    if (outR) *outR = r;
    return l;
}
double rms (const std::vector<float>& v, int a, int b) { double s = 0; for (int i = std::max (0, a); i < std::min ((int) v.size(), b); ++i) s += (double) v[(size_t) i] * v[(size_t) i]; return std::sqrt (s / std::max (1, b - a)); }
double diffDb (const std::vector<float>& a, const std::vector<float>& b, int from = 100)
{
    double e = 0, r = 0; for (size_t i = (size_t) from; i < a.size(); ++i) { const double d = (double) a[i] - b[i]; e += d * d; r += (double) b[i] * b[i]; }
    return 10.0 * std::log10 (std::max (1e-30, e / std::max (1e-30, r)));
}
// the RT60 from the Schroeder backward integral of an impulse response (T20 extrapolated: -5 .. -25 dB)
double rt60 (const std::vector<float>& h)
{
    std::vector<double> e (h.size()); double s = 0; for (int i = (int) h.size() - 1; i >= 0; --i) { s += (double) h[(size_t) i] * h[(size_t) i]; e[(size_t) i] = s; }
    int a = -1, b = -1; for (size_t i = 0; i < e.size(); ++i) { const double d = 10.0 * std::log10 (e[i] / e[0]); if (a < 0 && d <= -5.0) a = (int) i; if (b < 0 && d <= -25.0) { b = (int) i; break; } }
    return a >= 0 && b > a ? 3.0 * (double) (b - a) / fs : -1.0;
}
double goertzel (const std::vector<float>& v, int a, int b, float hz)
{
    double re = 0, im = 0; const int len = b - a;
    for (int i = 0; i < len; ++i) { const double w = 0.5 - 0.5 * std::cos (6.2831853 * i / len), ph = 6.2831853 * hz * (a + i) / fs; re += w * v[(size_t) (a + i)] * std::cos (ph); im += w * v[(size_t) (a + i)] * std::sin (ph); }
    return std::sqrt (re * re + im * im) / len;
}
}

int main()
{
    const auto in = noise ((int) (3.0f * fs), 0.2f, 3);
    std::printf ("parity with the source plugins\n");
    {
        rb1::Params p; p.mode = rb1::Classic; p.variant = 1; p.decay = 0.4f; p.size = 0.6f; p.tone = 0.5f; p.motion = 0.3f; p.param2 = 0.4f; p.value = 0.5f; p.preDelayMs = 0.0f; p.mix = 1.0f;
        const auto o = run (p, in, in);
        bigsky::Reverb e; e.prepare (fs); auto l = in, r = in; bigsky::Params q; q.type = 1; q.decay = 0.4f; q.predelay = 0.0f; q.mix = 1.0f; q.tone = 0.5f; q.p1 = 0.6f; q.p2 = 0.4f; q.mod = 0.3f; q.value = 0.5f;
        for (size_t pos = 0; pos < l.size(); pos += 480) { const int n = (int) std::min<size_t> (480, l.size() - pos); e.process (l.data() + pos, r.data() + pos, n, q); }
        const double d = diffDb (o, l, 2000); std::printf ("  CLASSIC hall against the BS-1: %.1f dB\n", d);
        check (d < -70.0, "CLASSIC equals the BS-1 engine");
    }
    {
        rb1::Params p; p.mode = rb1::Space; p.variant = 0; p.decay = 0.5f; p.size = 0.5f; p.diffusion = 0.6f; p.tone = 0.5f; p.motion = 0.4f; p.texture = 0.5f; p.preDelayMs = 0.0f; p.mix = 1.0f;
        const auto o = run (p, in, in);
        sp1::Engine e; e.prepare (fs); auto l = in, r = in; sp1::Params q; q.mode = 0; q.size = 0.5f; q.decay = 0.5f; q.diffusion = 0.6f; q.tone = 0.5f; q.motion = 0.4f; q.texture = 0.5f;
        for (size_t pos = 0; pos < l.size(); pos += 480) { const int n = (int) std::min<size_t> (480, l.size() - pos); e.process (l.data() + pos, r.data() + pos, n, q, 1.0f, 1.0f); }
        const double d = diffDb (o, l, 2000); std::printf ("  SPACE shimmer against the SP-1: %.1f dB\n", d);
        check (d < -70.0, "SPACE equals the SP-1 engine");
    }
    {
        rb1::Params p; p.mode = rb1::Resonate; p.variant = 1; p.decay = 0.55f; p.size = 0.6f; p.damping = 0.4f; p.tension = 0.25f; p.motion = 0.3f; p.fine = 0.5f; p.root = 0; p.scale = 2; p.blend = 0.8f; p.preDelayMs = 0.0f; p.mix = 1.0f;
        const auto o = run (p, in, in);
        sr1::Engine e; e.prepare (fs); auto l = in, r = in; sr1::Params q; q.root = 0; q.scale = 2; q.texture = 1; q.strings = 0.6f; q.decay = 0.55f; q.damping = 0.4f; q.tension = 0.25f; q.motion = 0.3f; q.fine = 0.5f;
        for (size_t pos = 0; pos < l.size(); pos += 480) { const int n = (int) std::min<size_t> (480, l.size() - pos); e.process (l.data() + pos, r.data() + pos, n, q, 0.8f, 1.0f); }
        const double d = diffDb (o, l, 2000); std::printf ("  RESONATE against the SR-1: %.1f dB\n", d);
        check (d < -70.0, "RESONATE equals the SR-1 engine");
    }

    std::printf ("decay, pre-delay, duck, freeze\n");
    for (int type : { 0, 1, 2 })
    {
        for (float decay : { 0.3f, 0.6f })
        {
            std::vector<float> click ((int) (40.0f * fs), 0.0f); click[(size_t) (0.5f * fs)] = 0.5f;                  // after the type has settled
            rb1::Params p; p.mode = rb1::Classic; p.variant = type; p.decay = decay; p.preDelayMs = 0.0f; p.mix = 1.0f; p.motion = 0.0f;
            auto h = run (p, click, click); h.erase (h.begin(), h.begin() + (int) (0.5f * fs));
            const double rt = rt60 (h), want = (double) bigsky::decaySeconds (type, decay);
            std::printf ("  %s decay %.1f: RT60 %.2f s (the knob says %.2f s)\n", rb1::classicLongName (type), (double) decay, rt, want);
            check (rt > 0 && std::abs (rt - want) / want < 0.35, "CLASSIC: the RT60 follows DECAY (within 35 %)");
        }
    }
    {
        // PRE-DELAY: with the modulation off the engine is time-invariant, so the 100 ms output is the 0 ms output 100 ms later
        std::vector<float> click ((int) (3.0f * fs), 0.0f); click[(size_t) (0.5f * fs)] = 0.5f;
        rb1::Params p; p.mode = rb1::Classic; p.variant = 0; p.decay = 0.3f; p.mix = 1.0f; p.preDelayMs = 0.0f; p.motion = 0.0f;
        const auto a = run (p, click, click); p.preDelayMs = 100.0f; const auto b = run (p, click, click);
        double best = -1e9; int bestShift = 0;
        for (int sh = 4000; sh <= 5600; sh += 4) { std::vector<float> s2 (a.size(), 0.0f); for (size_t i = 0; i + (size_t) sh < a.size(); ++i) s2[i + (size_t) sh] = a[i]; const double d = -diffDb (b, s2, (int) (0.4f * fs)); if (d > best) { best = d; bestShift = sh; } }
        std::printf ("  pre-delay: the best match is the 0 ms tail shifted by %d samples (%.1f ms), %.1f dB\n", bestShift, bestShift * 1000.0 / fs, -best);
        check (std::abs (bestShift * 1000.0 / fs - 100.0) < 1.0 && best > 30.0, "PRE-DELAY 100 ms delays the tail by 100 ms");
    }
    {
        const auto loud = noise ((int) (2.0f * fs), 0.5f, 9);
        rb1::Params p; p.mode = rb1::Classic; p.variant = 0; p.mix = 1.0f; p.preDelayMs = 0.0f; p.decay = 0.3f;
        const auto plain = run (p, loud, loud); p.duck = 1.0f; const auto ducked = run (p, loud, loud);
        const double ratio = rms (ducked, (int) fs, (int) (2.0f * fs)) / rms (plain, (int) fs, (int) (2.0f * fs));
        std::printf ("  DUCK 100 %%: the wet level is %.2f of the undamped one while loud noise plays\n", ratio);
        check (ratio < 0.3, "DUCK dips the wet sound while the input plays");
    }
    {
        // FREEZE: SPACE holds the tail; the other modes stop the input
        std::vector<float> burst ((int) (30.0f * fs), 0.0f); for (int i = 0; i < 4800; ++i) burst[(size_t) i] = 0.4f * std::sin (6.2831853f * 220.0f * (float) i / fs);
        rb1::Params p; p.mode = rb1::Space; p.variant = 1; p.mix = 1.0f; p.preDelayMs = 0.0f; p.decay = 0.4f; p.freeze = true;
        // freeze switched on right at the start of the tail
        rb1::Engine e; e.prepare (fs); auto l = burst, r = burst; rb1::Params q = p; q.freeze = false;
        e.process (l.data(), r.data(), 4800, q, 1.0f);
        q.freeze = true; for (size_t pos = 4800; pos < l.size(); pos += 480) { const int n = (int) std::min<size_t> (480, l.size() - pos); e.process (l.data() + pos, r.data() + pos, n, q, 1.0f); }
        const double a = rms (l, (int) (3.0f * fs), (int) (4.0f * fs)), b = rms (l, (int) (25.0f * fs), (int) (26.0f * fs));
        std::printf ("  FREEZE (SPACE, infinite): the tail %.1f dB at 3 s and %.1f dB at 25 s\n", 20.0 * std::log10 (a), 20.0 * std::log10 (b));
        check (std::abs (20.0 * std::log10 (b / a)) < 3.0, "FREEZE holds the SPACE tail (within 3 dB over 22 s)");
        rb1::Params c; c.mode = rb1::Classic; c.variant = 0; c.mix = 1.0f; c.preDelayMs = 0.0f; c.freeze = true;
        std::vector<float> loud = noise ((int) fs, 0.5f, 4); const auto o = run (c, loud, loud);
        check (rms (o, (int) (0.5f * fs), (int) fs) < 1e-6, "FREEZE mutes the input of CLASSIC (nothing new enters the tail)");
    }
    std::printf ("resonator, robustness\n");
    {
        // one string at C3 (131 Hz): excite with noise, listen to the ring: the strongest line within 5 cents
        rb1::Params p; p.mode = rb1::Resonate; p.variant = 2; p.size = 0.0f; p.decay = 1.0f; p.damping = 0.0f; p.tension = 0.0f; p.motion = 0.0f; p.fine = 0.5f; p.root = 0; p.scale = 2; p.blend = 1.0f; p.mix = 1.0f; p.preDelayMs = 0.0f;
        std::vector<float> x ((int) (6.0f * fs), 0.0f); const auto nz = noise ((int) (0.5f * fs), 0.5f, 6); std::copy (nz.begin(), nz.end(), x.begin());
        const auto o = run (p, x, x);
        const float want = sr1::noteHz (sr1::stringNote (0, 0, 2), 0.0f);
        double best = 0, bestCents = 0; for (float c = -50.0f; c <= 50.0f; c += 1.0f) { const double g = goertzel (o, (int) (1.0f * fs), (int) (5.0f * fs), want * std::pow (2.0f, c / 1200.0f)); if (g > best) { best = g; bestCents = c; } }
        std::printf ("  RESONATE string 1: strongest line at %.0f cents from C3 (%.1f Hz)\n", bestCents, (double) want);
        check (std::abs (bestCents) <= 5.0, "RESONATE strings are tuned to the scale (within 5 cents)");
    }
    {
        bool ok = true; const auto x = noise ((int) fs, 0.8f, 11);
        for (int m = 0; m < rb1::kNumModes; ++m) for (int v = 0; v < (m == 0 ? 12 : m == 1 ? 5 : 3); ++v) for (int s = 0; s < 3; ++s)
        {
            rb1::Params p; p.mode = m; p.variant = v; p.decay = s == 0 ? 0.0f : 1.0f; p.size = s == 1 ? 1.0f : 0.0f; p.tone = s == 1 ? 1.0f : 0.0f; p.motion = 1.0f; p.preDelayMs = s == 2 ? 250.0f : 0.0f;
            p.duck = s == 2 ? 1.0f : 0.0f; p.freeze = s == 2; p.mix = 1.0f; p.param2 = s == 1 ? 1.0f : 0.0f; p.value = 1.0f; p.diffusion = 1.0f; p.texture = 1.0f; p.damping = 1.0f; p.tension = 1.0f; p.blend = 1.0f;
            p.root = s * 4; p.scale = s * 2;
            const auto o = run (p, x, x, nullptr, 4.0f); float peak = 0; bool fin = true; for (float y : o) { if (! std::isfinite (y)) fin = false; peak = std::max (peak, std::abs (y)); }
            if (! fin || peak > 100.0f) { ok = false; std::printf ("    %s %d setting %d: peak %.2f%s\n", rb1::modeName (m), v, s, peak, fin ? "" : " NOT FINITE"); }
        }
        check (ok, "every mode, type and extreme setting stays finite and bounded");
        rb1::Params p; p.mix = 0.0f; const auto o = run (p, in, in);
        check (o == in, "MIX 0 passes the input bit-exact");
        std::vector<float> silent ((int) fs, 0.0f); const auto so = run (p, silent, silent); double e = 0; for (float y : so) e += (double) y * y;
        check (e == 0.0, "silence in, silence out");
    }
    std::printf ("%d problems\n", problems);
    return problems == 0 ? 0 : 1;
}
