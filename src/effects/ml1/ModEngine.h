// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

// ML-1 Modulation Lab: four classic modulation effects in one plugin, all driven by one LFO.
//   CHORUS    2 / 3 / 4 voices of a short (11 ms +) delay, each swept by the LFO a fraction of a cycle apart; the dry sound is added.
//             WIDTH sets how far the right channel's LFO runs behind the left one (stereo spread).
//   VIBRATO   one delay swept around 4 ms and heard alone: a pure pitch wobble (LFO SINE or TRIANGLE). WIDTH as above.
//   FLANGER   one very short delay (0.3 .. 6 ms) added to the dry sound, with FEEDBACK around the delay (POSITIVE or NEGATIVE: the negative one sounds hollow).
//   PHASER    4 / 6 / 8 all-pass stages swept by the LFO and added to the dry sound, with FEEDBACK around the chain.
// RATE is a frequency range per mode, or (SYNC) a note length of the host tempo. MIX blends the effect with the dry sound; LEVEL is the output gain.
// Header-only, no JUCE dependency, no allocation after prepare().
#include <algorithm>
#include <atomic>
#include <cmath>
#include <vector>

namespace ml1
{
constexpr float kPi = 3.14159265358979f, kTwoPi = 6.28318530717959f;
constexpr int kNumModes = 4, kNumDivisions = 6;
enum Mode { Chorus, Vibrato, Flanger, Phaser };

inline int clampMode (int m) { return std::clamp (m, 0, kNumModes - 1); }
inline const char* modeName (int m)
{
    static const char* n[kNumModes] = { "CHORUS", "VIBRATO", "FLANGER", "PHASER" };
    return n[clampMode (m)];
}
inline int variantCount (int mode) { return mode == Vibrato || mode == Flanger ? 2 : 3; }
inline const char* variantTitle (int mode)
{
    static const char* n[kNumModes] = { "VOICES", "LFO WAVE", "FEEDBACK", "STAGES" };
    return n[clampMode (mode)];
}
inline const char* variantName (int mode, int v)
{
    static const char* n[kNumModes][3] = { { "2 VOICES", "3 VOICES", "4 VOICES" }, { "SINE", "TRIANGLE", "" },
                                           { "POSITIVE", "NEGATIVE", "" },        { "4 STAGES", "6 STAGES", "8 STAGES" } };
    return n[clampMode (mode)][std::clamp (v, 0, 2)];
}
inline const char* extraName (int mode) { return mode >= Flanger ? "FEEDBACK" : "WIDTH"; }
inline int voiceCount (int variant) { return 2 + std::clamp (variant, 0, 2); }
inline int stageCount (int variant) { return 4 + 2 * std::clamp (variant, 0, 2); }

// tempo sync: one LFO cycle lasts this many beats (a beat = a quarter note)
inline const char* divisionName (int i)
{
    static const char* n[kNumDivisions] = { "1 BAR", "1/2", "1/4", "1/8", "1/8T", "1/16" };
    return n[std::clamp (i, 0, kNumDivisions - 1)];
}
inline int divisionIndex (float v) { return std::clamp ((int) (v * (float) kNumDivisions), 0, kNumDivisions - 1); }
inline float divisionHz (int i, float bpm)
{
    static const float beats[kNumDivisions] = { 4.0f, 2.0f, 1.0f, 0.5f, 1.0f / 3.0f, 0.25f };
    return std::max (20.0f, bpm) / 60.0f / beats[std::clamp (i, 0, kNumDivisions - 1)];
}

// LFO rate range per mode (Hz), exponential over the knob
inline float rateHz (int mode, float v)
{
    static const float lo[kNumModes] = { 0.1f, 0.5f, 0.03f, 0.03f }, hi[kNumModes] = { 6.0f, 10.0f, 6.0f, 8.0f };
    const int m = clampMode (mode);
    return lo[m] * std::pow (hi[m] / lo[m], std::clamp (v, 0.0f, 1.0f));
}

// LFO shape: phase 0..1 -> -1..+1
inline float lfoShape (float phase, bool triangle)
{
    phase -= std::floor (phase);
    if (triangle) return phase < 0.5f ? 4.0f * phase - 1.0f : 3.0f - 4.0f * phase;
    return std::sin (kTwoPi * phase);
}

// what the sweep does at one LFO value (shared with the editor's picture)
inline float chorusDelayMs (int voice, float lfo, float depth)  { return 11.0f + 2.3f * (float) voice + lfo * (0.4f + 4.6f * depth); }
inline float vibratoDelayMs (float lfo, float depth)            { return 4.0f + lfo * 3.6f * depth; }
inline float flangerDelayMs (float lfo, float depth)            { const float u = 0.5f + 0.5f * lfo; return 0.3f + (0.7f + 5.0f * depth) * u * u; }
inline float phaserCentreHz (float lfo, float depth)            { return 120.0f * std::pow (60.0f, 0.5f + 0.5f * lfo * (0.2f + 0.8f * depth)); }
inline float feedbackAmount (float extra)                       { return 0.9f * std::clamp (extra, 0.0f, 1.0f); }

struct Params
{
    int mode = Chorus, variant = 0;
    bool sync = false;
    float bpm = 120.0f;
    float rate = 0.4f, depth = 0.5f, extra = 0.5f, mix = 1.0f;       // 0 .. 1
};

class Engine
{
public:
    // what the editor shows (written by the audio thread, read by the message thread)
    std::atomic<float> visPhase { 0.0f }, visHz { 1.0f };

    void prepare (float sampleRate)
    {
        fs = sampleRate;
        size = 1; while ((float) size < 0.04f * fs + 8.0f) size <<= 1;
        mask = size - 1;
        for (auto& b : line) b.assign ((size_t) size, 0.0f);
        reset();
    }

    void reset()
    {
        for (auto& b : line) std::fill (b.begin(), b.end(), 0.0f);
        for (auto& c : ap) for (auto& z : c) z = 0.0f;
        w = 0; phase = 0.0f;
        fbIn[0] = fbIn[1] = fbLp[0] = fbLp[1] = fbDc[0] = fbDc[1] = 0.0f;
        rateS = -1.0f; mixS = levelS = depthS = extraS = -1.0f; fade = 1.0f; lastMode = -1;
    }

    void process (float* l, float* r, int n, const Params& p, float levelGain)
    {
        if (p.mode != lastMode)
        {
            if (lastMode >= 0) { fade = 0.0f; for (auto& c : ap) for (auto& z : c) z = 0.0f; fbIn[0] = fbIn[1] = fbLp[0] = fbLp[1] = fbDc[0] = fbDc[1] = 0.0f; }
            lastMode = p.mode;
        }
        const float fadeInc = 1.0f / (0.03f * fs), sm = 1.0f - std::exp (-1.0f / (0.02f * fs));
        const float hz = p.sync ? divisionHz (divisionIndex (p.rate), p.bpm) : rateHz (p.mode, p.rate);
        if (rateS < 0.0f) { rateS = hz; mixS = p.mix; levelS = levelGain; depthS = p.depth; extraS = p.extra; }

        const bool triangle = p.mode == Vibrato && p.variant == 1;
        const bool negative = p.mode == Flanger && p.variant == 1;
        const int voices = voiceCount (p.variant), stages = stageCount (p.variant);
        const float msToSamples = 0.001f * fs;
        const float dcA = 1.0f - std::exp (-kTwoPi * 40.0f / fs), lpA = 1.0f - std::exp (-kTwoPi * 9000.0f / fs);

        for (int i = 0; i < n; ++i)
        {
            rateS += (hz - rateS) * sm * 0.5f;
            mixS += (p.mix - mixS) * sm; levelS += (levelGain - levelS) * sm;
            depthS += (p.depth - depthS) * sm; extraS += (p.extra - extraS) * sm;
            fade = std::min (1.0f, fade + fadeInc);
            phase += rateS / fs; if (phase >= 1.0f) phase -= 1.0f;
            const float xl = l[i], xr = r[i];
            const float x[2] = { xl, xr };
            float y[2];

            // right-channel LFO offset: WIDTH (chorus, vibrato) or a fixed quarter-turn (flanger, phaser)
            const float offset = p.mode <= Vibrato ? 0.5f * extraS : 0.25f;

            for (int c = 0; c < 2; ++c)
            {
                const float ph = phase + (c == 0 ? 0.0f : offset);
                if (p.mode == Chorus)
                {
                    float sum = 0.0f;
                    for (int v = 0; v < voices; ++v)
                        sum += tap (c, chorusDelayMs (v, lfoShape (ph + (float) v / (float) voices, false), depthS) * msToSamples);
                    line[c][(size_t) w] = x[c];
                    y[c] = 0.6f * (x[c] + sum / (float) voices);
                }
                else if (p.mode == Vibrato)
                {
                    const float d = vibratoDelayMs (lfoShape (ph, triangle), depthS) * msToSamples;
                    line[c][(size_t) w] = x[c];
                    y[c] = tap (c, d);
                }
                else if (p.mode == Flanger)
                {
                    const float fb = feedbackAmount (extraS) * (negative ? -1.0f : 1.0f);
                    const float d = flangerDelayMs (lfoShape (ph, false), depthS) * msToSamples;
                    const float wet = tap (c, d);
                    fbLp[c] += lpA * (wet - fbLp[c]);                                  // loop damping
                    fbDc[c] += dcA * (fbLp[c] - fbDc[c]);                              // keep DC out of the loop
                    line[c][(size_t) w] = x[c] + fb * softClip (fbLp[c] - fbDc[c]);
                    y[c] = 0.5f * (x[c] + wet) * (1.0f - 0.6f * std::abs (fb));
                }
                else
                {
                    const float fb = feedbackAmount (extraS) * 0.95f;
                    const float fc = std::min (phaserCentreHz (lfoShape (ph, false), depthS), 0.45f * fs);
                    const float tn = std::tan (kPi * fc / fs), a = (tn - 1.0f) / (tn + 1.0f);
                    float v = x[c] + fb * softClip (fbIn[c]);
                    for (int k = 0; k < stages; ++k)
                    {
                        const float o = a * v + ap[c][k];
                        ap[c][k] = v - a * o;
                        v = o;
                    }
                    fbIn[c] = v;
                    y[c] = 0.5f * (x[c] + v) * (1.0f - 0.5f * fb);
                }
            }
            w = (w + 1) & mask;
            if ((i & 63) == 0) visPhase.store (phase, std::memory_order_relaxed);

            const float m = mixS * fade;
            l[i] = (xl + (y[0] - xl) * m) * levelS;
            r[i] = (xr + (y[1] - xr) * m) * levelS;
        }
        visHz.store (rateS, std::memory_order_relaxed);
    }

private:
    static float softClip (float x) { return std::abs (x) < 1.0f ? x * (1.5f - 0.5f * x * x) : (x > 0.0f ? 1.0f : -1.0f); }     // smooth limit at +-1

    // the line holds the input; w is the next write position, so a delay of d samples reads d samples behind it (4-point Hermite interpolation)
    float tap (int c, float d) const
    {
        d = std::clamp (d, 2.0f, (float) mask - 4.0f);
        const float fi = std::floor (d), fr = d - fi;
        const int i1 = (w - (int) fi) & mask;
        const auto& b = line[c];
        const float ym1 = b[(size_t) ((i1 + 1) & mask)], y0 = b[(size_t) i1], y1 = b[(size_t) ((i1 - 1) & mask)], y2 = b[(size_t) ((i1 - 2) & mask)];
        const float c1 = 0.5f * (y1 - ym1), c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2, c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
        return ((c3 * fr + c2) * fr + c1) * fr + y0;
    }

    float fs = 48000.0f, phase = 0.0f;
    float rateS = -1.0f, mixS = 1.0f, levelS = 1.0f, depthS = 0.5f, extraS = 0.5f, fade = 1.0f;
    float fbIn[2] {}, fbLp[2] {}, fbDc[2] {};
    float ap[2][8] {};
    int size = 1, mask = 0, w = 0, lastMode = -1;
    std::vector<float> line[2];
};
} // namespace ml1
