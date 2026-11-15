// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
// Offline check of the DY-1 engine: finite and bounded at extreme settings; the GATE silences a tone below its threshold and passes one above;
// the EXPANDER turns quiet sounds down more with a higher RATIO; the TRANSIENT SHAPER raises the start of a plucked note with ATTACK and lowers
// its tail with SUSTAIN; switched-off stages and MIX 0 leave the sound alone.
#include <cstdio>
#include <vector>
#include "DynamicEngine.h"

static std::vector<float> run (const dy1::Params& p, const std::vector<float>& in, float level = 1.0f)
{
    dy1::Engine e;
    e.prepare (48000.0f);
    std::vector<float> out (in.size());
    std::vector<float> l (512), r (512);
    for (size_t pos = 0; pos < in.size(); pos += 512)
    {
        const int n = (int) std::min<size_t> (512, in.size() - pos);
        for (int i = 0; i < n; ++i) l[(size_t) i] = r[(size_t) i] = in[pos + (size_t) i];
        e.process (l.data(), r.data(), n, p, level);
        for (int i = 0; i < n; ++i) out[pos + (size_t) i] = l[(size_t) i];
    }
    return out;
}
static std::vector<float> tone (float amp, float seconds, float hz = 220.0f)
{
    std::vector<float> v ((size_t) (48000 * seconds));
    for (size_t i = 0; i < v.size(); ++i) v[i] = amp * std::sin (6.2831853f * hz * (float) i / 48000.0f);
    return v;
}
static double rms (const std::vector<float>& v, size_t a, size_t b) { double s = 0; for (size_t i = a; i < b; ++i) s += (double) v[i] * v[i]; return std::sqrt (s / (double) (b - a)); }
static std::vector<float> pluck (float amp, float seconds, float decay = 0.25f)         // a decaying tone with a sharp start
{
    auto v = tone (amp, seconds);
    for (size_t i = 0; i < v.size(); ++i) v[i] *= std::exp (-((float) i / 48000.0f) / decay);
    return v;
}

int main()
{
    int problems = 0;

    // 1. extreme settings stay finite and bounded
    for (int s = 0; s < 4; ++s)
    {
        dy1::Params p;
        p.gateOn = p.expOn = p.transOn = true;
        p.gateThresh = s == 0 ? -80.0f : s == 1 ? -10.0f : -45.0f; p.gateRelease = s == 0 ? 10.0f : 800.0f;
        p.expThresh = s == 1 ? -10.0f : -80.0f; p.expRatio = s == 2 ? 10.0f : 1.0f; p.expRelease = s == 3 ? 800.0f : 10.0f;
        p.attack = s == 0 ? -100.0f : 100.0f; p.sustain = s == 1 ? -100.0f : 100.0f; p.mix = s == 3 ? 0.5f : 1.0f;
        const auto o = run (p, pluck (0.9f, 2.0f));
        float peak = 0; bool finite = true;
        for (float x : o) { if (! std::isfinite (x)) finite = false; peak = std::max (peak, std::abs (x)); }
        std::printf ("setting %d: peak %.3f%s\n", s, peak, finite ? "" : "  NOT FINITE");
        if (! finite || peak > 4.0f) { ++problems; std::printf ("  problem\n"); }
    }

    // 2. gate: a -60 dB tone is silenced, a -30 dB tone passes (threshold -45 dB)
    {
        dy1::Params p; p.gateOn = true; p.expOn = false; p.transOn = false; p.gateThresh = -45.0f; p.gateRelease = 50.0f;
        const auto quiet = run (p, tone (0.001f, 1.0f)), loud = run (p, tone (0.0316f, 1.0f));
        const double q = rms (quiet, 24000, 48000) / (0.001 / 1.4142), l = rms (loud, 24000, 48000) / (0.0316 / 1.4142);
        std::printf ("gate: quiet tone %.3f of its level, loud tone %.3f\n", q, l);
        if (! (q < 0.05 && l > 0.9)) { ++problems; std::printf ("  the gate should silence the quiet tone and pass the loud one\n"); }
    }

    // 3. expander: more reduction with a higher ratio for a sound below the threshold
    {
        dy1::Params p; p.gateOn = false; p.expOn = true; p.transOn = false; p.expThresh = -30.0f; p.expRelease = 50.0f;
        p.expRatio = 1.5f; const auto a = run (p, tone (0.01f, 1.0f));
        p.expRatio = 6.0f; const auto b = run (p, tone (0.01f, 1.0f));
        const double ra = rms (a, 24000, 48000), rb = rms (b, 24000, 48000);
        std::printf ("expander: ratio 1.5 -> %.5f, ratio 6 -> %.5f\n", ra, rb);
        if (! (rb < ra * 0.5)) { ++problems; std::printf ("  a higher ratio should turn the quiet sound down more\n"); }
        p.expRatio = 6.0f; const auto c = run (p, tone (0.5f, 1.0f));
        if (! (rms (c, 24000, 48000) > 0.3)) { ++problems; std::printf ("  a loud sound should pass the expander\n"); }
    }

    // 4. transient shaper: ATTACK raises the start, SUSTAIN lowers the tail
    {
        dy1::Params p; p.gateOn = false; p.expOn = false; p.transOn = true;
        const auto in = pluck (0.5f, 1.5f), flat = run (p, in);
        p.attack = 100.0f; const auto att = run (p, in);
        const double s0 = rms (flat, 0, 1200), s1 = rms (att, 0, 1200);
        p.attack = 0.0f; p.sustain = -100.0f; const auto sus = run (p, in);
        const double t0 = rms (flat, 24000, 36000), t1 = rms (sus, 24000, 36000);
        std::printf ("transient: start %.3f -> %.3f with attack +100, tail %.4f -> %.4f with sustain -100\n", s0, s1, t0, t1);
        if (! (s1 > s0 * 1.2)) { ++problems; std::printf ("  attack should raise the start of the note\n"); }
        if (! (t1 < t0 * 0.7)) { ++problems; std::printf ("  sustain -100 should lower the tail\n"); }
    }

    // 5. all stages off, and MIX 0, leave the sound alone
    {
        dy1::Params p; p.gateOn = p.expOn = p.transOn = false;
        const auto in = pluck (0.5f, 1.0f);
        const auto o = run (p, in);
        double err = 0; for (size_t i = 4800; i < in.size(); ++i) err = std::max (err, (double) std::abs (o[i] - in[i]));
        p.gateOn = p.expOn = p.transOn = true; p.gateThresh = -10.0f; p.mix = 0.0f;
        const auto m = run (p, in);
        double err2 = 0; for (size_t i = 4800; i < in.size(); ++i) err2 = std::max (err2, (double) std::abs (m[i] - in[i]));
        std::printf ("stages off error %.5f, mix 0 error %.5f\n", err, err2);
        if (err > 0.005 || err2 > 0.001) { ++problems; std::printf ("  off / mix 0 should leave the sound alone\n"); }
    }

    std::printf ("%d problems\n", problems);
    return problems == 0 ? 0 : 1;
}
