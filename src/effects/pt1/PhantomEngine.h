// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

// PT-1 Phantom: a four-voice harmonizer.
//   Every voice shifts the playing by an INTERVAL (-24 .. +24 semitones), detuned by up to +-50 cents, at its own LEVEL and pan.
//   FIXED mode shifts by exactly that interval.  INTELLIGENT mode listens to the playing (pitch detection), adds the interval and snaps the
//   result to the nearest note of the chosen SCALE in the chosen KEY: the harmonies stay in key (a third above is major or minor as the
//   scale needs) and in tune, whatever you play.
// Header-only, no JUCE dependency, no allocation after prepare().
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

namespace pt1
{
constexpr float kPi = 3.14159265358979f;
constexpr int kNumVoices = 4, kNumScales = 6, kNumChords = 5;

inline const char* noteName (int midi) { static const char* n[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; return n[((midi % 12) + 12) % 12]; }
inline const char* scaleName (int s) { static const char* n[kNumScales] = { "MAJOR", "MINOR", "MAJ PENT", "MIN PENT", "DORIAN", "WHOLE" }; return n[std::clamp (s, 0, kNumScales - 1)]; }
inline const char* chordName (int c) { static const char* n[kNumChords] = { "TRIAD", "FIFTHS", "SUS", "CLUSTER", "OCTAVES" }; return n[std::clamp (c, 0, kNumChords - 1)]; }
inline int chordSemis (int chord, int voice)
{
    static const int c[kNumChords][kNumVoices] = { { 4, 7, 12, -5 }, { 7, -5, 12, -12 }, { 5, 7, 12, -7 }, { 1, 2, 3, -1 }, { 12, -12, 24, -24 } };
    return c[std::clamp (chord, 0, kNumChords - 1)][std::clamp (voice, 0, kNumVoices - 1)];
}
inline int intervalSemis (float v) { return (int) std::lround ((std::clamp (v, 0.0f, 1.0f) - 0.5f) * 48.0f); }    // -24 .. +24
inline float intervalValue (int semis) { return 0.5f + (float) semis / 48.0f; }
inline float detuneCents (float v) { return (std::clamp (v, 0.0f, 1.0f) - 0.5f) * 100.0f; }                      // -50 .. +50

inline bool inScale (int pitchClass, int scale)
{
    static const int sc[kNumScales][7] = { { 0, 2, 4, 5, 7, 9, 11 }, { 0, 2, 3, 5, 7, 8, 10 }, { 0, 2, 4, 7, 9, -1, -1 },
                                           { 0, 3, 5, 7, 10, -1, -1 }, { 0, 2, 3, 5, 7, 9, 10 }, { 0, 2, 4, 6, 8, 10, -1 } };
    for (int i = 0; i < 7; ++i) if (sc[std::clamp (scale, 0, kNumScales - 1)][i] == pitchClass) return true;
    return false;
}
// nearest note of the key's scale (ties go down)
inline int snapToScale (float midi, int root, int scale)
{
    const int m = (int) std::lround (midi);
    for (int d = 0; d <= 6; ++d)
        for (int sgn : { -1, 1 })
        {
            const int c = m + sgn * d;
            if (inScale ((((c - root) % 12) + 12) % 12, scale)) return c;
        }
    return m;
}

// two read heads half a window apart on a delay line, Hann windows that add up to 1
struct Shifter
{
    std::vector<float> buf;
    int mask = 0;
    int64_t w = 0;
    float ph = 0.0f, win = 2000.0f;
    void init (float fs)
    {
        win = 0.04f * fs;
        int n = 16; while ((float) n < win + 16.0f) n <<= 1;
        buf.assign ((size_t) n, 0.0f); mask = n - 1; w = 0; ph = 0.0f;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); ph = 0.0f; }
    float read (float d) const
    {
        const int i = (int) d;
        const float f = d - (float) i;
        const float a = buf[(size_t) ((w - 1 - i) & mask)], b = buf[(size_t) ((w - 2 - i) & mask)];
        return a + (b - a) * f;
    }
    float process (float x, float ratio)
    {
        buf[(size_t) (w & mask)] = x; ++w;
        ph += (1.0f - ratio) / win;
        ph -= std::floor (ph);
        float ph2 = ph + 0.5f; ph2 -= std::floor (ph2);
        const float s1 = std::sin (kPi * ph), s2 = std::sin (kPi * ph2);
        return read (ph * win) * s1 * s1 + read (ph2 * win) * s2 * s2;
    }
};

struct Params
{
    bool intelligent = true;
    int root = 0, scale = 0;
    int interval[kNumVoices] = { 4, 7, 12, -5 };                 // semitones
    float detune[kNumVoices] = { 6.0f, -6.0f, 8.0f, -8.0f };     // cents
    float level[kNumVoices] = { 0.7f, 0.6f, 0.5f, 0.5f };         // 0..1
    float spread = 0.7f;
};

class Engine
{
public:
    std::atomic<float> detected { -1.0f };                       // the note heard now (MIDI, -1 = none)
    std::atomic<float> voiceNote[kNumVoices];                    // the note each voice plays now (MIDI, -1 = none)

    void prepare (float sampleRate)
    {
        fs = sampleRate;
        dec = std::max (1, (int) std::lround (fs / 12000.0f));
        fsd = fs / (float) dec;
        for (auto& s : shifter) s.init (fs);
        reset();
    }

    void reset()
    {
        for (auto& s : shifter) s.clear();
        std::fill (std::begin (dbuf), std::end (dbuf), 0.0f);
        dw = 0; decAcc = 0.0f; decCount = 0; sinceAnalysis = 0; noteSm = 60.0f; voicedHold = 0; havePitch = false; dryG = 0.7f; wetG = 0.7f;
        for (int v = 0; v < kNumVoices; ++v) { ratio[v] = 1.0f; ratioT[v] = 1.0f; voiceNote[v].store (-1.0f); }
        detected.store (-1.0f);
    }

    void process (float* l, float* r, int n, const Params& p, float mix, float levelGain)
    {
        const float dryT = std::cos (mix * 0.5f * kPi), wetT = std::sin (mix * 0.5f * kPi) * levelGain;
        const float smooth = 1.0f - std::exp (-1.0f / (0.03f * fs));
        for (int i = 0; i < n; ++i)
        {
            const float mono = 0.5f * (l[i] + r[i]);
            decAcc += mono;
            if (++decCount >= dec)                                   // decimated input for the pitch detector
            {
                dbuf[dw & 1023] = decAcc / (float) dec; ++dw; decAcc = 0.0f; decCount = 0;
                if (++sinceAnalysis >= 128) { sinceAnalysis = 0; analyse(); }
            }
            if ((i & 15) == 0)
            {
                dryG += 0.1f * (dryT - dryG); wetG += 0.1f * (wetT - wetG);
                for (int v = 0; v < kNumVoices; ++v) setTarget (p, v);
            }
            float wl = 0.0f, wr = 0.0f;
            for (int v = 0; v < kNumVoices; ++v)
            {
                ratio[v] += (ratioT[v] - ratio[v]) * smooth;
                const float y = shifter[v].process (mono, ratio[v]) * p.level[v];
                const float pan = (((float) v / 3.0f) * 2.0f - 1.0f) * ((v & 1) ? -1.0f : 1.0f) * p.spread;       // spread the voices over the stereo field
                wl += y * std::cos ((pan + 1.0f) * kPi * 0.25f);
                wr += y * std::sin ((pan + 1.0f) * kPi * 0.25f);
            }
            l[i] = l[i] * dryG + 1.5f * std::tanh (wl / 1.5f) * wetG;
            r[i] = r[i] * dryG + 1.5f * std::tanh (wr / 1.5f) * wetG;
        }
    }

private:
    void setTarget (const Params& p, int v)
    {
        const float cents = p.detune[v];
        if (! p.intelligent)
        {
            ratioT[v] = std::pow (2.0f, ((float) p.interval[v] + cents * 0.01f) / 12.0f);
            voiceNote[v].store (havePitch ? noteSm + (float) p.interval[v] : -1.0f, std::memory_order_relaxed);
            return;
        }
        if (! havePitch) return;                                    // nothing heard: keep the last harmony
        const int target = snapToScale (noteSm + (float) p.interval[v], p.root, p.scale);
        ratioT[v] = std::pow (2.0f, ((float) target - noteSm + cents * 0.01f) / 12.0f);
        voiceNote[v].store ((float) target, std::memory_order_relaxed);
    }

    // pitch detection (YIN) on the decimated signal: the last 512 samples, lags for 70 Hz .. 1 kHz
    void analyse()
    {
        constexpr int W = 512;
        const int tauMin = std::max (2, (int) (fsd / 1000.0f)), tauMax = std::min (400, (int) (fsd / 70.0f));
        float x[W + 400];
        double energy = 0.0;
        for (int j = 0; j < W + tauMax; ++j) x[j] = dbuf[(dw - (W + tauMax) + j) & 1023];
        for (int j = 0; j < W; ++j) energy += (double) x[j] * x[j];
        if (std::sqrt (energy / W) < 0.004)                          // too quiet
        {
            if (voicedHold > 0 && --voicedHold == 0) { havePitch = false; detected.store (-1.0f); }
            return;
        }
        float d[401], cm[401];
        d[0] = 0.0f; cm[0] = 1.0f;
        float run = 0.0f;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            float s = 0.0f;
            for (int j = 0; j < W; ++j) { const float a = x[j] - x[j + tau]; s += a * a; }
            d[tau] = s;
            run += s;
            cm[tau] = run > 0.0f ? s * (float) tau / run : 1.0f;
        }
        int best = -1;
        for (int tau = tauMin; tau < tauMax; ++tau)
            if (cm[tau] < 0.15f)
            {
                while (tau + 1 < tauMax && cm[tau + 1] < cm[tau]) ++tau;
                best = tau; break;
            }
        if (best < 0)
        {
            float mn = 1.0f;
            for (int tau = tauMin; tau < tauMax; ++tau) if (cm[tau] < mn) { mn = cm[tau]; best = tau; }
            if (mn > 0.3f) best = -1;
        }
        if (best < 0) { if (voicedHold > 0 && --voicedHold == 0) { havePitch = false; detected.store (-1.0f); } return; }
        float tauF = (float) best;                                    // parabolic interpolation
        if (best > 1 && best < tauMax)
        {
            const float a = cm[best - 1], b = cm[best], c = cm[best + 1];
            const float den = a - 2.0f * b + c;
            if (std::fabs (den) > 1.0e-9f) tauF += 0.5f * (a - c) / den;
        }
        const float f = fsd / tauF;
        const float note = 69.0f + 12.0f * std::log2 (f / 440.0f);
        if (! havePitch || std::fabs (note - noteSm) > 1.2f) noteSm = note;     // a new note: jump
        else noteSm += 0.25f * (note - noteSm);                                  // the same note: follow slowly (bends, vibrato)
        havePitch = true; voicedHold = 20;                                       // about 0.2 s of memory
        detected.store (noteSm, std::memory_order_relaxed);
    }

    float fs = 48000.0f, fsd = 12000.0f, decAcc = 0.0f, noteSm = 60.0f, dryG = 0.7f, wetG = 0.7f;
    int dec = 4, decCount = 0, sinceAnalysis = 0, voicedHold = 0;
    int64_t dw = 0;
    bool havePitch = false;
    float dbuf[1024] {};
    float ratio[kNumVoices] {}, ratioT[kNumVoices] {};
    Shifter shifter[kNumVoices];
};
} // namespace pt1
