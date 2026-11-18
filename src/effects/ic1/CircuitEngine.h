// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once
#include "dsp/Interp.h"
#include "dsp/DcBlocker.h"
#include "dsp/SoftClip.h"
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

// IC-1 Infinite Circuit: a feedback network of four nodes. Every node is a delay line followed by a phase-shift stage (two all-passes), a resonant filter (low, band
// or high pass) and a saturator; its output times its own feedback level goes back into the network through a routing matrix:
//   PARALLEL   every node feeds itself (four separate echoes)        SERIES    1 -> 2 -> 3 -> 4 -> 1 (a ring)
//   PING-PONG  1 <-> 2 and 3 <-> 4 (left and right swap each time)   CASCADE   every node feeds itself and the next one
//   MESH       all nodes feed all nodes (a Hadamard matrix, as in a feedback delay network)        STAR   node 1 is a hub
// CROSS blends the routing with a left / right swap. The nodes 1 and 3 take the left channel, 2 and 4 the right one (INPUT: PARALLEL feeds all four, SERIES only
// node 1, SPLIT the nodes 1 and 2). The output is node 1 + node 3 on the left and node 2 + node 4 on the right.
// Every routing matrix has a spectral norm of at most 1, so with REGEN below 100 % the loop gain stays below 1 (the filter's resonance can still ring); the
// saturator keeps the loop bounded whatever happens. SELF-OSC lets REGEN go past 1 (the loop then grows until the saturator holds it: a self-oscillating circuit),
// HOLD mutes the input and sets the loop gain to 1 (an endless sustain), EXCITE injects a short noise burst to start an oscillation.
// No latency, no allocation after prepare(), no locks; the node levels for the display are published in atomics.
namespace ic1
{
constexpr int kNodes = 4, kRoutes = 6;
enum Route { Parallel = 0, Series, PingPong, Cascade, Mesh, Star };
enum Inject { InParallel = 0, InSeries, InSplit };
enum FilterType { LowPass = 0, BandPass, HighPass };
inline const char* routeName (int r) { static const char* n[kRoutes] = { "PARALLEL", "SERIES", "PING-PONG", "CASCADE", "MESH", "STAR" }; return n[juce::jlimit (0, kRoutes - 1, r)]; }
inline const char* injectName (int i) { static const char* n[3] = { "PARALLEL", "SERIES", "SPLIT" }; return n[juce::jlimit (0, 2, i)]; }
inline const char* filterName (int f) { static const char* n[3] = { "LOW PASS", "BAND PASS", "HIGH PASS" }; return n[juce::jlimit (0, 2, f)]; }
constexpr float kTimeRatio[kNodes] = { 1.0f, 1.147f, 1.373f, 1.729f };         // the node delay times relative to TIME at SPREAD 100 % (not harmonically related)

struct Params
{
    int route = Parallel, inject = InParallel, filter = LowPass;
    float timeMs = 300.0f;                       // 5 .. 1500
    float spread = 0.5f;                         // 0 .. 1: the node times move apart
    float level[kNodes] { 0.7f, 0.7f, 0.7f, 0.7f };   // the independent feedback levels 0 .. 1
    float cross = 0.0f;                          // 0 .. 1 cross-feedback between the channels
    float regen = 0.85f;                         // 0 .. 1 loop gain
    bool selfOsc = false, hold = false;
    float cutoff = 4000.0f;                      // 40 .. 16000
    float res = 0.2f;                            // 0 .. 1
    float tilt = 0.0f;                           // 0 .. 1: the cutoffs fan out over the nodes (+-1.5 octaves)
    float phase = 0.0f;                          // 0 .. 1 amount of the all-pass stage
    float rate = 0.3f, depth = 0.0f;             // the modulation: Hz and 0 .. 1
    float drive = 0.0f;                          // 0 .. 1
};

// the feedback matrix R[to][from] of a routing blended with the left / right swap (every entry already includes the normalisation: the spectral norm is at most 1)
inline void routingMatrix (int route, float cross, float R[kNodes][kNodes])
{
    float M[kNodes][kNodes] {};
    switch (juce::jlimit (0, kRoutes - 1, route))
    {
        case Parallel: for (int i = 0; i < kNodes; ++i) M[i][i] = 1.0f; break;
        case Series:   for (int i = 0; i < kNodes; ++i) M[(i + 1) % kNodes][i] = 1.0f; break;
        case PingPong: M[1][0] = M[0][1] = M[3][2] = M[2][3] = 1.0f; break;
        case Cascade:  for (int i = 0; i < kNodes; ++i) { M[i][i] = 0.5f; M[(i + 1) % kNodes][i] = 0.5f; } break;
        case Mesh:
        {
            static const int h[4][4] = { { 1, 1, 1, 1 }, { 1, -1, 1, -1 }, { 1, 1, -1, -1 }, { 1, -1, -1, 1 } };
            for (int i = 0; i < kNodes; ++i) for (int j = 0; j < kNodes; ++j) M[i][j] = 0.5f * (float) h[i][j];
            break;
        }
        case Star: { const float a = 1.0f / std::sqrt (3.0f); for (int j = 1; j < kNodes; ++j) { M[0][j] = a; M[j][0] = a; } break; }
    }
    const float c = juce::jlimit (0.0f, 1.0f, cross);
    for (int i = 0; i < kNodes; ++i)
        for (int j = 0; j < kNodes; ++j) R[i][j] = (1.0f - c) * M[i][j] + c * ((i ^ 1) == j ? 1.0f : 0.0f);                // the swap: 1 <-> 2, 3 <-> 4
}

class Engine
{
public:
    void prepare (float sampleRate)
    {
        fs = sampleRate;
        bufSize = 1; while (bufSize < (int) (2.4f * fs)) bufSize <<= 1;
        for (auto& n : nodes) n.buf.assign ((size_t) bufSize, 0.0f);
        mixSmooth.reset ((double) fs, 0.02); levelSmooth.reset ((double) fs, 0.02); powerSmooth.reset ((double) fs, 0.02);
        reset();
    }
    void reset()
    {
        for (auto& n : nodes) { std::fill (n.buf.begin(), n.buf.end(), 0.0f); n.w = 0; n.ic1 = n.ic2 = 0.0f; n.ap[0] = n.ap[1] = 0.0f; n.dc.reset(); n.fb = 0.0f; n.y = 0.0f; n.meter = 0.0f; }
        lfo = 0.0; exciteLeft = 0; excitePending.store (false); smoothersFresh = true;
        for (auto& m : levelPub) m.store (0.0f);
        rng.setSeed (99);
    }

    void requestExcite() noexcept { excitePending.store (true); }          // a short noise burst into all nodes (starts a self-oscillation)
    float nodeLevel (int i) const noexcept { return levelPub[(size_t) juce::jlimit (0, kNodes - 1, i)].load (std::memory_order_relaxed); }

    // in place on l / r. mix 0 .. 1, levelGain linear, power true = on
    void process (float* l, float* r, int n, const Params& p, float mix, float levelGain, bool power)
    {
        if (smoothersFresh) { mixSmooth.setCurrentAndTargetValue (mix); levelSmooth.setCurrentAndTargetValue (levelGain); powerSmooth.setCurrentAndTargetValue (power ? 1.0f : 0.0f); smoothersFresh = false; }
        mixSmooth.setTargetValue (mix); levelSmooth.setTargetValue (levelGain); powerSmooth.setTargetValue (power ? 1.0f : 0.0f);
        if (excitePending.exchange (false)) exciteLeft = (int) (0.006f * fs);

        float R[kNodes][kNodes];
        routingMatrix (p.route, p.cross, R);
        float times[kNodes], inGain[kNodes] {};
        for (int i = 0; i < kNodes; ++i)
            times[i] = juce::jlimit (2.0f, (float) bufSize - 8.0f, p.timeMs * (1.0f + p.spread * (kTimeRatio[i] - 1.0f)) * 0.001f * fs);
        if (p.inject == InParallel) for (int i = 0; i < kNodes; ++i) inGain[i] = 0.7f;
        else if (p.inject == InSeries) inGain[0] = 0.7f;
        else { inGain[0] = 0.7f; inGain[1] = 0.7f; }
        const float regen = p.hold ? 1.0f : p.selfOsc ? p.regen * 1.15f : p.regen * 0.97f;
        const float k = 1.0f + 5.0f * p.drive;                                       // the saturator: tanh (k x) / k, unity for small signals
        const float Q = 0.55f + p.res * p.res * 10.0f, damp = 1.0f / Q;
        const double lfoStep = juce::MathConstants<double>::twoPi * (double) p.rate / (double) fs;
        const float modMs = 0.5f + 6.0f * p.depth, modOct = 0.9f * p.depth;
        const float apFc[kNodes] = { 300.0f, 520.0f, 900.0f, 1600.0f };

        for (int s = 0; s < n; ++s)
        {
            lfo += lfoStep; if (lfo > juce::MathConstants<double>::twoPi) lfo -= juce::MathConstants<double>::twoPi;
            const float inL = l[s], inR = r[s];
            const float mono = 0.5f * (inL + inR);
            float exc = 0.0f; if (exciteLeft > 0) { exc = 0.5f * (rng.nextFloat() * 2.0f - 1.0f); --exciteLeft; }
            float yOut[kNodes], fbOld[kNodes], fbNew[kNodes];
            for (int j = 0; j < kNodes; ++j) fbOld[j] = nodes[(size_t) j].fb;                  // every node reads the feedback of the same (previous) sample: the matrix norm holds
            for (int i = 0; i < kNodes; ++i)
            {
                auto& nd = nodes[(size_t) i];
                float fb = 0.0f;
                for (int j = 0; j < kNodes; ++j) fb += R[i][j] * fbOld[j];
                const float src = p.inject == InSeries ? mono : ((i & 1) ? inR : inL);
                const float u = (p.hold ? 0.0f : inGain[i] * src) + regen * fb + exc;
                nd.buf[(size_t) nd.w] = u;

                // the delay, with the modulation
                const float ph = (float) lfo + (float) i * 1.5708f;
                float t = times[i] + modMs * 0.001f * fs * std::sin (ph) * p.depth;
                t = juce::jlimit (2.0f, (float) bufSize - 8.0f, t);
                const float rp = (float) nd.w - t;
                const int i0 = (int) std::floor (rp); const float fr = rp - (float) i0;
                const float a = nd.buf[(size_t) (i0 & (bufSize - 1))], b = nd.buf[(size_t) ((i0 + 1) & (bufSize - 1))];
                float d = vibr::dsp::lerp (a, b, fr);
                nd.w = (nd.w + 1) & (bufSize - 1);

                // the phase-shift stage: two first-order all-passes, blended in by PHASE
                if (p.phase > 0.0f)
                {
                    const float tg = std::tan (juce::MathConstants<float>::pi * std::min (0.45f * fs, apFc[i]) / fs), c = (tg - 1.0f) / (tg + 1.0f);
                    float x = d;
                    for (int q = 0; q < 2; ++q) { const float yv = c * x + nd.ap[q]; nd.ap[q] = x - c * yv; x = yv; }
                    d = d * (1.0f - p.phase) + x * p.phase;
                }

                // the resonant filter (a state variable filter), its cutoff fanned out by TILT and moved by the modulation
                const float oct = p.tilt * ((float) i - 1.5f) + modOct * std::sin (ph + 0.9f);          // TILT: -1.5 .. +1.5 octaves over the four nodes
                const float fc = juce::jlimit (20.0f, 0.45f * fs, p.cutoff * std::exp2 (oct));
                const float g = std::tan (juce::MathConstants<float>::pi * fc / fs), a1 = 1.0f / (1.0f + g * (g + damp)), a2 = g * a1, a3 = g * a2;
                const float v3 = d - nd.ic2, v1 = a1 * nd.ic1 + a2 * v3, v2 = nd.ic2 + a2 * nd.ic1 + a3 * v3;
                nd.ic1 = 2.0f * v1 - nd.ic1; nd.ic2 = 2.0f * v2 - nd.ic2;
                float y = p.filter == LowPass ? v2 : p.filter == BandPass ? damp * v1 : d - damp * v1 - v2;

                // a DC blocker, then the saturator and the independent feedback level
                y = nd.dc.process (y, 0.9995f);
                nd.y = y;
                const float sat = std::tanh (k * y) / k;
                fbNew[i] = sat * (p.hold ? 1.0f : p.level[i]);
                yOut[i] = y;
                nd.meter = std::max (std::fabs (sat), nd.meter * 0.9995f);
            }
            for (int i = 0; i < kNodes; ++i) nodes[(size_t) i].fb = fbNew[i];
            const float wetL = 0.7f * (yOut[0] + yOut[2]), wetR = 0.7f * (yOut[1] + yOut[3]);
            const float m = mixSmooth.getNextValue() * powerSmooth.getNextValue(), lv = levelSmooth.getNextValue(), pw = powerSmooth.getCurrentValue(), gl = 1.0f + (lv - 1.0f) * pw;
            l[s] = (inL * (1.0f - m) + softClip (wetL) * m) * gl;
            r[s] = (inR * (1.0f - m) + softClip (wetR) * m) * gl;
        }
        for (int i = 0; i < kNodes; ++i) levelPub[(size_t) i].store (nodes[(size_t) i].meter, std::memory_order_relaxed);
    }

private:
    struct Node
    {
        std::vector<float> buf;
        int w = 0;
        float ic1 = 0.0f, ic2 = 0.0f, ap[2] { 0.0f, 0.0f }, fb = 0.0f, y = 0.0f, meter = 0.0f; vibr::dsp::DcBlocker dc;
    };
    static float softClip (float x) noexcept { return vibr::dsp::softClip (x); }

    float fs = 48000.0f;
    int bufSize = 1 << 17;
    std::array<Node, kNodes> nodes;
    double lfo = 0.0;
    int exciteLeft = 0;
    bool smoothersFresh = true;
    juce::Random rng { 99 };
    juce::SmoothedValue<float> mixSmooth, levelSmooth, powerSmooth;
    std::atomic<bool> excitePending { false };
    std::array<std::atomic<float>, kNodes> levelPub {};
};
} // namespace ic1
