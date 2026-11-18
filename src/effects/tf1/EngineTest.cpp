// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
// Offline check of the TF-1 engine (console app, not part of the normal build): cmake --build <build dir> --target TF1EngineTest, then run it; exit code 0 = pass.
#include <JuceHeader.h>
#include "TimeEngine.h"
#include <cstdio>
#include "test/EngineTestKit.h"
#include <memory>
#include <set>

namespace
{
using namespace vibr::test;

bool finite (const Buf& b, float limit = 5.0f) { for (float v : b) if (! std::isfinite (v) || std::fabs (v) > limit) return false; return true; }

tf1::Params pattern (int op)
{
    tf1::Params p;
    for (auto& o : p.ops) o = op;
    return p;
}
// runs the stereo-identical signal in blocks of 256 (free running clock at 120 bpm, 1/16: 6000 samples per step) and returns the left output
Buf run (tf1::Engine& e, const Buf& in, const tf1::Params& p, float mix = 1.0f, float level = 1.0f, bool power = true, tf1::Host host = {})
{
    Buf l = in, r = in;
    for (size_t i = 0; i < l.size(); i += 256)
    {
        const int n = (int) std::min<size_t> (256, l.size() - i);
        e.process (l.data() + i, r.data() + i, n, p, mix, level, power, host);
    }
    return l;
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    auto e = std::make_unique<tf1::Engine>();
    e->prepare (fs);
    constexpr int S = 6000;                                                                                   // one step: 120 bpm, 1/16

    // 1. a pattern of PLAY steps is the input, bit exact, with no latency
    {
        const Buf in = noise (fs, 0.4f), out = run (*e, in, pattern (tf1::Play));
        bool exact = true; for (size_t i = 0; i < in.size(); ++i) if (std::fabs (out[i] - in[i]) > 1.0e-7f) { exact = false; break; }
        check (exact, "all PLAY: the output is the input (no latency)");
        check (std::fabs (e->stepLengthSamples() - (float) S) < 1.0f, "a step is 6000 samples at 120 bpm, 1/16", e->stepLengthSamples());
    }
    // 2. MIX 0 and power off pass the input, whatever the pattern does
    {
        e->reset();
        auto p = pattern (tf1::Repeat); p.chance = 1.0f; p.crush = 1.0f;
        const Buf in = noise (fs, 0.4f), a = run (*e, in, p, 0.0f);
        e->reset();
        const Buf b = run (*e, in, p, 1.0f, 2.0f, false);
        bool exact = true; for (size_t i = 0; i < in.size(); ++i) if (a[i] != in[i] || (i > 4800 && b[i] != in[i])) { exact = false; break; }
        check (exact, "MIX 0 and power off (after its 20 ms crossfade) pass the input bit exact, also with LEVEL up");
    }
    // 3. REPEAT loops a piece: the output repeats with the loop length
    {
        e->reset();
        auto p = pattern (tf1::Repeat); p.loop = 0.4f;                                                        // 1/8 of a step: 750 samples
        const Buf out = run (*e, noise (S * 6, 0.3f), p);
        double err = 0.0; int n = 0;
        for (int i = 3 * S + 750 + 100; i < 4 * S - 100; ++i) { err = std::max (err, (double) std::fabs (out[(size_t) i] - out[(size_t) (i - 750)])); ++n; }
        check (err < 1.0e-5 && rms (out, 3 * S + 100, 4 * S - 100) > 0.05, "REPEAT loops 1/8 of the step: the output repeats every 750 samples", err);
    }
    // 4. REVERSE plays the step before backwards
    {
        e->reset();
        tf1::Params p; p.ops[1] = tf1::Reverse;                                                               // step 1 reverses, the others play
        Buf in ((size_t) S * 3, 0.0f); in[5000] = 0.5f;                                                       // an impulse 1000 samples before the end of step 0
        const Buf out = run (*e, in, p);
        int at = -1; float best = 0.0f; for (int i = S; i < 2 * S; ++i) if (std::fabs (out[(size_t) i]) > best) { best = std::fabs (out[(size_t) i]); at = i; }
        check (at == S + 999 && best > 0.45f, "REVERSE mirrors the step before: an impulse at 5000 comes back at 6999", at);
    }
    // 5. SCRAMBLE plays slices of the steps before, in a random order
    {
        e->reset();
        auto p = pattern (tf1::Scramble); p.scramble = 1.0f;
        Buf in ((size_t) S * 16);
        for (int j = 0; j < 16; ++j) for (int i = 0; i < S; ++i) in[(size_t) (j * S + i)] = 0.05f * (float) (j + 1);          // step j is a constant 0.05 (j + 1)
        const Buf out = run (*e, in, p);
        std::set<int> seen; bool valid = true;
        for (int j = 8; j < 15; ++j)
        {
            const float v = out[(size_t) (j * S + S / 2)];
            const int level = (int) std::lround (v / 0.05f);                                                  // which earlier step this slice comes from (1 .. j)
            seen.insert (level);
            if (level < 1 || level > j || std::fabs (v - 0.05f * (float) level) > 1.0e-4f) { valid = false; std::printf ("   scramble step %d: value %g level %d\n", j, v, level); }
        }
        check (valid && seen.size() >= 3, "SCRAMBLE plays whole slices of earlier steps, in a varied order", (double) seen.size());
    }
    // 6. PITCH jumps by one of the allowed intervals
    {
        e->reset();
        tf1::Params p = pattern (tf1::Pitch); p.pitch = 1.0f; p.division = 0;                                  // 1/4 notes
        tf1::Host host; host.bpm = 60.0;                                                                        // 48000 samples per step
        const Buf out = run (*e, sine (500.0f, 48000 * 7), p, 1.0f, 1.0f, true, host);
        bool allOk = true, anyJump = false;
        for (int step = 3; step < 6; ++step)
        {
            const double f = peakHz (out, step * 48000 + 20000), ratio = f / 500.0;
            bool match = false;
            for (const int st : { -12, -7, -5, -3, 3, 5, 7, 12, 19, 24 }) if (std::fabs (ratio / std::pow (2.0, st / 12.0) - 1.0) < 0.015) match = true;
            if (! match) allOk = false;
            if (std::fabs (ratio - 1.0) > 0.03) anyJump = true;
        }
        check (allOk && anyJump, "PITCH jumps by one of the allowed intervals (500 Hz tone)");
    }
    // 7. STRETCH keeps the pitch
    {
        e->reset();
        tf1::Params p = pattern (tf1::Stretch); p.stretch = 1.0f; p.division = 0;
        tf1::Host host; host.bpm = 60.0;
        const Buf out = run (*e, sine (500.0f, 48000 * 6), p, 1.0f, 1.0f, true, host);
        bool ok = true; for (int step = 3; step < 5; ++step) if (std::fabs (peakHz (out, step * 48000 + 20000) - 500.0) > 5.0) ok = false;
        check (ok && rms (out, 4 * 48000 + 5000, 4 * 48000 + 40000) > 0.05, "STRETCH keeps a 500 Hz tone at 500 Hz");
    }
    // 8. MUTE is a gap
    {
        e->reset();
        const Buf out = run (*e, noise (S * 4, 0.4f), pattern (tf1::Mute));
        check (rms (out, 300, S * 4) < 1.0e-7, "MUTE gives silence", rms (out, 300, S * 4));
    }
    // 9. CRUSH reduces the number of levels
    {
        e->reset();
        auto p = pattern (tf1::Reverse); p.crush = 1.0f;
        const Buf out = run (*e, sine (300.0f, S * 4), p);
        std::set<int> levels; for (int i = 2 * S + 200; i < 3 * S - 200; ++i) levels.insert ((int) std::lround (out[(size_t) i] * 100000.0f));
        check (levels.size() <= 20 && levels.size() >= 3, "CRUSH 100 % leaves a handful of levels", (double) levels.size());
    }
    // 10. HOLD keeps the buffer: a repeat of a held tone goes on after the input is silent
    {
        e->reset();
        auto p = pattern (tf1::Repeat); p.loop = 1.0f;                                                       // a whole step loops (6000 samples = 55 cycles of 440 Hz)
        Buf l = sine (440.0f, 48000 * 4), r = l;
        for (int i = 48000; i < 48000 * 4; ++i) l[(size_t) i] = r[(size_t) i] = 0.0f;
        for (size_t i = 0; i < l.size(); i += 256) { p.hold = i >= 48000; e->process (l.data() + i, r.data() + i, 256, p, 1.0f, 1.0f, true, {}); }
        check (rms (l, 48000 * 3, 48000 * 4) > 0.1 && e->held(), "HOLD: the held tone keeps repeating after the input stopped", rms (l, 48000 * 3, 48000 * 4));
        e->reset();
        p.hold = false;
        Buf in = sine (440.0f, 48000 * 4); for (int i = 48000; i < 48000 * 4; ++i) in[(size_t) i] = 0.0f;
        check (rms (run (*e, in, p), 48000 * 3, 48000 * 4) < 1.0e-4, "without HOLD the repeat is silent once the input stopped");
    }
    // 11. CHANCE puts random operations into a pattern of PLAY steps
    {
        e->reset();
        auto p = pattern (tf1::Play); p.chance = 1.0f;
        const Buf in = noise (fs * 3, 0.4f), out = run (*e, in, p);
        double diff = 0.0; for (size_t i = 0; i < in.size(); ++i) diff += (double) (out[i] - in[i]) * (out[i] - in[i]);
        check (std::sqrt (diff / in.size()) > 0.05, "CHANCE 100 % changes the steps", std::sqrt (diff / in.size()));
    }
    // 12. the pattern length and the free running clock
    {
        e->reset();
        auto p = pattern (tf1::Play); p.length = 4;
        Buf l = noise (S * 20, 0.3f), r = l; int maxStep = 0, changes = 0, last = -1;
        for (size_t i = 0; i < l.size(); i += 256)
        {
            e->process (l.data() + i, r.data() + i, 256, p, 1.0f, 1.0f, true, {});
            maxStep = std::max (maxStep, e->currentStep());
            if (e->currentStep() != last) { ++changes; last = e->currentStep(); }
        }
        check (maxStep == 3 && changes >= 19, "a pattern of 4 steps cycles 0 .. 3 at one step per 6000 samples", maxStep);
    }
    // 13. host sync: the step follows the host position
    {
        e->reset();
        const auto p = pattern (tf1::Play);
        tf1::Host host; host.playing = true; host.bpm = 120.0; host.ppq = 0.0;
        Buf l (5000, 0.1f), r = l;
        e->process (l.data(), r.data(), 5000, p, 1.0f, 1.0f, true, host);
        const int a = e->currentStep();
        host.ppq = 5000.0 * 120.0 / 60.0 / fs;                                                                  // the next block starts 5000 samples later
        Buf l2 (1500, 0.1f), r2 = l2;
        e->process (l2.data(), r2.data(), 1500, p, 1.0f, 1.0f, true, host);
        check (a == 0 && e->currentStep() == 1, "host position: step 1 starts at sample 6000 (1/16 at 120 bpm)", e->currentStep());
        host.ppq = 4.25 + 0.01;                                                                                 // a locate: 17 steps in, 17 mod 16 = step 1
        e->process (l2.data(), r2.data(), 256, p, 1.0f, 1.0f, true, host);
        check (e->currentStep() == 1, "a locate in the host lands on the right step of the pattern", e->currentStep());
    }
    // 14. the buffer display data
    {
        e->reset();
        run (*e, sine (200.0f, 48000 * 2), pattern (tf1::Play));
        check (e->peakCount() >= 90 && std::fabs (e->peak (e->peakCount() - 1) - 0.5f) < 0.02f, "the recording is published as peaks", e->peakCount());
    }
    // 15. extremes: finite and bounded
    {
        juce::Random rng (3);
        e->reset();
        bool ok = true;
        for (int round = 0; round < 16 && ok; ++round)
        {
            tf1::Params p;
            for (auto& o : p.ops) o = rng.nextInt (tf1::kOps);
            p.length = 1 + rng.nextInt (16); p.division = rng.nextInt (6); p.chance = rng.nextFloat(); p.loop = rng.nextFloat(); p.scramble = rng.nextFloat();
            p.pitch = rng.nextFloat(); p.stretch = rng.nextFloat(); p.crush = rng.nextFloat(); p.hold = rng.nextBool();
            tf1::Host host; host.bpm = 40.0 + rng.nextFloat() * 200.0; host.playing = rng.nextBool(); host.ppq = rng.nextFloat() * 1000.0;
            ok = finite (run (*e, noise (fs / 2, 0.9f, round), p, rng.nextFloat(), 4.0f, true, host));
        }
        check (ok, "16 random settings at the extremes: output finite and bounded");
    }
    // 16. another sample rate
    {
        auto e2 = std::make_unique<tf1::Engine>();
        e2->prepare (96000.0f);
        Buf l (96000, 0.0f), r = l; l[1000] = 0.5f; r[1000] = 0.5f;
        const auto p = pattern (tf1::Play);
        for (size_t i = 0; i < l.size(); i += 512) e2->process (l.data() + i, r.data() + i, 512, p, 1.0f, 1.0f, true, {});
        check (std::fabs (l[1000] - 0.5f) < 1.0e-6f && std::fabs (e2->stepLengthSamples() - 12000.0f) < 1.0f, "96 kHz: no latency and a step of 12000 samples");
    }

    // the Time Lab modes: SCRUB and GRAINS are the RV-1 and the GR-1 engine (bit exact) with the shared MIX, LEVEL and power
    {
        const Buf in = noise (S * 4, 0.3f, 9);
        tf1::Params p; p.mode = tf1::Scrub; p.scrub.mode = rv1::Glitch; p.scrub.length = 0.4f; p.scrub.speed = 0.6f; p.scrub.smooth = 0.5f; p.scrub.random = 0.5f; p.scrub.repeats = 0.3f;
        e->reset(); const Buf out = run (*e, in, p, 0.7f, 1.0f, true);
        rv1::Engine ref; ref.prepare (fs); Buf l = in, r = in;
        auto q = p.scrub; q.bpm = 120.0f;
        for (size_t i = 0; i < l.size(); i += 256) { const int n = (int) std::min<size_t> (256, l.size() - i); ref.process (l.data() + i, r.data() + i, n, q, 0.7f, 1.0f); }
        double err = 0.0; for (size_t i = 0; i < l.size(); ++i) err = std::max (err, (double) std::fabs (out[i] - l[i]));
        check (err < 1.0e-6 && rms (out, S, S * 4) > 0.01, "SCRUB is the RV-1 engine: the output equals it (MIX 70 %)", err);
    }
    {
        const Buf in = noise (S * 4, 0.3f, 10);
        tf1::Params p; p.mode = tf1::Grains; p.grain.shape = 1; p.grain.size = 0.4f; p.grain.density = 0.6f; p.grain.position = 0.3f; p.grain.spray = 0.4f; p.grain.pitch = 0.6f; p.grain.random = 0.3f; p.grain.reverse = 0.2f;
        e->reset(); const Buf out = run (*e, in, p, 0.6f, 1.0f, true);
        gr1::Engine ref; ref.prepare (fs); Buf l = in, r = in;
        auto q = p.grain; q.bpm = 120.0f;
        for (size_t i = 0; i < l.size(); i += 256) { const int n = (int) std::min<size_t> (256, l.size() - i); ref.process (l.data() + i, r.data() + i, n, q, 0.6f, 1.0f); }
        double err = 0.0; for (size_t i = 0; i < l.size(); ++i) err = std::max (err, (double) std::fabs (out[i] - l[i]));
        check (err < 1.0e-6 && rms (out, S, S * 4) > 0.01, "GRAINS is the GR-1 engine: the output equals it (MIX 60 %)", err);
    }
    {
        const Buf in = noise (S * 2, 0.3f, 11);
        tf1::Params p; p.mode = tf1::Grains;
        e->reset(); const Buf off = run (*e, in, p, 1.0f, 1.0f, false);
        double err = 0.0; for (size_t i = 0; i < in.size(); ++i) err = std::max (err, (double) std::fabs (off[i] - in[i]));
        check (err < 1.0e-6, "GRAINS with the power off passes the input unchanged", err);
        tf1::Params q; q.mode = tf1::Scrub;
        e->reset(); const Buf off2 = run (*e, in, q, 1.0f, 1.0f, false);
        err = 0.0; for (size_t i = 0; i < in.size(); ++i) err = std::max (err, (double) std::fabs (off2[i] - in[i]));
        check (err < 1.0e-6, "SCRUB with the power off passes the input unchanged", err);
        e->reset(); const Buf back = run (*e, in, pattern (tf1::Play));
        err = 0.0; for (size_t i = 0; i < in.size(); ++i) err = std::max (err, (double) std::fabs (back[i] - in[i]));
        check (err < 1.0e-6, "back in STEPS mode with every step PLAY: the input passes", err);
    }
    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
