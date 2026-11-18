// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

// SR-1 Sympathetic Resonator: virtual strings that ring along with the playing.
//   STRINGS (1 .. 12) tuned to a SCALE in a key (ROOT, FINE tuning): every string is a damped feedback loop (a delay line, a damping
//   filter and a dispersion all-pass) that the input excites, so it rings when the playing contains its note or one of its partials.
//   DECAY is the ring time, DAMPING how fast the highs die, TENSION the stiffness (inharmonic, bell-like partials), MOTION a slow drift
//   of the tuning, TEXTURE the material: WOOD (dull, short), METAL (bright, stiff) or GLASS (pure, long).
// Header-only, no JUCE dependency, no allocation after prepare().
#include <algorithm>
#include <atomic>
#include <cmath>
#include <complex>
#include <cstdint>
#include <vector>

namespace sr1
{
constexpr float kPi = 3.14159265358979f;
constexpr int kMaxStrings = 12, kNumScales = 6, kNumTextures = 3;
enum Texture { Wood, Metal, Glass };

inline const char* noteName (int semitone)
{
    static const char* n[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return n[((semitone % 12) + 12) % 12];
}
inline const char* scaleName (int s)
{
    static const char* n[kNumScales] = { "MAJOR", "MINOR", "MAJ PENT", "MIN PENT", "DORIAN", "WHOLE" };
    return n[std::clamp (s, 0, kNumScales - 1)];
}
inline const char* textureName (int t)
{
    static const char* n[kNumTextures] = { "WOOD", "METAL", "GLASS" };
    return n[std::clamp (t, 0, kNumTextures - 1)];
}
inline int stringCount (float v) { return std::clamp (1 + (int) std::lround (std::clamp (v, 0.0f, 1.0f) * 11.0f), 1, kMaxStrings); }
inline float fineCents (float v) { return (std::clamp (v, 0.0f, 1.0f) - 0.5f) * 100.0f; }                     // -50 .. +50 cents
inline float decaySeconds (float v) { return 0.3f * std::pow (60.0f, std::clamp (v, 0.0f, 1.0f)); }           // 0.3 .. 18 s

// the MIDI note of string i: the scale degrees climb from the root (C3 .. B3) over the octaves
inline int stringNote (int i, int root, int scale)
{
    static const int sc[kNumScales][7] = { { 0, 2, 4, 5, 7, 9, 11 }, { 0, 2, 3, 5, 7, 8, 10 }, { 0, 2, 4, 7, 9, 0, 0 },
                                           { 0, 3, 5, 7, 10, 0, 0 }, { 0, 2, 3, 5, 7, 9, 10 }, { 0, 2, 4, 6, 8, 10, 0 } };
    static const int len[kNumScales] = { 7, 7, 5, 5, 7, 6 };
    const int s = std::clamp (scale, 0, kNumScales - 1);
    return 48 + root + sc[s][i % len[s]] + 12 * (i / len[s]);
}
inline float noteHz (int midi, float cents) { return 440.0f * std::pow (2.0f, ((float) (midi - 69) + cents / 100.0f) / 12.0f); }

struct Params
{
    int root = 0, scale = 2, texture = 1;
    float strings = 0.6f, decay = 0.55f, damping = 0.4f, tension = 0.25f, motion = 0.3f, fine = 0.5f;     // 0..1
};

class Engine
{
public:
    std::atomic<float> energy[kMaxStrings];           // for the editor: how strongly each string rings (0 .. 1)

    void prepare (float sampleRate)
    {
        fs = sampleRate;
        size = 1; while ((float) size < fs / 30.0f + 16.0f) size <<= 1;
        mask = size - 1;
        for (auto& s : str) s.buf.assign ((size_t) size, 0.0f);
        reset();
    }

    void reset()
    {
        for (int i = 0; i < kMaxStrings; ++i)
        {
            auto& s = str[i];
            std::fill (s.buf.begin(), s.buf.end(), 0.0f);
            s.w = 0; s.lp = 0.0f; s.ap1 = s.ap2 = s.x1 = s.x2 = 0.0f; s.env = 0.0f; s.L = 100.0f;
            s.phase = (float) i * 0.37f; s.rate = 0.11f + 0.05f * (float) ((i * 7) % 5);
            energy[i].store (0.0f);
        }
        dryG = 0.7f; wetG = 0.7f; sinceUpdate = 0;
    }

    void process (float* l, float* r, int n, const Params& p, float mix, float levelGain)
    {
        const int N = stringCount (p.strings);
        const float dryT = std::cos (mix * 0.5f * kPi), wetT = std::sin (mix * 0.5f * kPi) * levelGain;
        for (int i = 0; i < n; ++i)
        {
            if ((sinceUpdate++ & 31) == 0) { control (p, N); dryG += 0.2f * (dryT - dryG); wetG += 0.2f * (wetT - wetG); }
            const float x = 0.5f * (l[i] + r[i]);
            float wl = 0.0f, wr = 0.0f;
            for (int k = 0; k < N; ++k)
            {
                String& s = str[k];
                // slow drift of the tuning
                s.phase += s.rate / fs; if (s.phase > 1.0f) s.phase -= 1.0f;
                const float L = s.Lbase * (1.0f + motionDepth * std::sin (2.0f * kPi * s.phase));
                s.L += (L - s.L) * 0.05f;
                const float y = read (s, std::max (4.0f, s.L));
                s.lp += s.damp * (y - s.lp);                                         // damping: one-pole low pass
                const float a = s.disp;                                              // dispersion: two first-order all-passes
                float v = s.lp;
                float o = -a * v + s.x1 + a * s.ap1; s.x1 = v; s.ap1 = o; v = o;
                o = -a * v + s.x2 + a * s.ap2; s.x2 = v; s.ap2 = o; v = o;
                s.buf[(size_t) (s.w & mask)] = x * s.inG + s.g * v;
                ++s.w;
                s.env += (std::fabs (y) - s.env) * 0.0015f;
                wl += y * s.gl; wr += y * s.gr;
            }
            const float norm = 2.0f / std::sqrt ((float) N);
            wl = 1.5f * std::tanh (wl * norm / 1.5f);
            wr = 1.5f * std::tanh (wr * norm / 1.5f);
            l[i] = l[i] * dryG + wl * wetG;
            r[i] = r[i] * dryG + wr * wetG;
        }
        for (int k = 0; k < kMaxStrings; ++k) energy[k].store (k < N ? std::min (1.0f, str[k].env * 8.0f) : 0.0f, std::memory_order_relaxed);
    }

private:
    struct String
    {
        std::vector<float> buf;
        int64_t w = 0;
        float lp = 0, ap1 = 0, ap2 = 0, x1 = 0, x2 = 0, env = 0, L = 100, Lbase = 100, damp = 0.5f, disp = 0, g = 0.99f, inG = 0.05f, gl = 0.7f, gr = 0.7f;
        float phase = 0, rate = 0.1f;
    };

    float read (const String& s, float d) const
    {
        const int i = (int) d;
        const float f = d - (float) i;
        const float a = s.buf[(size_t) ((s.w - 1 - i) & mask)], b = s.buf[(size_t) ((s.w - 2 - i) & mask)];
        return a + (b - a) * f;
    }

    void control (const Params& p, int N)
    {
        // material: damping corner (Hz), stiffness, ring time factor
        static const float texFc[kNumTextures] = { 1800.0f, 9000.0f, 4500.0f }, texDisp[kNumTextures] = { 0.0f, 0.25f, 0.06f }, texDecay[kNumTextures] = { 0.4f, 1.0f, 2.2f }, texGain[kNumTextures] = { 1.0f, 0.8f, 1.3f };   // texGain evens out the level (the tuned, stiffer strings ring harder)
        const int t = std::clamp (p.texture, 0, kNumTextures - 1);
        const float fc = std::min (texFc[t] * std::pow (0.08f, p.damping), 0.45f * fs);
        const float damp = 1.0f - std::exp (-2.0f * kPi * fc / fs);
        const float disp = std::clamp (texDisp[t] + p.tension * 0.6f, 0.0f, 0.82f);
        const float T = decaySeconds (p.decay) * texDecay[t];
        motionDepth = p.motion * 0.0075f;                                              // up to about 13 cents
        for (int k = 0; k < N; ++k)
        {
            String& s = str[k];
            const float f = noteHz (stringNote (k, p.root, p.scale), fineCents (p.fine));
            const float period = fs / f;
            // delay of the damping filter and the two dispersion all-passes at this string's own pitch (phase delay), so the loop is tuned
            // exactly even when the stiffness is high
            const float w = 2.0f * kPi * f / fs;
            const std::complex<float> z1 = std::polar (1.0f, -w);
            const auto lpH = damp / (1.0f - (1.0f - damp) * z1);
            const auto apH = (-disp + z1) / (1.0f - disp * z1);
            const float lpDelay = -std::arg (lpH) / w, apDelay = -2.0f * std::arg (apH) / w;
            s.Lbase = std::clamp (period - lpDelay - apDelay, 4.0f, (float) (size - 8));
            s.damp = damp; s.disp = disp;
            s.g = std::min (0.9995f, std::pow (10.0f, -3.0f * period / (fs * T)));
            s.inG = ((1.0f - s.g) * 3.0f + 0.004f) * texGain[t];                                   // the string is a narrow resonator: drive it hard so the wet level is useful
            const float pan = ((float) k / (float) std::max (1, N - 1) * 2.0f - 1.0f) * 0.8f * (N > 1 ? 1.0f : 0.0f);
            s.gl = std::cos ((pan + 1.0f) * kPi * 0.25f); s.gr = std::sin ((pan + 1.0f) * kPi * 0.25f);
        }
    }

    float fs = 48000.0f, dryG = 0.7f, wetG = 0.7f, motionDepth = 0.0f;
    int size = 1, mask = 0, sinceUpdate = 0;
    String str[kMaxStrings];
};
} // namespace sr1
