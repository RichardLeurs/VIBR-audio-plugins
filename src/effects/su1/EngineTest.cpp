// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
// Offline check of the SU-1 engine: finite output; GAIN scales and -60 silences; PAN is an equal-loudness balance (centre unchanged, hard left
// silences the right); WIDTH 0 makes the sound mono and 200 % doubles the sides; BASS MONO removes the bass from the sides only; INVERT, SWAP,
// MONO and the MID / SIDE listening aids do what they say; the correlation meter reads +1 for mono and -1 for opposite channels.
#include <cstdio>
#include <vector>
#include "UtilityEngine.h"

struct Out { std::vector<float> l, r; float corr = 1.0f; };

static Out run (const su1::Params& p, float hzL, float ampL, float hzR, float ampR, float phaseR = 0.0f, float seconds = 1.0f, float fs = 48000.0f)
{
    Out o;
    su1::Engine e;
    e.prepare (fs);
    const int total = (int) (fs * seconds);
    o.l.resize ((size_t) total); o.r.resize ((size_t) total);
    std::vector<float> l (512), r (512);
    for (int pos = 0; pos < total; pos += 512)
    {
        const int n = std::min (512, total - pos);
        for (int i = 0; i < n; ++i)
        {
            const float t = (float) (pos + i) / fs;
            l[(size_t) i] = ampL * std::sin (6.2831853f * hzL * t);
            r[(size_t) i] = ampR * std::sin (6.2831853f * hzR * t + phaseR);
        }
        e.process (l.data(), r.data(), n, p);
        for (int i = 0; i < n; ++i) { o.l[(size_t) (pos + i)] = l[(size_t) i]; o.r[(size_t) (pos + i)] = r[(size_t) i]; }
    }
    o.corr = e.corr.load();
    return o;
}
static double rms (const std::vector<float>& v, size_t a = 24000, size_t b = 48000) { double s = 0; for (size_t i = a; i < b; ++i) s += (double) v[i] * v[i]; return std::sqrt (s / (double) (b - a)); }

int main()
{
    int problems = 0;
    const auto check = [&] (bool ok, const char* msg) { if (! ok) { ++problems; std::printf ("  PROBLEM: %s\n", msg); } };
    su1::Params base;

    // 1. neutral: nothing changes
    {
        const auto o = run (base, 440.0f, 0.5f, 660.0f, 0.4f);
        double err = 0; for (size_t i = 24000; i < o.l.size(); ++i) err = std::max (err, (double) std::abs (o.l[i] - 0.5f * std::sin (6.2831853f * 440.0f * (float) i / 48000.0f)));
        std::printf ("neutral error %.5f\n", err);
        check (err < 0.002, "default settings should leave the sound unchanged");
    }

    // 2. gain
    {
        su1::Params p = base; p.gainDb = -6.0f;
        const auto o = run (p, 440.0f, 0.5f, 440.0f, 0.5f);
        const double g = rms (o.l) / (0.5 / 1.4142);
        std::printf ("gain -6 dB: %.3f (expect 0.501)\n", g);
        check (std::abs (g - 0.501) < 0.01, "-6 dB should halve the level");
        p.gainDb = -60.0f; const auto q = run (p, 440.0f, 0.5f, 440.0f, 0.5f);
        check (rms (q.l) < 1e-5, "-60 should silence");
    }

    // 3. pan: centre unity, hard left silences the right
    {
        su1::Params p = base; p.pan = -1.0f;
        const auto o = run (p, 440.0f, 0.5f, 440.0f, 0.5f);
        std::printf ("hard left: L %.3f R %.5f\n", rms (o.l), rms (o.r));
        check (rms (o.r) < 1e-4 && rms (o.l) > 0.3, "hard left should silence the right channel and keep the left");
        p.pan = 0.5f; const auto h = run (p, 440.0f, 0.5f, 440.0f, 0.5f);
        check (rms (h.r) > 0.34 && rms (h.l) < 0.25 && rms (h.l) > 0.1, "pan right of centre should keep the right and turn the left down");
    }

    // 4. width: 0 = mono, 200 % doubles the sides
    {
        su1::Params p = base; p.width = 0.0f;
        const auto o = run (p, 440.0f, 0.5f, 440.0f, 0.5f, 2.0f);        // right channel shifted by about 115 degrees: plenty of side
        double diff = 0; for (size_t i = 24000; i < o.l.size(); ++i) diff = std::max (diff, (double) std::abs (o.l[i] - o.r[i]));
        std::printf ("width 0: max difference L - R %.5f\n", diff);
        check (diff < 1e-3, "width 0 should make L equal R");
        su1::Params w1 = base, w2 = base; w2.width = 2.0f;
        const auto a = run (w1, 440.0f, 0.5f, 440.0f, 0.5f, 2.0f), b = run (w2, 440.0f, 0.5f, 440.0f, 0.5f, 2.0f);
        double sa = 0, sb = 0; for (size_t i = 24000; i < a.l.size(); ++i) { sa += std::pow (a.l[i] - a.r[i], 2.0); sb += std::pow (b.l[i] - b.r[i], 2.0); }
        std::printf ("side energy ratio at 200%%: %.2f (expect 4)\n", sb / sa);
        check (std::abs (sb / sa - 4.0) < 0.3, "200 % should double the side level");
    }

    // 5. bass mono: a 60 Hz out-of-phase tone is removed, a 2 kHz one is not
    {
        su1::Params p = base; p.bassMono = 200.0f;
        const auto lo = run (p, 60.0f, 0.5f, 60.0f, 0.5f, 3.14159f), hi = run (p, 2000.0f, 0.5f, 2000.0f, 0.5f, 3.14159f);
        std::printf ("bass mono 200 Hz: 60 Hz side level %.3f (was 0.354), 2 kHz side level %.3f\n", rms (lo.l), rms (hi.l));
        check (rms (lo.l) < 0.12 && rms (hi.l) > 0.3, "bass mono should remove the low side signal and keep the high one");
    }

    // 6. channels
    {
        su1::Params p = base; p.invL = true;
        const auto o = run (p, 440.0f, 0.5f, 660.0f, 0.5f);
        check (std::abs (o.l[30000] + 0.5f * std::sin (6.2831853f * 440.0f * 30000.0f / 48000.0f)) < 0.01f, "invert L should flip the left channel");
        p = base; p.swap = true;
        const auto s = run (p, 440.0f, 0.5f, 660.0f, 0.2f);
        check (rms (s.l) < rms (s.r) * 1.0 + 1.0 && std::abs (rms (s.l) - 0.2 / 1.4142) < 0.01, "swap should exchange the channels");
        p = base; p.mono = true;
        const auto m = run (p, 440.0f, 0.5f, 660.0f, 0.2f);
        double d = 0; for (size_t i = 24000; i < m.l.size(); ++i) d = std::max (d, (double) std::abs (m.l[i] - m.r[i]));
        check (d < 1e-4, "mono should make L equal R");
        p = base; p.solo = 2;
        const auto side = run (p, 440.0f, 0.5f, 440.0f, 0.5f);          // identical channels: no side at all
        check (rms (side.l) < 1e-3, "solo side of a mono signal should be silent");
        p.solo = 1; const auto mid = run (p, 440.0f, 0.5f, 440.0f, 0.5f);
        check (std::abs (rms (mid.l) - 0.5 / 1.4142) < 0.01, "solo mid of a mono signal should be unchanged");
    }

    // 7. correlation meter
    {
        const auto a = run (base, 440.0f, 0.5f, 440.0f, 0.5f), b = run (base, 440.0f, 0.5f, 440.0f, 0.5f, 3.14159f);
        std::printf ("correlation: mono %.2f, opposite %.2f\n", a.corr, b.corr);
        check (a.corr > 0.98 && b.corr < -0.98, "correlation should be +1 for mono and -1 for opposite channels");
    }

    // 8. extremes stay finite
    {
        su1::Params p; p.invL = p.swap = true; p.width = 2.0f; p.spread = 1.0f; p.bassMono = 300.0f; p.pan = 1.0f; p.autoDepth = 1.0f; p.autoRate = 10.0f; p.gainDb = 12.0f;
        const auto o = run (p, 100.0f, 0.9f, 3000.0f, 0.9f, 1.0f, 2.0f);
        bool ok = true; float peak = 0; for (size_t i = 0; i < o.l.size(); ++i) { if (! std::isfinite (o.l[i]) || ! std::isfinite (o.r[i])) ok = false; peak = std::max ({ peak, std::abs (o.l[i]), std::abs (o.r[i]) }); }
        std::printf ("extremes: peak %.2f\n", peak);
        check (ok && peak < 12.0f, "extreme settings should stay finite and bounded");
    }

    std::printf ("%d problems\n", problems);
    return problems == 0 ? 0 : 1;
}
