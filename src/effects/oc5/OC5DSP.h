// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

// OC-5 style octave generator.
//   VINTAGE: classic monophonic octave divider (OC-2 style) - the input is filtered down to its fundamental, a comparator
//            and two flip-flops divide it by 2 and by 4 (-1 and -2 octave squares, shaped by the input envelope and
//            low-passed), and a full-wave rectifier gives +1 octave.
//   POLY   : polyphonic pitch shifting (two cross-faded delay taps) of the signal after a RANGE low-pass, so only the
//            lower strings (or only the bottom note of a chord) get the octave effect. -1 and +1 octave.
// Header-only, no JUCE dependency, no allocation after prepare().
#include <algorithm>
#include <cmath>
#include <vector>

namespace oc5
{
constexpr float kPi = 3.14159265358979f;

struct OnePole
{
    float z = 0.0f;
    float lp (float x, float a) noexcept { z = x + a * (z - x); return z; }     // a = exp (-2 pi fc / sr)
    void reset() noexcept { z = 0.0f; }
};

inline float coef (float hz, float sr) noexcept { return std::exp (-2.0f * kPi * std::min (hz, 0.45f * sr) / sr); }

// Fixed-ratio pitch shifter: a read position that drifts through a delay line, two taps half a window apart cross-faded
struct PitchShifter
{
    std::vector<float> buf;
    int mask = 0, writePos = 0;
    float phase = 0.0f, window = 1600.0f, ratio = 0.5f;

    void prepare (float sr, float ratioIn)
    {
        ratio = ratioIn;
        window = 0.032f * sr;
        int size = 1;
        while ((float) size < window * 2.0f + 8.0f) size <<= 1;
        buf.assign ((size_t) size, 0.0f);
        mask = size - 1; writePos = 0; phase = 0.0f;
    }

    float read (float delay) const noexcept
    {
        float pos = (float) writePos - delay;
        const int i0 = (int) std::floor (pos);
        const float f = pos - (float) i0;
        const float a = buf[(size_t) (i0 & mask)], b = buf[(size_t) ((i0 + 1) & mask)];
        return a + f * (b - a);
    }

    float process (float x) noexcept
    {
        buf[(size_t) writePos] = x;
        const float d0 = phase * window, d1 = std::fmod (phase + 0.5f, 1.0f) * window;
        const float w0 = std::sin (kPi * phase), w1 = std::sin (kPi * std::fmod (phase + 0.5f, 1.0f));
        const float y = (read (d0 + 2.0f) * w0 * w0 + read (d1 + 2.0f) * w1 * w1);
        phase += (1.0f - ratio) / window;
        phase -= std::floor (phase);
        writePos = (writePos + 1) & mask;
        return y;
    }
};

struct Params
{
    float direct = 0.5f, up1 = 0.0f, down1 = 0.5f, down2 = 0.0f;   // 0..1 knob positions
    float range = 0.5f;                                            // POLY: how high up the octave effect reaches
    bool poly = false;
};

class Octave
{
public:
    void prepare (float sampleRate)
    {
        sr = sampleRate;
        up.prepare (sr, 2.0f);
        down.prepare (sr, 0.5f);
        smoothA = std::exp (-1.0f / (0.02f * sr));
        for (auto* z : { &lp1, &lp2, &s1a, &s1b, &s2a, &s2b, &upLp, &rg1, &rg2 }) z->reset();
        hpZ = hpPrev = 0.0f;
        ff1 = ff2 = 1.0f; cmp = false; env = 0.0f;
        sup = sdown1 = sdown2 = sdirect = 0.0f; srange = 0.5f; spoly = 0.0f;
    }

    float process (float x, const Params& p) noexcept
    {
        // parameter smoothing
        sdirect += (gain (p.direct) - sdirect) * (1.0f - smoothA);
        sup     += (gain (p.up1)    - sup)     * (1.0f - smoothA);
        sdown1  += (gain (p.down1)  - sdown1)  * (1.0f - smoothA);
        sdown2  += (gain (p.down2)  - sdown2)  * (1.0f - smoothA);
        srange  += (p.range         - srange)  * (1.0f - smoothA);
        spoly   += ((p.poly ? 1.0f : 0.0f) - spoly) * (1.0f - smoothA);

        vintage (x);                                   // both paths always run, so switching modes only cross-fades
        polyphonic (x);

        const float upMix   = (1.0f - spoly) * vintUp    + spoly * polyUp;
        const float downMix = (1.0f - spoly) * vintDown1 + spoly * polyDown;
        return sdirect * x + sup * upMix + sdown1 * downMix + sdown2 * (1.0f - spoly) * vintDown2;   // -2 octave: VINTAGE only
    }

private:
    static float gain (float k) noexcept { return 1.4f * k * k; }

    void vintage (float x) noexcept
    {
        // isolate the fundamental: two one-pole low-passes (about 12 dB/oct at 700 Hz)
        const float a = coef (700.0f, sr);
        const float y = lp2.lp (lp1.lp (x, a), a);

        // envelope follower on the filtered signal
        const float ay = std::abs (y);
        env += (ay > env ? 0.2f : 0.0006f) * (ay - env);

        // comparator with hysteresis scaled by the envelope; gated so noise does not toggle the dividers
        const float th = 0.12f * env + 0.0005f;
        if (env > 0.004f)
        {
            if (! cmp && y > th)  { cmp = true;  ff1 = -ff1; if (ff1 > 0.0f) ff2 = -ff2; }
            else if (cmp && y < -th) cmp = false;
        }

        // -1 / -2 octave: squares scaled by the envelope, then softened
        const float sq1 = ff1 * env * 0.85f, sq2 = ff2 * env * 0.85f;
        vintDown1 = s1b.lp (s1a.lp (sq1, coef (900.0f, sr)), coef (900.0f, sr));
        vintDown2 = s2b.lp (s2a.lp (sq2, coef (450.0f, sr)), coef (450.0f, sr));

        // +1 octave: full-wave rectified fundamental, DC removed
        const float rect = std::abs (y);
        hpZ = rect - hpPrev + 0.995f * hpZ; hpPrev = rect;            // DC blocker
        vintUp = upLp.lp (hpZ * 2.6f, coef (3500.0f, sr));
    }

    void polyphonic (float x) noexcept
    {
        // RANGE: low-pass in front of the shifters, 100 Hz (lowest note only) .. 2.4 kHz (most of the neck)
        const float fc = 100.0f * std::pow (24.0f, srange);
        const float a = coef (fc, sr);
        const float y = rg2.lp (rg1.lp (x, a), a);
        polyDown = down.process (y) * 1.3f;
        polyUp   = up.process (y) * 1.2f;
    }

    float sr = 44100.0f, smoothA = 0.999f;
    OnePole lp1, lp2, s1a, s1b, s2a, s2b, upLp, rg1, rg2;
    PitchShifter up, down;
    float hpZ = 0.0f, hpPrev = 0.0f, env = 0.0f, ff1 = 1.0f, ff2 = 1.0f;
    bool cmp = false;
    float sup = 0, sdown1 = 0, sdown2 = 0, sdirect = 0, srange = 0.5f, spoly = 0;
    float vintDown1 = 0, vintDown2 = 0, vintUp = 0, polyDown = 0, polyUp = 0;
};
} // namespace oc5
