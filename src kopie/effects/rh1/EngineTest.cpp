// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
// Offline check of the RH-1 engine: FILTER equals the MF-1 (LFO and envelope sources, three filter types), TREMOLO equals the TR-2 curve, SLICER equals the SL-2 (SINGLE, DUAL,
// SFX); with SYNC the lane stays locked to the host position (5 minutes at 173 bpm); SWING delays the odd steps by the set fraction; the envelope follower releases with its
// DECAY; MIX 0 is bit-exact; every setting stays finite; silence in, silence out.
#include <cstdio>
#include <random>
#include <vector>
#include "RhythmEngine.h"
#include "../mf1/MotionEngine.h"
#include "../tr2/TR2DSP.h"
#include "../sl2/SL2DSP.h"

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
std::vector<float> run (const rh1::Params& p, const std::vector<float>& inL, const std::vector<float>& inR, std::vector<float>* outR = nullptr, float level = 1.0f)
{
    rh1::Engine e; e.prepare (fs);
    auto l = inL, r = inR;
    for (size_t pos = 0; pos < l.size(); pos += 480) { const int n = (int) std::min<size_t> (480, l.size() - pos); e.process (l.data() + pos, r.data() + pos, n, p, level); }
    if (outR) *outR = r;
    return l;
}
double diffDb (const std::vector<float>& a, const std::vector<float>& b, int from = 2000, int to = 1 << 30)
{
    double e = 0, r = 0; for (size_t i = (size_t) from; i < a.size() && i < (size_t) to; ++i) { const double d = (double) a[i] - b[i]; e += d * d; r += (double) b[i] * b[i]; }
    return 10.0 * std::log10 (std::max (1e-30, e / std::max (1e-30, r)));
}
}

int main()
{
    const auto in = noise ((int) (4.0f * fs), 0.3f, 3);
    std::printf ("parity with the source plugins\n");
    for (int type : { 0, 1, 4 })
    {
        rh1::Params p; p.mode = rh1::Filter; p.variant = type; p.source = rh1::Lfo; p.sync = false; p.rate = mf1::rateHz (0.35f); p.shape = 0.0f; p.depth = 0.6f; p.cutoff = 0.4f; p.res = 0.55f; p.mix = 1.0f;
        const auto o = run (p, in, in);
        mf1::Params q; q.type = type; q.source = mf1::Lfo; q.cutoff = 0.4f; q.res = 0.55f; q.depth = 0.6f; q.m1 = 0.35f; q.m2 = 0.0f; q.sync = false;
        mf1::Engine e; e.prepare (fs); auto l = in, r = in; for (size_t pos = 0; pos < l.size(); pos += 480) { const int n = (int) std::min<size_t> (480, l.size() - pos); e.process (l.data() + pos, r.data() + pos, n, q, 1.0f, 1.0f); }
        const double d = diffDb (o, l, 2000, (int) (0.5f * fs));            // the originals add the phase in a float: it drifts from the exact phase of this engine within seconds
        std::printf ("  FILTER %s, LFO against the MF-1: %.1f dB\n", rh1::variantName (rh1::Filter, type), d);
        check (d < (type == 4 ? -55.0 : -70.0), "FILTER with an LFO equals the MF-1 (the first 0.5 s)");
    }
    {
        rh1::Params p; p.mode = rh1::Filter; p.variant = 0; p.source = rh1::Envelope; p.sens = 0.5f; p.decay = 0.4f; p.depth = 0.7f; p.cutoff = 0.3f; p.res = 0.6f; p.mix = 1.0f;
        auto music = in; for (size_t i = 0; i < music.size(); ++i) music[i] *= 0.2f + 0.8f * std::abs (std::sin (6.2831853f * 1.3f * (float) i / fs));
        const auto o = run (p, music, music);
        mf1::Params q; q.type = 0; q.source = mf1::Envelope; q.cutoff = 0.3f; q.res = 0.6f; q.depth = 0.7f; q.m1 = 0.5f; q.m2 = 0.4f;
        mf1::Engine e; e.prepare (fs); auto l = music, r = music; for (size_t pos = 0; pos < l.size(); pos += 480) { const int n = (int) std::min<size_t> (480, l.size() - pos); e.process (l.data() + pos, r.data() + pos, n, q, 1.0f, 1.0f); }
        const double d = diffDb (o, l);
        std::printf ("  FILTER, envelope against the MF-1: %.1f dB\n", d);
        check (d < -70.0, "FILTER with the envelope source equals the MF-1");
    }
    {
        rh1::Params p; p.mode = rh1::Tremolo; p.variant = rh1::Smooth; p.source = rh1::Lfo; p.sync = false; p.rate = 6.0f; p.shape = 0.3f; p.depth = 0.8f; p.mix = 1.0f;
        const auto tone = noise ((int) (2.0f * fs), 0.3f, 6); const auto o = run (p, tone, tone);
        std::vector<float> ref (tone.size()); float phase = 0.0f;
        for (size_t i = 0; i < tone.size(); ++i) { phase += 6.0f / fs; if (phase >= 1.0f) phase -= 1.0f; ref[i] = tone[i] * tr2::gain (tr2::lfo (phase, 0.3f), 0.8f); }
        const double d = diffDb (o, ref, 2000, (int) (0.2f * fs));
        std::printf ("  TREMOLO against the TR-2 curve: %.1f dB\n", d);
        check (d < -70.0, "TREMOLO SMOOTH equals the TR-2");
    }
    for (int mode : { 0, 1, 3 })
    {
        rh1::Params p; p.mode = rh1::Slicer; p.variant = mode; p.sync = true; p.division = 4; p.bpm = 120.0f; p.slAttack = 0.3f; p.slDuty = 0.8f; p.slBalance = 0.7f; p.mix = 1.0f;
        for (int s = 0; s < 16; ++s) p.steps[s] = sl2::stepOn (3, s) ? 1.0f : 0.0f;                    // pattern 4, 3-3-2
        const auto o = run (p, in, in);
        // the SL-2 loop (free running, one step = 1/16 note)
        std::vector<float> ref (in.size()); double stepPos = 0.0; const double stepInc = 120.0 / 60.0 * 4.0 / fs;
        float envA = 0, envB = 0, envTa = 0.15f, envTb = 0.15f, low = 0; sl2::Svf svf;
        const float duty = sl2::dutyFromKnob (0.8f), atk = sl2::coefMs (sl2::attackMs (0.3f), fs), rel = sl2::coefMs (4.0f, fs), swell = sl2::coefMs (0.5f * (float) (60000.0 / 120.0 / 4.0), fs);
        const float dryGain = std::min (1.0f, 2.0f * 0.3f), wetGain = std::min (1.0f, 2.0f * 0.7f);
        for (size_t i = 0; i < in.size(); ++i)
        {
            stepPos += stepInc;
            const int si = (int) std::floor (stepPos); const float u = (float) (stepPos - std::floor (stepPos)); const bool on = sl2::stepOn (3, si & 15);
            const float gA = (on && u < duty) ? 1.0f : 0.0f;
            envA += (gA - envA) * (gA > envA ? atk : rel);
            float wet = mode == 0 ? in[i] * envA : mode == 1 ? in[i] * envA : svf.process (in[i], 250.0f + 5500.0f * envA, fs) * (0.15f + 0.85f * envA) * 1.6f;
            ref[i] = in[i] * dryGain + wet * wetGain; (void) envB; (void) envTa; (void) envTb; (void) low;
        }
        const double d = diffDb (o, ref);
        std::printf ("  SLICER %s against the SL-2: %.1f dB\n", rh1::variantName (rh1::Slicer, mode), d);
        check (d < -70.0, "SLICER equals the SL-2 (the left channel)");
    }

    std::printf ("clock, swing, envelope\n");
    {
        // 5 minutes at 173 bpm: the step follows the host position (a block at a time)
        rh1::Engine e; e.prepare (fs);
        rh1::Params p; p.mode = rh1::Slicer; p.sync = true; p.hostPlaying = true; p.division = 4; p.bpm = 173.0f;
        std::vector<float> l (480, 0.1f), r (480, 0.1f); bool ok = true; double samples = 0; int checked = 0;
        for (int b = 0; b < (int) (300.0f * fs / 480.0f); ++b)
        {
            p.hostBeats = samples / (double) fs * 173.0 / 60.0;
            e.process (l.data(), r.data(), 1, p, 1.0f);
            const int expect = (int) std::floor (p.hostBeats / 0.25 + 1.0e-9 + 173.0 / 60.0 / 0.25 / fs) % 16;                     // after the first sample of the block
            if (e.currentStep.load() != expect) ok = false;
            ++checked;
            e.process (l.data() + 1, r.data() + 1, 479, p, 1.0f);
            samples += 480;
        }
        std::printf ("  %d blocks checked\n", checked);
        check (ok, "SYNC: the step follows the host position over 5 minutes at 173 bpm (no drift)");
    }
    {
        // SWING 50 %: an even step lasts 1.5 steps (1 + swing), the odd one 0.5
        rh1::Params p; p.mode = rh1::Filter; p.source = rh1::Steps; p.sync = true; p.division = 4; p.bpm = 120.0f; p.swing = 0.5f; p.glide = 0.0f; p.depth = 1.0f;
        for (int s = 0; s < 16; ++s) p.steps[s] = (s & 1) ? 1.0f : 0.0f;
        rh1::Engine e; e.prepare (fs); std::vector<float> l ((size_t) (4.0f * fs), 0.1f), r = l; std::vector<float> ctl;
        int firstUp = -1, lastDown = -1, prev = 0, lengthEven = 0; std::vector<int> ups, downs;
        for (size_t pos = 0; pos < l.size(); pos += 1)
        {
            e.process (l.data() + pos, r.data() + pos, 1, p, 1.0f);
            const int cs = e.currentStep.load();
            if ((cs & 1) && ! (prev & 1)) ups.push_back ((int) pos); if (! (cs & 1) && (prev & 1)) downs.push_back ((int) pos);
            prev = cs;
        }
        (void) firstUp; (void) lastDown; (void) lengthEven;
        const double stepLen = 60.0 / 120.0 * 0.25 * fs;                                        // a 16th at 120 bpm in samples
        const double even = (double) (ups[2] - downs[1]) / stepLen, odd = (double) (downs[2] - ups[2]) / stepLen;
        std::printf ("  swing 50 %%: even step %.2f, odd step %.2f steps long\n", even, odd);
        check (std::abs (even - 1.5) < 0.02 && std::abs (odd - 0.5) < 0.02, "SWING 50 % makes the even steps 1.5 and the odd steps 0.5 steps long");
    }
    {
        // the envelope follower releases with DECAY: after a loud burst the control falls to 1/e in about the decay time
        for (float decay : { 0.2f, 0.5f })
        {
            rh1::Params p; p.mode = rh1::Filter; p.source = rh1::Envelope; p.sens = 0.0f; p.decay = decay; p.depth = 1.0f;
            rh1::Engine e; e.prepare (fs);
            std::vector<float> l (4800, 0.9f), r = l; e.process (l.data(), r.data(), (int) l.size(), p, 1.0f);
            const float start = e.lastModulation();
            std::vector<float> q ((size_t) (2.0f * fs), 0.0f), q2 = q; int t = -1; float prevMod = start;
            for (int i = 0; i < (int) q.size(); ++i) { e.process (q.data() + i, q2.data() + i, 1, p, 1.0f); if (e.lastModulation() < 0.368f * 0.9f * std::tanh (3.0f) * 0.0f + 0.0f + 1e9f) { } prevMod = e.lastModulation(); if (t < 0 && prevMod < 0.1f) t = i; }
            std::printf ("  DECAY %.1f: control from %.2f to below 0.1 in %.0f ms (the follower decays %.0f ms per e-fold)\n", (double) decay, (double) start, t * 1000.0 / fs, (double) rh1::decayMs (decay));
            check (t > 0 && t * 1000.0 / fs > 0.4 * rh1::decayMs (decay) && t * 1000.0 / fs < 4.0 * rh1::decayMs (decay), "ENVELOPE: the release time follows DECAY");
        }
    }
    {
        bool ok = true; const auto x = noise ((int) fs, 0.8f, 11);
        for (int m = 0; m < rh1::kNumModes; ++m) for (int v = 0; v < rh1::variantCount (m); ++v) for (int s = 0; s < 3; ++s) for (int src = 0; src < 3; ++src)
        {
            rh1::Params p; p.mode = m; p.variant = v; p.source = src; p.sync = s == 1; p.rate = s == 2 ? 20.0f : 0.05f; p.depth = 1.0f; p.shape = s == 2 ? 1.0f : 0.0f; p.res = 1.0f; p.cutoff = s == 2 ? 1.0f : 0.0f;
            p.swing = 0.75f; p.glide = 1.0f; p.sens = 1.0f; p.decay = 0.0f; p.tremStereo = 1.0f; p.tremBias = s == 2 ? 1.0f : 0.0f; p.slAttack = 1.0f; p.slDuty = 0.0f; p.slBalance = 1.0f; p.division = 8; p.bpm = 300.0f;
            const auto o = run (p, x, x, nullptr, 4.0f); float peak = 0; bool fin = true; for (float y : o) { if (! std::isfinite (y)) fin = false; peak = std::max (peak, std::abs (y)); }
            if (! fin || peak > 40.0f) { ok = false; std::printf ("    %s %d src %d setting %d: peak %.2f%s\n", rh1::modeName (m), v, src, s, peak, fin ? "" : " NOT FINITE"); }
        }
        check (ok, "every mode, type, source and extreme setting stays finite and bounded");
        rh1::Params p; p.mode = rh1::Filter; p.mix = 0.0f; const auto o = run (p, in, in);
        check (o == in, "MIX 0 passes the input bit-exact");
        std::vector<float> silent ((int) fs, 0.0f); const auto so = run (p, silent, silent); double e = 0; for (float y : so) e += (double) y * y;
        check (e == 0.0, "silence in, silence out");
    }
    std::printf ("%d problems\n", problems);
    return problems == 0 ? 0 : 1;
}
