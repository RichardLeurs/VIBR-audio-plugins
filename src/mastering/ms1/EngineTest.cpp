// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
// Offline check of the MS-1 engine against the standards and the spec: the loudness meter reads -23.0 LUFS for a -23 dBFS 997 Hz stereo sine and
// gates silence correctly; the true-peak detector finds the inter-sample peak of a sine at a quarter of the sample rate; the limiter keeps the true
// peak under the ceiling (measured with an independent 16x sinc oversampler) at several sample rates, leaves quiet material untouched except for the
// delay it reports, and has the latency it reports; dither quantises to the right number of bits.
#include <array>
#include <cstdio>
#include <numeric>
#include <random>
#include <vector>
#include "MasteringEngine.h"

static float sine (float amp, float hz, int i, float fs, float phase = 0.0f) { return amp * std::sin (6.28318530718f * hz * (float) i / fs + phase); }

// an independent true-peak measurement: 16x oversampling by a long windowed sinc
static float truePeak (const std::vector<float>& x, size_t from, size_t to)
{
    float peak = 0.0f;
    const int taps = 64, up = 16;
    for (size_t n = from; n < to; ++n)
        for (int ph = 0; ph < up; ++ph)
        {
            double acc = 0.0;
            for (int k = -taps; k <= taps; ++k)
            {
                const long idx = (long) n + k; if (idx < 0 || idx >= (long) x.size()) continue;
                const double t = (double) k - (double) ph / up, s = std::abs (t) < 1e-9 ? 1.0 : std::sin (3.14159265358979 * t) / (3.14159265358979 * t);
                const double w = 0.5 + 0.5 * std::cos (3.14159265358979 * t / (taps + 1));
                acc += x[(size_t) idx] * s * w;
            }
            peak = std::max (peak, (float) std::abs (acc));
        }
    return peak;
}

int main()
{
    int problems = 0;
    const auto check = [&] (bool ok, const char* msg) { if (! ok) { ++problems; std::printf ("  PROBLEM: %s\n", msg); } };

    // 1. loudness: -23 dBFS 997 Hz stereo sine = -23.0 LUFS (EBU Tech 3341 case 1), at three sample rates
    for (float fs : { 44100.0f, 48000.0f, 96000.0f })
    {
        ms1::Loudness m; m.prepare (fs);
        const int total = (int) (fs * 25.0f);
        std::vector<float> l ((size_t) total), r ((size_t) total);
        const float amp = std::pow (10.0f, -23.0f / 20.0f);
        for (int i = 0; i < total; ++i) l[(size_t) i] = r[(size_t) i] = sine (amp, 997.0f, i, fs);
        for (int pos = 0; pos < total; pos += 512) m.process (l.data() + pos, r.data() + pos, std::min (512, total - pos));
        std::printf ("%.0f Hz: momentary %.2f  short-term %.2f  integrated %.2f  LRA %.2f\n", fs, m.momentary.load(), m.shortTerm.load(), m.integrated.load(), m.range.load());
        check (std::abs (m.momentary.load() + 23.0f) < 0.1f && std::abs (m.shortTerm.load() + 23.0f) < 0.1f && std::abs (m.integrated.load() + 23.0f) < 0.1f, "-23 dBFS sine should read -23.0 LUFS");
        check (m.range.load() < 0.2f, "a steady tone has no loudness range");
    }

    // 2. gating: 10 s of -23 LUFS, 10 s of silence (below -70) and 10 s of -23 LUFS: integrated stays -23; 10 s at -40 LUFS (relative gate) is ignored
    {
        const float fs = 48000.0f;
        ms1::Loudness m; m.prepare (fs);
        std::vector<float> l, r;
        const auto add = [&] (float db, int seconds) { const float a = std::pow (10.0f, db / 20.0f); for (int i = 0; i < (int) fs * seconds; ++i) { const float v = sine (a, 997.0f, i, fs); l.push_back (v); r.push_back (v); } };
        add (-23.0f, 10); add (-90.0f, 10); add (-23.0f, 10); add (-40.0f, 10);
        for (size_t pos = 0; pos < l.size(); pos += 512) m.process (l.data() + pos, r.data() + pos, (int) std::min<size_t> (512, l.size() - pos));
        std::printf ("gating: integrated %.2f (expect -23.0)\n", m.integrated.load());
        check (std::abs (m.integrated.load() + 23.0f) < 0.15f, "silence and a quiet passage should be gated out of the integrated loudness");
    }

    // 3. true peak: a sine at fs/4 with a 45 degree phase has samples at +-0.707 and a true peak of 1.0
    {
        ms1::TruePeak tp; tp.reset();
        float peak = 0.0f, samplePeak = 0.0f;
        for (int i = 0; i < 400; ++i) { const float x = sine (1.0f, 12000.0f, i, 48000.0f, 0.7853982f); samplePeak = std::max (samplePeak, std::abs (x)); peak = std::max (peak, tp.push (x)); }
        std::printf ("true peak: sample peak %.3f, true peak %.3f (expect 1.0)\n", samplePeak, peak);
        check (peak > 0.98f && peak < 1.03f && samplePeak < 0.72f, "the true-peak detector should find the inter-sample peak");
    }

    // 4. limiter: loud material stays under the ceiling (true peak, independent measurement) at several sample rates
    for (float fs : { 44100.0f, 48000.0f, 96000.0f, 192000.0f })
    {
        ms1::Limiter lim; lim.prepare (fs);
        ms1::LimiterParams p; p.gainDb = 12.0f; p.ceilingDb = -1.0f; p.releaseMs = 100.0f; p.lookaheadMs = 2.0f; p.link = 1.0f;
        const int total = (int) (fs * 2.0f);
        std::vector<float> l ((size_t) total), r ((size_t) total);
        // noise like real programme material: band-limited (a 2-pole low-pass at about a sixth of the sample rate); white noise to the Nyquist
        // frequency is a worst case for every practical true-peak detector (the ideal reconstruction needs very long filters)
        std::mt19937 rng (7); std::uniform_real_distribution<float> u (-1.0f, 1.0f);
        float nl1 = 0, nl2 = 0, nr1 = 0, nr2 = 0;
        for (int i = 0; i < total; ++i)
        {
            const float env = (i / (int) (fs * 0.25f)) % 2 == 0 ? 0.7f : 0.15f;                                     // loud and quiet passages
            nl1 += 0.35f * (u (rng) - nl1); nl2 += 0.35f * (nl1 - nl2); nr1 += 0.35f * (u (rng) - nr1); nr2 += 0.35f * (nr1 - nr2);
            l[(size_t) i] = env * (0.6f * sine (1.0f, 220.0f, i, fs) + 1.2f * nl2) + (i % 5000 == 0 ? 0.9f : 0.0f);      // sines, noise and clicks
            r[(size_t) i] = env * (0.6f * sine (1.0f, 331.0f, i, fs, 1.0f) + 1.2f * nr2);
        }
        for (int pos = 0; pos < total; pos += 512) lim.process (l.data() + pos, r.data() + pos, std::min (512, total - pos), p);
        const float tpL = truePeak (l, 4000, (size_t) total - 4000), tpR = truePeak (r, 4000, (size_t) total - 4000), ceilLin = std::pow (10.0f, -1.0f / 20.0f);
        const float worst = ms1::gainToDb (std::max (tpL, tpR));
        std::printf ("limiter %.0f Hz: true peak %.3f dBTP (ceiling -1.000)\n", fs, worst);
        check (std::max (tpL, tpR) < ceilLin * 1.012f, "the true peak should stay within 0.1 dB of the ceiling at every sample rate");
        (void) ceilLin;
    }

    // 5. quiet material is untouched (except the reported delay), and the delay equals latency()
    {
        const float fs = 48000.0f;
        ms1::Limiter lim; lim.prepare (fs);
        ms1::LimiterParams p; p.gainDb = 0.0f; p.ceilingDb = -1.0f; p.lookaheadMs = 2.0f;
        const int total = 20000, imp = 5000;
        std::vector<float> l ((size_t) total, 0.0f), r ((size_t) total, 0.0f);
        for (int i = 0; i < total; ++i) l[(size_t) i] = r[(size_t) i] = sine (0.3f, 440.0f, i, fs);
        l[(size_t) imp] = r[(size_t) imp] = 0.45f;
        const auto ref = l;
        for (int pos = 0; pos < total; pos += 512) lim.process (l.data() + pos, r.data() + pos, std::min (512, total - pos), p);
        const int lat = lim.latency();
        double err = 0; for (int i = lat + 100; i < total; ++i) err = std::max (err, (double) std::abs (l[(size_t) i] - ref[(size_t) (i - lat)]));
        std::printf ("quiet material: latency %d samples, max error %.6f\n", lat, err);
        check (err < 1e-4, "material under the ceiling should pass unchanged, delayed by latency()");
    }

    // 6. the loudness target: gain into the limiter raises the loudness, the ceiling holds
    {
        const float fs = 48000.0f;
        ms1::Limiter lim; lim.prepare (fs);
        ms1::LimiterParams p; p.gainDb = 10.0f; p.ceilingDb = -1.0f;
        const int total = (int) (fs * 3.0f);
        std::vector<float> l ((size_t) total), r ((size_t) total);
        for (int i = 0; i < total; ++i) l[(size_t) i] = r[(size_t) i] = sine (0.2f, 100.0f, i, fs) + sine (0.1f, 3000.0f, i, fs);
        for (int pos = 0; pos < total; pos += 512) lim.process (l.data() + pos, r.data() + pos, std::min (512, total - pos), p);
        const float peak = *std::max_element (l.begin() + 5000, l.end());
        std::printf ("gain 10 dB into a -1 dB ceiling: peak %.3f, reduction now %.2f dB\n", peak, lim.grDb.load());
        check (peak <= std::pow (10.0f, -1.0f / 20.0f) + 1e-4f && lim.grDb.load() < -0.5f, "the limiter should hold the ceiling and reduce the gain");
    }

    // 7. dither: 16 bit output has at most 2^16 levels; shapes stay bounded; off passes the input
    {
        ms1::Dither d; d.reset();
        bool onGrid = true, bounded = true;
        for (int shape = 0; shape < 3; ++shape)
            for (int i = 0; i < 20000; ++i)
            {
                const float x = 0.5f * std::sin (0.01f * (float) i), y = d.process (0, x, 16, shape);
                const float lsb = std::ldexp (1.0f, -15), k = y / lsb;
                if (std::abs (k - std::round (k)) > 1e-3f) onGrid = false;
                if (std::abs (y - x) > 8.0f * lsb) bounded = false;
            }
        check (onGrid, "16 bit output should lie on the 16 bit grid");
        check (bounded, "the dither noise should stay within a few LSB");
        check (d.process (0, 0.123456f, 0, 0) == 0.123456f, "dither off should pass the input");
    }

    // 8. corrective EQ: flat = bit-exact; every band measures its set gain at its frequency; M / S / L / R modes touch only their part;
    //    parameter jumps stay smooth; extreme settings stay finite
    {
        const float fs = 48000.0f;
        const auto defaults = [] { std::array<ms1::EqBand, ms1::kEqBands> b; static const float f[8] = { 80, 150, 400, 1000, 2500, 5000, 9000, 12000 }; for (int i = 0; i < 8; ++i) { b[(size_t) i].freq = f[i]; b[(size_t) i].q = i == 0 || i == 7 ? 0.71f : 1.0f; } return b; };
        const auto runEq = [&] (std::array<ms1::EqBand, ms1::kEqBands> bands, const std::vector<float>& inL, const std::vector<float>& inR, std::vector<float>& outL, std::vector<float>& outR)
        {
            ms1::Eq eq; eq.prepare (fs); outL = inL; outR = inR;
            for (size_t pos = 0; pos < outL.size(); pos += 512) { const int n = (int) std::min<size_t> (512, outL.size() - pos); eq.process (outL.data() + pos, outR.data() + pos, n, bands.data()); }
        };
        const int total = 48000;
        std::vector<float> l ((size_t) total), r ((size_t) total), ol, orr;
        for (int i = 0; i < total; ++i) { l[(size_t) i] = sine (0.3f, 440.0f, i, fs) + sine (0.1f, 5000.0f, i, fs); r[(size_t) i] = sine (0.2f, 700.0f, i, fs, 1.0f); }

        runEq (defaults(), l, r, ol, orr);
        check (ol == l && orr == r, "a flat EQ should leave the signal bit-exact");

        // each band at its own frequency: a long sine, measured after the settling time
        bool gainsOk = true;
        for (int band = 0; band < ms1::kEqBands; ++band)
            for (float gain : { -12.0f, 6.0f, 15.0f })
            {
                auto b = defaults(); b[(size_t) band].gainDb = gain;
                const float f = b[(size_t) band].freq;
                std::vector<float> tl ((size_t) total), tr ((size_t) total);
                for (int i = 0; i < total; ++i) tl[(size_t) i] = tr[(size_t) i] = sine (0.1f, f, i, fs);
                runEq (b, tl, tr, ol, orr);
                double e = 0, e0 = 0; for (int i = 24000; i < total; ++i) { e += (double) ol[(size_t) i] * ol[(size_t) i]; e0 += (double) tl[(size_t) i] * tl[(size_t) i]; }
                const float measured = (float) (10.0 * std::log10 (e / e0)), predicted = ms1::eqResponseDb (band, b[(size_t) band], f, fs);
                if (std::abs (measured - predicted) > 0.1f) { gainsOk = false; std::printf ("  band %d %+.0f dB at %.0f Hz: measured %.2f, predicted %.2f\n", band + 1, gain, f, measured, predicted); }
                if (band != 0 && band != 7 && std::abs (measured - gain) > 0.1f) { gainsOk = false; std::printf ("  band %d bell: measured %.2f dB for %+.0f dB\n", band + 1, measured, gain); }
            }
        check (gainsOk, "each band should measure its set gain at its frequency (bells) and match the displayed response (all)");

        {   // modes: SIDE boost leaves a mono signal alone, MID boost leaves a pure side signal alone, L leaves R alone
            auto b = defaults(); b[3].gainDb = 9.0f; b[3].freq = 440.0f;
            std::vector<float> mono ((size_t) total), side ((size_t) total), negSide ((size_t) total);
            for (int i = 0; i < total; ++i) { mono[(size_t) i] = sine (0.2f, 440.0f, i, fs); negSide[(size_t) i] = -mono[(size_t) i]; }
            b[3].mode = ms1::EqSide; runEq (b, mono, mono, ol, orr);
            double err = 0; for (int i = 0; i < total; ++i) err = std::max (err, (double) std::abs (ol[(size_t) i] - mono[(size_t) i]) + std::abs (orr[(size_t) i] - mono[(size_t) i]));
            check (err < 1e-5, "a SIDE band should not change a mono signal");
            b[3].mode = ms1::EqMid; runEq (b, mono, negSide, ol, orr);
            err = 0; for (int i = 0; i < total; ++i) err = std::max (err, (double) std::abs (ol[(size_t) i] - mono[(size_t) i]) + std::abs (orr[(size_t) i] - negSide[(size_t) i]));
            check (err < 1e-5, "a MID band should not change a pure side signal");
            b[3].mode = ms1::EqLeft; runEq (b, mono, mono, ol, orr);
            err = 0; for (int i = 0; i < total; ++i) err = std::max (err, (double) std::abs (orr[(size_t) i] - mono[(size_t) i]));
            check (err == 0.0 && std::abs (ol[40000]) > 0.0f && ol != mono, "a LEFT band should change only the left channel");
            b[3].mode = ms1::EqSide; runEq (b, mono, negSide, ol, orr);          // pure side signal: boosted by 9 dB (x2.8) in both channels with opposite sign
            double peakL = 0; for (int i = 24000; i < total; ++i) peakL = std::max (peakL, (double) std::abs (ol[(size_t) i]));
            check (std::abs (20.0 * std::log10 (peakL / 0.2) - 9.0) < 0.2, "a SIDE band should boost a pure side signal by its gain");
        }

        {   // a jump of every parameter in the middle of the signal: no click larger than the signal itself, everything finite
            auto b = defaults();
            ms1::Eq eq; eq.prepare (fs);
            std::vector<float> a ((size_t) total), c ((size_t) total);
            for (int i = 0; i < total; ++i) a[(size_t) i] = c[(size_t) i] = sine (0.3f, 300.0f, i, fs);
            float maxStep = 0.0f, prev = 0.0f; bool finite = true;
            for (int pos = 0; pos < total; pos += 256)
            {
                if (pos == 20480) for (int k = 0; k < ms1::kEqBands; ++k) { b[(size_t) k].gainDb = k % 2 ? 18.0f : -18.0f; b[(size_t) k].q = 8.0f; b[(size_t) k].freq = 300.0f * (float) (k + 1); }
                if (pos == 30720) for (int k = 0; k < ms1::kEqBands; ++k) b[(size_t) k].gainDb = 0.0f;
                eq.process (a.data() + pos, c.data() + pos, 256, b.data());
                for (int i = pos; i < pos + 256; ++i) { if (! std::isfinite (a[(size_t) i])) finite = false; maxStep = std::max (maxStep, std::abs (a[(size_t) i] - prev)); prev = a[(size_t) i]; }
            }
            std::printf ("EQ jumps: largest sample step %.3f (the 300 Hz sine alone steps %.3f)\n", maxStep, 0.3f * 6.2832f * 300.0f / fs);
            check (finite && maxStep < 1.5f, "parameter jumps should be ramped: finite and without clicks");
        }
    }

    // 9. bus compressor: static curve, attack / release time, stereo link, mix, sidechain filter, off = bit-exact
    {
        const float fs = 48000.0f;
        const auto runBus = [&] (const ms1::BusParams& p, std::vector<float>& l, std::vector<float>& r, float* grOut = nullptr, std::vector<float>* grTrace = nullptr)
        {
            ms1::BusCompressor c; c.prepare (fs);
            for (size_t pos = 0; pos < l.size(); pos += 256)
            {
                const int n = (int) std::min<size_t> (256, l.size() - pos);
                c.process (l.data() + pos, r.data() + pos, n, p);
                if (grTrace) grTrace->push_back (c.grDb.load());
            }
            if (grOut) *grOut = c.grDb.load();
        };
        const int total = 96000;
        const auto tone = [&] (float db, float hz) { std::vector<float> v ((size_t) total); for (int i = 0; i < total; ++i) v[(size_t) i] = ms1::dbToGain (db) * sine (1.0f, hz, i, fs); return v; };
        const auto rmsDb = [] (const std::vector<float>& v, int a, int b) { double e = 0; for (int i = a; i < b; ++i) e += (double) v[(size_t) i] * v[(size_t) i]; return 10.0 * std::log10 (e / (b - a) + 1e-20); };

        ms1::BusParams p; p.on = true; p.thresholdDb = -20.0f; p.ratio = 2; p.attack = 2; p.release = 2; p.makeupDb = 0.0f; p.mix = 1.0f;     // 4:1, 1 ms
        {   // a -8 dB peak sine is 12 dB over the threshold: 9 dB of reduction at 4:1
            auto l = tone (-8.0f, 1000.0f), r = l; const auto in = l;
            runBus (p, l, r);
            const double reduction = rmsDb (in, 60000, total) - rmsDb (l, 60000, total);
            std::printf ("BUS 4:1, 12 dB over: reduction %.2f dB (expect about 9)\n", reduction);
            check (std::abs (reduction - 9.0) < 0.6, "the static curve should reduce 12 dB over a 4:1 threshold by about 9 dB");
            check (l == r, "the two channels of a mono signal stay identical (stereo link)");
        }
        {   // below the threshold: untouched (outside the soft knee)
            auto l = tone (-40.0f, 1000.0f), r = l; const auto in = l;
            runBus (p, l, r);
            check (std::abs (rmsDb (in, 60000, total) - rmsDb (l, 60000, total)) < 0.01, "a signal well below the threshold should not change");
        }
        {   // attack: a step from silence to 12 dB over; after about the attack time the reduction has reached 63 % of its final value
            ms1::BusParams pa = p; pa.attack = 4;                                    // 10 ms
            std::vector<float> l ((size_t) total), r;
            for (int i = 0; i < total; ++i) l[(size_t) i] = i < 4800 ? 0.0f : ms1::dbToGain (-8.0f) * sine (1.0f, 1000.0f, i, fs);
            r = l;
            ms1::BusCompressor c; c.prepare (fs);
            std::vector<float> gr;
            for (int pos = 0; pos + 48 <= total; pos += 48) { c.process (l.data() + pos, r.data() + pos, 48, pa); gr.push_back (c.grDb.load()); }
            // block 100 = 4800 samples (the step); the final reduction after 1 s
            const float finalGr = gr.back();
            int idx63 = -1; for (size_t k = 100; k < gr.size(); ++k) if (gr[k] <= 0.632f * finalGr) { idx63 = (int) k; break; }
            const float ms = idx63 < 0 ? -1.0f : (float) (idx63 - 100) * 48.0f / fs * 1000.0f;
            std::printf ("BUS attack 10 ms: 63 %% of the reduction after %.1f ms\n", ms);
            check (ms > 5.0f && ms < 20.0f, "the attack time should be about the set attack");
        }
        {   // release: after the loud part ends, the reduction falls to 37 % in about the release time (600 ms), and AUTO recovers faster at first
            const auto releaseMs = [&] (int rel)
            {
                ms1::BusParams pr = p; pr.release = rel;
                std::vector<float> l ((size_t) total * 2), r;
                for (int i = 0; i < total * 2; ++i) l[(size_t) i] = i < total ? ms1::dbToGain (-8.0f) * sine (1.0f, 1000.0f, i, fs) : 0.0f;
                r = l;
                ms1::BusCompressor c; c.prepare (fs);
                float peak = 0.0f; int found = -1; int blocks = 0;
                for (int pos = 0; pos + 48 <= 2 * total; pos += 48, ++blocks)
                {
                    c.process (l.data() + pos, r.data() + pos, 48, pr);
                    const float g = -c.grDb.load();
                    if (pos < total) peak = g; else if (found < 0 && g < 0.368f * peak) found = pos - total;
                }
                return found < 0 ? -1.0f : (float) found / fs * 1000.0f;
            };
            const float r600 = releaseMs (2), rAuto = releaseMs (4);
            std::printf ("BUS release 600 ms: 37 %% after %.0f ms; AUTO: %.0f ms\n", r600, rAuto);
            check (r600 > 400.0f && r600 < 800.0f, "the release time should be about the set release");
            check (rAuto > 0.0f && rAuto < r600, "AUTO release should start recovering faster than 600 ms");
        }
        {   // mix 0 = untouched; off = bit-exact; makeup is applied
            auto l = tone (-8.0f, 1000.0f), r = l; const auto in = l;
            ms1::BusParams pm = p; pm.mix = 0.0f; runBus (pm, l, r);
            check (l == in, "mix 0 should leave the signal bit-exact");
            l = in; r = in; pm = p; pm.on = false; runBus (pm, l, r);
            check (l == in && r == in, "the stage off should be bit-exact");
            l = tone (-40.0f, 1000.0f); r = l; const auto q = l; pm = p; pm.makeupDb = 6.0f; runBus (pm, l, r);
            check (std::abs (rmsDb (l, 60000, total) - rmsDb (q, 60000, total) - 6.0) < 0.05, "make-up gain 6 dB should add 6 dB");
        }
        {   // the sidechain high-pass: a loud 40 Hz tone is reduced much less with the 150 Hz filter
            auto a = tone (-8.0f, 40.0f), b = a, ar = a, br = a;
            float grOff = 0, grHp = 0;
            runBus (p, a, ar, &grOff);
            ms1::BusParams ph = p; ph.sidechain = 3; runBus (ph, b, br, &grHp);
            std::printf ("BUS sidechain: gain reduction %.1f dB without, %.1f dB with the 150 Hz high-pass\n", grOff, grHp);
            check (grHp > grOff + 3.0f, "the sidechain high-pass should reduce the effect of low frequencies on the detector");
        }
        {   // extreme and finite: loud noise, every ratio / attack / release, no NaN
            bool finite = true;
            for (int ra = 0; ra < 4; ++ra) for (int at = 0; at < 6; ++at) for (int re = 0; re < 5; ++re)
            {
                ms1::BusParams pe = p; pe.ratio = ra; pe.attack = at; pe.release = re; pe.thresholdDb = -30.0f; pe.makeupDb = 12.0f; pe.sidechain = (ra + at) % 4;
                std::vector<float> l (4800), r (4800); unsigned sd = 1; for (int i = 0; i < 4800; ++i) { sd = sd * 1664525u + 1013904223u; l[(size_t) i] = ((float) (sd >> 8) / 8388608.0f - 1.0f); r[(size_t) i] = -l[(size_t) i]; }
                runBus (pe, l, r);
                for (float x : l) if (! std::isfinite (x)) finite = false;
            }
            check (finite, "every setting should stay finite");
        }
    }

    // 10. dynamic EQ: a -12 dB band cut that only applies when the band is loud (ABOVE), a +12 dB boost that only applies when it is quiet (BELOW)
    {
        const float fs = 48000.0f;
        const auto level = [&] (ms1::EqBand band, float toneDb)
        {
            std::array<ms1::EqBand, ms1::kEqBands> b; for (auto& x : b) x.on = false; b[3] = band;
            const int total = 96000; std::vector<float> l ((size_t) total), r;
            for (int i = 0; i < total; ++i) l[(size_t) i] = ms1::dbToGain (toneDb) * sine (1.0f, 1000.0f, i, fs);
            r = l; const auto in = l; ms1::Eq eq; eq.prepare (fs);
            for (size_t pos = 0; pos < l.size(); pos += 256) eq.process (l.data() + pos, r.data() + pos, 256, b.data());
            double e = 0, e0 = 0; for (int i = 48000; i < total; ++i) { e += (double) l[(size_t) i] * l[(size_t) i]; e0 += (double) in[(size_t) i] * in[(size_t) i]; }
            return (float) (10.0 * std::log10 (e / e0));
        };
        ms1::EqBand cut; cut.freq = 1000.0f; cut.q = 1.0f; cut.gainDb = -12.0f; cut.dyn = true; cut.dynThresholdDb = -30.0f; cut.dynRatio = 4.0f;
        const float loud = level (cut, -10.0f), quiet = level (cut, -50.0f);
        std::printf ("dynamic cut: loud tone %.2f dB, quiet tone %.2f dB\n", loud, quiet);
        check (loud < -10.5f && std::abs (quiet) < 0.3f, "a dynamic cut applies to a loud band and leaves a quiet one alone");
        ms1::EqBand boost = cut; boost.gainDb = 12.0f; boost.dynBelow = true;
        const float bq = level (boost, -50.0f), bl = level (boost, -10.0f);
        std::printf ("dynamic boost below the threshold: quiet tone %.2f dB, loud tone %.2f dB\n", bq, bl);
        check (bq > 8.0f && std::abs (bl) < 0.3f, "a boost below the threshold lifts a quiet band and leaves a loud one alone");
        ms1::EqBand stat = cut; stat.dyn = false;
        check (std::abs (level (stat, -50.0f) + 12.0f) < 0.3f, "the same band static cuts 12 dB at any level");
    }

    // 11. spectrum calibration: a -6 dBFS sine reads -6 dB at its own band in both modes
    for (int mode = 0; mode < 2; ++mode)
        for (float hz : { 100.0f, 1000.0f, 5000.0f })
        {
            const float fs = 48000.0f; ms1::Spectrum sp; sp.prepare (fs); sp.setMode (mode);
            for (int i = 0; i < 48000; ++i) sp.push (0.5f * std::sin (6.28318530718f * hz * (float) i / fs));
            int best = 0; for (int b = 0; b < ms1::kBands; ++b) if (sp.bands[b].load() > sp.bands[best].load()) best = b;
            const float centre = 20.0f * std::pow (1000.0f, ((float) best + 0.5f) / (float) ms1::kBands);
            std::printf ("spectrum %s, %.0f Hz sine at -6 dBFS: %.2f dB in the band at %.0f Hz\n", mode ? "FFT 8192" : "SMOOTH", hz, (double) sp.bands[best].load(), centre);
            check (std::abs (sp.bands[best].load() + 6.02f) < 0.7f && std::abs (centre / hz - 1.0f) < 0.1f, "a sine reads its level in the band of its frequency");
        }

    // 12. level meters: sample peak, true RMS, the 3-second true peak, DR
    {
        const float fs = 48000.0f; ms1::LevelMeter lm; lm.prepare (fs);
        std::vector<float> l (4800), r (4800);
        for (int pos = 0; pos < 480000; pos += 4800)
        {
            for (int i = 0; i < 4800; ++i) l[(size_t) i] = r[(size_t) i] = 0.5f * std::sin (6.28318530718f * 1000.0f * (float) (pos + i) / fs);
            lm.process (l.data(), r.data(), 4800, 0.5f);
        }
        std::printf ("level meters, 0.5 sine: peak %.2f dB, RMS %.2f dB, window peak %.2f dB, DR %.2f\n", (double) lm.samplePeakDb.load(), (double) lm.rmsDb.load(), (double) lm.windowPeakDb.load(), (double) lm.dr.load());
        check (std::abs (lm.samplePeakDb.load() + 6.02f) < 0.05f, "sample peak of a 0.5 sine is -6 dB");
        check (std::abs (lm.rmsDb.load() + 9.03f) < 0.05f, "true RMS of a 0.5 sine is -9 dB");
        check (std::abs (lm.windowPeakDb.load() + 6.02f) < 0.05f, "the 3-second peak follows the true peak given");
        check (lm.drValid.load() && std::abs (lm.dr.load()) < 0.2f, "DR of a steady sine is 0");
        ms1::LevelMeter lc; lc.prepare (fs);                                                // a click of 0.9 once per 3 s block on a 0.1 sine: DR = 20 log (0.9 / 0.1)
        for (int pos = 0; pos < 480000; pos += 4800)
        {
            for (int i = 0; i < 4800; ++i) l[(size_t) i] = r[(size_t) i] = 0.1f * std::sin (6.28318530718f * 1000.0f * (float) (pos + i) / fs) + (((pos + i) % 144000) == 1000 ? 0.9f : 0.0f);
            lc.process (l.data(), r.data(), 4800, 0.9f);
        }
        std::printf ("  clicks on a quiet sine: DR %.2f (expect 19.1)\n", (double) lc.dr.load());
        check (std::abs (lc.dr.load() - 19.1f) < 1.5f, "DR of clicks (0.9, plus the sine at that instant) on a 0.1 sine is about 19 dB");
        lc.reset(); check (! lc.drValid.load() && lc.samplePeakDb.load() < -100.0f, "reset clears the meters");
    }

    // 13. stereo meter: mono +1, out of phase -1, independent noise about 0; the integrated value skips silence
    {
        const float fs = 48000.0f; std::mt19937 rng (5); std::uniform_real_distribution<float> u (-1.0f, 1.0f);
        const auto run = [&] (int mode, int seconds, int silence)
        {
            ms1::StereoMeter sm; sm.prepare (fs);
            std::vector<float> l (4800), r (4800);
            for (int pos = 0; pos < (seconds + silence) * 48000; pos += 4800)
            {
                for (int i = 0; i < 4800; ++i)
                {
                    const bool quiet = pos >= seconds * 48000; const float a = u (rng) * 0.3f, b = u (rng) * 0.3f;
                    l[(size_t) i] = quiet ? 0.0f : a; r[(size_t) i] = quiet ? 0.0f : (mode == 0 ? a : mode == 1 ? -a : b);
                }
                sm.process (l.data(), r.data(), 4800);
            }
            return std::make_pair (sm.corrShort.load(), sm.corrInt.load());
        };
        const auto mono = run (0, 8, 0), anti = run (1, 8, 0), indep = run (2, 20, 0), gap = run (0, 6, 6);
        std::printf ("stereo meter: mono %.2f / %.2f, out of phase %.2f / %.2f, independent %.2f / %.2f, mono then silence: integrated %.2f\n", (double) mono.first, (double) mono.second, (double) anti.first, (double) anti.second, (double) indep.first, (double) indep.second, (double) gap.second);
        check (mono.first > 0.99f && mono.second > 0.99f, "a mono signal reads +1");
        check (anti.first < -0.99f && anti.second < -0.99f, "an out-of-phase signal reads -1");
        check (std::abs (indep.second) < 0.1f, "independent noise integrates to about 0");
        check (gap.second > 0.99f, "the integrated value ignores silence");
    }

    // 14. EBU Tech 3341 / 3342 test signals through the loudness meter (a stereo 997 Hz sine, both channels equal: the K-weighting is +0.691 dB there, which the -0.691 offset cancels, so loudness = 20 log A)
    {
        const float fs = 48000.0f;
        const auto amp = [] (float lufs) { return std::pow (10.0f, lufs / 20.0f); };
        struct Seg { float lufs, seconds; };
        struct Res { float integrated, range, maxM, maxS; };
        const auto measure = [&] (const std::vector<Seg>& segs)
        {
            ms1::Loudness ld; ld.prepare (fs); float maxM = -120.0f, maxS = -120.0f;
            std::vector<float> l (4800), r;
            long pos = 0;
            for (const auto& sg : segs)
                for (int blk = 0; blk < (int) (sg.seconds * 10.0f); ++blk)
                {
                    for (int i = 0; i < 4800; ++i) l[(size_t) i] = amp (sg.lufs) * std::sin (6.28318530718f * 997.0f * (float) (pos + i) / fs);
                    r = l; ld.process (l.data(), r.data(), 4800); pos += 4800;
                    maxM = std::max (maxM, ld.momentary.load()); maxS = std::max (maxS, ld.shortTerm.load());
                }
            return Res { ld.integrated.load(), ld.range.load(), maxM, maxS };
        };
        { const auto m = measure ({ { -23.0f, 20.0f } }); std::printf ("EBU 3341-1 (-23 LUFS): M max %.2f, S max %.2f, I %.2f\n", (double) m.maxM, (double) m.maxS, (double) m.integrated);
          check (std::abs (m.maxM + 23.0f) < 0.1f && std::abs (m.maxS + 23.0f) < 0.1f && std::abs (m.integrated + 23.0f) < 0.1f, "3341 case 1: -23 LUFS reads -23.0 +-0.1 (momentary, short-term, integrated)"); }
        { const auto m = measure ({ { -33.0f, 20.0f } }); check (std::abs (m.maxM + 33.0f) < 0.1f && std::abs (m.maxS + 33.0f) < 0.1f && std::abs (m.integrated + 33.0f) < 0.1f, "3341 case 2: -33 LUFS reads -33.0 +-0.1"); }
        { const auto m = measure ({ { -36.0f, 10.0f }, { -23.0f, 60.0f }, { -36.0f, 10.0f } }); std::printf ("EBU 3341-3: I %.2f\n", (double) m.integrated);
          check (std::abs (m.integrated + 23.0f) < 0.1f, "3341 case 3: the relative gate removes the quiet parts: -23.0 +-0.1"); }
        { const auto m = measure ({ { -72.0f, 20.0f }, { -23.0f, 20.0f } }); check (std::abs (m.integrated + 23.0f) < 0.1f, "the absolute gate (-70 LUFS) removes -72 LUFS"); }
        const auto lra = [&] (const std::vector<Seg>& segs) { return measure (segs).range; };
        const float r1 = lra ({ { -20.0f, 20.0f }, { -30.0f, 20.0f } }), r2 = lra ({ { -20.0f, 20.0f }, { -15.0f, 20.0f } }), r3 = lra ({ { -40.0f, 20.0f }, { -20.0f, 20.0f } }),
                    r4 = lra ({ { -50.0f, 20.0f }, { -35.0f, 20.0f }, { -20.0f, 20.0f }, { -35.0f, 20.0f }, { -50.0f, 20.0f } });
        std::printf ("EBU 3342 LRA: %.2f (10), %.2f (5), %.2f (20), %.2f (15)\n", (double) r1, (double) r2, (double) r3, (double) r4);
        check (std::abs (r1 - 10.0f) < 1.0f && std::abs (r2 - 5.0f) < 1.0f && std::abs (r3 - 20.0f) < 1.0f && std::abs (r4 - 15.0f) < 1.0f, "3342 cases 1 to 4: loudness range 10 / 5 / 20 / 15 LU within +-1");
    }

    std::printf ("%d problems\n", problems);
    return problems == 0 ? 0 : 1;
}
