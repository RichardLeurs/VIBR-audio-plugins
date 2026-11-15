// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
// Offline check of the AL-1 engine (exit code 0 = pass): silence in, silence out for every type; finite output at extreme settings; the same latency for all 14 types;
// the knobs of a block that is off change nothing (bit exact); the gate closes on a decaying note; the limiter ceiling; aliasing of a 6 kHz tone at ULTRA full gain;
// a smooth filter model; the loudness of the 14 types at gain 5 / master 5; no clicks while the parameters are changed at random.
#include <cstdio>
#include <random>
#include <vector>
#include "AL1Engine.h"

namespace
{
int problems = 0;
void check (bool ok, const char* what) { std::printf ("  %-100s %s\n", what, ok ? "ok" : "FAILED"); if (! ok) ++problems; }
const double fs = 44100.0;

// a guitar-like test signal: a few harmonics of 110 Hz
std::vector<float> chord (int n, float amp, double f0 = 110.0)
{
    std::vector<float> v ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        double s = 0; for (int h = 1; h <= 6; ++h) s += std::sin (6.283185307 * f0 * h * i / fs) / (double) h;
        v[(size_t) i] = amp * (float) s * 0.5f;
    }
    return v;
}
std::vector<float> noise (int n, float amp, unsigned seed)
{
    std::mt19937 rng (seed); std::uniform_real_distribution<float> u (-1.0f, 1.0f);
    std::vector<float> v ((size_t) n); for (auto& x : v) x = amp * u (rng);
    return v;
}
// runs the engine over `in` (stereo, the same on both channels) in blocks of 512; returns the left channel
std::vector<float> run (const al1::Params& p, const std::vector<float>& in, int block = 512)
{
    al1::Engine e; e.prepare (fs, block);
    auto l = in, r = in;
    for (size_t pos = 0; pos < l.size(); pos += (size_t) block)
    {
        const int n = (int) std::min<size_t> ((size_t) block, l.size() - pos);
        float* ch[2] = { l.data() + pos, r.data() + pos };
        e.process (ch, 2, n, p);
    }
    return l;
}
double rms (const std::vector<float>& v, int a, int b) { double s = 0; for (int i = a; i < b; ++i) s += (double) v[(size_t) i] * v[(size_t) i]; return std::sqrt (s / (b - a)); }
double db (double x) { return 20.0 * std::log10 (std::max (x, 1e-12)); }
bool allFinite (const std::vector<float>& v) { for (float x : v) if (! std::isfinite (x)) return false; return true; }
}

int main()
{
    std::setvbuf (stdout, nullptr, _IONBF, 0);
    juce::ScopedJuceInitialiser_GUI juceInit;
    std::printf ("AL-1 engine check\n");

    {
        bool ok = true; const std::vector<float> z ((size_t) 20000, 0.0f);
        for (int t = 0; t < al1::kNumTypes; ++t)
        {
            al1::Params p; p.type = t; p.gain = 10.0f; p.boostOn = true; p.eqOn = true; p.ambience = 5.0f; p.deep = true; p.bright = true;
            const auto o = run (p, z); float peak = 0; for (float x : o) peak = std::max (peak, std::abs (x));
            if (peak > 1.0e-6f) { ok = false; std::printf ("    %s: %.3g\n", al1::kTypes[t].name, (double) peak); }
        }
        check (ok, "silence in -> silence out for every type");
    }
    {
        bool ok = true; const auto x = noise (44100, 0.9f, 5);
        for (int t = 0; t < al1::kNumTypes; ++t) for (int s = 0; s < 3; ++s)
        {
            al1::Params p; p.type = t; p.gain = 10.0f; p.master = 10.0f; p.inGainDb = 18.0f; p.boostOn = true; p.boostDrive = 10.0f; p.eqOn = true;
            p.ambience = 10.0f; p.sag = s == 0 ? 0.0f : 10.0f; p.presence = s == 1 ? 10.0f : 0.0f; p.resonance = s == 1 ? 10.0f : 0.0f; p.bass = p.middle = p.treble = s == 1 ? 10.0f : 0.0f;
            p.eqLow = p.eqMid = p.eqHigh = s == 1 ? 10.0f : 0.0f; p.limiter = s != 2; p.out = 10.0f; p.gateOn = s != 1;
            const auto o = run (p, x); if (! allFinite (o)) { ok = false; std::printf ("    %s setting %d not finite\n", al1::kTypes[t].name, s); }
        }
        check (ok, "no NaN or inf at extreme settings (gain 10, master 10, input +18 dB, all blocks on)");
    }
    {
        bool ok = true; int lat0 = -1;
        for (int t = 0; t < al1::kNumTypes; ++t)
        {
            al1::Engine e; e.prepare (fs, 512);
            al1::Params p; p.type = t; std::vector<float> a ((size_t) 1024, 0.0f), b = a; float* ch[2] = { a.data(), b.data() }; e.process (ch, 2, 512, p);
            if (lat0 < 0) lat0 = e.latency(); else if (e.latency() != lat0) ok = false;
        }
        std::printf ("  latency %d samples\n", lat0);
        check (ok && lat0 > 0, "latency is the same for all 14 types");
        // the delay seen at the output is the same in every type: a clean low sine comes out with the same phase (CLEAN vs the other 1-stage types, cab and cuts out)
        bool same = true; double ref = 0;
        for (int t = 0; t < 4; ++t)
        {
            al1::Params p; p.type = t; p.cabOn = false; p.gateOn = false; p.gain = 0.0f; p.bass = p.middle = p.treble = 5.0f; p.master = 5.0f; p.limiter = false;
            std::vector<float> in ((size_t) 8000, 0.0f); in[100] = 0.01f;
            const auto o = run (p, in); int pk = 0; for (int i = 0; i < (int) o.size(); ++i) if (std::abs (o[(size_t) i]) > std::abs (o[(size_t) pk])) pk = i;
            if (t == 0) ref = pk; else if (std::abs (pk - ref) > 4) same = false;
        }
        check (same, "an impulse peaks at the same time in the clean types (no type-dependent delay)");
    }
    {
        // a block that is off is a pass-through: its knobs change nothing, bit for bit
        const auto x = chord (44100, 0.3f);
        al1::Params a, b;
        a.gateOn = b.gateOn = false; a.boostOn = b.boostOn = false; a.cabOn = b.cabOn = false; a.eqOn = b.eqOn = false; a.limiter = b.limiter = false; a.ambience = b.ambience = 0.0f;
        b.gateThresh = 9.0f; b.gateRelease = 9.0f; b.boostDrive = 10.0f; b.boostLevel = 9.0f; b.boostTight = 0.0f; b.lowCut = 9.0f; b.highCut = 1.0f; b.mic = 9.0f; b.irALevel = 1.0f;
        b.eqLow = 0.0f; b.eqMid = 10.0f; b.eqFreq = 9.0f; b.eqHigh = 0.0f;
        const auto oa = run (a, x), ob = run (b, x);
        check (oa == ob, "gate, boost, cabinet, EQ, limiter and ambience switched off: their knobs change nothing (bit exact)");
        al1::Params c = a; c.inGainDb = 0.0f; const auto oc = run (c, x);
        check (rms (oc, 20000, 40000) > 1.0e-4, "the amp still produces sound with every optional block off");
    }
    {
        // the gate closes on a decaying note
        std::vector<float> x ((size_t) 88200);
        for (size_t i = 0; i < x.size(); ++i) x[i] = 0.5f * std::exp (-(float) i / 6000.0f) * std::sin (6.2831853f * 196.0f * (float) i / 44100.0f);
        al1::Params on; on.gain = 9.0f; on.type = 7; on.gateOn = true; on.gateThresh = 5.0f; al1::Params off = on; off.gateOn = false;
        const auto a = run (on, x), b = run (off, x);
        const double ra = rms (a, 80000, 88000), rb = rms (b, 80000, 88000);
        std::printf ("  gate on %.1f dB, off %.1f dB at the end of the note\n", db (ra), db (rb));
        check (db (ra) < db (rb) - 30.0 && db (ra) < -70.0, "the gate closes on a decaying note (at least 30 dB quieter than without)");
    }
    {
        const auto x = chord (44100, 4.0f);
        al1::Params p; p.type = 12; p.gain = 10.0f; p.master = 10.0f; p.out = 10.0f; p.inGainDb = 12.0f; p.limiter = true; p.gateOn = false;
        const auto o = run (p, x); float peak = 0; for (float y : o) peak = std::max (peak, std::abs (y));
        std::printf ("  limiter peak %.3f (%.2f dBFS)\n", (double) peak, db (peak));
        check (peak <= 0.9441f, "the limiter never exceeds -0.5 dBFS on a +12 dB input");
        p.limiter = false; const auto o2 = run (p, x); float peak2 = 0; for (float y : o2) peak2 = std::max (peak2, std::abs (y));
        check (peak2 > 1.05f, "(control) without the limiter the same signal exceeds full scale");
    }
    {
        // aliasing: a 6 kHz sine (bin aligned) at the highest gain of ULTRA, everything but the harmonics below Nyquist stays under -60 dB
        const int N = 1 << 15;
        const double f0 = std::round (6000.0 * N / fs) * fs / N;
        std::vector<float> x ((size_t) N * 2);
        for (size_t i = 0; i < x.size(); ++i) x[i] = 0.5f * (float) std::sin (6.283185307179586 * f0 * (double) i / fs);
        al1::Params p; p.type = 13; p.gain = 10.0f; p.master = 5.0f; p.gateOn = false; p.cabOn = false; p.limiter = false; p.bass = p.middle = p.treble = 5.0f; p.presence = 5.0f; p.resonance = 5.0f;
        if (auto* e = std::getenv ("AL1_SAG")) p.sag = (float) std::atof (e);
        if (auto* e = std::getenv ("AL1_TYPE")) p.type = std::atoi (e);
        if (auto* e = std::getenv ("AL1_GAIN")) p.gain = (float) std::atof (e);
        const auto o = run (p, x);
        juce::dsp::FFT fft (15); std::vector<float> data ((size_t) N * 2, 0.0f);
        for (int i = 0; i < N; ++i) data[(size_t) i] = o[(size_t) (N + i)];
        fft.performFrequencyOnlyForwardTransform (data.data());
        const int b0 = (int) std::lround (f0 * N / fs); const float fund = data[(size_t) b0];
        float worst = 0; int worstBin = 0;
        for (int k = 8; k < N / 2; ++k)
        {
            bool harmonic = false; for (int h = 1; h <= 4; ++h) if (std::abs (k - h * b0) <= 3) harmonic = true;
            if (! harmonic && data[(size_t) k] > worst) { worst = data[(size_t) k]; worstBin = k; }
        }
        if (std::getenv ("AL1_DEBUG")) for (int k = 8; k < N / 2; ++k) if (db (data[(size_t) k] / fund) > -70.0) std::printf ("      bin %d  %.0f Hz  %.1f dB\n", k, k * fs / N, db (data[(size_t) k] / fund));
        std::printf ("  alias: worst %.1f dB re fundamental at %.0f Hz (osFactor %d)\n", db (worst / fund), worstBin * fs / N, al1::kOsFactor);
        check (db (worst / fund) < -60.0, "aliasing of a 6 kHz sine at ULTRA gain 10 stays below -60 dB");
    }
    {
        // the filter model: smooth, no peak above +12 dB (all types, mic centre / middle / edge, with the cuts)
        bool ok = true; double maxDb = -99, maxBump = -99;
        for (int t = 0; t < al1::kNumTypes; ++t) for (int m = 0; m <= 10; m += 5) for (int lc = 0; lc <= 10; lc += 5) for (int hc = 0; hc <= 10; hc += 5)
        {
            al1::CabModel c; c.setModel (fs, al1::kTypes[t], (float) m * 0.1f); c.setCuts (fs, (float) lc, (float) hc);
            std::vector<double> d;
            for (int i = 0; i <= 120; ++i) d.push_back (db (c.modelMag (fs, 20.0 * std::pow (1000.0, i / 120.0)) * c.cutsMag (fs, 20.0 * std::pow (1000.0, i / 120.0))));
            for (int i = 0; i <= 120; ++i)
            {
                maxDb = std::max (maxDb, d[(size_t) i]);
                if (i >= 10 && i <= 110 && d[(size_t) i] > -40.0) maxBump = std::max (maxBump, d[(size_t) i] - std::max (d[(size_t) i - 10], d[(size_t) i + 10]));   // a peak: how far it stands above both neighbours 10 steps (a bit under an octave) away
            }
        }
        std::printf ("  filter model: max %.1f dB, sharpest bump %.2f dB\n", maxDb, maxBump);
        ok = maxDb <= 12.0 && maxBump < 4.0;
        check (ok, "the filter model response is smooth (no resonance above +12 dB, no bump above 4 dB over its neighbours)");
    }
    {
        // loudness: all types within 3 dB at gain 5 / master 5 (the filter model on); prints the numbers used to set trimDb
        const auto x = chord (44100, 0.1f); double lo = 99, hi = -99, ref = 0;
        for (int t = 0; t < al1::kNumTypes; ++t)
        {
            al1::Params p; p.type = t; const auto o = run (p, x); const double d = db (rms (o, 22050, 44100));
            if (t == 0) ref = d;
            std::printf ("    %-8s %6.1f dB  (trim now %+.1f, would be %+.1f)\n", al1::kTypes[t].name, d, (double) al1::kTypes[t].trimDb, al1::kTypes[t].trimDb + (-18.0 - d));
            lo = std::min (lo, d); hi = std::max (hi, d);
        }
        (void) ref;
        check (hi - lo <= 3.0, "the loudness of the 14 types at gain 5, master 5 is within 3 dB");
    }
    {
        // an impulse response loaded into slot A is convolved (an IR of one pulse after 200 samples, level 0 dB): the output is the dry path delayed by 200 samples
        juce::WavAudioFormat wav; juce::MemoryBlock mb;
        {
            auto stream = std::make_unique<juce::MemoryOutputStream> (mb, false);
            std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (stream.get(), 44100.0, 1, 24, {}, 0));
            if (w != nullptr)
            {
                stream.release();
                juce::AudioBuffer<float> ir (1, 400); ir.clear(); ir.setSample (0, 200, 1.0f);
                w->writeFromAudioSampleBuffer (ir, 0, 400);
            }
        }
        al1::Engine e; e.prepare (fs, 512); e.loadIr (0, mb.getData(), mb.getSize());
        al1::Params p; p.type = 0; p.gain = 2.0f; p.gateOn = false; p.limiter = false; p.irALevel = 7.0f; p.lowCut = 0.0f; p.highCut = 10.0f;
        al1::Params q = p; q.cabOn = false;
        al1::Engine f; f.prepare (fs, 512);
        const auto x = chord (512 * 40, 0.1f); std::vector<float> a = x, b = x, c = x, d = x;
        for (int pos = 0; pos < (int) x.size(); pos += 512)
        {
            float* ca[2] = { a.data() + pos, b.data() + pos }; float* cb[2] = { c.data() + pos, d.data() + pos };
            e.process (ca, 2, 512, p); f.process (cb, 2, 512, q);
            juce::Thread::sleep (20);
        }
        // the IR (one pulse: a gain stage) must replace the filter model: sound, finite, and not the same as the model path
        double eIr = 0, eModel = 0, eDiff = 0; bool finite = true; const int end = (int) x.size();
        for (int i = end - 4000; i < end; ++i)
        {
            if (! std::isfinite (a[(size_t) i])) finite = false;
            eIr += (double) a[(size_t) i] * a[(size_t) i]; eModel += (double) c[(size_t) i] * c[(size_t) i]; const double df = a[(size_t) i] - c[(size_t) i]; eDiff += df * df;
        }
        const double corr = finite && eIr > 0 && eDiff > 0.01 * eModel ? 1.0 : 0.0;
        std::printf ("  IR A level %.1f dB, no-cabinet path %.1f dB, difference %.1f dB\n", db (std::sqrt (eIr / 4000)), db (std::sqrt (eModel / 4000)), db (std::sqrt (eDiff / 4000)));
        check (mb.getSize() > 0 && e.hasIr (0) && corr > 0.5, "an impulse response in slot A is convolved: sound, finite, different from the filter model");
    }
    {
        // 5 minutes of random parameter changes at 44.1 kHz. A click = the energy of the first difference (RMS over 128 sample windows) in the 4096 samples after a change that
        // exceeds 1.5 times (1.8 dB) what the same signal has in steady state with the old and with the new settings (the highest of the two); it must stay below -30 dB.
        const int block = 256, blocks = (int) (300.0 * fs / block), W = 128;
        const auto src = chord (64000, 0.15f, 110.25);                  // 160 periods of 400 samples: the loop has no jump
        std::mt19937 rng (7); std::uniform_real_distribution<float> u (0.0f, 10.0f);
        al1::Params p; p.eqOn = true; p.ambience = std::getenv ("AL1_NOAMB") ? 0.0f : 2.0f; p.limiter = true; p.boostOn = true; p.gateOn = false;
        std::vector<al1::Params> states { p }; std::vector<int> startBlock { 0 };
        al1::Engine e; e.prepare (fs, block);
        std::vector<float> outAll; outAll.reserve ((size_t) blocks * block);
        std::vector<float> l ((size_t) block), r ((size_t) block);
        size_t srcPos = 0; int nextChange = 100;
        for (int b = 0; b < blocks; ++b)
        {
            if (b >= nextChange)
            {
                nextChange = b + 40 + (int) (rng() % 400);
                switch (rng() % 17)
                {
                    case 0: p.gain = u (rng); break;            case 1: p.master = u (rng); break;       case 2: p.bass = u (rng); break;
                    case 3: p.middle = u (rng); break;          case 4: p.treble = u (rng); break;       case 5: p.presence = u (rng); break;
                    case 6: p.resonance = u (rng); break;       case 7: p.sag = u (rng); break;          case 8: p.eqMid = u (rng); break;
                    case 9: p.out = 3.0f + 0.7f * u (rng); break; case 10: p.lowCut = u (rng); p.highCut = u (rng); p.mic = u (rng); break;
                    case 11: p.ambience = std::getenv ("AL1_NOAMB") ? 0.0f : u (rng); break;       case 12: p.type = (int) (rng() % al1::kNumTypes); break;
                    case 13: p.boostDrive = u (rng); break;     case 14: p.boostTight = u (rng); break;
                    case 15: p.eqFreq = std::clamp (p.eqFreq + 0.4f * u (rng) - 2.0f, 0.0f, 10.0f); break;       // a sweep of the mid frequency by a turn of up to 2 units
                    default: p.inGainDb = -6.0f + 1.2f * u (rng); break;
                }
                states.push_back (p); startBlock.push_back (b);
            }
            for (int i = 0; i < block; ++i) l[(size_t) i] = r[(size_t) i] = src[(srcPos++) % 64000];
            float* ch[2] = { l.data(), r.data() }; e.process (ch, 2, block, p);
            outAll.insert (outAll.end(), l.begin(), l.end());
        }
        // the steady state of settings `q` over samples [from, to)
        const auto steady = [&] (const al1::Params& q, int from, int to)
        {
            al1::Engine ref; ref.prepare (fs, block);
            std::vector<float> a ((size_t) block), c ((size_t) block), res;
            for (int pos = from; pos < to; pos += block)
            {
                for (int i = 0; i < block; ++i) a[(size_t) i] = c[(size_t) i] = src[(size_t) ((pos + i) % 64000)];
                float* ch[2] = { a.data(), c.data() }; ref.process (ch, 2, block, q);
                res.insert (res.end(), a.begin(), a.end());
            }
            return res;
        };
        bool fin = allFinite (outAll);
        double worst = 0; int worstK = 0, count = 0;
        for (size_t k = 1; k < states.size(); ++k)
        {
            if ((size_t) (startBlock[k] * block + 4096 + 2 * W) >= outAll.size()) continue;
            const int s0 = startBlock[k] * block, from = std::max (0, s0 - 22050) / block * block, to = s0 + 4096;
            const auto A = steady (states[k - 1], from, to), B = steady (states[k], from, to);
            const auto rd = [&] (const std::vector<float>& v, int off) { double sum = 0; for (int i = 0; i < W; ++i) { const double df = v[(size_t) (off + i + 1)] - v[(size_t) (off + i)]; sum += df * df; } return sum / W; };
            for (int w = 0; w + W < 4096; w += W)
            {
                const int off = s0 - from + w;
                if (off + W + 1 >= (int) A.size() || off + W + 1 >= (int) B.size()) break;
                double dT = 0; for (int i = 0; i < W; ++i) { const double df = outAll[(size_t) (s0 + w + i + 1)] - outAll[(size_t) (s0 + w + i)]; dT += df * df; } dT /= W;
                const double ex = std::sqrt (std::max (0.0, dT - 1.5 * std::max (rd (A, off), rd (B, off))));
                if (ex > worst) { worst = ex; worstK = (int) k; }
                if (std::getenv ("AL1_DEBUG") && (ex > 0.0316 || (std::getenv ("AL1_K") && (int) k == std::atoi (std::getenv ("AL1_K"))))) std::printf ("      k %d w %d T %.1f A %.1f B %.1f dB\n", (int) k, w / W, 10 * std::log10 (dT + 1e-20), 10 * std::log10 (rd (A, off) + 1e-20), 10 * std::log10 (rd (B, off) + 1e-20));
            }
            ++count;
        }
        std::printf ("  %d changes, largest click %.1f dB at %.1f s (state %d)\n", count, db (worst), startBlock[(size_t) worstK] * block / fs, worstK);
        if (worstK > 0)
        {
            const auto& x = states[(size_t) worstK - 1]; const auto& y = states[(size_t) worstK];
            std::printf ("      changed:");
#define CH(f) if (x.f != y.f) std::printf (" %s %.2f -> %.2f", #f, (double) x.f, (double) y.f);
            CH (inGainDb) CH (type) CH (gain) CH (bass) CH (middle) CH (treble) CH (presence) CH (resonance) CH (sag) CH (master) CH (lowCut) CH (highCut) CH (mic) CH (ambience)
            CH (eqMid) CH (eqFreq) CH (out) CH (boostDrive) CH (boostTight)
#undef CH
            std::printf ("\n");
        }
        check (fin && db (worst) < -30.0, "5 minutes of random parameter changes: finite, and no click above -30 dB");
    }
    std::printf ("%d problems\n", problems);
    return problems == 0 ? 0 : 1;
}
