// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
// Offline check of the DL-1 engine: DIGITAL equals the SD-1 and ANALOG the MM-1 (their processing loops are repeated here with the same numbers and the
// outputs compared, below -60 dB difference); a click returns after exactly TIME; the repeats fall by FEEDBACK; ping-pong alternates the channels; HOLD keeps the
// loop; the low cut and the hi cut act in the loop; DUCKED ducks; mix 0 is bit-exact; every setting stays finite; a mode change does not click; sync follows the tempo.
#include <cstdio>
#include <random>
#include <vector>
#include "DelayEngine.h"

namespace
{
int problems = 0;
void check (bool ok, const char* what) { std::printf ("  %-90s %s\n", what, ok ? "ok" : "FAILED"); if (! ok) ++problems; }
const float fs = 48000.0f;

std::vector<float> noise (int n, float amp, unsigned seed = 1)
{
    std::mt19937 rng (seed); std::uniform_real_distribution<float> u (-1.0f, 1.0f);
    std::vector<float> v ((size_t) n); for (auto& x : v) x = amp * u (rng);
    return v;
}
std::vector<float> run (const dl1::Params& p, const std::vector<float>& inL, const std::vector<float>& inR, std::vector<float>* outR = nullptr, float level = 1.0f)
{
    dl1::Engine e; e.prepare (fs);
    auto l = inL, r = inR;
    for (size_t pos = 0; pos < l.size(); pos += 512) { const int n = (int) std::min<size_t> (512, l.size() - pos); e.process (l.data() + pos, r.data() + pos, n, p, level); }
    if (outR) *outR = r;
    return l;
}
double rms (const std::vector<float>& v, int a, int b) { double s = 0; for (int i = a; i < b; ++i) s += (double) v[(size_t) i] * v[(size_t) i]; return std::sqrt (s / (b - a)); }
double diffDb (const std::vector<float>& a, const std::vector<float>& b, int from)
{
    double e = 0, r = 0; for (size_t i = (size_t) from; i < a.size(); ++i) { const double d = (double) a[i] - b[i]; e += d * d; r += (double) b[i] * b[i]; }
    return 10.0 * std::log10 (std::max (1e-30, e / std::max (1e-30, r)));
}

// ---- the delay line of the SD-1 and MM-1, verbatim (the position is added in a float)
class OriginalLine
{
public:
    void prepare (double sampleRate, double seconds) { int n = 1; while (n < (int) (sampleRate * seconds)) n <<= 1; buffer.assign ((size_t) n, 0.0f); mask = n - 1; writePos = 0; maxDelay = (float) (n - 8); }
    void write (float x) { buffer[(size_t) writePos] = x; writePos = (writePos + 1) & mask; }
    float getMaxDelay() const { return maxDelay; }
    float read (float delaySamples) const
    {
        const float pos = (float) writePos - 1.0f - delaySamples, fl = std::floor (pos), f = pos - fl;
        const int i1 = (int) fl;
        const float y0 = buffer[(size_t) ((i1 - 1) & mask)], y1 = buffer[(size_t) (i1 & mask)], y2 = buffer[(size_t) ((i1 + 1) & mask)], y3 = buffer[(size_t) ((i1 + 2) & mask)];
        const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }
private:
    std::vector<float> buffer; int mask = 0, writePos = 0; float maxDelay = 0.0f;
};

// ---- the SD-1 loop (src/effects/sd1/PluginProcessor.cpp) with constant parameters, one channel at a time
std::vector<float> sd1Reference (const std::vector<float>& in, int channel, float delayMs, float fb, float gIn, float dry, float eff, float modI, float rateHz, float cutHz)
{
    OriginalLine line; line.prepare (fs, 2.4);
    std::vector<float> out (in.size());
    float cut1 = 0, cut2 = 0, phase = 0;
    const float cutCoef = 1.0f - std::exp (-dl1::kTwoPi * std::min (cutHz, 0.45f * fs) / fs);
    for (size_t n = 0; n < in.size(); ++n)
    {
        phase += dl1::kTwoPi * rateHz / fs; if (phase > dl1::kTwoPi) phase -= dl1::kTwoPi;
        const float depthMs = modI * std::min (6.0f, 0.25f * delayMs);
        const float raw = in[n], x = std::tanh (raw * gIn);
        const float lfo = std::sin (phase + (float) channel * 0.5f * dl1::kPi);
        const float d = std::clamp ((delayMs + depthMs * lfo) * 0.001f * fs, 2.0f, line.getMaxDelay());
        const float wet = line.read (d);
        cut1 += cutCoef * (wet - cut1); cut2 += cutCoef * (cut1 - cut2);
        line.write (x + std::tanh (cut2 * fb));
        out[n] = x * dry + wet * eff;
    }
    return out;
}
// ---- the MM-1 loop (src/effects/mm1/PluginProcessor.cpp)
std::vector<float> mm1Reference (const std::vector<float>& in, int channel, float dMs, float fb, float dry, float wet, float gOut, float depth, float rateHz)
{
    OriginalLine line; line.prepare (fs, 0.9);
    std::vector<float> out (in.size());
    float lp1 = 0, lp2 = 0, lp3 = 0, phase = 0;
    const float cutHz = std::clamp (14000.0f * std::sqrt (45.0f / dMs), 2200.0f, 9000.0f);
    const float coef = 1.0f - std::exp (-dl1::kTwoPi * std::min (cutHz, 0.45f * fs) / fs);
    for (size_t n = 0; n < in.size(); ++n)
    {
        phase += dl1::kTwoPi * rateHz / fs; if (phase > dl1::kTwoPi) phase -= dl1::kTwoPi;
        const float depthMs = depth * depth * std::min (9.0f, 0.30f * dMs);
        const float raw = in[n], x = std::tanh (raw * 1.3f) / 1.3f;
        const float lfo = std::sin (phase + (float) channel * 0.5f * dl1::kPi);
        const float d = std::clamp ((dMs + depthMs * lfo) * 0.001f * fs, 2.0f, line.getMaxDelay());
        const float echo = line.read (d);
        lp1 += coef * (echo - lp1); lp2 += coef * (lp1 - lp2); lp3 += coef * (lp2 - lp3);
        line.write (x + std::tanh (lp3 * fb));
        out[n] = (x * dry + lp3 * wet) * gOut;
    }
    return out;
}
} // namespace

int main()
{
    const int total = (int) (fs * 3.0f);

    std::printf ("parity with the SD-1 and the MM-1\n");
    {
        // SD-1 at its defaults: time knob 4 -> 416 ms x factor 1, regeneration 6.5 -> +0.3 * 1.02, hi-cut 3 (8139 Hz), direct 7, effect 5, mod 0
        const float timeMs = 256.0f * std::pow (1024.0f / 256.0f, (4.0f - 3.0f) / 3.0f);
        const float fb = (6.5f - 5.0f) / 5.0f * 1.02f, dry = std::pow (0.7f, 1.6f) * 1.1f, eff = std::pow (0.5f, 1.4f) * 1.3f, cut = 20000.0f * std::pow (0.05f, 0.3f);
        auto in = noise (total, 0.4f);
        dl1::Params p; p.mode = dl1::Digital; p.timeMs = timeMs; p.feedback = 0.3f; p.highHz = cut; p.modDepth = 0.0f; p.digDirect = 0.7f; p.digEffect = 0.5f; p.digInput = 0.5f; p.digFactor = 1.0f; p.modRate = 1.0f;
        p.feedback = 0.3f;                                                                                   // x 1.02 inside = (6.5 - 5) / 5 * 1.02
        const auto out = run (p, in, in);
        const auto ref = sd1Reference (in, 0, timeMs, fb, 1.0f, dry, eff, 0.0f, 1.0f, cut);
        std::printf ("  DIGITAL against the SD-1 at its defaults: %.1f dB\n", diffDb (out, ref, 24000));
        check (diffDb (out, ref, 24000) < -50.0, "DIGITAL equals the SD-1 (defaults; the finer delay read is the only difference)");
        // with modulation and a different time factor: time 2.5 -> factor 0.5 (x0.5), mod intensity 0.6 at 3 Hz
        dl1::Params q = p; q.timeMs = 600.0f; q.digFactor = 0.5f; q.modDepth = 0.6f; q.modRate = 3.0f; q.feedback = 0.5f; q.highHz = 5000.0f;
        const float ms = 600.0f * 0.25f * std::pow (4.0f, 0.5f);
        const auto out2 = run (q, in, in), ref2 = sd1Reference (in, 0, ms, 0.5f * 1.02f, 1.0f, dry, eff, 0.6f, 3.0f, 5000.0f);
        std::printf ("  DIGITAL with modulation and TIME FACTOR 0.5: %.1f dB\n", diffDb (out2, ref2, 24000));
        check (diffDb (out2, ref2, 24000) < -45.0, "DIGITAL equals the SD-1 (modulated, factor 0.5)");
    }
    {
        // MM-1 at its defaults: delay knob 5 (40 ms .. 550 ms exponential), feedback 4, blend 5, level 5 (0.3536 x 2.2), depth 2, rate 3 (0.15 .. 9 Hz)
        const float dMs = 40.0f * std::pow (550.0f / 40.0f, 0.5f), fb = 0.4f * 1.03f, blend = 0.5f, dry = std::cos (blend * 0.5f * dl1::kPi), wet = std::sin (blend * 0.5f * dl1::kPi) * 1.25f;
        const float gOut = std::pow (0.5f, 1.5f) * 2.2f, depth = 0.2f, rate = 0.15f * std::pow (60.0f, 0.3f);
        auto in = noise (total, 0.4f, 2);
        dl1::Params p; p.mode = dl1::Analog; p.variant = 0; p.timeMs = dMs; p.feedback = 0.4f; p.highHz = 20000.0f; p.anaBlend = blend; p.anaDepth = depth; p.modRate = rate;
        const auto out = run (p, in, in), ref = mm1Reference (in, 0, dMs, fb, dry, wet, gOut, depth, rate);
        std::printf ("  ANALOG against the MM-1 at its defaults: %.1f dB\n", diffDb (out, ref, 24000));
        check (diffDb (out, ref, 24000) < -50.0, "ANALOG equals the MM-1 (defaults; the finer delay read is the only difference)");
        dl1::Params q = p; q.timeMs = 120.0f; q.feedback = 0.7f; q.anaBlend = 0.8f; q.anaDepth = 0.7f; q.modRate = 4.0f;
        const float d2 = 120.0f, b2 = 0.8f;
        const auto out2 = run (q, in, in), ref2 = mm1Reference (in, 0, d2, 0.7f * 1.03f, std::cos (b2 * 0.5f * dl1::kPi), std::sin (b2 * 0.5f * dl1::kPi) * 1.25f, gOut, 0.7f, 4.0f);
        std::printf ("  ANALOG with other settings: %.1f dB\n", diffDb (out2, ref2, 24000));
        check (diffDb (out2, ref2, 24000) < -45.0, "ANALOG equals the MM-1 (120 ms, deep modulation)");
        // the right channel has the LFO a quarter turn on (the same offset as in the originals)
        std::vector<float> r2; run (p, in, in, &r2); const auto refR = mm1Reference (in, 1, dMs, fb, dry, wet, gOut, depth, rate);
        check (diffDb (r2, refR, 24000) < -50.0, "ANALOG right channel equals the MM-1");
    }

    std::printf ("time, feedback, ping-pong, hold, filters, ducking\n");
    {
        dl1::Params p; p.mode = dl1::Digital; p.timeMs = 250.0f; p.feedback = 0.5f; p.modDepth = 0.0f; p.digDirect = 0.0f; p.digEffect = 0.769f; p.highHz = 20000.0f; p.digFactor = 1.0f;
        std::vector<float> click ((size_t) fs * 3), zero (click.size(), 0.0f); click[1000] = 0.05f;               // small: the soft clip is linear here
        const auto o = run (p, click, click);
        const int expect = 1000 + (int) std::lround (250.0f * 0.001f * fs);
        size_t pk = 0; for (size_t i = 2000; i < o.size(); ++i) if (std::abs (o[i]) > std::abs (o[pk]) || pk == 0) pk = i;
        std::printf ("  first repeat at %zu, expected %d\n", pk, expect);
        check (std::abs ((int) pk - expect) <= 1, "a click returns after exactly TIME (250 ms)");
        const auto win = [&] (size_t at) { float m = 0; for (size_t i = at - 6; i <= at + 6; ++i) m = std::max (m, std::abs (o[i])); return (double) m; };
        const double first = win ((size_t) expect), second = win ((size_t) expect + (size_t) (0.25f * fs));
        std::printf ("  repeats: %.4f, %.4f (ratio %.2f, FEEDBACK 0.5)\n", first, second, second / first);
        check (std::abs (second / first - 0.5 * 1.02) < 0.12, "the second repeat is about FEEDBACK times the first (0.5)");
        // the sync period: 1/4 at 120 bpm = 500 ms; at 60 bpm 1000 ms
        dl1::Params s = p; s.sync = true; s.division = 2; s.bpm = 120.0f; const auto o2 = run (s, click, click);
        size_t pk2 = 0; for (size_t i = 2000; i < o2.size(); ++i) if (std::abs (o2[i]) > std::abs (o2[pk2]) || pk2 == 0) pk2 = i;
        check (std::abs ((int) pk2 - (1000 + (int) (0.5f * fs))) <= 1, "SYNC: 1/4 at 120 bpm is 500 ms");
        s.bpm = 60.0f; const auto o3 = run (s, click, click); size_t pk3 = 0; for (size_t i = 2000; i < o3.size(); ++i) if (std::abs (o3[i]) > std::abs (o3[pk3]) || pk3 == 0) pk3 = i;
        check (std::abs ((int) pk3 - (1000 + (int) (1.0f * fs))) <= 1, "SYNC: 1/4 at 60 bpm is 1000 ms");
        // ping-pong: the left channel gets the input, the right channel only the repeats; the repeats alternate
        dl1::Params pp = p; pp.spread = 1.0f; pp.digDirect = 0.0f; std::vector<float> r; std::vector<float> clickL = click, clickR = zero; clickL[1000] = 0.05f;
        const auto l = run (pp, clickL, clickR, &r);
        const size_t t1 = (size_t) expect, t2 = (size_t) expect + (size_t) (0.25f * fs);
        const auto peakAt = [] (const std::vector<float>& v, size_t at) { float m = 0; for (size_t i = at - 3; i <= at + 3; ++i) m = std::max (m, std::abs (v[i])); return m; };      // the interpolated read lands within a sample
        std::printf ("  ping-pong: L %.4f / R %.4f at the first repeat, L %.4f / R %.4f at the second\n", peakAt (l, t1), peakAt (r, t1), peakAt (l, t2), peakAt (r, t2));
        check (peakAt (l, t1) > 10.0 * peakAt (r, t1) && peakAt (r, t2) > 10.0 * peakAt (l, t2), "SPREAD 100 %: the repeats alternate between the channels (L, R, L ...)");
        // hold: the loop level stays (feedback 100 %, input muted)
        dl1::Params h = p; h.hold = true; h.timeMs = 100.0f; h.digDirect = 0.0f; h.highHz = 20000.0f;
        auto burst = noise ((int) fs * 12, 0.0f); for (int i = 0; i < 4800; ++i) burst[(size_t) i] = 0.3f * std::sin (6.2831853f * 440.0f * (float) i / fs);
        // the hold starts after the burst: the engine is run with hold off for the burst, then on
        dl1::Engine e; e.prepare (fs); auto bl = burst, br = burst; dl1::Params off = h; off.hold = false; off.feedback = 0.5f;
        e.process (bl.data(), br.data(), 4800, off, 1.0f);
        std::vector<float> rest (4800 * 100, 0.0f), rest2 = rest; double early = 0, late = 0;
        for (int blk = 0; blk < 100; ++blk) { e.process (rest.data() + blk * 4800, rest2.data() + blk * 4800, 4800, h, 1.0f); }
        for (int i = 4800 * 3; i < 4800 * 8; ++i) early += (double) rest[(size_t) i] * rest[(size_t) i];
        for (int i = 4800 * 90; i < 4800 * 95; ++i) late += (double) rest[(size_t) i] * rest[(size_t) i];
        std::printf ("  HOLD: the loop level changes by %.2f dB over 4 seconds\n", 10.0 * std::log10 (late / std::max (1e-20, early)));
        check (std::abs (10.0 * std::log10 (late / std::max (1e-20, early))) < 1.5, "HOLD keeps the loop level (within 1.5 dB over 4 s)");
        // hi cut and low cut act in the loop: a 50 ms burst, then the energy of the repeats after the first (0.25 s .. 1.5 s) with and without the filter
        const auto repeats = [&] (const dl1::Params& q, const std::vector<float>& burst) { const auto o = run (q, burst, burst); return rms (o, (int) (0.25f * fs), (int) (1.5f * fs)); };
        dl1::Params base = p; base.timeMs = 100.0f; base.feedback = 0.8f; base.digDirect = 0.0f; base.digEffect = 0.0f; base.digInput = 0.5f;
        base.digEffect = 0.0f;                                                                              // only what comes back through the loop is heard ...
        dl1::Params loud = base; loud.digEffect = 1.0f;                                                       // ... at the output level of the effect
        std::vector<float> hfBurst ((size_t) fs * 2, 0.0f), lfBurst = hfBurst; std::mt19937 rr (7); std::uniform_real_distribution<float> uu (-1.0f, 1.0f);
        for (int i = 2; i < 2400; ++i) { hfBurst[(size_t) i] = 0.3f * (uu (rr) - 0.9f * (hfBurst[(size_t) i - 1] / 0.3f)); }                                    // mostly highs
        for (int i = 1; i < 2400; ++i) { lfBurst[(size_t) i] = 0.3f * std::sin (6.2831853f * 60.0f * (float) i / fs) * (0.5f - 0.5f * std::cos (6.2831853f * (float) i / 2400.0f)); }   // a bass burst
        dl1::Params dark = loud; dark.highHz = 700.0f; dl1::Params bright = loud;
        std::printf ("  loop energy of a bright burst: %.5f with the hi cut at 700 Hz, %.5f open\n", repeats (dark, hfBurst), repeats (bright, hfBurst));
        check (repeats (dark, hfBurst) < 0.4 * repeats (bright, hfBurst), "HI CUT 700 Hz makes the repeats of a bright burst darker (they fade much faster)");
        dl1::Params thin = loud; thin.lowHz = 800.0f;
        std::printf ("  loop energy of a bass burst: %.5f with the low cut at 800 Hz, %.5f open\n", repeats (thin, lfBurst), repeats (bright, lfBurst));
        check (repeats (thin, lfBurst) < 0.4 * repeats (bright, lfBurst), "LOW CUT 800 Hz makes the repeats of a bass burst thinner (they fade much faster)");
        // ducking
        dl1::Params du = p; du.variant = 1; du.digDirect = 0.0f; du.timeMs = 300.0f; du.feedback = 0.0f;
        std::vector<float> sustained = noise ((int) fs * 2, 0.3f, 5);
        const auto plain = run (p, sustained, sustained), ducked = run (du, sustained, sustained);
        dl1::Params p2 = p; p2.feedback = 0.0f; p2.timeMs = 300.0f; p2.digDirect = 0.0f; const auto plain2 = run (p2, sustained, sustained);
        check (rms (ducked, 24000, 90000) < 0.5 * rms (plain2, 24000, 90000), "DUCKED: the repeats duck while the input plays");
        (void) plain;
    }

    std::printf ("robustness\n");
    {
        bool ok = true;
        auto in = noise ((int) fs, 0.6f, 6);
        for (int mode = 0; mode < dl1::kNumModes; ++mode) for (int v = 0; v < dl1::variantCount (mode); ++v) for (int s = 0; s < 6; ++s)
        {
            dl1::Params p; p.mode = mode; p.variant = v;
            p.timeMs = s == 0 ? 1.0f : s == 1 ? 2000.0f : 300.0f; p.feedback = s == 2 ? 1.0f : 0.5f; p.spread = s == 3 ? 1.0f : 0.0f; p.hold = s == 4; p.modDepth = s == 5 ? 1.0f : 0.2f; p.modRate = 10.0f;
            p.lowHz = 1000.0f; p.highHz = 1000.0f; p.sync = s == 1; p.anaGrit = 1.0f; p.digInput = 1.0f; p.digEffect = 1.0f; p.digDirect = 1.0f; p.anaBlend = 1.0f; p.anaDepth = 1.0f;
            const auto o = run (p, in, in); float peak = 0; bool fin = true; for (float x : o) { if (! std::isfinite (x)) fin = false; peak = std::max (peak, std::abs (x)); }
            if (! fin || peak > 8.0f) { ok = false; std::printf ("    %s type %d setting %d: peak %.2f%s\n", dl1::modeName (mode), v, s, peak, fin ? "" : " NOT FINITE"); }
        }
        check (ok, "every mode, type and extreme setting stays finite and bounded");
        dl1::Params p; p.mix = 0.0f; p.feedback = 0.9f; const auto o = run (p, in, in);
        check (o == in, "MIX 0 passes the input bit-exact");
        std::vector<float> silent ((int) fs, 0.0f); const auto so = run (p, silent, silent); double e = 0; for (float x : so) e += (double) x * x;
        check (e == 0.0, "silence in, silence out");
        // a mode change in the middle of a sine: no step larger than a small multiple of the sine's own step
        dl1::Engine eng; eng.prepare (fs); dl1::Params a; a.mode = dl1::Digital; a.mix = 1.0f; dl1::Params b = a; b.mode = dl1::Analog;
        std::vector<float> l ((size_t) fs), r; for (size_t i = 0; i < l.size(); ++i) l[i] = 0.4f * std::sin (6.2831853f * 220.0f * (float) i / fs); r = l;
        float maxStep = 0, prev = 0;
        for (size_t pos = 0; pos < l.size(); pos += 256) { eng.process (l.data() + pos, r.data() + pos, 256, pos < l.size() / 2 ? a : b, 1.0f); for (size_t i = pos; i < pos + 256; ++i) { maxStep = std::max (maxStep, std::abs (l[i] - prev)); prev = l[i]; } }
        std::printf ("  mode change: largest step %.3f (the sine alone: %.3f)\n", (double) maxStep, 0.4 * 6.2831853 * 220.0 / fs);
        check (maxStep < 0.4f, "a mode change in the middle of a sine does not click");
    }

    // ---- TAPE and DUAL
    {
        const auto near = [] (const std::vector<float>& v, int at) { float m = 0; for (int i = std::max (0, at - 3); i <= std::min ((int) v.size() - 1, at + 3); ++i) m = std::max (m, std::abs (v[(size_t) i])); return m; };
        std::vector<float> click ((int) (2.0f * fs), 0.0f); click[1000] = 0.3f;
        dl1::Params t; t.mode = dl1::Tape; t.timeMs = 250.0f; t.feedback = 0.0f; t.modDepth = 0.0f; t.tapeWow = 0.0f; t.highHz = 20000.0f; t.lowHz = 20.0f; t.tapeSat = 0.0f; t.mix = 1.0f;
        const int tA = (int) (0.25f * fs);
        const auto one = run (t, click, click);
        check (near (one, 1000 + tA) > 0.05f && near (one, 1000 + 2 * tA) < 0.005f, "TAPE, 1 HEAD: a click returns once, after TIME");
        t.variant = 2; const auto three = run (t, click, click);
        check (near (three, 1000 + tA) > 0.05f && near (three, 1000 + 2 * tA) > 0.03f && near (three, 1000 + 3 * tA) > 0.02f, "TAPE, 3 HEADS: taps at x1, x2 and x3 of TIME (spacing 50 %)");
        t.tapeSpacing = 0.25f; const auto narrow = run (t, click, click);
        check (near (narrow, 1000 + tA + tA / 2) > 0.03f && near (narrow, 1000 + 2 * tA) > 0.02f, "TAPE: HEAD SPACING 25 % puts the heads at x1, x1.5, x2");
        // wow: a 1 kHz tone through the tape shows side bands at the wow rate; with WOW 0 it does not
        const int n = (int) (8.0f * fs); std::vector<float> tone ((size_t) n);
        for (int i = 0; i < n; ++i) tone[(size_t) i] = 0.3f * std::sin (6.2831853f * 1000.0f * (float) i / fs);
        const auto bandEnergy = [&] (const std::vector<float>& v, float hz)
        {
            double re = 0, im = 0; const int a = (int) (1.0f * fs), len = n - a;
            for (int i = 0; i < len; ++i) { const double w = 0.5 - 0.5 * std::cos (6.2831853 * i / len), ph = 6.2831853 * hz * (a + i) / fs; re += w * v[(size_t) (a + i)] * std::cos (ph); im += w * v[(size_t) (a + i)] * std::sin (ph); }
            return std::sqrt (re * re + im * im) / len;
        };
        dl1::Params w; w.mode = dl1::Tape; w.timeMs = 200.0f; w.feedback = 0.0f; w.modDepth = 0.0f; w.highHz = 20000.0f; w.mix = 1.0f; w.tapeSat = 0.0f; w.tapeWow = 1.0f;
        const auto wowy = run (w, tone, tone); w.tapeWow = 0.0f; const auto still = run (w, tone, tone);
        const double sbWow = bandEnergy (wowy, 1000.0f + 0.55f), sbStill = bandEnergy (still, 1000.0f + 0.55f);
        std::printf ("  tape side band at 0.55 Hz: WOW 100 %% %.5f, WOW 0 %.5f\n", sbWow, sbStill);
        check (sbWow > 8.0 * sbStill, "TAPE: wow puts side bands at the wow rate round a 1 kHz tone");
        // saturation and age
        dl1::Params q = w; q.tapeWow = 0.0f; q.tapeSat = 1.0f; const auto sat = run (q, tone, tone);
        check (bandEnergy (sat, 3000.0f) > 4.0 * bandEnergy (still, 3000.0f) + 1e-6, "TAPE: SATURATION adds harmonics (3 kHz)");
        std::vector<float> quiet ((size_t) n, 0.0f); q.tapeSat = 0.0f; q.tapeAge = 1.0f; const auto aged = run (q, quiet, quiet);
        check (rms (aged, (int) fs, n) > 1e-5, "TAPE: AGE adds hiss");

        dl1::Params d; d.mode = dl1::Dual; d.timeMs = 250.0f; d.dualRatio = 0.5f; d.feedback = 0.0f; d.dualFbB = 0.0f; d.modDepth = 0.0f; d.highHz = 20000.0f; d.dualHighB = 20000.0f;
        d.dualLevelA = 1.0f; d.dualLevelB = 1.0f; d.dualPingA = false; d.dualPingB = false; d.mix = 1.0f; d.spread = 0.0f;
        const int tB = (int) (0.125f * fs);
        const auto par = run (d, click, click);
        check (near (par, 1000 + tA) > 0.05f && near (par, 1000 + tB) > 0.05f && near (par, 1000 + tA + tB) < 0.005f, "DUAL PARALLEL: A returns at TIME, B at TIME x ratio");
        d.variant = 1; const auto ser = run (d, click, click);
        check (near (ser, 1000 + tA) > 0.05f && near (ser, 1000 + tA + tB) > 0.05f && near (ser, 1000 + tB) < 0.005f, "DUAL SERIES: B repeats what A returns (TIME + TIME B)");
        d.variant = 0; d.dualLevelA = 0.0f; const auto onlyB = run (d, click, click);
        check (near (onlyB, 1000 + tA) < 0.005f && near (onlyB, 1000 + tB) > 0.05f, "DUAL: LEVEL A 0 silences A");
        d.dualLevelA = 1.0f; d.dualPingB = true; d.dualLevelB = 1.0f; d.dualFbB = 0.6f; std::vector<float> clickL = click, clickR ((click.size()), 0.0f);
        std::vector<float> oR; const auto oL = run (d, clickL, clickR, &oR);
        check (near (oL, 1000 + tB) > 0.05f && near (oR, 1000 + 2 * tB) > 0.01f && near (oL, 1000 + 3 * tB) > 0.003f, "DUAL: PING B alternates the repeats of B between the channels");
        d.dualPingB = false; d.dualFbB = 0.5f; d.dualLevelA = 0.0f; const auto fbB = run (d, click, click);
        check (std::abs (near (fbB, 1000 + 2 * tB) / std::max (1e-6f, near (fbB, 1000 + tB)) - 0.5 * 1.03) < 0.12, "DUAL: FEEDBACK B sets the repeats of B");
    }

    // ---- MODULATED (the DM-1 engine inside)
    {
        std::vector<float> click ((int) (2.0f * fs), 0.0f); click[1000] = 0.3f;
        dl1::Params m; m.mode = dl1::Modulated; m.timeMs = 250.0f; m.feedback = 0.0f; m.modDepth = 0.0f; m.mix = 1.0f; m.modVolume = 1.0f; m.modVoice = 0;
        const auto o = run (m, click, click);
        float pk = 0; int at = 0; for (int i = 2000; i < (int) o.size(); ++i) if (std::abs (o[(size_t) i]) > pk) { pk = std::abs (o[(size_t) i]); at = i; }
        std::printf ("  MODULATED classic: first repeat %.3f at %d (expected %d)\n", (double) pk, at - 1000, (int) (0.25f * fs));
        check (pk > 0.05f && std::abs (at - 1000 - (int) (0.25f * fs)) <= 3, "MODULATED, CLASSIC: a click returns after TIME");
        bool ok = true;
        const auto in = noise ((int) fs, 0.5f, 7);
        for (int v = 0; v < dl1::kNumVoices; ++v) for (int s = 0; s < 3; ++s)
        {
            dl1::Params q = m; q.modVoice = v; q.feedback = s == 1 ? 1.0f : 0.5f; q.modDepth = 1.0f; q.modRate = 10.0f; q.modVariation = s == 2 ? 1.0f : 0.0f; q.timeMs = s == 2 ? 20.0f : 300.0f; q.hold = s == 2;
            const auto r = run (q, in, in); float peak = 0; bool fin = true; for (float x : r) { if (! std::isfinite (x)) fin = false; peak = std::max (peak, std::abs (x)); }
            if (! fin || peak > 8.0f) { ok = false; std::printf ("    voicing %s setting %d: peak %.2f%s\n", dl1::voiceName (v), s, peak, fin ? "" : " NOT FINITE"); }
        }
        check (ok, "MODULATED: all 12 voicings stay finite and bounded at extreme settings");
        m.mix = 0.0f; const auto z = run (m, in, in);
        check (z == in, "MODULATED: MIX 0 passes the input bit-exact");
    }

    std::printf ("%d problems\n", problems);
    return problems == 0 ? 0 : 1;
}
