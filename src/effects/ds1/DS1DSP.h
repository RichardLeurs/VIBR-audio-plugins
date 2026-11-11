// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

// DS-1 style distortion, modelled on the circuit's signal path:
//   input -> transistor booster (treble emphasis) -> op-amp gain stage (DIST) -> hard-clipping diode pair
//         -> passive TONE section (low-pass / high-pass blend) -> LEVEL
// The nonlinear part (booster, op-amp, diodes) runs at an oversampled rate; TONE and LEVEL run at the host rate.
// Header-only, no JUCE dependency, real-time safe (no allocation in the process functions).
#include <algorithm>
#include <cmath>

namespace ds1
{
constexpr float kPi = 3.14159265358979f;

struct OnePole
{
    float z = 0.0f, a = 0.0f;
    void setCutoff (float hz, float sr) { a = std::exp (-2.0f * kPi * hz / sr); }
    float lp (float x) noexcept { z = x + a * (z - x); return z; }          // 1st-order low-pass
    float hp (float x) noexcept { return x - lp (x); }                       // complementary high-pass
    void reset() noexcept { z = 0.0f; }
};

// Op-amp gain stage: unity below the corner, (1 + Rf/Rg) above it; the op-amp's gain-bandwidth limit rolls the top off.
struct GainStage
{
    OnePole hpf, lpf;
    float lpCutoffSet = -1.0f;

    void prepare (float osRate)
    {
        rate = osRate;
        hpf.setCutoff (90.0f, rate);
        lpf.setCutoff (20000.0f, rate);
        hpf.reset(); lpf.reset();
    }

    // g = total gain (>= 1)
    float process (float x, float g) noexcept
    {
        const float y = x + (g - 1.0f) * hpf.hp (x);
        return lpf.lp (y);
    }

    void setBandwidth (float g)                       // call at block rate; the GBW product shrinks the bandwidth as gain rises
    {
        const float fc = std::clamp (1.2e6f / std::max (g, 1.0f), 4500.0f, 0.45f * rate);
        if (std::abs (fc - lpCutoffSet) > 50.0f) { lpf.setCutoff (fc, rate); lpCutoffSet = fc; }
    }

private:
    float rate = 44100.0f;
};

// Pair of silicon diodes to ground after the op-amp: a hard knee that limits at roughly +/-0.6 V.
inline float diodeClip (float x) noexcept
{
    constexpr float vf = 0.6f;
    const float u = x / vf;
    const float a = std::abs (u);
    // x / (1 + |x|^p)^(1/p): sharp knee, p = 4
    const float a2 = a * a;
    return vf * u / std::sqrt (std::sqrt (1.0f + a2 * a2));
}

struct Booster
{
    OnePole hpf;
    void prepare (float osRate) { hpf.setCutoff (1100.0f, osRate); hpf.reset(); }
    float process (float x) noexcept { return 1.4f * (x + 1.6f * hpf.hp (x)); }   // high shelf: more bite before the op-amp
};

// One audio channel of the nonlinear section (oversampled rate)
struct DistChannel
{
    Booster booster;
    GainStage opamp;
    OnePole inHp;                                   // input coupling

    void prepare (float osRate) { booster.prepare (osRate); opamp.prepare (osRate); inHp.setCutoff (30.0f, osRate); inHp.reset(); }
    float process (float x, float gain) noexcept
    {
        x = inHp.hp (x) * 2.0f;                      // roughly guitar level -> pedal level
        x = booster.process (x);
        x = opamp.process (x, gain);
        return diodeClip (x);
    }
};

// TONE: blend of a low-pass (dark) and a high-pass (bright) branch; centre leaves a mid scoop, as on the pedal
struct ToneChannel
{
    OnePole low, high;
    void prepare (float sr) { low.setCutoff (900.0f, sr); high.setCutoff (2400.0f, sr); low.reset(); high.reset(); }
    float process (float x, float tone) noexcept         // tone 0..1
    {
        const float l = low.lp (x), h = high.hp (x);
        return (1.0f - tone) * 1.25f * l + tone * 2.4f * h;     // the bright branch carries less energy, so it gets more gain
    }
};

// DIST knob 0..1 -> stage gain (about 3 dB .. 46 dB); exponential like the pot's audio taper
inline float distGain (float dist01) noexcept
{
    return 1.4f + 200.0f * dist01 * dist01 * (0.4f + 0.6f * dist01);
}

// LEVEL knob 0..1 -> output gain
inline float levelGain (float level01) noexcept { return 0.02f + 1.5f * level01 * level01; }
} // namespace ds1
