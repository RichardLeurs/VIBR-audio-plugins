// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

// MS-1 Mastering Suite, version 0.1 "foundation" (see .claude/mastering_specs.md): the parts every master needs.
//   INPUT TRIM -> LIMITER (true-peak safe, with a loudness target) -> OUTPUT (gain, dither)
//   plus the honest meters: loudness (ITU-R BS.1770-4: momentary, short-term, integrated, loudness range), true peak (4x oversampled) and a
//   pre / post spectrum.
// Everything here is header-only, has no JUCE dependency and does not allocate after prepare().
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

namespace ms1
{
constexpr float kPi = 3.14159265358979f, kTwoPi = 6.28318530717959f;
constexpr int kBands = 128;                          // the spectrum display: 128 log-spaced bands, 20 Hz .. 20 kHz

inline float dbToGain (float db) { return std::exp (db * 0.11512925f); }
inline float gainToDb (float g) { return 8.68588964f * std::log (std::max (g, 1.0e-9f)); }

//==============================================================================
// Targets: loudness target (integrated LUFS), true-peak ceiling and the limiter character they come with
struct Target { const char* name; float lufs, ceiling; int character; };
constexpr int kNumTargets = 8;
inline const Target& target (int i)
{
    static const Target t[kNumTargets] = { { "STREAMING", -14.0f, -1.0f, 0 },   { "APPLE MUSIC", -16.0f, -1.0f, 0 },  { "TIDAL / AMAZON", -14.0f, -1.0f, 0 },
                                           { "CD / LOUD", -9.0f, -0.3f, 2 },    { "VINYL PRE-MASTER", -16.0f, -3.0f, 0 }, { "BROADCAST R128", -23.0f, -1.0f, 0 },
                                           { "CLUB", -8.0f, -0.3f, 2 },         { "CUSTOM", -14.0f, -1.0f, 0 } };
    return t[std::clamp (i, 0, kNumTargets - 1)];
}

//==============================================================================
// K-weighting of ITU-R BS.1770-4: a high shelf (+4 dB above 1.7 kHz) and a high-pass (38 Hz), coefficients for any sample rate
struct KFilter
{
    float b[2][3] {}, a[2][2] {};
    float z[2][2] {};

    void prepare (float fs)
    {
        {   // stage 1: shelf
            const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
            const double K = std::tan (3.14159265358979 * f0 / (double) fs), Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416);
            const double a0 = 1.0 + K / Q + K * K;
            b[0][0] = (float) ((Vh + Vb * K / Q + K * K) / a0); b[0][1] = (float) (2.0 * (K * K - Vh) / a0); b[0][2] = (float) ((Vh - Vb * K / Q + K * K) / a0);
            a[0][0] = (float) (2.0 * (K * K - 1.0) / a0);       a[0][1] = (float) ((1.0 - K / Q + K * K) / a0);
        }
        {   // stage 2: high-pass (the numerator is not normalised: 1, -2, 1)
            const double f0 = 38.13547087602444, Q = 0.5003270373238773;
            const double K = std::tan (3.14159265358979 * f0 / (double) fs), a0 = 1.0 + K / Q + K * K;
            b[1][0] = 1.0f; b[1][1] = -2.0f; b[1][2] = 1.0f;
            a[1][0] = (float) (2.0 * (K * K - 1.0) / a0);       a[1][1] = (float) ((1.0 - K / Q + K * K) / a0);
        }
        reset();
    }
    void reset() { for (auto& r : z) r[0] = r[1] = 0.0f; }
    float tick (float x)
    {
        float v = x;
        for (int s = 0; s < 2; ++s)         // direct form II transposed
        {
            const float y = b[s][0] * v + z[s][0];
            z[s][0] = b[s][1] * v - a[s][0] * y + z[s][1];
            z[s][1] = b[s][2] * v - a[s][1] * y;
            v = y;
        }
        return v;
    }
};

//==============================================================================
// Loudness meter: momentary (400 ms), short-term (3 s), integrated (gated, -70 LUFS absolute and -10 LU relative) and loudness range (-20 LU
// relative gate, 10th to 95th percentile of the short-term values). The gating uses 0.1 LU histograms, so nothing is allocated while playing.
class Loudness
{
public:
    std::atomic<float> momentary { -120.0f }, shortTerm { -120.0f }, integrated { -120.0f }, range { 0.0f };
    static constexpr int kHistory = 300;              // the short-term history for the display: one value per second, the last 5 minutes
    std::atomic<float> history[kHistory];
    std::atomic<int> historyPos { 0 };

    void prepare (float sampleRate)
    {
        fs = sampleRate;
        subLen = std::max (1, (int) std::lround (0.1f * fs));
        for (auto& k : kf) k.prepare (fs);
        resetAll();
    }

    void resetAll()
    {
        for (auto& k : kf) k.reset();
        subCount = 0; subEnergy = 0.0; ringPos = 0; ringFilled = 0; sinceSecond = 0;
        for (auto& r : ring) r = 0.0;
        std::fill (std::begin (blockCount), std::end (blockCount), 0); std::fill (std::begin (blockEnergy), std::end (blockEnergy), 0.0);
        std::fill (std::begin (stCount), std::end (stCount), 0);
        momentary.store (-120.0f); shortTerm.store (-120.0f); integrated.store (-120.0f); range.store (0.0f);
        for (auto& h : history) h.store (-120.0f);
        historyPos.store (0);
    }

    void resetIntegrated()      // the reset button: the integrated value and the range start again
    {
        std::fill (std::begin (blockCount), std::end (blockCount), 0); std::fill (std::begin (blockEnergy), std::end (blockEnergy), 0.0);
        std::fill (std::begin (stCount), std::end (stCount), 0);
        integrated.store (-120.0f); range.store (0.0f);
    }

    void process (const float* l, const float* r, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            const float a = kf[0].tick (l[i]), b = kf[1].tick (r[i]);
            subEnergy += (double) a * a + (double) b * b;
            if (++subCount >= subLen)
            {
                pushSub (subEnergy / (double) subLen);
                subCount = 0; subEnergy = 0.0;
            }
        }
    }

    static float toLufs (double energy) { return energy > 1.0e-12 ? (float) (-0.691 + 10.0 * std::log10 (energy)) : -120.0f; }

private:
    static constexpr int kBins = 800;                 // -70 .. +10 LUFS in 0.1 LU steps
    static int binOf (float lufs) { return std::clamp ((int) std::floor ((lufs + 70.0f) * 10.0f), 0, kBins - 1); }
    static double binEnergy (int b) { return std::pow (10.0, (((double) b + 0.5) * 0.1 - 70.0 + 0.691) / 10.0); }

    void pushSub (double e)
    {
        ring[(size_t) ringPos] = e; ringPos = (ringPos + 1) % 30; ringFilled = std::min (30, ringFilled + 1);
        const auto mean = [&] (int count)
        {
            double s = 0.0; for (int k = 0; k < count; ++k) s += ring[(size_t) ((ringPos - 1 - k + 60) % 30)];
            return s / (double) count;
        };
        if (ringFilled >= 4)
        {
            const double e4 = mean (4);
            const float m = toLufs (e4);
            momentary.store (m, std::memory_order_relaxed);
            if (m > -70.0f) { const int b = binOf (m); ++blockCount[b]; blockEnergy[b] += e4; }
        }
        if (ringFilled >= 30)
        {
            const float s = toLufs (mean (30));
            shortTerm.store (s, std::memory_order_relaxed);
            if (s > -70.0f) ++stCount[binOf (s)];
        }
        if (ringFilled >= 4) updateIntegrated();
        if (ringFilled >= 30) updateRange();
        if (++sinceSecond >= 10)
        {
            sinceSecond = 0;
            const int p = historyPos.load (std::memory_order_relaxed);
            history[p].store (shortTerm.load (std::memory_order_relaxed), std::memory_order_relaxed);
            historyPos.store ((p + 1) % kHistory, std::memory_order_release);
        }
    }

    void updateIntegrated()
    {
        double sum = 0.0; long count = 0;
        for (int b = 0; b < kBins; ++b) { sum += blockEnergy[b]; count += blockCount[b]; }
        if (count == 0) { integrated.store (-120.0f, std::memory_order_relaxed); return; }
        const float gate = toLufs (sum / (double) count) - 10.0f;             // the relative gate
        double s2 = 0.0; long c2 = 0;
        for (int b = binOf (gate); b < kBins; ++b) { s2 += blockEnergy[b]; c2 += blockCount[b]; }
        integrated.store (c2 > 0 ? toLufs (s2 / (double) c2) : -120.0f, std::memory_order_relaxed);
    }

    void updateRange()
    {
        double sum = 0.0; long count = 0;
        for (int b = 0; b < kBins; ++b) { sum += (double) stCount[b] * binEnergy (b); count += stCount[b]; }
        if (count < 2) { range.store (0.0f, std::memory_order_relaxed); return; }
        const float gate = toLufs (sum / (double) count) - 20.0f;
        long total = 0; for (int b = binOf (gate); b < kBins; ++b) total += stCount[b];
        if (total < 2) { range.store (0.0f, std::memory_order_relaxed); return; }
        const auto percentile = [&] (double p)
        {
            const long target = (long) std::floor (p * (double) (total - 1) + 0.5);
            long acc = 0;
            for (int b = binOf (gate); b < kBins; ++b) { acc += stCount[b]; if (acc > target) return (float) (((double) b + 0.5) * 0.1 - 70.0); }
            return 0.0f;
        };
        range.store (std::max (0.0f, percentile (0.95) - percentile (0.10)), std::memory_order_relaxed);
    }

    float fs = 48000.0f;
    KFilter kf[2];
    int subLen = 4800, subCount = 0, ringPos = 0, ringFilled = 0, sinceSecond = 0;
    double subEnergy = 0.0, ring[30] {};
    long blockCount[kBins] {}, stCount[kBins] {};
    double blockEnergy[kBins] {};
};

//==============================================================================
// True-peak detector: 8x oversampling by an FIR of 384 taps in 8 phases (a Kaiser-windowed sinc cut at the original Nyquist frequency, flat to
// within 0.1 dB up to about 0.45 of the sample rate; the 4x of ITU-R BS.1770-4 can miss an inter-sample peak by up to 0.7 dB, 8x by 0.15 dB).
// `push` returns the largest absolute value among the eight interpolated samples; its delay is `kDelay` input samples.
class TruePeak
{
public:
    static constexpr int kDelay = 24, kUp = 8, kPhaseTaps = 48, kTaps = kUp * kPhaseTaps;
    void reset() { std::fill (std::begin (hist), std::end (hist), 0.0f); pos = 0; }
    float push (float x)
    {
        hist[pos & 63] = x; pos = (pos + 1) & 63;
        const auto& t = taps();
        float peak = 0.0f;
        for (int ph = 0; ph < kUp; ++ph)
        {
            float acc = 0.0f;
            for (int k = 0; k < kPhaseTaps; ++k) acc += t[(size_t) (ph + kUp * k)] * hist[(pos - 1 - k) & 63];
            peak = std::max (peak, std::abs (acc));
        }
        return peak;
    }
private:
    static double bessel0 (double x) { double s = 1.0, t = 1.0; for (int k = 1; k < 40; ++k) { t *= (x / (2.0 * k)) * (x / (2.0 * k)); s += t; } return s; }
    static const std::array<float, kTaps>& taps()
    {
        static const std::array<float, kTaps> t = []
        {
            std::array<float, kTaps> h {};
            double sum = 0.0;
            const double beta = 8.0, mid = (kTaps - 1) * 0.5;
            for (int n = 0; n < kTaps; ++n)
            {
                const double x = ((double) n - mid) / (double) kUp, s = std::abs (x) < 1e-9 ? 1.0 : std::sin (3.14159265358979 * x) / (3.14159265358979 * x);
                const double u = ((double) n - mid) / mid, w = bessel0 (beta * std::sqrt (std::max (0.0, 1.0 - u * u))) / bessel0 (beta);
                h[(size_t) n] = (float) (s * w); sum += h[(size_t) n];
            }
            for (auto& v : h) v = (float) ((double) v * (double) kUp / sum);       // each phase has unity gain at DC
            return h;
        }();
        return t;
    }
    float hist[64] {};
    int pos = 0;
};

//==============================================================================
// Small radix-2 FFT for the spectrum (a power of two, set at construction)
class Fft
{
public:
    explicit Fft (int logSize = 12) : lg (logSize), n (1 << logSize), cosT ((size_t) n / 2), sinT ((size_t) n / 2), rev ((size_t) n)
    {
        for (int i = 0; i < n / 2; ++i) { cosT[(size_t) i] = (float) std::cos (-2.0 * 3.14159265358979 * i / n); sinT[(size_t) i] = (float) std::sin (-2.0 * 3.14159265358979 * i / n); }
        for (int i = 0; i < n; ++i)
        {
            int r = 0; for (int b = 0; b < lg; ++b) if (i & (1 << b)) r |= 1 << (lg - 1 - b);
            rev[(size_t) i] = r;
        }
    }
    int size() const { return n; }
    void transform (std::vector<float>& re, std::vector<float>& im) const
    {
        for (int i = 0; i < n; ++i) { const int j = rev[(size_t) i]; if (j > i) { std::swap (re[(size_t) i], re[(size_t) j]); std::swap (im[(size_t) i], im[(size_t) j]); } }
        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len / 2, step = n / len;
            for (int i = 0; i < n; i += len)
                for (int k = 0; k < half; ++k)
                {
                    const float wr = cosT[(size_t) (k * step)], wi = sinT[(size_t) (k * step)];
                    const size_t a = (size_t) (i + k), b = (size_t) (i + k + half);
                    const float tr = re[b] * wr - im[b] * wi, ti = re[b] * wi + im[b] * wr;
                    re[b] = re[a] - tr; im[b] = im[a] - ti; re[a] += tr; im[a] += ti;
                }
        }
    }
private:
    int lg, n;
    std::vector<float> cosT, sinT;
    std::vector<int> rev;
};

// Spectrum analyser: a Hann-windowed FFT every half window, summed into 128 log bands (20 Hz .. 20 kHz), in dBFS (a full-scale sine reads 0 dB).
//   mode 0 SMOOTH  4096-point FFT, each band is the power of a 1/6 octave around its centre (a sine reads its own level, noise rises 3 dB per octave)
//   mode 1 FFT     8192-point FFT, each band is the strongest bin in it (the finest picture of the lows and of single tones)
class Spectrum
{
public:
    static constexpr int kMaxN = 8192;
    std::atomic<float> bands[kBands];

    void setMode (int m) { mode.store (m != 0 ? 1 : 0, std::memory_order_relaxed); }

    void prepare (float sampleRate)
    {
        fs = sampleRate;
        re.assign ((size_t) kMaxN, 0.0f); im.assign ((size_t) kMaxN, 0.0f);
        for (int w = 0; w < 2; ++w)
        {
            const int n = w == 0 ? 4096 : 8192;
            win[w].assign ((size_t) n, 0.0f);
            double ws = 0.0;
            for (int i = 0; i < n; ++i) { win[w][(size_t) i] = (float) (0.5 - 0.5 * std::cos (2.0 * 3.14159265358979 * i / n)); ws += (double) win[w][(size_t) i]; }
            norm[w] = (float) (2.0 / ws);
        }
        for (int b = 0; b <= kBands; ++b) edge[(size_t) b] = 20.0f * std::pow (1000.0f, (float) b / (float) kBands);          // 20 Hz .. 20 kHz
        reset();
    }
    void reset() { std::fill (std::begin (buf), std::end (buf), 0.0f); pos = 0; since = 0; for (auto& b : bands) b.store (-120.0f); for (auto& s : smooth) s = -120.0f; }
    void push (float x)
    {
        buf[(size_t) pos] = x; pos = (pos + 1) % kMaxN;
        const int n = mode.load (std::memory_order_relaxed) == 1 ? 8192 : 4096;
        if (++since >= n / 2) { since = 0; analyse (n); }
    }
private:
    void analyse (int n)
    {
        const int w = n == 8192 ? 1 : 0;
        for (int i = 0; i < n; ++i) { re[(size_t) i] = buf[(size_t) ((pos + kMaxN - n + i) % kMaxN)] * win[w][(size_t) i]; im[(size_t) i] = 0.0f; }
        (w == 1 ? fft8192 : fft4096).transform (re, im);
        const float binHz = fs / (float) n;
        for (int b = 0; b < kBands; ++b)
        {
            float amp;
            if (w == 1)                                                                   // FFT: the strongest bin of the band
            {
                const int lo = std::max (1, (int) std::floor (edge[(size_t) b] / binHz)), hi = std::min (n / 2 - 1, std::max (lo, (int) std::floor (edge[(size_t) b + 1] / binHz)));
                float peak = 0.0f;
                for (int k = lo; k <= hi; ++k) peak = std::max (peak, re[(size_t) k] * re[(size_t) k] + im[(size_t) k] * im[(size_t) k]);
                amp = std::sqrt (peak) * norm[w];
            }
            else                                                                          // SMOOTH: the power of 1/6 octave (at least 3 bins), divided by the noise bandwidth of the window (1.5 bins)
            {
                const float fc = std::sqrt (edge[(size_t) b] * edge[(size_t) b + 1]), f1 = fc * 0.94387f, f2 = fc * 1.05946f;      // 2^(-1/12), 2^(1/12)
                const int mid = std::clamp ((int) std::lround (fc / binHz), 2, n / 2 - 2);
                const int lo = std::max (1, std::min ((int) std::floor (f1 / binHz), mid - 1)), hi = std::min (n / 2 - 1, std::max ((int) std::ceil (f2 / binHz), mid + 1));
                float sum = 0.0f;
                for (int k = lo; k <= hi; ++k) sum += re[(size_t) k] * re[(size_t) k] + im[(size_t) k] * im[(size_t) k];
                amp = std::sqrt (sum / 1.5f) * norm[w];
            }
            const float db = gainToDb (amp);
            smooth[(size_t) b] = db > smooth[(size_t) b] ? db : smooth[(size_t) b] * 0.8f + db * 0.2f;
            bands[b].store (smooth[(size_t) b], std::memory_order_relaxed);
        }
    }
    float fs = 48000.0f, norm[2] { 1.0f, 1.0f };
    std::atomic<int> mode { 0 };
    Fft fft4096 { 12 }, fft8192 { 13 };
    std::vector<float> re, im, win[2];
    std::array<float, kMaxN> buf {};
    std::array<float, kBands + 1> edge {};
    std::array<float, kBands> smooth {};
    int pos = 0, since = 0;
};

//==============================================================================
// Level meters of the output: sample peak (hold), programme RMS, the true peak of the last 3 seconds (for PSR) and a DR value in the style of the Pleasurize
// "DR" meter: per channel the average power of the loudest 20 % of the 3 second blocks against the second-highest block peak, averaged over the channels.
// The DR value uses the RMS scaled so that a full-scale sine reads 0 dB (x sqrt 2), as the DR meter does; the RMS shown is the true RMS. All of it is reset together.
class LevelMeter
{
public:
    static constexpr int kMaxBlocks = 2048;                                            // the last 100 minutes
    std::atomic<float> samplePeakDb { -120.0f }, rmsDb { -120.0f }, windowPeakDb { -120.0f }, dr { 0.0f };
    std::atomic<bool> drValid { false };

    void prepare (float sampleRate) { fs = sampleRate; blockLen = std::max (1, (int) std::lround (3.0f * fs)); slotLen = std::max (1, (int) std::lround (0.1f * fs)); reset(); }
    void reset()
    {
        peak = 0.0f; sumSq = 0.0; count = 0;
        for (auto& c : ch) c = Chan();
        blocks = 0; blockCount = 0;
        for (auto& s : slot) s = 0.0f;
        slotPos = 0; slotFill = 0;
        samplePeakDb.store (-120.0f); rmsDb.store (-120.0f); windowPeakDb.store (-120.0f); dr.store (0.0f); drValid.store (false);
    }

    void process (const float* l, const float* r, int n, float truePeakLin)
    {
        const float* in[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
            for (int c = 0; c < 2; ++c)
            {
                const float a = std::abs (in[c][i]);
                peak = std::max (peak, a);
                ch[c].blockPeak = std::max (ch[c].blockPeak, a);
                ch[c].blockSq += (double) a * a;
            }
            sumSq += (double) l[i] * l[i] + (double) r[i] * r[i];
            if (++blockCount >= blockLen) endBlock();
            if (++slotFill >= slotLen) { slotFill = 0; slotPos = (slotPos + 1) % kSlots; slot[(size_t) slotPos] = 0.0f; }
        }
        count += n;
        slot[(size_t) slotPos] = std::max (slot[(size_t) slotPos], truePeakLin);
        float wp = 0.0f; for (float v : slot) wp = std::max (wp, v);
        windowPeakDb.store (gainToDb (wp), std::memory_order_relaxed);
        samplePeakDb.store (gainToDb (peak), std::memory_order_relaxed);
        if (count > 0) rmsDb.store (gainToDb ((float) std::sqrt (sumSq / (2.0 * (double) count))), std::memory_order_relaxed);        // the true RMS of both channels: a full-scale sine reads -3 dB
    }

private:
    static constexpr int kSlots = 30;                                                  // 30 slots of 100 ms = 3 s
    struct Chan { double blockSq = 0.0; float blockPeak = 0.0f; float rms2[kMaxBlocks] {}, peaks[kMaxBlocks] {}; };

    void endBlock()
    {
        const int idx = blocks % kMaxBlocks;
        for (auto& c : ch)
        {
            c.rms2[idx] = (float) (c.blockSq / (double) blockLen); c.peaks[idx] = c.blockPeak;
            c.blockSq = 0.0; c.blockPeak = 0.0f;
        }
        ++blocks; blockCount = 0;
        const int n = std::min (blocks, kMaxBlocks);
        double total = 0.0;
        for (auto& c : ch)
        {
            float tmp[kMaxBlocks], pk[kMaxBlocks];
            std::copy (c.rms2, c.rms2 + n, tmp); std::copy (c.peaks, c.peaks + n, pk);
            const int top = std::max (1, (int) std::ceil (0.2 * n));
            std::nth_element (tmp, tmp + (top - 1), tmp + n, std::greater<float>());
            double acc = 0.0; for (int k = 0; k < top; ++k) acc += tmp[k];
            const double rms = std::sqrt (2.0 * acc / (double) top);                  // x sqrt 2: a full-scale sine is 0 dB
            float second = 0.0f;
            if (n >= 2) { std::nth_element (pk, pk + 1, pk + n, std::greater<float>()); second = pk[1]; } else second = pk[0];
            total += 20.0 * std::log10 (std::max (1.0e-9, (double) second) / std::max (1.0e-9, rms));
        }
        dr.store ((float) (total / 2.0), std::memory_order_relaxed);
        drValid.store (true, std::memory_order_relaxed);
    }

    float fs = 48000.0f, peak = 0.0f, slot[kSlots] {};
    double sumSq = 0.0;
    long count = 0;
    int blockLen = 144000, slotLen = 4800, blocks = 0, blockCount = 0, slotPos = 0, slotFill = 0;
    Chan ch[2];
};

// Stereo field of the output: the phase correlation (short-term, 3 s, and integrated since the reset, only over parts that are not silent) and the last samples
// for the goniometer.
class StereoMeter
{
public:
    static constexpr int kScope = 512;
    std::atomic<float> corrShort { 1.0f }, corrInt { 1.0f };
    float scopeL[kScope] {}, scopeR[kScope] {};
    std::atomic<int> scopePos { 0 };

    void prepare (float sampleRate) { fs = sampleRate; reset(); }
    void reset() { ll = rr = lr = 0.0f; sll = srr = slr = 0.0; corrShort.store (1.0f); corrInt.store (1.0f); }

    void process (const float* l, const float* r, int n)
    {
        const float cc = std::exp (-1.0f / (3.0f * fs));
        double bll = 0.0, brr = 0.0, blr = 0.0;
        for (int i = 0; i < n; ++i)
        {
            ll = cc * ll + (1.0f - cc) * l[i] * l[i]; rr = cc * rr + (1.0f - cc) * r[i] * r[i]; lr = cc * lr + (1.0f - cc) * l[i] * r[i];
            bll += (double) l[i] * l[i]; brr += (double) r[i] * r[i]; blr += (double) l[i] * r[i];
            if ((i & 3) == 0) { const int w = scopePos.load (std::memory_order_relaxed); scopeL[w] = l[i]; scopeR[w] = r[i]; scopePos.store ((w + 1) % kScope, std::memory_order_relaxed); }
        }
        if ((bll + brr) / (2.0 * std::max (1, n)) > 1.0e-7) { sll += bll; srr += brr; slr += blr; }              // not silent (above about -70 dBFS RMS)
        corrShort.store (ll * rr > 1.0e-12f ? std::clamp (lr / std::sqrt (ll * rr), -1.0f, 1.0f) : 1.0f, std::memory_order_relaxed);
        corrInt.store (sll * srr > 1.0e-12 ? (float) std::clamp (slr / std::sqrt (sll * srr), -1.0, 1.0) : 1.0f, std::memory_order_relaxed);
    }

private:
    float fs = 48000.0f, ll = 0.0f, rr = 0.0f, lr = 0.0f;
    double sll = 0.0, srr = 0.0, slr = 0.0;
};

//==============================================================================
// Limiter: a lookahead brick-wall limiter that is true-peak safe. The detector sees the 4x oversampled signal; the gain is the minimum of the
// required gain over the lookahead window, smoothed by a moving average of the same length (so the gain is already down when the peak arrives and
// moves without a click), then released with a one-pole release that never lets the gain above that curve. Two channels, with a stereo link.
struct LimiterParams
{
    float gainDb = 0.0f, ceilingDb = -1.0f, releaseMs = 100.0f, lookaheadMs = 2.0f, link = 1.0f;
    int character = 0;                                // 0 transparent, 1 punchy, 2 loud: the release time is x1, x0.5, x0.25
    bool autoRelease = false;                         // AUTO: a fast (40 ms) and a slow (600 ms) release at once, averaged: short peaks recover quickly, sustained loudness slowly
};

class Limiter
{
public:
    std::atomic<float> grDb { 0.0f };                 // the gain reduction now (dB, <= 0), for the meter

    static constexpr int kMaxLook = 1024;             // 5 ms at 192 kHz
    int latency() const { return look - 1 + TruePeak::kDelay; }

    void prepare (float sampleRate)
    {
        fs = sampleRate;
        for (auto& d : delay) d.assign (2048, 0.0f);
        look = 96;
        for (int c = 0; c < 2; ++c) { tp[c].reset(); st[c].reset (look); }
        setLookahead (2.0f);
        wpos = 0;
    }

    void reset()
    {
        for (int c = 0; c < 2; ++c) { tp[c].reset(); st[c].reset (look); std::fill (delay[c].begin(), delay[c].end(), 0.0f); }
        wpos = 0; grDb.store (0.0f);
    }

    void setLookahead (float ms)
    {
        const int l = std::clamp ((int) std::lround (ms * 0.001f * fs), 8, kMaxLook);
        if (l != look) { look = l; for (int c = 0; c < 2; ++c) st[c].reset (look); }
    }

    // `in` is the input of this block, `out` the limited and delayed result (the same buffers can be used)
    void process (float* l, float* r, int n, const LimiterParams& p)
    {
        setLookahead (p.lookaheadMs);
        const float gain = dbToGain (p.gainDb), ceil = dbToGain (p.ceilingDb - 0.15f);          // a small margin for the inter-sample peaks of the moving gain and the finite detector
        const float relMul = p.character == 1 ? 0.5f : p.character == 2 ? 0.25f : 1.0f;
        const float relCoef = 1.0f - std::exp (-1.0f / std::max (1.0f, (p.autoRelease ? 40.0f : p.releaseMs) * 0.001f * relMul * fs));
        const float slowCoef = p.autoRelease ? 1.0f - std::exp (-1.0f / std::max (1.0f, 600.0f * 0.001f * relMul * fs)) : 0.0f;
        const float slowAtk = p.autoRelease ? 1.0f - std::exp (-1.0f / std::max (1.0f, 150.0f * 0.001f * fs)) : 0.0f;
        const int audioDelay = look - 1 + TruePeak::kDelay;
        float* io[2] = { l, r };
        float minGr = 1.0f;
        for (int i = 0; i < n; ++i)
        {
            float x[2] = { io[0][i] * gain, io[1][i] * gain }, det[2];
            for (int c = 0; c < 2; ++c) det[c] = tp[c].push (x[c]);
            const float dMax = std::max (det[0], det[1]);
            for (int c = 0; c < 2; ++c)
            {
                const float d = p.link * dMax + (1.0f - p.link) * det[c];
                const float req = d > ceil ? ceil / d : 1.0f;
                const float g = st[c].step (req, look, relCoef, slowCoef, slowAtk);
                delay[c][(size_t) (wpos & 2047)] = x[c];
                const float y = delay[c][(size_t) ((wpos - audioDelay) & 2047)] * g;
                io[c][i] = std::clamp (y, -ceil, ceil);                                              // a last safety on the sample peak
                minGr = std::min (minGr, g);
            }
            ++wpos;
        }
        grDb.store (gainToDb (minGr), std::memory_order_relaxed);
    }

private:
    struct Chan
    {
        // sliding minimum (monotonic queue), moving average of the minimum, release
        float qv[kMaxLook + 8] {}; int qi[kMaxLook + 8] {}; int qh = 0, qt = 0, idx = 0;
        float avgBuf[kMaxLook + 8] {}; double sum = 0.0; int ai = 0;
        float rel = 1.0f, rel2 = 1.0f;
        void reset (int look)
        {
            qh = qt = idx = ai = 0; rel = rel2 = 1.0f;
            for (auto& v : avgBuf) v = 1.0f;
            sum = (double) look;                       // `look` values of 1.0 in the average
        }
        float step (float req, int look, float relCoef, float slowCoef = 0.0f, float slowAtk = 0.0f)
        {
            const int cap = kMaxLook + 8;
            while (qt != qh && qv[(qt - 1 + cap) % cap] >= req) qt = (qt - 1 + cap) % cap;
            qv[qt] = req; qi[qt] = idx; qt = (qt + 1) % cap;
            while (qi[qh] <= idx - look) qh = (qh + 1) % cap;
            const float mn = qv[qh];
            sum += (double) mn - (double) avgBuf[ai]; avgBuf[ai] = mn; ai = (ai + 1) % look;
            const float box = (float) (sum / (double) look);
            rel = box < rel ? box : rel + (box - rel) * relCoef;
            ++idx;
            if (slowCoef <= 0.0f) return rel;
            rel2 = box < rel2 ? rel2 + (box - rel2) * slowAtk : rel2 + (box - rel2) * slowCoef;      // the slow stage follows the reduction with a delay: it only drops after sustained reduction
            return std::min (box, 0.5f * (rel + rel2));                                              // never above the gain the lookahead requires: the ceiling holds
        }
    };
    float fs = 48000.0f;
    int look = 96, wpos = 0;
    TruePeak tp[2];
    Chan st[2];
    std::vector<float> delay[2];
};

//==============================================================================
// Corrective EQ: 8 bands in series, minimum phase (no latency): band 1 a low shelf, bands 2 .. 7 bells, band 8 a high shelf (RBJ biquads, Q for the shelf
// slope too). Each band works on Stereo, Left only, Right only, Mid only or Side only. A flat or switched-off band costs nothing and leaves the signal
// bit-exact; changing a band ramps its coefficients over the block (no zipper noise) and a band that becomes flat fades out into the plain signal.
constexpr int kEqBands = 8;
enum EqMode { EqStereo, EqLeft, EqRight, EqMid, EqSide };
// dyn: the gain of the band follows the level in the band (a band-pass at the band's frequency): ABOVE the threshold the band's gain is applied (a cut that grows
// with the level: a de-esser style cut, or a boost that opens up with the level), BELOW the threshold the gain is applied when the band is quiet. The static gain is
// the most the band will do; between 0 and that it follows (level - threshold) * (1 - 1 / ratio).
struct EqBand
{
    float freq = 1000.0f, gainDb = 0.0f, q = 1.0f; int mode = EqStereo; bool on = true;
    bool dyn = false, dynBelow = false;
    float dynThresholdDb = -30.0f, dynRatio = 2.0f, dynAttackMs = 10.0f, dynReleaseMs = 100.0f;
};

struct BiquadCoefs { float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f; };

// RBJ cookbook; band 0 = low shelf, 7 = high shelf, the others bells
inline BiquadCoefs eqCoefs (int band, const EqBand& p, float fs)
{
    const double f = std::clamp ((double) p.freq, 10.0, 0.45 * (double) fs), q = std::clamp ((double) p.q, 0.1, 20.0);
    const double A = std::pow (10.0, (double) p.gainDb / 40.0), w0 = 6.283185307179586 * f / (double) fs, cw = std::cos (w0), sw = std::sin (w0), al = sw / (2.0 * q);
    double b0, b1, b2, a0, a1, a2;
    if (band == 0)
    {
        const double t = 2.0 * std::sqrt (A) * al;
        b0 = A * ((A + 1) - (A - 1) * cw + t);  b1 = 2 * A * ((A - 1) - (A + 1) * cw);  b2 = A * ((A + 1) - (A - 1) * cw - t);
        a0 = (A + 1) + (A - 1) * cw + t;        a1 = -2 * ((A - 1) + (A + 1) * cw);     a2 = (A + 1) + (A - 1) * cw - t;
    }
    else if (band == kEqBands - 1)
    {
        const double t = 2.0 * std::sqrt (A) * al;
        b0 = A * ((A + 1) + (A - 1) * cw + t);  b1 = -2 * A * ((A - 1) + (A + 1) * cw); b2 = A * ((A + 1) + (A - 1) * cw - t);
        a0 = (A + 1) - (A - 1) * cw + t;        a1 = 2 * ((A - 1) - (A + 1) * cw);      a2 = (A + 1) - (A - 1) * cw - t;
    }
    else
    {
        b0 = 1 + al * A;  b1 = -2 * cw;  b2 = 1 - al * A;
        a0 = 1 + al / A;  a1 = -2 * cw;  a2 = 1 - al / A;
    }
    return { (float) (b0 / a0), (float) (b1 / a0), (float) (b2 / a0), (float) (a1 / a0), (float) (a2 / a0) };
}

// the magnitude (dB) of one band at a frequency, for the display
inline float eqResponseDb (int band, const EqBand& p, float f, float fs)
{
    if (! p.on) return 0.0f;
    const auto c = eqCoefs (band, p, fs);
    const double w = 6.283185307179586 * (double) f / (double) fs, c1 = std::cos (w), s1 = std::sin (w), c2 = std::cos (2 * w), s2 = std::sin (2 * w);
    const double nr = c.b0 + c.b1 * c1 + c.b2 * c2, ni = -(c.b1 * s1 + c.b2 * s2), dr = 1.0 + c.a1 * c1 + c.a2 * c2, di = -(c.a1 * s1 + c.a2 * s2);
    return (float) (10.0 * std::log10 (std::max (1.0e-12, (nr * nr + ni * ni) / (dr * dr + di * di))));
}

class Eq
{
public:
    std::atomic<float> dynAmount[kEqBands];            // how much of its gain each dynamic band applies now (0 .. 1), for the display

    // linear-phase mode: the filters are not run here, but the detectors of the dynamic bands are (dynAmount tells how much of each band's gain applies)
    void analyse (const float* l, const float* r, int n, const EqBand* bands)
    {
        for (size_t k = 0; k < (size_t) kEqBands; ++k)
        {
            const bool live = bands[k].on && std::abs (bands[k].gainDb) > 0.005f;
            dynAmount[k].store (live ? (bands[k].dyn ? detect (st[k], k, bands[k], l, r, n) : 1.0f) : 0.0f, std::memory_order_relaxed);
        }
    }

    void prepare (float sampleRate) { fs = sampleRate; reset(); }
    void reset() { for (auto& b : st) b = State(); for (auto& d : dynAmount) d.store (0.0f); }

    void process (float* l, float* r, int n, const EqBand* bands)
    {
        for (int k = 0; k < kEqBands; ++k)
        {
            State& s = st[(size_t) k];
            const EqBand& p = bands[k];
            if (p.mode != s.mode) { s.z[0][0] = s.z[0][1] = s.z[1][0] = s.z[1][1] = 0.0f; s.mode = p.mode; }
            EqBand pe = p;                                                         // the band as it is applied (a dynamic band: the gain follows the level)
            if (p.dyn && p.on && std::abs (p.gainDb) > 0.005f)
            {
                const float frac = detect (s, k, p, l, r, n);
                pe.gainDb = p.gainDb * frac;
                dynAmount[k].store (frac, std::memory_order_relaxed);
            }
            else dynAmount[k].store (p.on && std::abs (p.gainDb) > 0.005f ? 1.0f : 0.0f, std::memory_order_relaxed);
            const bool active = p.on && std::abs (pe.gainDb) > 0.005f;
            const BiquadCoefs target = active ? eqCoefs (k, pe, fs) : BiquadCoefs();
            if (! active && s.flat) continue;                                      // nothing to do: bit-exact
            if (s.flat) { s.c = target; }                                          // switched on from flat: ramp from the plain filter
            const BiquadCoefs c0 = s.flat ? BiquadCoefs() : s.c;
            const float inv = 1.0f / (float) n;
            const float d0 = (target.b0 - c0.b0) * inv, d1 = (target.b1 - c0.b1) * inv, d2 = (target.b2 - c0.b2) * inv, d3 = (target.a1 - c0.a1) * inv, d4 = (target.a2 - c0.a2) * inv;
            BiquadCoefs c = c0;
            for (int i = 0; i < n; ++i)
            {
                c.b0 += d0; c.b1 += d1; c.b2 += d2; c.a1 += d3; c.a2 += d4;
                switch (p.mode)
                {
                    case EqLeft:   l[i] = tick (s, 0, c, l[i]); break;
                    case EqRight:  r[i] = tick (s, 1, c, r[i]); break;
                    case EqMid:    { const float m = 0.5f * (l[i] + r[i]), d = tick (s, 0, c, m) - m; l[i] += d; r[i] += d; break; }
                    case EqSide:   { const float sd = 0.5f * (l[i] - r[i]), d = tick (s, 0, c, sd) - sd; l[i] += d; r[i] -= d; break; }
                    default:       l[i] = tick (s, 0, c, l[i]); r[i] = tick (s, 1, c, r[i]); break;
                }
            }
            s.c = target;
            s.flat = ! active;                                                     // the ramp to the plain filter is complete after this block
            if (s.flat) { s.z[0][0] = s.z[0][1] = s.z[1][0] = s.z[1][1] = 0.0f; }
        }
    }

private:
    struct State { BiquadCoefs c; float z[2][2] {}; int mode = EqStereo; bool flat = true; float bz1 = 0.0f, bz2 = 0.0f, env = 0.0f; };
    static float tick (State& s, int ch, const BiquadCoefs& c, float x)           // transposed direct form II
    {
        const float y = c.b0 * x + s.z[ch][0];
        s.z[ch][0] = c.b1 * x - c.a1 * y + s.z[ch][1];
        s.z[ch][1] = c.b2 * x - c.a2 * y;
        return y;
    }
    // the detector of a dynamic band: a band-pass (constant 0 dB peak gain) at the band's frequency on the signal the band works on, a peak follower, and the
    // fraction (0 .. 1) of the band's gain that applies at the level at the end of the block
    float detect (State& s, size_t k, const EqBand& p, const float* l, const float* r, int n)
    {
        const double f = std::clamp ((double) p.freq, 20.0, 0.45 * (double) fs), w0 = 6.283185307179586 * f / (double) fs, al = std::sin (w0) / (2.0 * std::max (0.5, (double) p.q)), a0 = 1.0 + al;
        const float b0 = (float) (al / a0), b2 = -b0, a1 = (float) (-2.0 * std::cos (w0) / a0), a2 = (float) ((1.0 - al) / a0);
        const float atk = std::exp (-1.0f / (std::max (0.5f, p.dynAttackMs) * 0.001f * fs)), rel = std::exp (-1.0f / (std::max (5.0f, p.dynReleaseMs) * 0.001f * fs));
        for (int i = 0; i < n; ++i)
        {
            float x;
            switch (p.mode) { case EqLeft: x = l[i]; break; case EqRight: x = r[i]; break; case EqSide: x = 0.5f * (l[i] - r[i]); break; default: x = 0.5f * (l[i] + r[i]); break; }
            const float y = b0 * x + s.bz1;                                                  // transposed direct form II, b1 = 0
            s.bz1 = -a1 * y + s.bz2; s.bz2 = b2 * x - a2 * y;
            const float a = std::abs (y);
            s.env = a > s.env ? atk * s.env + (1.0f - atk) * a : rel * s.env + (1.0f - rel) * a;
        }
        (void) k;
        const float level = gainToDb (s.env) + 0.0f, ratio = std::max (1.0f, p.dynRatio);
        const float over = p.dynBelow ? p.dynThresholdDb - level : level - p.dynThresholdDb;
        const float amount = std::max (0.0f, over) * (1.0f - 1.0f / ratio);                // dB of the band's gain that applies
        return std::clamp (amount / std::max (0.1f, std::abs (p.gainDb)), 0.0f, 1.0f);
    }

    float fs = 48000.0f;
    std::array<State, kEqBands> st {};
};

//==============================================================================
// Bus compressor (glue): feed-forward, VCA-style, stereo-linked (one gain for both channels, from the louder channel). Threshold, ratio (1.5, 2, 4, 10),
// attack (0.1 .. 30 ms) and release (0.1 .. 1.2 s, or AUTO: a fast and a slow release at once, so short peaks recover quickly and sustained material
// slowly), make-up gain, dry / wet mix (parallel compression) and a sidechain high-pass (off / 60 / 90 / 150 Hz) so the lows do not pump the mix.
// A 6 dB soft knee. The ballistics work on the gain reduction in dB. No latency. Off = bit-exact (the stage fades in and out over 20 ms).
struct BusParams
{
    bool on = false;
    float thresholdDb = -12.0f, makeupDb = 0.0f, mix = 1.0f;
    int ratio = 1, attack = 3, release = 2, sidechain = 0;       // indexes into the tables below
};
constexpr int kBusRatios = 4, kBusAttacks = 6, kBusReleases = 5, kBusSidechains = 4;
inline float busRatioValue (int i)   { static const float v[kBusRatios] = { 1.5f, 2.0f, 4.0f, 10.0f }; return v[std::clamp (i, 0, kBusRatios - 1)]; }
inline float busAttackMs (int i)     { static const float v[kBusAttacks] = { 0.1f, 0.3f, 1.0f, 3.0f, 10.0f, 30.0f }; return v[std::clamp (i, 0, kBusAttacks - 1)]; }
inline float busReleaseMs (int i)    { static const float v[kBusReleases] = { 100.0f, 300.0f, 600.0f, 1200.0f, 0.0f }; return v[std::clamp (i, 0, kBusReleases - 1)]; }   // 0 = auto
inline float busSidechainHz (int i)  { static const float v[kBusSidechains] = { 0.0f, 60.0f, 90.0f, 150.0f }; return v[std::clamp (i, 0, kBusSidechains - 1)]; }

class BusCompressor
{
public:
    std::atomic<float> grDb { 0.0f };                 // the gain reduction now (dB, <= 0), for the meter

    void prepare (float sampleRate) { fs = sampleRate; reset(); }
    void reset() { grFast = grSlow = 0.0f; act = 0.0f; for (auto& h : hp) h = Hp(); grDb.store (0.0f); }

    void process (float* l, float* r, int n, const BusParams& p)
    {
        if (! p.on && act <= 0.0f) { grFast = grSlow = 0.0f; grDb.store (0.0f, std::memory_order_relaxed); return; }          // off: untouched
        const float ratioSlope = 1.0f / busRatioValue (p.ratio) - 1.0f;                                                       // <= 0
        const float atk = std::exp (-1.0f / (busAttackMs (p.attack) * 0.001f * fs));
        const float relMs = busReleaseMs (p.release);
        const bool autoRel = relMs <= 0.0f;
        const float relC = std::exp (-1.0f / ((autoRel ? 150.0f : relMs) * 0.001f * fs)), relSlow = std::exp (-1.0f / (1.5f * fs));
        const float makeup = dbToGain (p.makeupDb), actInc = 1.0f / (0.02f * fs), knee = 6.0f;
        const float scHz = busSidechainHz (p.sidechain);
        if (scHz != hpHz) { hpHz = scHz; if (scHz > 0.0f) for (auto& h : hp) h.set (scHz, fs); }
        float minGr = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            act = p.on ? std::min (1.0f, act + actInc) : std::max (0.0f, act - actInc);
            float dl = l[i], dr = r[i];
            if (scHz > 0.0f) { dl = hp[0].tick (dl); dr = hp[1].tick (dr); }
            const float level = gainToDb (std::max (std::abs (dl), std::abs (dr)));
            const float over = level - p.thresholdDb;
            float red = 0.0f;                                                       // the reduction in dB (>= 0)
            if (2.0f * over > knee) red = -ratioSlope * over;
            else if (2.0f * over > -knee) { const float x = over + knee * 0.5f; red = -ratioSlope * x * x / (2.0f * knee); }
            // ballistics
            grFast = red > grFast ? atk * grFast + (1.0f - atk) * red : relC * grFast + (1.0f - relC) * red;
            float gr;
            if (autoRel)
            {
                grSlow = red > grSlow ? atk * grSlow + (1.0f - atk) * red : relSlow * grSlow + (1.0f - relSlow) * red;
                gr = 0.5f * (grFast + grSlow);                                              // a quick partial recovery, then a slow tail
            }
            else gr = grFast;
            const float g = dbToGain (-gr) * makeup;
            const float wl = l[i] * g, wr = r[i] * g, m = p.mix * act;
            l[i] = l[i] + (wl - l[i]) * m;
            r[i] = r[i] + (wr - r[i]) * m;
            minGr = std::min (minGr, -gr);
        }
        grDb.store (minGr, std::memory_order_relaxed);
    }

private:
    struct Hp       // 2-pole Butterworth high-pass (the sidechain filter)
    {
        float b0 = 1, b1 = -2, b2 = 1, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void set (float hz, float fs)
        {
            const double K = std::tan (3.14159265358979 * (double) hz / (double) fs), q = 0.70710678, a0 = 1.0 + K / q + K * K;
            b0 = (float) (1.0 / a0); b1 = -2.0f * b0; b2 = b0;
            a1 = (float) (2.0 * (K * K - 1.0) / a0); a2 = (float) ((1.0 - K / q + K * K) / a0);
        }
        float tick (float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    };
    float fs = 48000.0f, grFast = 0.0f, grSlow = 0.0f, act = 0.0f, hpHz = -1.0f;
    Hp hp[2];
};

//==============================================================================
// Dither: quantises to 16 / 20 / 24 bit with TPDF dither and optional noise shaping (0 flat, 1 gentle, 2 aggressive)
class Dither
{
public:
    void reset() { e1[0] = e1[1] = e2[0] = e2[1] = 0.0f; }
    float process (int ch, float x, int bits, int shape)
    {
        if (bits <= 0) return x;
        const float lsb = std::ldexp (1.0f, 1 - bits);
        const float c1 = shape == 1 ? 0.5f : shape == 2 ? 1.5f : 0.0f, c2 = shape == 2 ? -0.6f : 0.0f;
        const float v = x - c1 * e1[ch] - c2 * e2[ch];
        const float d = (rnd() - rnd()) * lsb;                                  // triangular noise, +-1 LSB
        const float q = std::round ((v + d) / lsb) * lsb;
        e2[ch] = e1[ch]; e1[ch] = q - v;
        return q;
    }
private:
    float rnd() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s & 0xffffff) / 16777216.0f; }
    uint32_t s = 0x2545f491u;
    float e1[2] {}, e2[2] {};
};
} // namespace ms1
