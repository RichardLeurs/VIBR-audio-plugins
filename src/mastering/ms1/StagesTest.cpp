// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
// Offline check of the MS-1 stages of version 0.4: the Linkwitz-Riley crossovers add up flat; the multiband compressor reduces only the band that is
// over its threshold, solo works and idle / off is neutral; the imager's width, mono-below and balance do what they say and leave the mid alone;
// the colour stage has a constant latency of 24 samples, the characters make the right harmonics and the 4x oversampling keeps aliasing low.
#include <array>
#include <cstdio>
#include <random>
#include <vector>
#include "MasteringStages.h"

static const float fs = 48000.0f;
static std::vector<float> sineWave (float amp, float hz, int n, float phase = 0.0f) { std::vector<float> v ((size_t) n); for (int i = 0; i < n; ++i) v[(size_t) i] = amp * std::sin (6.28318530718f * hz * (float) i / fs + phase); return v; }
static double rmsOf (const std::vector<float>& v, int a, int b) { double e = 0; for (int i = a; i < b; ++i) e += (double) v[(size_t) i] * v[(size_t) i]; return std::sqrt (e / (b - a)); }
static double db (double x) { return 20.0 * std::log10 (std::max (1e-12, x)); }
static double bin (const std::vector<float>& v, float hz, int a, int b)         // single-bin amplitude
{
    double re = 0, im = 0; for (int i = a; i < b; ++i) { const double ph = 6.283185307 * hz * i / fs; re += v[(size_t) i] * std::cos (ph); im += v[(size_t) i] * std::sin (ph); }
    return 2.0 * std::sqrt (re * re + im * im) / (b - a);
}
template <class F> static void blocks (std::vector<float>& l, std::vector<float>& r, F f) { for (size_t pos = 0; pos < l.size(); pos += 256) f (l.data() + pos, r.data() + pos, (int) std::min<size_t> (256, l.size() - pos)); }

int main()
{
    int problems = 0;
    const auto check = [&problems] (bool ok, const char* what) { std::printf ("  %-78s %s\n", what, ok ? "ok" : "FAILED"); if (! ok) ++problems; };
    const int total = 48000;

    std::printf ("crossovers\n");
    {
        bool flat3 = true, flat2 = true; double worst = 0;
        for (float f : { 30.0f, 60.0f, 100.0f, 120.0f, 300.0f, 1000.0f, 1200.0f, 2500.0f, 6000.0f, 9000.0f, 15000.0f })
        {
            const auto in = sineWave (0.5f, f, total);
            ms1::Crossover<3> c3; const float fr3[3] = { 120.0f, 1200.0f, 6000.0f }; c3.set (fr3, fs);
            ms1::Crossover<2> c2; const float fr2[2] = { 200.0f, 4000.0f }; c2.set (fr2, fs);
            std::vector<float> o3 ((size_t) total), o2 ((size_t) total);
            for (int i = 0; i < total; ++i) { float b3[4], b2[3]; c3.split (0, in[(size_t) i], b3); c2.split (0, in[(size_t) i], b2); o3[(size_t) i] = b3[0] + b3[1] + b3[2] + b3[3]; o2[(size_t) i] = b2[0] + b2[1] + b2[2]; }
            const double d3 = db (rmsOf (o3, 24000, total) / rmsOf (in, 24000, total)), d2 = db (rmsOf (o2, 24000, total) / rmsOf (in, 24000, total));
            worst = std::max ({ worst, std::abs (d3), std::abs (d2) });
            if (std::abs (d3) > 0.05) flat3 = false; if (std::abs (d2) > 0.05) flat2 = false;
        }
        std::printf ("  worst deviation of the band sum: %.4f dB\n", worst);
        check (flat3, "4 bands (3 splits): the sum is flat within 0.05 dB");
        check (flat2, "3 bands (2 splits): the sum is flat within 0.05 dB");
    }

    std::printf ("multiband compressor\n");
    {
        ms1::MbParams p; p.on = true;
        for (auto& b : p.band) { b.thresholdDb = 0.0f; b.ratio = 1.0f; }
        const auto tone = sineWave (0.3f, 1000.0f, total * 2);
        {   // idle compressors: neutral
            auto l = tone, r = tone; ms1::MultibandComp mb; mb.prepare (fs);
            blocks (l, r, [&] (float* a, float* b, int n) { mb.process (a, b, n, p); });
            check (std::abs (db (rmsOf (l, 48000, total * 2) / rmsOf (tone, 48000, total * 2))) < 0.05, "idle (ratio 1): the level is unchanged within 0.05 dB");
        }
        {   // off: bit-exact
            auto l = tone, r = tone; ms1::MultibandComp mb; mb.prepare (fs); auto q = p; q.on = false;
            blocks (l, r, [&] (float* a, float* b, int n) { mb.process (a, b, n, q); });
            check (l == tone && r == tone, "off: bit-exact");
        }
        {   // only the band of 400 Hz (band 2: 120 .. 1200 Hz) is over its threshold: 4:1 over a -10.5 dB peak at -30 dB = 19.5 over -> 14.6 dB
            auto q = p; q.band[1].thresholdDb = -30.0f; q.band[1].ratio = 4.0f; q.band[1].attackMs = 1.0f;
            const auto tone = sineWave (0.3f, 400.0f, total * 2);
            auto l = tone, r = tone; ms1::MultibandComp mb; mb.prepare (fs);
            blocks (l, r, [&] (float* a, float* b, int n) { mb.process (a, b, n, q); });
            const double red = db (rmsOf (tone, 48000, total * 2) / rmsOf (l, 48000, total * 2));
            std::printf ("  band 2 reduction %.2f dB (gr meter %.2f, %.2f, %.2f, %.2f)\n", red, mb.gr[0].load(), mb.gr[1].load(), mb.gr[2].load(), mb.gr[3].load());
            check (red > 11.0 && red < 16.0 && mb.gr[2].load() > -0.5f && mb.gr[3].load() > -0.5f, "only the band that holds the tone is compressed, by about the expected amount");
        }
        {   // solo of band 4: the 1 kHz tone is gone
            auto q = p; q.band[3].solo = true;
            auto l = tone, r = tone; ms1::MultibandComp mb; mb.prepare (fs);
            blocks (l, r, [&] (float* a, float* b, int n) { mb.process (a, b, n, q); });
            check (db (rmsOf (l, 48000, total * 2) / rmsOf (tone, 48000, total * 2)) < -30.0, "solo of the top band: a 1 kHz tone is removed");
        }
        {   // extreme and finite
            bool finite = true; std::mt19937 rng (3); std::uniform_real_distribution<float> u (-1, 1);
            auto q = p; for (auto& b : q.band) { b.thresholdDb = -60.0f; b.ratio = 20.0f; b.attackMs = 0.1f; b.releaseMs = 10.0f; b.makeupDb = 12.0f; b.autoRelease = true; }
            std::vector<float> l (48000), r (48000); for (int i = 0; i < 48000; ++i) { l[(size_t) i] = u (rng); r[(size_t) i] = u (rng); }
            ms1::MultibandComp mb; mb.prepare (fs); blocks (l, r, [&] (float* a, float* b, int n) { mb.process (a, b, n, q); });
            for (float x : l) if (! std::isfinite (x)) finite = false;
            check (finite, "extreme settings on noise stay finite");
        }
    }

    std::printf ("imager\n");
    {
        ms1::ImagerParams p; p.on = true;
        const auto tone = sineWave (0.3f, 1000.0f, total * 2), tone60 = sineWave (0.3f, 60.0f, total * 2);
        std::vector<float> neg (tone.size()); for (size_t i = 0; i < tone.size(); ++i) neg[i] = -tone[i];
        const auto run = [&] (const ms1::ImagerParams& q, std::vector<float> l, std::vector<float> r)
        {
            ms1::Imager im; im.prepare (fs); blocks (l, r, [&] (float* a, float* b, int n) { im.process (a, b, n, q); }); return std::make_pair (l, r);
        };
        {   auto q = p; q.on = false; auto [l, r] = run (q, tone, neg); check (l == tone && r == neg, "off: bit-exact"); }
        {   auto [l, r] = run (p, tone, tone); check (std::abs (db (rmsOf (l, 48000, total * 2) / rmsOf (tone, 48000, total * 2))) < 0.05, "width 100 %: a mono signal keeps its level (the mid is never touched)"); }
        {   auto [l, r] = run (p, tone, neg); check (std::abs (db (rmsOf (l, 48000, total * 2) / rmsOf (tone, 48000, total * 2))) < 0.05, "width 100 %: a pure side signal keeps its level"); }
        {   auto q = p; q.width[1] = 2.0f; auto [l, r] = run (q, tone, neg);
            const double g = db (rmsOf (l, 48000, total * 2) / rmsOf (tone, 48000, total * 2)); std::printf ("  width 200 %% on a side tone: %+.2f dB\n", g);
            check (std::abs (g - 6.02) < 0.15, "width 200 %: a side signal in that band gets +6 dB"); }
        {   auto q = p; q.width[1] = 0.0f; auto [l, r] = run (q, tone, neg); check (rmsOf (l, 48000, total * 2) < 0.002, "width 0 %: a pure side signal is removed"); }
        {   auto q = p; q.width[0] = q.width[1] = q.width[2] = 0.0f; std::vector<float> other = sineWave (0.3f, 1000.0f, total * 2, 1.0f); auto [l, r] = run (q, tone, other);
            double e = 0; for (int i = 48000; i < total * 2; ++i) e = std::max (e, (double) std::abs (l[(size_t) i] - r[(size_t) i])); check (e < 1e-4, "width 0 % in all bands: the output is mono"); }
        {   auto q = p; q.monoBelowHz = 150.0f; auto [l, r] = run (q, tone60, sineWave (-0.3f, 60.0f, total * 2));
            const double g = db (rmsOf (l, 48000, total * 2) / rmsOf (tone60, 48000, total * 2)); std::printf ("  mono below 150 Hz on a 60 Hz side tone: %.1f dB\n", g);
            check (g < -28.0, "mono-below 150 Hz: the side signal at 60 Hz is removed (24 dB per octave)");
            auto [l2, r2] = run (q, tone, neg); check (std::abs (db (rmsOf (l2, 48000, total * 2) / rmsOf (tone, 48000, total * 2))) < 0.3, "mono-below 150 Hz: a 1 kHz side signal passes"); }
        {   auto q = p; q.balanceDb = 6.0f; auto [lm, rm] = run (q, tone, tone); auto [ls, rs] = run (q, tone, neg);
            const double gm = db (rmsOf (lm, 48000, total * 2) / rmsOf (tone, 48000, total * 2)), gs = db (rmsOf (ls, 48000, total * 2) / rmsOf (tone, 48000, total * 2));
            std::printf ("  balance +6 dB: mid %+.2f dB, side %+.2f dB\n", gm, gs);
            check (std::abs (gm - 3.0) < 0.15 && std::abs (gs + 3.0) < 0.15, "balance +6 dB: mid +3 dB, side -3 dB"); }
        {   ms1::Imager im; im.prepare (fs); auto l = tone, r = neg; blocks (l, r, [&] (float* a, float* b, int n) { im.process (a, b, n, p); });
            check (im.correlation.load() < -0.9f, "the correlation meter reads -1 for an out-of-phase signal"); }
    }

    std::printf ("colour\n");
    {
        const auto tone = sineWave (0.01f, 1000.0f, total);
        ms1::ColourParams p; p.on = false;
        {   // off: the signal delayed by kLatency, bit-exact
            auto l = tone, r = tone; ms1::Colour c; c.prepare (fs); blocks (l, r, [&] (float* a, float* b, int n) { c.process (a, b, n, p); });
            bool same = true; for (int i = ms1::Colour::kLatency; i < total; ++i) if (l[(size_t) i] != tone[(size_t) (i - ms1::Colour::kLatency)]) same = false;
            check (same, "off: the signal is delayed by exactly 24 samples and otherwise bit-exact"); }
        {   // on with almost no drive: the oversampling path is transparent and has the same delay
            p.on = true; p.drive = 0.001f;
            auto l = tone, r = tone; ms1::Colour c; c.prepare (fs); blocks (l, r, [&] (float* a, float* b, int n) { c.process (a, b, n, p); });
            double e = 0; for (int i = 24000; i < total; ++i) e = std::max (e, (double) std::abs (l[(size_t) i] - tone[(size_t) (i - ms1::Colour::kLatency)]));
            std::printf ("  on, drive 0.1 %%: largest difference to the delayed input %.5f (amplitude 0.01)\n", e);
            check (e < 0.0008, "on: the oversampled path has the same delay (alignment); only the 3 % auto-gain differs at low level"); }
        {   // harmonics at a high level: tube makes the 2nd, tape and transformer the 3rd
            const auto loud = sineWave (0.5f, 1000.0f, total);
            double h2[3], h3[3], h1[3];
            for (int ch = 0; ch < 3; ++ch)
            {
                ms1::ColourParams q; q.on = true; q.character = ch; q.drive = 0.8f; auto l = loud, r = loud; ms1::Colour c; c.prepare (fs);
                blocks (l, r, [&] (float* a, float* b, int n) { c.process (a, b, n, q); });
                h1[ch] = bin (l, 1000.0f, 24000, total); h2[ch] = db (bin (l, 2000.0f, 24000, total) / h1[ch]); h3[ch] = db (bin (l, 3000.0f, 24000, total) / h1[ch]);
                std::printf ("  character %d: 2nd %.1f dBc, 3rd %.1f dBc\n", ch, h2[ch], h3[ch]);
            }
            check (h2[1] > -40.0 && h2[1] > h2[0] + 20.0, "tube: even harmonics (2nd above -40 dBc and well above tape's)");
            check (h3[0] > -45.0 && h3[2] > -45.0 && h2[0] < -60.0 && h2[2] < -60.0, "tape and transformer: odd harmonics (3rd strong, 2nd below -60 dBc)");
        }
        {   // aliasing: a 7 kHz tone, hard driven: the 5th harmonic (35 kHz) would fold to 13 kHz without oversampling
            const auto loud = sineWave (0.9f, 7000.0f, total);
            ms1::ColourParams q; q.on = true; q.character = ms1::Tape; q.drive = 1.0f; auto l = loud, r = loud; ms1::Colour c; c.prepare (fs);
            blocks (l, r, [&] (float* a, float* b, int n) { c.process (a, b, n, q); });
            const double alias = db (bin (l, 13000.0f, 24000, total) / bin (l, 7000.0f, 24000, total));
            std::printf ("  aliased 5th harmonic at 13 kHz: %.1f dBc\n", alias);
            check (alias < -65.0, "4x oversampling keeps the aliased harmonic below -65 dBc");
        }
        {   // mid / side modes: SIDE colour leaves a mono signal untouched (delayed), MID colour a pure side signal
            const auto loud = sineWave (0.4f, 500.0f, total); std::vector<float> neg (loud.size()); for (size_t i = 0; i < loud.size(); ++i) neg[i] = -loud[i];
            ms1::ColourParams q; q.on = true; q.drive = 0.9f; q.mode = ms1::ColourSide; auto l = loud, r = loud; ms1::Colour c; c.prepare (fs);
            blocks (l, r, [&] (float* a, float* b, int n) { c.process (a, b, n, q); });
            double e = 0; for (int i = 24000; i < total; ++i) e = std::max (e, (double) std::abs (l[(size_t) i] - loud[(size_t) (i - 24)]));
            check (e < 1e-5, "SIDE mode: a mono signal passes untouched (delayed)");
            q.mode = ms1::ColourMid; l = loud; r = neg; ms1::Colour c2; c2.prepare (fs); blocks (l, r, [&] (float* a, float* b, int n) { c2.process (a, b, n, q); });
            e = 0; for (int i = 24000; i < total; ++i) e = std::max (e, (double) std::abs (l[(size_t) i] - loud[(size_t) (i - 24)]));
            check (e < 1e-5, "MID mode: a pure side signal passes untouched (delayed)");
        }
        {   // auto-gain: the level of a -10 dB signal changes little with the drive; extreme settings are finite
            const auto mid = sineWave (0.3f, 440.0f, total); bool finite = true; double lo = 99, hi = -99;
            for (int ch = 0; ch < 3; ++ch) for (float drive : { 0.0f, 0.3f, 0.6f, 1.0f })
            {
                ms1::ColourParams q; q.on = true; q.character = ch; q.drive = drive; q.tiltDb = 6.0f; q.outDb = 12.0f; auto l = mid, r = mid; ms1::Colour c; c.prepare (fs);
                blocks (l, r, [&] (float* a, float* b, int n) { c.process (a, b, n, q); }); for (float x : l) if (! std::isfinite (x)) finite = false;
                ms1::ColourParams q2 = q; q2.tiltDb = 0.0f; q2.outDb = 0.0f; l = mid; r = mid; ms1::Colour c2; c2.prepare (fs);
                blocks (l, r, [&] (float* a, float* b, int n) { c2.process (a, b, n, q2); });
                const double g = db (rmsOf (l, 24000, total) / rmsOf (mid, 24000, total)); lo = std::min (lo, g); hi = std::max (hi, g);
            }
            std::printf ("  level change at 0.3 amplitude over all characters and drives: %.2f .. %.2f dB\n", lo, hi);
            check (finite, "extreme settings stay finite"); check (lo > -3.0 && hi < 3.0, "auto-gain keeps the level of a typical signal within +-3 dB");
        }
    }

    std::printf("extras of version 0.5\n");
    {   // imager auto safe width: a side-heavy band is backed off, a mono one is not
        ms1::ImagerParams p; p.on = true; p.width[1] = 2.0f; p.safe = true;
        const auto tone = sineWave (0.3f, 1000.0f, total * 2); std::vector<float> neg (tone.size()); for (size_t i = 0; i < tone.size(); ++i) neg[i] = -tone[i];
        const auto run = [&] (const ms1::ImagerParams& q, std::vector<float> l, std::vector<float> r)
        { ms1::Imager im; im.prepare (fs); blocks (l, r, [&] (float* a, float* b, int n) { im.process (a, b, n, q); }); return std::make_pair (l, r); };
        auto [ls, rs] = run (p, tone, neg); auto q = p; q.safe = false; auto [lu, ru] = run (q, tone, neg);
        const double dSafe = db (rmsOf (ls, 48000, total * 2)), dOff = db (rmsOf (lu, 48000, total * 2));
        std::printf ("  side signal at width 200 %%: %.1f dB without safe, %.1f dB with\n", dOff, dSafe);
        check (dSafe < dOff - 20.0, "auto safe width removes the side level of an out-of-phase band");
        auto [lm, rm] = run (p, tone, tone); check (std::abs (db (rmsOf (lm, 48000, total * 2) / rmsOf (tone, 48000, total * 2))) < 0.05, "a mono signal is not touched by auto safe width");
        ms1::Imager im; im.prepare (fs); auto l = tone, r = tone; ms1::ImagerParams ip; ip.on = true;
        blocks (l, r, [&] (float* a, float* b, int n) { im.process (a, b, n, ip); });
        check (im.scopePos.load() != 0, "the goniometer buffer is filled");
    }
    {   // multiband: the knee, the RMS detector and the sidechain high-pass
        ms1::MbParams p; p.on = true; for (auto& b : p.band) { b.thresholdDb = -30.0f; b.ratio = 4.0f; b.attackMs = 1.0f; }
        const auto tone = sineWave (0.3f, 400.0f, total * 2);
        const auto reduction = [&] (const ms1::MbParams& q, std::vector<float> in) { auto l = in, r = in; ms1::MultibandComp mb; mb.prepare (fs); blocks (l, r, [&] (float* a, float* b, int n) { mb.process (a, b, n, q); }); return db (rmsOf (in, 48000, total * 2) / rmsOf (l, 48000, total * 2)); };
        auto k0 = p; k0.kneeDb = 0.0f; auto k12 = p; k12.kneeDb = 12.0f; auto nearT = sineWave (0.03f, 400.0f, total * 2);          // -30.5 dB peak: right at the threshold
        const double r0 = reduction (k0, nearT), r12 = reduction (k12, nearT);
        std::printf ("  tone at the threshold: reduction %.2f dB with a hard knee, %.2f dB with a 12 dB knee\n", r0, r12);
        check (r12 > r0 + 0.5, "a wider knee starts compressing before the threshold");
        auto burst = sineWave (0.3f, 400.0f, total * 2); for (int i = 0; i < total * 2; ++i) if ((i % 4800) > 96) burst[(size_t) i] = 0.0f;       // 2 ms bursts every 100 ms
        auto rms = p; rms.rmsBlend = 1.0f; const double rp = reduction (p, burst), rr = reduction (rms, burst), rs = reduction (rms, tone), ps = reduction (p, tone);
        std::printf ("  bursts: reduction %.2f dB peak detector, %.2f dB RMS detector; sine: %.2f / %.2f dB\n", rp, rr, ps, rs);
        check (rr < rp - 0.5, "the RMS detector reduces less than the peak detector on short bursts");
        check (std::abs (rs - ps) < 1.0, "the RMS detector is calibrated to read like the peak detector on a sine");
        auto lowTone = sineWave (0.3f, 60.0f, total * 2); auto sc = p; sc.sidechainHz = 150.0f;
        check (reduction (sc, lowTone) < reduction (p, lowTone) - 3.0, "the sidechain high-pass makes the low band react less to a bass tone");
    }

    std::printf ("linear-phase EQ and match EQ\n");
    {
        std::array<ms1::EqBand, ms1::kEqBands> bands; for (auto& b : bands) b.on = false;
        bands[3].on = true; bands[3].freq = 1000.0f; bands[3].gainDb = 6.0f; bands[3].q = 1.0f;
        const int n = ms1::linearIrLength (fs);
        const auto ir = ms1::designLinearIr (bands, fs, n);
        check (n == 4096 && ms1::linearIrLength (96000.0f) == 8192 && ms1::linearIrLength (192000.0f) == 16384, "the FIR length is 4096 / 8192 / 16384 by sample rate");
        double symErr = 0, peak = 0; size_t pk = 0; for (int i = 1; i < n; ++i) { symErr = std::max (symErr, (double) std::abs (ir.a[(size_t) i] - ir.a[(size_t) (n - i)])); if (std::abs (ir.a[(size_t) i]) > peak) { peak = std::abs (ir.a[(size_t) i]); pk = (size_t) i; } }
        check (symErr < 1.0e-6 && pk == (size_t) n / 2, "the FIR is symmetric (linear phase) with its centre at N / 2");
        const auto magAt = [&] (const std::vector<float>& h, float hz) { double re = 0, im = 0; for (int i = 0; i < n; ++i) { const double ph = 6.283185307 * hz * i / fs; re += h[(size_t) i] * std::cos (ph); im -= h[(size_t) i] * std::sin (ph); } return db (std::hypot (re, im)); };
        std::printf ("  FIR response: %.2f dB at 1 kHz, %.2f dB at 100 Hz, %.2f dB at 10 kHz\n", magAt (ir.a, 1000.0f), magAt (ir.a, 100.0f), magAt (ir.a, 10000.0f));
        check (std::abs (magAt (ir.a, 1000.0f) - 6.0) < 0.3 && std::abs (magAt (ir.a, 100.0f) - ms1::eqResponseDb (3, bands[3], 100.0f, fs)) < 0.3, "the FIR has the magnitude of the bands (1 kHz bell +6 dB, and its skirt at 100 Hz)");
        check (! ir.midSide && std::abs (magAt (ir.b, 1000.0f) - 6.0) < 0.3, "Stereo bands act on both channels");
        bands[3].mode = ms1::EqLeft; const auto irL = ms1::designLinearIr (bands, fs, n);
        check (std::abs (magAt (irL.a, 1000.0f) - 6.0) < 0.3 && std::abs (magAt (irL.b, 1000.0f)) < 0.2, "a Left band acts on the left channel only");
        bands[3].mode = ms1::EqSide; const auto irS = ms1::designLinearIr (bands, fs, n);
        check (irS.midSide && std::abs (magAt (irS.a, 1000.0f)) < 0.2 && std::abs (magAt (irS.b, 1000.0f) - 6.0) < 0.3, "a Side band acts on the side channel of the mid / side pair");
        for (auto& b : bands) b.on = false; const auto flat = ms1::designLinearIr (bands, fs, n);
        double e = 0; for (int i = 0; i < n; ++i) e = std::max (e, (double) std::abs (flat.a[(size_t) i] - (i == n / 2 ? 1.0f : 0.0f)));
        check (e < 1.0e-6, "a flat EQ is a pure delay of N / 2 samples");

        // match EQ: the reference has +3 dB above 2 kHz, plus an overall level difference
        std::array<float, ms1::kBands> in {}, ref {};
        for (int b = 0; b < ms1::kBands; ++b) { const float f = 20.0f * std::pow (1000.0f, ((float) b + 0.5f) / (float) ms1::kBands); in[(size_t) b] = -40.0f - 4.5f * std::log2 (f / 1000.0f); ref[(size_t) b] = in[(size_t) b] + 7.0f + (f > 2000.0f ? 3.0f : 0.0f); }
        const auto fit = ms1::matchEqFit (ref.data(), in.data());
        const auto total = [&] (float hz) { float t = 0; for (int i = 0; i < ms1::kEqBands; ++i) t += ms1::eqResponseDb (i, fit[(size_t) i], hz, fs); return t; };
        std::printf ("  match EQ: %+.2f dB at 200 Hz, %+.2f dB at 1 kHz, %+.2f dB at 6 kHz, %+.2f dB at 12 kHz\n", total (200.0f), total (1000.0f), total (6000.0f), total (12000.0f));
        float maxGain = 0; for (const auto& b : fit) maxGain = std::max (maxGain, std::abs (b.gainDb));
        check (maxGain <= 4.0f + 1.0e-4f, "no band exceeds +-4 dB");
        check (total (6000.0f) - total (200.0f) > 1.5f && total (6000.0f) - total (200.0f) < 4.5f, "the fit follows the tilt of the difference (the level difference is removed)");
        std::array<float, ms1::kBands> same = in; for (auto& v : same) v += 12.0f;
        const auto flatFit = ms1::matchEqFit (same.data(), in.data());
        bool allZero = true; for (const auto& b : flatFit) if (b.gainDb != 0.0f) allZero = false;
        check (allZero, "a reference that differs only by level gives a flat EQ");
    }

    std::printf ("limiter auto release, linear-phase crossover, measured width\n");
    {
        const float freq[3] = { 120.0f, 1200.0f, 6000.0f };
        const int n = ms1::linearIrLength (fs);
        const auto bandsIr = ms1::designLinearBands (freq, fs, n);
        double sumErr = 0.0; for (int i = 0; i < n; ++i) { double sum = 0.0; for (const auto& b : bandsIr) sum += b[(size_t) i]; sumErr = std::max (sumErr, std::abs (sum - (i == n / 2 ? 1.0 : 0.0))); }
        check (sumErr < 2.0e-4, "the four linear-phase band FIRs add up to a pure delay of N / 2 samples");
        const auto magAt = [&] (const std::vector<float>& h, float hz) { double re = 0, im = 0; for (int i = 0; i < n; ++i) { const double ph = 6.283185307 * hz * i / fs; re += h[(size_t) i] * std::cos (ph); im -= h[(size_t) i] * std::sin (ph); } return db (std::hypot (re, im)); };
        std::printf ("  band 2 (120 .. 1200 Hz): %.2f dB at 400 Hz, %.2f dB at 120 Hz (the crossover: -6 dB), %.2f dB at 50 Hz, %.2f dB at 5 kHz\n", magAt (bandsIr[1], 400.0f), magAt (bandsIr[1], 120.0f), magAt (bandsIr[1], 50.0f), magAt (bandsIr[1], 5000.0f));
        check (std::abs (magAt (bandsIr[1], 400.0f)) < 1.5 && std::abs (magAt (bandsIr[1], 120.0f) + 6.0) < 1.0 && magAt (bandsIr[1], 50.0f) < -15.0 && magAt (bandsIr[1], 5000.0f) < -30.0, "band 2 has the Linkwitz-Riley shape: -6 dB at its crossover, steep outside");
        // the same multiband engine on pre-split bands: idle compressors give back the sum of the bands, a compressed band reduces only its own part
        const int total = 24000; std::vector<std::vector<float>> bl (4, std::vector<float> ((size_t) total, 0.0f)), br = bl;
        for (int i = 0; i < total; ++i) { bl[1][(size_t) i] = br[1][(size_t) i] = 0.3f * std::sin (6.283185307f * 400.0f * (float) i / fs); bl[3][(size_t) i] = br[3][(size_t) i] = 0.1f * std::sin (6.283185307f * 9000.0f * (float) i / fs); }
        std::vector<float> l ((size_t) total), r ((size_t) total); const float* bpl[4] = { bl[0].data(), bl[1].data(), bl[2].data(), bl[3].data() }, * bpr[4] = { br[0].data(), br[1].data(), br[2].data(), br[3].data() };
        ms1::MbParams p; p.on = true; for (auto& b : p.band) { b.thresholdDb = 0.0f; b.ratio = 1.0f; }
        { ms1::MultibandComp mb; mb.prepare (fs); mb.processBands (l.data(), r.data(), total, p, bpl, bpr); double e = 0; for (int i = 12000; i < total; ++i) e = std::max (e, (double) std::abs (l[(size_t) i] - bl[1][(size_t) i] - bl[3][(size_t) i]));
          check (e < 1.0e-5, "pre-split bands with idle compressors add up to their sum"); }
        { auto q = p; q.band[1].thresholdDb = -30.0f; q.band[1].ratio = 4.0f; q.band[1].attackMs = 1.0f; ms1::MultibandComp mb; mb.prepare (fs); mb.processBands (l.data(), r.data(), total, q, bpl, bpr);
          check (mb.gr[1].load() < -8.0f && mb.gr[3].load() > -0.5f, "pre-split bands: only the loud band is compressed"); }
        { auto q = p; q.on = false; ms1::MultibandComp mb; mb.prepare (fs); mb.processBands (l.data(), r.data(), total, q, bpl, bpr); double e = 0; for (int i = 0; i < total; ++i) e = std::max (e, (double) std::abs (l[(size_t) i] - bl[1][(size_t) i] - bl[3][(size_t) i]));
          check (e < 1.0e-6, "stage off in linear phase: the sum of the bands (the delayed input)"); }

        // limiter: AUTO release recovers faster after a short peak than after a long one
        const auto recovery = [&] (float loudSeconds)
        {
            ms1::Limiter lim; lim.prepare (fs); ms1::LimiterParams lp; lp.gainDb = 12.0f; lp.autoRelease = true; lp.releaseMs = 100.0f;
            const int tot = (int) (fs * (loudSeconds + 2.0f)); std::vector<float> a ((size_t) tot), b;
            for (int i = 0; i < tot; ++i) a[(size_t) i] = (i < (int) (fs * loudSeconds) ? 0.9f : 0.03f) * std::sin (6.283185307f * 220.0f * (float) i / fs);
            b = a; int back = -1; const int start = (int) (fs * loudSeconds);
            for (int pos = 0; pos < tot; pos += 128) { const int m = std::min (128, tot - pos); lim.process (a.data() + pos, b.data() + pos, m, lp); if (pos >= start && back < 0 && lim.grDb.load() > -0.5f) back = pos - start; }
            return back < 0 ? 99999 : back;
        };
        const int shortPeak = recovery (0.05f), longPeak = recovery (1.5f);
        std::printf ("  limiter AUTO release: recovered %d samples after a 50 ms burst, %d after 1.5 s of loud sound\n", shortPeak, longPeak);
        check (shortPeak < longPeak, "AUTO release recovers sooner after a short peak than after sustained loudness");
        ms1::Limiter lim2; lim2.prepare (fs); ms1::LimiterParams lp2; lp2.gainDb = 12.0f; lp2.autoRelease = true; std::vector<float> a2 ((size_t) 96000), b2; std::mt19937 rng (4); std::uniform_real_distribution<float> u (-1.0f, 1.0f);
        for (auto& v : a2) v = 0.8f * u (rng); b2 = a2; float peak = 0; for (int pos = 0; pos < 96000; pos += 256) { lim2.process (a2.data() + pos, b2.data() + pos, 256, lp2); }
        for (int i = 8000; i < 96000; ++i) peak = std::max (peak, std::abs (a2[(size_t) i]));
        check (peak <= std::pow (10.0f, -1.0f / 20.0f) + 1.0e-4f, "AUTO release holds the ceiling on loud noise");

        // measured side / mid per band: a pure side tone reads high, a mono tone reads low
        ms1::ImagerParams ip; ip.on = true; std::vector<float> tone = sineWave (0.3f, 1000.0f, 48000), neg (tone.size()); for (size_t i = 0; i < tone.size(); ++i) neg[i] = -tone[i];
        { ms1::Imager im; im.prepare (fs); auto a = tone, b = neg; blocks (a, b, [&] (float* x, float* y, int m) { im.process (x, y, m, ip); }); std::printf ("  measured side / mid in the mid band: %.1f dB for a side tone\n", (double) im.sideMidDb[1].load()); check (im.sideMidDb[1].load() > 20.0f, "the measured side level of a pure side tone is far above its mid level"); }
        { ms1::Imager im; im.prepare (fs); auto a = tone, b = tone; blocks (a, b, [&] (float* x, float* y, int m) { im.process (x, y, m, ip); }); check (im.sideMidDb[1].load() < -40.0f, "the measured side level of a mono tone is far below its mid level"); }
    }

    std::printf ("%d problems\n", problems);
    return problems == 0 ? 0 : 1;
}
