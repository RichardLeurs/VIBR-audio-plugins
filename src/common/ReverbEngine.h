// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

// Big Sky style reverb machine: 12 reverb types on one engine.
//   Core: pre-delay -> input diffusion -> 8-line feedback delay network (Householder mix, modulated lines, per-line
//   damping), stereo taps out, then tone filter. The types change the network size, damping, input path and output stage:
//   ROOM, HALL, PLATE, SPRING (dispersive all-passes), SWELL / BLOOM (swelling input), CLOUD (huge and diffuse),
//   CHORALE (formant filters), SHIMMER (octave/5th pitch shift in the feedback), MAGNETO (4-head tape echo into the
//   reverb), NONLINEAR (gate / reverse / decay envelope), REFLECTIONS (multi-tap early reflections only).
// Header-only, no JUCE dependency, no allocation after prepare().
#include <algorithm>
#include <cmath>
#include <vector>

namespace bigsky
{
constexpr float kPi = 3.14159265358979f;
constexpr int kNumTypes = 12;
enum Type { Room, Hall, Plate, Spring, Swell, Bloom, Cloud, Chorale, Shimmer, Magneto, Nonlinear, Reflections };

inline const char* typeName (int t)
{
    static const char* n[kNumTypes] = { "ROOM", "HALL", "PLATE", "SPRING", "SWELL", "BLOOM", "CLOUD", "CHORAL", "SHIMMR", "MAGNTO", "NONLIN", "REFLEC" };
    return n[std::clamp (t, 0, kNumTypes - 1)];
}

// Decay knob (0..1) -> seconds: RT60 of the reverb, or the gate / reflection span for NONLINEAR and REFLECTIONS
inline float decaySeconds (int type, float d)
{
    static const float lo[kNumTypes] = { 0.15f, 0.6f, 0.3f, 0.4f, 1.0f, 1.0f, 2.0f, 0.8f, 1.0f, 0.4f, 0.05f, 0.1f };
    static const float hi[kNumTypes] = { 4.0f, 15.0f, 8.0f, 5.0f, 20.0f, 20.0f, 40.0f, 12.0f, 30.0f, 10.0f, 1.5f, 1.5f };
    const int t = std::clamp (type, 0, kNumTypes - 1);
    return lo[t] * std::pow (hi[t] / lo[t], std::clamp (d, 0.0f, 1.0f));
}
inline float predelayMs (float x) { return 300.0f * x * x; }

struct Params
{
    int type = 0;
    float decay = 0.5f, predelay = 0.0f, mix = 0.5f, tone = 0.5f, p1 = 0.5f, p2 = 0.5f, mod = 0.5f, value = 0.5f;   // 0..1
};

//==============================================================================
struct Delay
{
    std::vector<float> b;
    int mask = 0, w = 0;
    void init (int minSize) { int n = 16; while (n < minSize) n <<= 1; b.assign ((size_t) n, 0.0f); mask = n - 1; w = 0; }
    void clear() { std::fill (b.begin(), b.end(), 0.0f); }
    void push (float x) noexcept { b[(size_t) w] = x; w = (w + 1) & mask; }
    float read (float d) const noexcept          // d = 0 is the most recently pushed sample
    {
        const int i = (int) d;
        const float f = d - (float) i;
        const float a = b[(size_t) ((w - 1 - i) & mask)];
        const float c = b[(size_t) ((w - 2 - i) & mask)];
        return a + (c - a) * f;
    }
};

struct Allpass
{
    Delay d;
    int len = 1;
    float g = 0.6f;
    void init (int l) { len = std::max (1, l); d.init (len + 2); }
    float process (float x) noexcept
    {
        const float y = d.read ((float) (len - 1));
        const float v = x - g * y;
        d.push (v);
        return y + g * v;
    }
};

struct PitchShifter      // two cross-faded taps drifting through a delay line
{
    Delay d;
    float ph = 0.0f, windowSamples = 2048.0f;
    void init (float sr) { windowSamples = 0.06f * sr; d.init ((int) windowSamples + 8); }
    void clear() { d.clear(); ph = 0.0f; }
    float process (float x, float ratio) noexcept
    {
        d.push (x);
        ph += (1.0f - ratio) / windowSamples;
        ph -= std::floor (ph);
        float ph2 = ph + 0.5f; ph2 -= std::floor (ph2);
        const float a = d.read (ph * windowSamples), b = d.read (ph2 * windowSamples);
        const float wa = std::sin (kPi * ph), wb = std::sin (kPi * ph2);
        return a * wa + b * wb;
    }
};

struct SVF      // band-pass output, unity gain at the centre
{
    float s1 = 0.0f, s2 = 0.0f;
    float bp (float x, float g, float k) noexcept
    {
        const float hp = (x - (k + g) * s1 - s2) / (1.0f + g * (k + g));
        const float v1 = g * hp, b = v1 + s1;
        s1 = b + v1;
        const float v2 = g * b, l = v2 + s2;
        s2 = l + v2;
        return k * b;
    }
};

inline float coef (float hz, float sr) noexcept { return std::exp (-2.0f * kPi * std::min (hz, 0.45f * sr) / sr); }

//==============================================================================
class Reverb
{
public:
    void prepare (float sampleRate)
    {
        fs = sampleRate;
        pre.init ((int) (0.31f * fs) + 4);
        tape.init ((int) (2.0f * fs) + 4);
        refl.init ((int) (1.2f * fs) + 4);
        static const float apMs[4] = { 4.3f, 7.1f, 10.9f, 15.7f };
        for (int i = 0; i < 4; ++i) ap[i].init ((int) (apMs[i] * 0.001f * fs));
        for (auto& l : fdn) l.init ((int) (0.26f * fs));
        shifter.init (fs);
        for (int k = 0; k < N; ++k) inc[k] = 2.0f * kPi * (0.19f + 0.073f * (float) k) / fs;
        resetState();
        snap = true;
    }

    // Processes l / r in place: dry/wet mix included. The type changes with a short mute.
    void process (float* l, float* r, int n, const Params& p)
    {
        for (int off = 0; off < n; off += kBlock)
        {
            const int m = std::min (kBlock, n - off);
            control (p);
            const float dryT = std::cos (p.mix * 0.5f * kPi);
            const float wetT = std::sin (p.mix * 0.5f * kPi) * std::pow (10.0f, (p.value - 0.5f) * 24.0f / 20.0f);
            for (int i = off; i < off + m; ++i)
            {
                // type change: fade out, reset, fade in
                const bool want = (p.type == type);
                duck += (want ? 1.0f : -1.0f) * (1.0f / (0.03f * fs));
                duck = std::clamp (duck, 0.0f, 1.0f);
                if (! want && duck <= 0.0f) { type = p.type; resetState(); snap = true; control (p); }

                dryG += 0.01f * (dryT - dryG);
                wetG += 0.01f * (wetT - wetG);
                float yl, yr;
                tick (0.5f * (l[i] + r[i]), p, yl, yr);
                const float w = duck * wetG;
                l[i] = l[i] * dryG + yl * w;
                r[i] = r[i] * dryG + yr * w;
            }
        }
    }

private:
    static constexpr int N = 8, kBlock = 32;

    // ----- per-block control -----
    void control (const Params& p)
    {
        // network scale, damping, diffusion by type
        float scale = 1.0f, diff = 0.6f, dampHi = 12000.0f, dampLo = 1500.0f, fixedDamp = 0.0f, modMs = 3.0f;
        const float size = 0.6f + 0.8f * p.p1;
        switch (type)
        {
            case Room:    scale = 0.22f * size; diff = 0.5f; dampHi = 9000.0f; dampLo = 1200.0f; modMs = 0.4f; break;
            case Hall:    scale = 1.0f * size;  diff = 0.65f; modMs = 2.5f; break;
            case Plate:   scale = 0.5f * (0.7f + 0.6f * p.p1); diff = 0.72f; dampHi = 16000.0f; dampLo = 2500.0f; modMs = 1.0f; break;
            case Spring:  scale = 0.12f; diff = 0.0f; dampHi = 8000.0f; dampLo = 1500.0f; modMs = 0.3f; break;
            case Swell:   scale = 1.2f; diff = 0.65f; break;
            case Bloom:   scale = 1.0f; diff = 0.7f; break;
            case Cloud:   scale = 1.8f * size; diff = 0.75f; modMs = 4.0f; break;
            case Chorale: scale = 1.0f; diff = 0.65f; fixedDamp = 5000.0f; break;
            case Shimmer: scale = 1.1f; diff = 0.7f; fixedDamp = 7000.0f; modMs = 3.5f; break;
            case Magneto: scale = 0.9f; diff = 0.65f; fixedDamp = 4500.0f; break;
            case Nonlinear: scale = 0.5f; diff = 0.7f; fixedDamp = 8000.0f; modMs = 1.0f; break;
            default: break;
        }
        static const float baseMs[N] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.9f, 67.7f, 73.1f };
        const float maxLen = 0.25f * fs;
        for (int k = 0; k < N; ++k)
        {
            lenT[k] = std::clamp (baseMs[k] * 0.001f * fs * scale, 12.0f, maxLen - 8.0f);
            len[k] = snap ? lenT[k] : len[k] + 0.1f * (lenT[k] - len[k]);
        }
        const float rt = (type == Nonlinear) ? 0.8f : decaySeconds (type, p.decay);
        rtS = snap ? rt : rtS + 0.1f * (rt - rtS);
        for (int k = 0; k < N; ++k)
            g[k] = std::pow (10.0f, -3.0f * len[k] / (fs * std::max (0.05f, rtS)));

        const bool p2Damps = type <= Bloom || type == Cloud;
        const float fc = p2Damps ? dampHi * std::pow (dampLo / dampHi, p.p2) : fixedDamp;
        dampA = coef (fc > 0.0f ? fc : 8000.0f, fs);
        for (auto& a : ap) a.g = diff;
        modDepth = p.mod * modMs * 0.001f * fs;
        modDepth = std::min (modDepth, 0.4f * std::min (len[0], 0.2f * fs));

        // tone: below centre darker, above centre low-cut
        const float t = p.tone * 2.0f - 1.0f;
        lpA = coef (t < 0.0f ? 20000.0f * std::pow (0.07f, -t) : 20000.0f, fs);
        hpA = coef (t > 0.0f ? 40.0f * std::pow (15.0f, t) : 20.0f, fs);

        preT = std::min (predelayMs (p.predelay) * 0.001f * fs, 0.3f * fs);

        // per-type extras
        if (type == Chorale)
        {
            static const float f1[5] = { 800, 400, 350, 450, 325 }, f2[5] = { 1150, 1600, 2300, 800, 700 };
            const float pos = std::clamp (p.p1, 0.0f, 1.0f) * 4.0f;
            const int i0 = std::min (3, (int) pos);
            const float fr = pos - (float) i0;
            const float a = f1[i0] + fr * (f1[i0 + 1] - f1[i0]), b = f2[i0] + fr * (f2[i0 + 1] - f2[i0]);
            fg1 = std::tan (kPi * a / fs);
            fg2 = std::tan (kPi * b / fs);
        }
        if (type == Shimmer)
        {
            static const float ratios[5] = { 0.5f, 1.4983f, 2.0f, 2.9966f, 4.0f };
            shRatio = ratios[std::min (4, (int) (p.p2 * 5.0f))];
            shAmt = 0.05f + 0.4f * p.p1;
        }
        if (type == Magneto)
        {
            tapT = 0.04f + p.p1 * 0.36f;
            tapFb = p.p2 * 0.85f;
        }
        if (type == Reflections)
        {
            static const float r[12] = { 0.03f, 0.07f, 0.11f, 0.17f, 0.23f, 0.31f, 0.4f, 0.5f, 0.6f, 0.72f, 0.85f, 1.0f };
            const float span = std::min (decaySeconds (Reflections, p.decay) * 1000.0f * 0.4f * (0.4f + 1.2f * p.p1), 900.0f) * 0.001f * fs;
            const float slope = 0.5f + 2.5f * (1.0f - p.p2);
            for (int k = 0; k < 12; ++k)
            {
                const float tl = r[k] * span, tr = (r[k] * 0.87f + 0.04f) * span;
                rtL[k] = snap ? tl : rtL[k] + 0.2f * (tl - rtL[k]);
                rtR[k] = snap ? tr : rtR[k] + 0.2f * (tr - rtR[k]);
                ra[k] = std::pow (10.0f, -slope * r[k]) * ((k & 1) ? -1.0f : 1.0f);
            }
        }
        if (type == Nonlinear)
        {
            gateT = decaySeconds (Nonlinear, p.decay);
            gateThr = 0.3f * std::pow (0.03f, p.p2);
        }
        snap = false;
    }

    void resetState()
    {
        for (auto& l : fdn) l.clear();
        for (auto& a : ap) a.d.clear();
        tape.clear(); refl.clear(); shifter.clear();
        for (int k = 0; k < N; ++k) { lp[k] = 0.0f; ph[k] = (float) k * 0.7f; }
        env = 0.0f; gain = 0.0f; silent = 0; wetPrev = 0.0f; shLp = 0.0f; tapLp = 0.0f; tapTime = 0.0f;
        gateG = 0.0f; gateClock = 0.0f; gateActive = false; gateArmed = true;
        for (auto& z : disp) z[0] = z[1] = 0.0f;
        for (auto& s : fl) s = SVF();
        for (int c = 0; c < 2; ++c) lpz1[c] = lpz2[c] = hpz[c] = 0.0f;
    }

    // ----- per-sample engine -----
    void tick (float xin, const Params& p, float& outL, float& outR)
    {
        pre.push (xin);
        preS += 0.002f * (preT - preS);
        float x = pre.read (std::max (0.0f, preS));
        const float dcy = x - dcX + 0.9975f * dcY;                 // DC blocker
        dcX = x; dcY = dcy; x = dcy;

        env = std::max (std::fabs (x), env * 0.9997f);
        float eL = 0.0f, eR = 0.0f;                                // direct (non-reverb) signal, for MAGNETO
        float xf = x;

        switch (type)
        {
            case Swell: case Bloom:
            {
                const float rise = (type == Swell) ? 0.05f + p.p1 * 2.5f : 0.1f + p.p1 * 1.5f;
                const float floorG = (type == Swell) ? 0.0f : 0.35f;
                if (env > 0.01f) { silent = 0; gain = std::min (1.0f, gain + 1.0f / (rise * fs)); }
                else if (++silent > (int) (0.3f * fs)) gain = std::max (floorG, gain - 1.0f / (0.05f * fs));
                gain = std::max (gain, floorG);
                xf = x * gain;
                break;
            }
            case Spring:
            {
                const float a = 0.55f + 0.35f * p.p1;
                float s = x;
                for (auto& z : disp) { const float y = a * s + z[0] - a * z[1]; z[0] = s; z[1] = y; s = y; }
                xf = s;
                break;
            }
            case Shimmer:
            {
                const float sh = shifter.process (wetPrev, shRatio);
                shLp = sh + coef (5000.0f, fs) * (shLp - sh);
                xf = x + std::tanh (shLp * shAmt * 2.0f);
                break;
            }
            case Magneto:
            {
                tapTime += 0.001f * (tapT * fs - tapTime);
                const float wow = 1.0f + p.mod * 0.01f * std::sin (wowPh);
                wowPh += 2.0f * kPi * 0.7f / fs; if (wowPh > 2.0f * kPi) wowPh -= 2.0f * kPi;
                const float T = tapTime * wow;
                const float h1 = tape.read (T), h2 = tape.read (2.0f * T), h3 = tape.read (3.0f * T), h4 = tape.read (4.0f * T);
                tapLp = h4 + coef (4500.0f, fs) * (tapLp - h4);
                tape.push (x + std::tanh (tapLp * tapFb));
                eL = 0.8f * h1 + 0.45f * h3;
                eR = 0.6f * h2 + 0.35f * h4;
                xf = 0.7f * x + 0.5f * (eL + eR);
                break;
            }
            default: break;
        }

        float yl = 0.0f, yr = 0.0f;
        if (type == Reflections)
        {
            refl.push (x);
            for (int k = 0; k < 12; ++k)
            {
                yl += ra[k] * refl.read (std::max (1.0f, rtL[k]));
                yr += ra[k] * ((k & 2) ? -1.0f : 1.0f) * refl.read (std::max (1.0f, rtR[k]));
            }
            yl *= 0.3f; yr *= 0.3f;
        }
        else
        {
            for (auto& a : ap) xf = a.process (xf);
            static const float sL[N] = { 1, -1, 1, -1, 1, -1, 1, -1 }, sR[N] = { 1, 1, -1, -1, 1, 1, -1, -1 };
            static const float inj[N] = { 1, -1, 1, 1, -1, 1, -1, -1 };
            float z[N], sum = 0.0f;
            for (int k = 0; k < N; ++k)
            {
                const float m = modDepth * std::sin (ph[k]);
                ph[k] += inc[k]; if (ph[k] > 2.0f * kPi) ph[k] -= 2.0f * kPi;
                const float y = fdn[k].read (std::max (1.0f, len[k] - 1.0f + m));
                lp[k] = y + dampA * (lp[k] - y);
                z[k] = lp[k];
                sum += z[k];
                yl += sL[k] * z[k];
                yr += sR[k] * z[k];
            }
            const float s2 = sum * (2.0f / (float) N);
            for (int k = 0; k < N; ++k) fdn[k].push (0.35f * inj[k] * xf + g[k] * (z[k] - s2));
            yl *= 0.35f; yr *= 0.35f;
        }
        wetPrev = 0.5f * (yl + yr);

        if (type == Nonlinear)
        {
            if (gateArmed && env > gateThr) { gateArmed = false; gateActive = true; gateClock = 0.0f; }
            if (! gateArmed && env < gateThr * 0.4f) gateArmed = true;
            float target = 0.0f;
            if (gateActive)
            {
                gateClock += 1.0f / fs;
                const float u = gateClock / gateT;
                if (p.p1 < 0.34f)      target = u < 1.0f ? 1.0f : 0.0f;                  // gate
                else if (p.p1 < 0.67f) target = u < 1.0f ? u : 0.0f;                     // reverse (rising)
                else                   target = u < 1.0f ? std::exp (-4.0f * u) : 0.0f;  // decay
                if (u > 1.2f) gateActive = false;
            }
            gateG += 0.004f * (target - gateG);
            yl *= gateG; yr *= gateG;
        }

        if (type == Chorale)
        {
            const float k = 1.0f / 7.0f, fa = p.p2 * 0.9f;
            const float cl = 4.0f * (fl[0].bp (yl, fg1, k) + 0.8f * fl[1].bp (yl, fg2, k));
            const float cr = 4.0f * (fl[2].bp (yr, fg1, k) + 0.8f * fl[3].bp (yr, fg2, k));
            yl = yl * (1.0f - fa) + fa * cl;
            yr = yr * (1.0f - fa) + fa * cr;
        }

        yl += 0.5f * eL; yr += 0.5f * eR;

        // tone
        float o[2] = { yl, yr };
        for (int c = 0; c < 2; ++c)
        {
            lpz1[c] = o[c] + lpA * (lpz1[c] - o[c]);
            lpz2[c] = lpz1[c] + lpA * (lpz2[c] - lpz1[c]);
            hpz[c]  = lpz2[c] + hpA * (hpz[c] - lpz2[c]);
            o[c] = lpz2[c] - hpz[c];
        }
        outL = o[0]; outR = o[1];
    }

    float fs = 48000.0f;
    int type = 0;
    bool snap = true;
    float duck = 1.0f, dryG = 0.7f, wetG = 0.7f;

    Delay pre, tape, refl, fdn[N];
    Allpass ap[4];
    PitchShifter shifter;
    float lp[N] {}, len[N] {}, lenT[N] {}, g[N] {}, ph[N] {}, inc[N] {};
    float dampA = 0.5f, modDepth = 0.0f, rtS = 1.0f, lpA = 0.0f, hpA = 0.0f, preT = 0.0f, preS = 0.0f, dcX = 0.0f, dcY = 0.0f;
    float env = 0.0f, gain = 0.0f, wetPrev = 0.0f, shLp = 0.0f, shRatio = 2.0f, shAmt = 0.2f;
    int silent = 0;
    float tapT = 0.2f, tapFb = 0.4f, tapLp = 0.0f, tapTime = 0.0f, wowPh = 0.0f;
    float rtL[12] {}, rtR[12] {}, ra[12] {};
    float gateT = 0.3f, gateThr = 0.05f, gateG = 0.0f, gateClock = 0.0f;
    bool gateActive = false, gateArmed = true;
    float disp[24][2] {};
    SVF fl[4];
    float fg1 = 0.1f, fg2 = 0.2f;
    float lpz1[2] {}, lpz2[2] {}, hpz[2] {};
};
} // namespace bigsky
