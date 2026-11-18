// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once
// Helpers of the offline engine tests (the console apps EngineTest.cpp of the new plugins): the check counter, test signals and the measurements (rms, energy,
// strongest frequency, energy of a band). Use with `using namespace vibr::test;` inside the test's anonymous namespace; `fs` is the sample rate of the test.
#include <JuceHeader.h>
#include <cstdio>
#include <vector>

namespace vibr::test
{
inline int failures = 0;
inline float fs = 48000.0f;
using Buf = std::vector<float>;

inline void check (bool ok, const char* what, double a = 0.0, double b = 0.0)
{
    std::printf ("%s  %s", ok ? "ok  " : "FAIL", what);
    if (a != 0.0 || b != 0.0) std::printf ("   (%g, %g)", a, b);
    std::printf ("\n");
    if (! ok) ++failures;
}

inline Buf sine (float hz, int n, float amp = 0.5f, double phase0 = 0.0)
{
    Buf b ((size_t) n);
    for (int i = 0; i < n; ++i) b[(size_t) i] = amp * (float) std::sin (phase0 + 2.0 * juce::MathConstants<double>::pi * hz * i / fs);
    return b;
}
inline Buf noise (int n, float amp, int seed = 5)
{
    juce::Random rng (seed); Buf b ((size_t) n);
    for (auto& v : b) v = amp * (rng.nextFloat() * 2.0f - 1.0f);
    return b;
}
inline double energy (const Buf& b, int from, int to)
{
    double s = 0.0; for (int i = std::max (0, from); i < std::min ((int) b.size(), to); ++i) s += (double) b[(size_t) i] * b[(size_t) i];
    return s;
}
inline double rms (const Buf& b, int from, int to) { return std::sqrt (energy (b, from, to) / std::max (1, to - from)); }

// the spectrum of 16384 samples from `from` (Hann window) as complex bins
inline std::vector<juce::dsp::Complex<float>> spectrum16k (const Buf& b, int from)
{
    constexpr int order = 14, n = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<juce::dsp::Complex<float>> a ((size_t) n), o ((size_t) n);
    for (int i = 0; i < n; ++i) a[(size_t) i] = { b[(size_t) (from + i)] * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) n)), 0.0f };
    fft.perform (a.data(), o.data(), false);
    return o;
}
// frequency of the strongest component (Hann-windowed 16384 FFT with parabolic interpolation)
inline double peakHz (const Buf& b, int from)
{
    constexpr int n = 1 << 14;
    const auto o = spectrum16k (b, from);
    int best = 1; float bv = 0.0f;
    for (int k = 1; k < n / 2; ++k) { const float m = std::abs (o[(size_t) k]); if (m > bv) { bv = m; best = k; } }
    const float m0 = std::abs (o[(size_t) best - 1]), m1 = std::abs (o[(size_t) best]), m2 = std::abs (o[(size_t) best + 1]);
    const float den = m0 - 2.0f * m1 + m2, off = den != 0.0f ? 0.5f * (m0 - m2) / den : 0.0f;
    return ((double) best + off) * fs / n;
}
// energy (rms of the bins) of a band of the spectrum between lo and hi Hz
inline double bandRms (const Buf& b, int from, float lo, float hi)
{
    constexpr int n = 1 << 14;
    const auto o = spectrum16k (b, from);
    double s = 0.0;
    for (int k = (int) (lo * n / fs); k <= (int) (hi * n / fs); ++k) s += (double) std::norm (o[(size_t) k]);
    return std::sqrt (s);
}
}
