// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
// Offline check of the IC-1 engine (console app, not part of the normal build): cmake --build <build dir> --target IC1EngineTest, then run it; exit code 0 = pass.
#include <JuceHeader.h>
#include "CircuitEngine.h"
#include <cstdio>
#include "test/EngineTestKit.h"
#include <memory>

namespace
{
using namespace vibr::test;

bool finite (const Buf& b, float limit = 1.001f) { for (float v : b) if (! std::isfinite (v) || std::fabs (v) > limit) return false; return true; }

struct Out { Buf l, r; };
// runs a stereo signal in blocks of 256; `during` is called before each block with its start sample (to change parameters or press EXCITE)
Out run (ic1::Engine& e, Buf inL, Buf inR, ic1::Params p, float mix = 1.0f, float level = 1.0f, bool power = true, const std::function<void (size_t, ic1::Params&)>& during = {})
{
    for (size_t i = 0; i < inL.size(); i += 256)
    {
        if (during) during (i, p);
        const int n = (int) std::min<size_t> (256, inL.size() - i);
        e.process (inL.data() + i, inR.data() + i, n, p, mix, level, power);
    }
    return { inL, inR };
}
Buf impulse (int n, int at, float v = 0.5f) { Buf b ((size_t) n, 0.0f); b[(size_t) at] = v; return b; }
int argmax (const Buf& b, int from, int to) { int best = from; for (int i = from; i < to; ++i) if (std::fabs (b[(size_t) i]) > std::fabs (b[(size_t) best])) best = i; return best; }
ic1::Params dry()                                                                                           // a clean loop: a bright filter, no spread, no extras
{
    ic1::Params p; p.spread = 0.0f; p.cutoff = 16000.0f; p.res = 0.0f; p.regen = 0.5f;
    for (auto& l : p.level) l = 1.0f;
    p.timeMs = 100.0f;
    return p;
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    auto e = std::make_unique<ic1::Engine>();
    e->prepare (fs);
    const int T = 4800;                                                                                     // 100 ms

    // 1. silence in, silence out (also self-oscillating): the circuit needs a push
    {
        auto p = dry(); p.selfOsc = true; p.regen = 1.0f; p.res = 0.5f; p.route = ic1::Mesh;
        const auto o = run (*e, Buf (48000, 0.0f), Buf (48000, 0.0f), p);
        check (rms (o.l, 0, 48000) < 1.0e-9 && rms (o.r, 0, 48000) < 1.0e-9, "silence in gives silence out, also with SELF-OSC", rms (o.l, 0, 48000));
    }
    // 2. the echo comes back after TIME and the next one after twice that, scaled by the regeneration
    {
        e->reset();
        const auto o = run (*e, impulse (48000, 100), Buf (48000, 0.0f), dry());
        const int a = argmax (o.l, 100 + T - 40, 100 + T + 40);
        const double e1 = energy (o.l, a - 20, a + 20), e2 = energy (o.l, a + T - 20, a + T + 20);
        check (std::abs (a - (100 + T)) <= 6 && e1 > 1.0e-5, "the first echo comes back after TIME (100 ms)", a - (100 + T));
        check (e2 / e1 > 0.12 && e2 / e1 < 0.4, "the second echo has about the loop gain squared in energy (REGEN 50 %)", e2 / e1);
    }
    // 3. another sample rate keeps the time
    {
        auto e2 = std::make_unique<ic1::Engine>();
        fs = 96000.0f; e2->prepare (fs);
        const auto o = run (*e2, impulse (96000, 100), Buf (96000, 0.0f), dry());
        const int a = argmax (o.l, 100 + 9600 - 80, 100 + 9600 + 80);
        check (std::abs (a - (100 + 9600)) <= 8, "96 kHz: the echo comes back after 100 ms", a - (100 + 9600));
        fs = 48000.0f;
    }
    // 4. ping-pong: a sound in the left channel goes right with the second echo
    {
        e->reset();
        auto p = dry(); p.route = ic1::PingPong; p.inject = ic1::InSplit;
        const auto o = run (*e, impulse (48000, 100), Buf (48000, 0.0f), p);
        const double l1 = energy (o.l, 100 + T - 30, 100 + T + 30), r1 = energy (o.r, 100 + T - 30, 100 + T + 30), r2 = energy (o.r, 100 + 2 * T - 30, 100 + 2 * T + 30), l2 = energy (o.l, 100 + 2 * T - 30, 100 + 2 * T + 30);
        check (l1 > 100.0 * r1 && r2 > 100.0 * l2 && r2 > 1.0e-6, "PING-PONG: the first echo is left, the second one is right", r2 / std::max (1.0e-12, l2));
    }
    // 5. cross-feedback swaps the channels like the ping-pong routing
    {
        e->reset();
        auto p = dry(); p.route = ic1::Parallel; p.inject = ic1::InSplit; p.cross = 1.0f;
        const auto o = run (*e, impulse (48000, 100), Buf (48000, 0.0f), p);
        const double r2 = energy (o.r, 100 + 2 * T - 30, 100 + 2 * T + 30), l2 = energy (o.l, 100 + 2 * T - 30, 100 + 2 * T + 30);
        check (r2 > 100.0 * l2 && r2 > 1.0e-6, "CROSS 100 % sends the second echo to the other channel", r2 / std::max (1.0e-12, l2));
    }
    // 6. every routing matrix has a spectral norm of at most 1
    {
        bool ok = true; double worst = 0.0;
        for (int route = 0; route < ic1::kRoutes; ++route)
            for (const float cross : { 0.0f, 0.3f, 0.7f, 1.0f })
            {
                float R[4][4]; ic1::routingMatrix (route, cross, R);
                double v[4] = { 1, 0.3, -0.7, 0.5 }, norm = 0.0;
                for (int it = 0; it < 200; ++it)
                {
                    double w[4] {}, z[4] {};
                    for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) w[i] += R[i][j] * v[j];
                    for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) z[i] += R[j][i] * w[j];                    // R^T R v
                    norm = 0.0; for (double x : z) norm += x * x; norm = std::sqrt (norm);
                    for (int i = 0; i < 4; ++i) v[i] = z[i] / std::max (1.0e-12, norm);
                }
                worst = std::max (worst, std::sqrt (norm));
                if (std::sqrt (norm) > 1.001) ok = false;
            }
        check (ok, "all routings (with every CROSS) have a spectral norm of at most 1", worst);
    }
    // 7. below 100 % the loop dies away, in every routing
    {
        bool ok = true; double worst = 0.0;
        for (int route = 0; route < ic1::kRoutes; ++route)
        {
            e->reset();
            auto p = dry(); p.route = route; p.regen = 1.0f; p.timeMs = 70.0f; p.spread = 1.0f;
            const Buf in = noise (48000 / 2, 0.4f);
            Buf l = in, r = noise (48000 / 2, 0.4f, 9); l.resize (48000 * 14, 0.0f); r.resize (48000 * 14, 0.0f);
            const auto o = run (*e, l, r, p);
            const double early = rms (o.l, 48000, 48000 * 2), late = rms (o.l, 48000 * 12, 48000 * 13);
            worst = std::max (worst, late / std::max (1.0e-9, early));
            if (! (late < early * 0.25) || ! finite (o.l) || ! finite (o.r)) { ok = false; std::printf ("   %s: early %g late %g\n", ic1::routeName (route), early, late); }
        }
        check (ok, "REGEN 100 % without SELF-OSC still dies away (all six routings)", worst);
    }
    // 8. self-oscillation: EXCITE starts it, it stays bounded and keeps going
    {
        e->reset();
        auto p = dry(); p.route = ic1::Mesh; p.selfOsc = true; p.regen = 1.0f; p.res = 0.5f; p.cutoff = 2000.0f; p.timeMs = 60.0f;
        for (auto& l : p.level) l = 1.0f;
        const auto o = run (*e, Buf (48000 * 20, 0.0f), Buf (48000 * 20, 0.0f), p, 1.0f, 1.0f, true, [&] (size_t i, ic1::Params&) { if (i == 256) e->requestExcite(); });
        const double r1 = rms (o.l, 48000 * 3, 48000 * 4), r2 = rms (o.l, 48000 * 18, 48000 * 19);
        check (r1 > 0.02 && r2 > 0.02 && finite (o.l) && finite (o.r), "SELF-OSC: EXCITE starts a sound that goes on and stays bounded", r2);
    }
    // 9. HOLD keeps the loop alive after the input stopped
    {
        e->reset();
        auto p = dry(); p.regen = 0.85f;
        Buf in = noise (48000 * 3, 0.3f); for (int i = 24000; i < (int) in.size(); ++i) in[(size_t) i] = 0.0f;
        const auto o = run (*e, in, in, p, 1.0f, 1.0f, true, [&] (size_t i, ic1::Params& q) { q.hold = i >= 24000; });
        e->reset();
        const auto no = run (*e, in, in, p);
        const double ref = rms (o.l, 24000 + 4800, 24000 + 9600), held = rms (o.l, 48000 * 2, 48000 * 2 + 9600), free = rms (no.l, 48000 * 2, 48000 * 2 + 9600);
        check (held > 0.25 * ref && held > 5.0 * free, "HOLD: the loop keeps its level for seconds while it dies without", held / std::max (1.0e-9, free));
    }
    // 10. the filter shapes the feedback: a band-pass leaves its band
    {
        e->reset();
        auto p = dry(); p.filter = ic1::BandPass; p.cutoff = 1000.0f; p.res = 0.6f; p.regen = 0.95f; p.timeMs = 50.0f; p.spread = 1.0f;
        const Buf in = noise (48000 * 3, 0.3f);
        const auto o = run (*e, in, in, p);
        const double band = bandRms (o.l, 48000 * 2, 850.0f, 1150.0f), low = bandRms (o.l, 48000 * 2, 100.0f, 250.0f), high = bandRms (o.l, 48000 * 2, 3000.0f, 4500.0f);
        check (band > 3.0 * low && band > 3.0 * high, "BAND PASS at 1 kHz: the feedback gathers around the cutoff", band / std::max (low, high));
    }
    // 11. phase and modulation change the sound inside the loop
    {
        const Buf in = noise (48000 * 2, 0.3f);
        auto base = dry(); base.regen = 0.9f;
        e->reset(); const auto a = run (*e, in, in, base);
        auto ph = base; ph.phase = 1.0f; e->reset(); const auto b = run (*e, in, in, ph);
        auto md = base; md.depth = 0.8f; md.rate = 1.5f; e->reset(); const auto c = run (*e, in, in, md);
        double dab = 0.0, dac = 0.0; for (size_t i = 24000; i < a.l.size(); ++i) { dab += std::fabs (a.l[i] - b.l[i]); dac += std::fabs (a.l[i] - c.l[i]); }
        check (dab / a.l.size() > 0.005 && dac / a.l.size() > 0.005 && finite (b.l) && finite (c.l), "PHASE and the modulation change the sound and stay bounded", dab / a.l.size());
    }
    // 12. the node levels are published
    {
        e->reset();
        auto p = dry(); p.regen = 0.9f;
        run (*e, noise (48000, 0.5f), noise (48000, 0.5f, 8), p);
        check (e->nodeLevel (0) > 0.02f && e->nodeLevel (3) > 0.02f, "the node levels are published for the display", e->nodeLevel (0));
    }
    // 13. MIX 0 and power off pass the input bit exact
    {
        e->reset();
        const Buf in = noise (48000, 0.4f);
        auto p = dry(); p.regen = 0.95f; p.drive = 1.0f; p.phase = 1.0f;
        const auto a = run (*e, in, in, p, 0.0f);
        e->reset();
        const auto b = run (*e, in, in, p, 1.0f, 2.0f, false);
        bool exact = true; for (size_t i = 0; i < in.size(); ++i) if (a.l[i] != in[i] || b.r[i] != in[i]) { exact = false; break; }
        check (exact, "MIX 0 and power off pass the input bit exact (also with LEVEL up)");
    }
    // 14. extremes: finite and bounded
    {
        juce::Random rng (11);
        e->reset();
        bool ok = true;
        for (int round = 0; round < 20 && ok; ++round)
        {
            ic1::Params p;
            p.route = rng.nextInt (6); p.inject = rng.nextInt (3); p.filter = rng.nextInt (3);
            p.timeMs = 5.0f + rng.nextFloat() * 1495.0f; p.spread = rng.nextFloat(); for (auto& l : p.level) l = rng.nextFloat();
            p.cross = rng.nextFloat(); p.regen = rng.nextFloat(); p.selfOsc = rng.nextBool(); p.hold = rng.nextInt (4) == 0;
            p.cutoff = 40.0f + rng.nextFloat() * 15960.0f; p.res = rng.nextFloat(); p.tilt = rng.nextFloat(); p.phase = rng.nextFloat();
            p.rate = 0.02f + rng.nextFloat() * 8.0f; p.depth = rng.nextFloat(); p.drive = rng.nextFloat();
            const auto o = run (*e, noise (48000 / 2, 0.9f, round), noise (48000 / 2, 0.9f, round + 50), p, rng.nextFloat(), 1.0f, true, [&] (size_t i, ic1::Params&) { if (i == 0 && round % 5 == 0) e->requestExcite(); });
            ok = finite (o.l, 1.001f) && finite (o.r, 1.001f);
        }
        check (ok, "20 random settings at the extremes: output finite and never above 0 dBFS");
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
