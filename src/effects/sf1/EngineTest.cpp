// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
// Offline check of the SF-1 engine (console app, not part of the normal build): cmake --build <build dir> --target SF1EngineTest, then run it; exit code 0 = pass.
#include <JuceHeader.h>
#include "SpectralEngine.h"
#include <cstdio>
#include "test/EngineTestKit.h"
#include <memory>

namespace
{
using namespace vibr::test;

// runs a mono signal through both channels in blocks of 256 and returns the left output
Buf run (sf1::Engine& e, const Buf& in, const sf1::Params& p, float mix = 1.0f, float level = 1.0f, bool power = true)
{
    Buf l = in, r = in;
    for (size_t i = 0; i < l.size(); i += 256)
    {
        const int n = (int) std::min<size_t> (256, l.size() - i);
        e.process (l.data() + i, r.data() + i, n, p, mix, level, power);
    }
    return l;
}
double db (double x) { return 20.0 * std::log10 (std::max (1.0e-12, x)); }

bool finite (const Buf& b) { for (float v : b) if (! std::isfinite (v) || std::fabs (v) > 4.0f) return false; return true; }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    const int L = sf1::kN;
    auto e = std::make_unique<sf1::Engine>();
    e->prepare (fs);
    sf1::Params neutral;

    // 1. latency and transparency: neutral settings, mix 100 % = the input, one frame late
    {
        const Buf in = noise (fs * 2, 0.3f), out = run (*e, in, neutral);
        double err = 0.0, ref = 0.0;
        for (int i = L * 2; i < (int) in.size(); ++i) { const double d = (double) out[(size_t) i] - in[(size_t) (i - L)]; err += d * d; ref += (double) in[(size_t) (i - L)] * in[(size_t) (i - L)]; }
        check (db (std::sqrt (err / ref)) < -60.0, "neutral = the input delayed by the latency, error below -60 dB", db (std::sqrt (err / ref)));
        check (e->latency() == 2048, "latency is one frame (2048 samples)", e->latency());
    }
    // 2. mix 0 and power off are the delayed dry sound, bit exact
    {
        e->reset();
        sf1::Params p; p.smear = 1.0f; p.shiftHz = 300.0f; p.ring = 1.0f;
        const Buf in = noise (fs, 0.3f), out = run (*e, in, p, 0.0f);
        bool exact = true; for (int i = L; i < (int) in.size(); ++i) if (out[(size_t) i] != in[(size_t) (i - L)]) { exact = false; break; }
        check (exact, "MIX 0 passes the delayed input bit exact");
        e->reset();
        const Buf off = run (*e, in, p, 1.0f, 2.0f, false);
        exact = true; for (int i = L; i < (int) in.size(); ++i) if (off[(size_t) i] != in[(size_t) (i - L)]) { exact = false; break; }
        check (exact, "power off passes the delayed input bit exact (also with LEVEL up)");
    }
    // 3. silence in, silence out
    {
        e->reset();
        sf1::Params p; p.freeze[0] = p.freeze[1] = p.freeze[2] = p.freeze[3] = true; p.feedback = 0.9f; p.smear = 1.0f; p.morph = 1.0f;
        const Buf out = run (*e, Buf ((size_t) fs, 0.0f), p);
        check (rms (out, 0, (int) fs) < 1.0e-7, "silence in gives silence out in the extreme settings", rms (out, 0, (int) fs));
    }
    // 4. SHIFT moves a partial by the set number of Hz
    {
        e->reset();
        sf1::Params p; p.shiftHz = 240.0f;
        const Buf out = run (*e, sine (1000.0f, fs * 2), p);
        const double f = peakHz (out, fs);
        check (std::fabs (f - 1240.0) < 4.0, "SHIFT +240 Hz moves 1000 Hz to 1240 Hz", f);
        e->reset();
        p.shiftHz = -300.0f;
        const double f2 = peakHz (run (*e, sine (1000.0f, fs * 2), p), fs);
        check (std::fabs (f2 - 700.0) < 4.0, "SHIFT -300 Hz moves 1000 Hz to 700 Hz", f2);
    }
    // 5. SPREAD: L and R move in opposite directions
    {
        e->reset();
        sf1::Params p; p.shiftHz = 0.0f; p.spread = 1.0f;
        Buf l = sine (1000.0f, fs * 2), r = l;
        for (size_t i = 0; i < l.size(); i += 256) e->process (l.data() + i, r.data() + i, (int) std::min<size_t> (256, l.size() - i), p, 1.0f, 1.0f, true);
        const double fl = peakHz (l, fs), fr = peakHz (r, fs);
        check (fl > 1030.0 && fr < 970.0, "SPREAD 100 % shifts L up and R down by 40 Hz", fl - fr);
    }
    // 6. FREEZE sustains a tone after the input stops; unfreeze lets it go
    {
        e->reset();
        sf1::Params p;
        Buf in = sine (440.0f, fs * 4); for (int i = (int) fs; i < (int) in.size(); ++i) in[(size_t) i] = 0.0f;       // 1 s of tone, then silence
        Buf out; out.reserve (in.size());
        {   // freeze from 0.8 s on
            Buf l = in, r = in;
            for (size_t i = 0; i < l.size(); i += 256)
            {
                p.freeze[0] = p.freeze[1] = p.freeze[2] = p.freeze[3] = (double) i / fs > 0.8;
                e->process (l.data() + i, r.data() + i, (int) std::min<size_t> (256, l.size() - i), p, 1.0f, 1.0f, true);
            }
            out = l;
        }
        const double held = rms (out, (int) (fs * 2.5), (int) (fs * 3.5)), early = rms (out, (int) (fs * 0.5), (int) (fs * 0.7));
        check (held > early * 0.5, "FREEZE holds the tone for seconds after the input stopped", db (held / early));
        check (std::fabs (peakHz (out, (int) (fs * 2.0)) - 440.0) < 3.0, "the frozen tone stays at its pitch", peakHz (out, (int) (fs * 2.0)));
        check (finite (out), "the frozen sound is finite and bounded");
        e->reset();
        const Buf free = run (*e, in, neutral);
        check (rms (free, (int) (fs * 2.5), (int) (fs * 3.5)) < 1.0e-4, "without FREEZE the sound ends with the input");
    }
    // 7. band freeze: only the frozen band keeps sounding
    {
        e->reset();
        sf1::Params p; p.freeze[3] = false;
        Buf in = sine (440.0f, fs * 4, 0.4f), hi = sine (6000.0f, fs * 4, 0.4f);
        for (size_t i = 0; i < in.size(); ++i) in[i] += hi[i];
        for (int i = (int) fs; i < (int) in.size(); ++i) in[(size_t) i] = 0.0f;
        Buf l = in, r = in;
        for (size_t i = 0; i < l.size(); i += 256)
        {
            p.freeze[1] = (double) i / fs > 0.8;                                                                        // LOW MID (150 .. 800 Hz) only: the 440 Hz tone
            e->process (l.data() + i, r.data() + i, (int) std::min<size_t> (256, l.size() - i), p, 1.0f, 1.0f, true);
        }
        const double lowBand = bandRms (l, (int) (fs * 2.0), 300.0f, 600.0f), highBand = bandRms (l, (int) (fs * 2.0), 5000.0f, 7000.0f);
        check (db (lowBand / std::max (1.0e-9, highBand)) > 40.0, "freezing LOW MID keeps 440 Hz and lets 6 kHz end", db (lowBand / std::max (1.0e-9, highBand)));
    }
    // 8. SMEAR: the sound rings on after the input stops
    {
        e->reset();
        sf1::Params p; p.smear = 0.9f;
        Buf in = sine (800.0f, fs * 2); for (int i = (int) (fs * 0.5); i < (int) in.size(); ++i) in[(size_t) i] = 0.0f;
        const Buf out = run (*e, in, p), plain = run (*e, in, neutral);
        e->reset();
        const double a = rms (out, (int) (fs * 0.62), (int) (fs * 0.72)), b = rms (plain, (int) (fs * 0.62), (int) (fs * 0.72));
        check (a > b * 4.0 && a > 0.001, "SMEAR makes the sound ring on after the input stopped", db (a / std::max (1.0e-9, b)));
    }
    // 9. BLUR spreads a sine over neighbouring frequencies
    {
        e->reset();
        sf1::Params p; p.blur = 1.0f;
        const Buf out = run (*e, sine (1000.0f, fs * 2), p);
        e->reset();
        const Buf ref = run (*e, sine (1000.0f, fs * 2), neutral);
        const double wide = bandRms (out, (int) fs, 1100.0f, 1500.0f) / bandRms (out, (int) fs, 900.0f, 1100.0f), narrow = bandRms (ref, (int) fs, 1100.0f, 1500.0f) / bandRms (ref, (int) fs, 900.0f, 1100.0f);
        check (wide > narrow * 5.0, "BLUR spreads energy next to the partial", db (wide / narrow));
    }
    // 10. WINDOW isolates: only the region around its centre is left
    {
        e->reset();
        sf1::Params p; p.window = 1.0f; p.windowHz = 1000.0f; p.windowWidthOct = 0.5f;
        Buf in = sine (1000.0f, fs * 2, 0.3f), other = sine (4000.0f, fs * 2, 0.3f);
        for (size_t i = 0; i < in.size(); ++i) in[i] += other[i];
        const Buf out = run (*e, in, p);
        const double inside = bandRms (out, (int) fs, 900.0f, 1100.0f), outside = bandRms (out, (int) fs, 3800.0f, 4200.0f);
        check (db (inside / std::max (1.0e-9, outside)) > 30.0, "WINDOW at 1 kHz isolates 1 kHz from 4 kHz", db (inside / std::max (1.0e-9, outside)));
    }
    // 11. MORPH: capture a tone, then play another one: the morph brings the captured tone back
    {
        e->reset();
        sf1::Params p;
        run (*e, sine (500.0f, 4096), neutral);                                                                         // fill the frames with 500 Hz
        e->requestCapture();
        run (*e, sine (500.0f, 4096), neutral);
        check (e->hasMemory(), "CAPTURE stores a snapshot");
        p.morph = 1.0f;
        const Buf out = run (*e, sine (2000.0f, fs * 2), p);
        const double f = peakHz (out, (int) fs);
        check (std::fabs (f - 500.0) < 5.0, "MORPH 100 % plays the captured 500 Hz while the input is 2 kHz", f);
    }
    // 12. RING makes sidebands
    {
        e->reset();
        sf1::Params p; p.ring = 1.0f; p.ringHz = 300.0f;
        const Buf out = run (*e, sine (1000.0f, fs * 2), p);
        const double up = bandRms (out, (int) fs, 1250.0f, 1350.0f), down = bandRms (out, (int) fs, 650.0f, 750.0f), carrier = bandRms (out, (int) fs, 950.0f, 1050.0f);
        check (up > carrier * 0.1 && down > carrier * 0.1, "RING adds sidebands at 700 and 1300 Hz", db (up / std::max (1.0e-9, carrier)));
    }
    // 13. FEEDBACK is stable and never runs away
    {
        e->reset();
        sf1::Params p; p.feedback = 1.0f; p.shiftHz = 50.0f; p.smear = 0.5f;
        const Buf out = run (*e, noise (fs * 6, 0.5f), p);
        check (finite (out), "FEEDBACK 100 % with SHIFT stays finite and bounded");
        Buf tone = sine (300.0f, fs * 2); tone.resize ((size_t) (fs * 12), 0.0f);
        e->reset();
        const Buf tail = run (*e, tone, p);
        check (rms (tail, (int) (fs * 10), (int) (fs * 12)) < 0.05, "FEEDBACK 100 % dies away after the input", rms (tail, (int) (fs * 10), (int) (fs * 12)));
    }
    // 14. all knobs at the extremes, random: finite and bounded
    {
        juce::Random rng (9);
        e->reset();
        bool ok = true;
        for (int round = 0; round < 12 && ok; ++round)
        {
            sf1::Params p;
            for (auto& f : p.freeze) f = rng.nextBool();
            p.smear = rng.nextFloat(); p.blur = rng.nextFloat(); p.shiftHz = (rng.nextFloat() * 2.0f - 1.0f) * 1000.0f; p.spread = rng.nextFloat(); p.drift = rng.nextFloat();
            p.ring = rng.nextFloat(); p.ringHz = 20.0f + rng.nextFloat() * 1980.0f; p.feedback = rng.nextFloat(); p.morph = rng.nextFloat(); p.window = rng.nextFloat();
            p.windowHz = 40.0f + rng.nextFloat() * 11960.0f; p.windowWidthOct = 0.15f + rng.nextFloat() * 2.85f; p.scanHz = rng.nextFloat() * 4.0f;
            if (round == 3) e->requestCapture();
            ok = finite (run (*e, noise (fs / 2, 0.8f, round), p, rng.nextFloat(), 4.0f));
        }
        check (ok, "12 random settings at the extremes: output finite and bounded");
    }
    // 15. a different sample rate works and keeps the latency
    {
        auto e2 = std::make_unique<sf1::Engine>();
        e2->prepare (96000.0f);
        Buf l ((size_t) 96000, 0.0f), r = l; l[1000] = 0.5f; r[1000] = 0.5f;
        for (size_t i = 0; i < l.size(); i += 512) e2->process (l.data() + i, r.data() + i, 512, neutral, 0.0f, 1.0f, true);
        check (std::fabs (l[1000 + (size_t) L] - 0.5f) < 1.0e-6f, "96 kHz: the dry impulse comes out exactly one frame later");
    }

    // the mode HARMONY is the PT-1 engine (bit exact) delayed by one frame, like the dry sound of the FOLD mode
    {
        const Buf in = [&] { Buf b = sine (220.0f, 48000, 0.3f); const Buf c = sine (330.0f, 48000, 0.2f); for (size_t k = 0; k < b.size(); ++k) b[k] += c[k]; return b; }();
        sf1::Params p; p.mode = 1; p.harm.root = 2; p.harm.scale = 4; p.harm.interval[0] = 4; p.harm.interval[1] = 7; p.harm.spread = 0.6f;
        auto e2 = std::make_unique<sf1::Engine>(); e2->prepare (fs);
        const Buf out = run (*e2, in, p, 0.5f, 1.0f, true);
        pt1::Engine ref; ref.prepare (fs); Buf l = in, r = in;
        for (size_t i = 0; i < l.size(); i += 256) { const int n = (int) std::min<size_t> (256, l.size() - i); ref.process (l.data() + i, r.data() + i, n, p.harm, 0.5f, 1.0f); }
        double err = 0.0; for (int i = 0; i + sf1::kN < (int) l.size(); ++i) err = std::max (err, (double) std::fabs (out[(size_t) (i + sf1::kN)] - l[(size_t) i]));
        check (err < 1.0e-6 && rms (out, 4000, 40000) > 0.05, "HARMONY is the PT-1 engine delayed by one frame (MIX 50 %)", err);
        // power off: the dry sound, one frame late
        auto e3 = std::make_unique<sf1::Engine>(); e3->prepare (fs);
        const Buf off = run (*e3, in, p, 1.0f, 1.0f, false);
        double err2 = 0.0; for (int i = 0; i + sf1::kN < (int) in.size(); ++i) err2 = std::max (err2, (double) std::fabs (off[(size_t) (i + sf1::kN)] - in[(size_t) i]));
        check (err2 < 1.0e-6, "HARMONY with the power off passes the dry sound one frame late", err2);
    }
    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
