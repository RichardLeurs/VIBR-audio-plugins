// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

// TR-08 style analogue-ish drum voices. Header-only, no allocation, no JUCE dependency.
// All voices are simple physical-ish models (decaying sines with pitch envelopes, filtered noise, square-wave metal banks).
namespace tr08
{
constexpr float kPi = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717959f;

enum Inst { BD, SD, LT, MT, HT, RS, CP, CB, CY, OH, CH, kNumInst };

// Snapshot of the panel, all values normalised 0..1 (levels are already tapered to gain).
struct Params
{
    float level[kNumInst] {};
    float bdTone = 0.5f, bdDecay = 0.5f, sdTone = 0.5f, sdSnappy = 0.5f;
    float tune[3] { 0.5f, 0.5f, 0.5f };      // LT/LC, MT/MC, HT/HC
    bool alt[5] {};                          // conga (LT, MT, HT), claves (RS), maracas (CP)
    float cyTone = 0.5f, cyDecay = 0.5f, ohDecay = 0.5f;
    float accent = 0.5f;                     // how much an accented step is louder
    int variant[kNumInst] {};                // sample variant per voice: 0 = cycle through all, n = the n-th (clamped)
};

// One decoded drum sample (mono, peak-normalised). Built on a background thread, read-only on the audio thread.
struct SampleSlot
{
    std::vector<float> data;
    float rate = 44100.0f;
};

// A loaded kit: slots 0..10 are the voices (BD SD LT MT HT RS CP CB CY OH CH),
// 11..15 the alternates behind the panel switches (low / mid / hi conga, claves, maracas).
struct SampleSet
{
    static constexpr int kAlt = kNumInst;      // first alternate slot
    std::vector<SampleSlot> slot[16];          // every variant of each voice, in file order
};

namespace detail
{
struct Noise
{
    uint32_t s = 22222u;
    float next() noexcept
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (float) (int32_t) s * (1.0f / 2147483648.0f);
    }
};

struct Decay
{
    float v = 0.0f, k = 0.0f;
    void trig (float tau, float sr, float start = 1.0f) noexcept { v = start; setTau (tau, sr); }
    void setTau (float tau, float sr) noexcept { k = std::exp (-1.0f / (tau * sr)); }
    float tick() noexcept { const float r = v; v *= k; if (v < 1.0e-6f) v = 0.0f; return r; }
};

// Topology-preserving state variable filter
struct Svf
{
    float s1 = 0.0f, s2 = 0.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, k = 1.0f;
    void set (float fc, float q, float sr) noexcept
    {
        fc = std::min (fc, sr * 0.45f);
        const float g = std::tan (kPi * fc / sr);
        k = 1.0f / q;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    void tick (float x, float& lp, float& bp, float& hp) noexcept
    {
        const float v3 = x - s2;
        const float v1 = a1 * s1 + a2 * v3;
        const float v2 = s2 + a2 * s1 + a3 * v3;
        s1 = 2.0f * v1 - s1;
        s2 = 2.0f * v2 - s2;
        lp = v2; bp = v1; hp = x - k * v1 - v2;
    }
};

inline float blep (float t, float dt) noexcept
{
    if (t < dt)        { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

// band-limited square wave; phase in [0,1)
inline float square (float& ph, float dt) noexcept
{
    float v = ph < 0.5f ? 1.0f : -1.0f;
    v += blep (ph, dt);
    float p2 = ph + 0.5f; if (p2 >= 1.0f) p2 -= 1.0f;
    v -= blep (p2, dt);
    ph += dt; if (ph >= 1.0f) ph -= 1.0f;
    return v;
}

struct Metal
{
    float ph[6] {};
    float tick (float sr, float scale) noexcept
    {
        static constexpr float f[6] = { 263.0f, 400.0f, 421.0f, 474.0f, 587.0f, 845.0f };
        float sum = 0.0f;
        for (int i = 0; i < 6; ++i)
            sum += square (ph[i], f[i] * scale / sr);
        return sum * (1.0f / 6.0f);
    }
};
} // namespace detail

//==============================================================================
class DrumKit
{
public:
    Params params;   // read when a voice is triggered (levels are smoothed internally)

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        reset();
        for (int i = 0; i < kNumInst; ++i) lvl[i] = params.level[i];
    }

    void reset() { st = State(); for (auto& r : rr) r = 0; }

    // nullptr = built-in synth voices. Switching kits cuts any sample that is still playing (it points into the old set).
    void setSamples (const SampleSet* set) noexcept
    {
        if (set == samples) return;
        samples = set;
        for (auto& v : st.sv) v.on = false;
    }

    // vel 0..1; accent adds the panel's accent amount on top
    void trigger (int inst, float vel, bool accent) noexcept
    {
        const float g = vel * (accent ? 1.0f + params.accent * 0.9f : 1.0f);
        const Params& p = params;
        auto& s = st;

        if (inst == CH)                                      // closed hat chokes the open hat (synth and sample)
            s.sv[OH].env.setTau (0.004f, sr);
        if (samples != nullptr && startSample (inst, g))
            return;

        switch (inst)
        {
            case BD:
                s.bd.gain = g; s.bd.ph = 0.25f;
                s.bd.amp.trig (0.04f + p.bdDecay * 0.50f, sr);
                s.bd.pitch.trig (0.028f, sr);
                s.bd.click.trig (0.0025f, sr, p.bdTone);
                break;
            case SD:
                s.sd.gain = g; s.sd.ph1 = s.sd.ph2 = 0.0f;
                s.sd.f1 = 165.0f + p.sdTone * 55.0f;
                s.sd.tone.trig (0.085f, sr);
                s.sd.pitch.trig (0.012f, sr);
                s.sd.noise.trig (0.05f + p.sdSnappy * 0.17f, sr);
                s.sd.noiseLevel = 0.15f + 0.85f * p.sdSnappy;
                s.sd.flt.set (1200.0f + p.sdTone * 2600.0f, 0.9f, sr);
                break;
            case LT: case MT: case HT:
            {
                const int i = inst - LT;
                auto& t = s.tom[i];
                const bool conga = p.alt[i];
                static constexpr float baseTom[3] = { 82.0f, 123.0f, 165.0f }, baseCon[3] = { 190.0f, 255.0f, 345.0f };
                static constexpr float tauTom[3] = { 0.28f, 0.22f, 0.18f }, tauCon[3] = { 0.14f, 0.12f, 0.10f };
                t.gain = g; t.ph = 0.0f;
                t.f = (conga ? baseCon[i] : baseTom[i]) * (0.8f + p.tune[i] * 0.55f);
                t.amp.trig (conga ? tauCon[i] : tauTom[i], sr);
                t.pitch.trig (conga ? 0.006f : 0.018f, sr);
                t.pitchAmt = conga ? 0.25f : 0.7f;
                t.click.trig (0.002f, sr, conga ? 0.15f : 0.0f);
                break;
            }
            case RS:
                s.rs.gain = g; s.rs.claves = p.alt[3]; s.rs.ph1 = s.rs.ph2 = 0.0f;
                s.rs.amp.trig (s.rs.claves ? 0.03f : 0.010f, sr);
                s.rs.tick.trig (0.0012f, sr, 0.5f);
                break;
            case CP:
                s.cp.gain = g; s.cp.maracas = p.alt[4]; s.cp.t = 0.0f; s.cp.on = true;
                s.cp.flt.set (s.cp.maracas ? 6000.0f : 1050.0f, s.cp.maracas ? 1.0f : 2.2f, sr);
                s.cp.env.trig (0.025f, sr);
                break;
            case CB:
                s.cb.gain = g; s.cb.ph1 = s.cb.ph2 = 0.0f;
                s.cb.e1.trig (0.016f, sr); s.cb.e2.trig (0.12f, sr);
                s.cb.flt.set (2500.0f, 1.3f, sr);
                break;
            case CY:
                s.cy.gain = g;
                s.cy.e1.trig (0.02f, sr);
                s.cy.e2.trig (0.45f + p.cyDecay * 1.6f, sr);
                s.cy.lo.set (3500.0f, 0.8f, sr);
                s.cy.hi.set (8000.0f, 0.8f, sr);
                break;
            case OH:
                s.oh.gain = g;
                s.oh.env.trig (0.06f + p.ohDecay * 0.55f, sr);
                s.hatHp.set (6500.0f, 0.8f, sr);
                s.hatHpO.set (6500.0f, 0.8f, sr);
                break;
            case CH:
                s.ch.gain = g;
                s.ch.env.trig (0.030f, sr);
                s.hatHp.set (6500.0f, 0.8f, sr);
                s.oh.env.setTau (0.004f, sr);       // closed hat chokes the open hat
                break;
            default: break;
        }
    }

    float process() noexcept
    {
        for (int i = 0; i < kNumInst; ++i)
            lvl[i] += (params.level[i] - lvl[i]) * 0.004f;     // ~5 ms smoothing of the level knobs

        auto& s = st;
        float out = 0.0f;
        float lp, bp, hp;

        // ---- bass drum
        if (s.bd.amp.v > 0.0f)
        {
            const float pe = s.bd.pitch.tick();
            const float f = 48.0f * (1.0f + 3.2f * pe);
            s.bd.ph += f / sr; if (s.bd.ph >= 1.0f) s.bd.ph -= 1.0f;
            const float body = std::sin (kTwoPi * s.bd.ph) * s.bd.amp.tick();
            const float click = s.noise.next() * s.bd.click.tick();
            out += lvl[BD] * s.bd.gain * std::tanh (1.6f * (body + 0.5f * click)) * 0.85f;
        }

        // ---- snare
        if (s.sd.tone.v > 0.0f || s.sd.noise.v > 0.0f)
        {
            const float pe = s.sd.pitch.tick();
            const float f1 = s.sd.f1 * (1.0f + 0.5f * pe), f2 = f1 * 1.85f;
            s.sd.ph1 += f1 / sr; if (s.sd.ph1 >= 1.0f) s.sd.ph1 -= 1.0f;
            s.sd.ph2 += f2 / sr; if (s.sd.ph2 >= 1.0f) s.sd.ph2 -= 1.0f;
            const float tone = (std::sin (kTwoPi * s.sd.ph1) + 0.6f * std::sin (kTwoPi * s.sd.ph2)) * s.sd.tone.tick();
            s.sd.flt.tick (s.noise.next(), lp, bp, hp);
            const float noise = hp * s.sd.noise.tick() * s.sd.noiseLevel;
            out += lvl[SD] * s.sd.gain * (0.55f * tone + 0.9f * noise);
        }

        // ---- toms / congas
        for (int i = 0; i < 3; ++i)
        {
            auto& t = s.tom[i];
            if (t.amp.v > 0.0f)
            {
                const float f = t.f * (1.0f + t.pitchAmt * t.pitch.tick());
                t.ph += f / sr; if (t.ph >= 1.0f) t.ph -= 1.0f;
                const float v = std::sin (kTwoPi * t.ph) * t.amp.tick() + s.noise.next() * t.click.tick();
                out += lvl[LT + i] * t.gain * 0.9f * v;
            }
        }

        // ---- rimshot / claves
        if (s.rs.amp.v > 0.0f)
        {
            float v;
            if (s.rs.claves)
            {
                s.rs.ph1 += 2500.0f / sr; if (s.rs.ph1 >= 1.0f) s.rs.ph1 -= 1.0f;
                v = std::sin (kTwoPi * s.rs.ph1) * s.rs.amp.tick();
            }
            else
            {
                s.rs.ph1 += 455.0f / sr;  if (s.rs.ph1 >= 1.0f) s.rs.ph1 -= 1.0f;
                s.rs.ph2 += 1750.0f / sr; if (s.rs.ph2 >= 1.0f) s.rs.ph2 -= 1.0f;
                v = (0.6f * std::sin (kTwoPi * s.rs.ph1) + 0.5f * std::sin (kTwoPi * s.rs.ph2)) * s.rs.amp.tick()
                    + s.noise.next() * s.rs.tick.tick();
            }
            out += lvl[RS] * s.rs.gain * 0.8f * v;
        }

        // ---- hand clap / maracas
        if (s.cp.on)
        {
            const float t = s.cp.t / sr;
            s.cp.t += 1.0f;
            float e;
            if (s.cp.maracas)
                e = s.cp.env.tick();
            else
                e = t < 0.030f ? std::exp (-std::fmod (t, 0.010f) / 0.003f) : 0.8f * std::exp (-(t - 0.030f) / 0.085f);
            s.cp.flt.tick (s.noise.next(), lp, bp, hp);
            out += lvl[CP] * s.cp.gain * (s.cp.maracas ? 0.9f : 2.0f) * (s.cp.maracas ? hp : bp) * e;
            if (t > 0.6f) s.cp.on = false;
        }

        // ---- cowbell
        if (s.cb.e2.v > 0.0f)
        {
            const float sq = detail::square (s.cb.ph1, 540.0f / sr) + detail::square (s.cb.ph2, 800.0f / sr);
            s.cb.flt.tick (sq * 0.5f, lp, bp, hp);
            out += lvl[CB] * s.cb.gain * 1.2f * bp * (0.6f * s.cb.e1.tick() + 0.4f * s.cb.e2.tick());
        }

        // ---- cymbal
        if (s.cy.e2.v > 0.0f)
        {
            const float m = s.cy.metal.tick (sr, 1.12f);
            float a, b, c, lo, hi;
            s.cy.lo.tick (m, a, lo, c);
            s.cy.hi.tick (m, a, b, hi);
            const float v = lo * (1.0f - p_cyTone()) * 0.9f + hi * p_cyTone();
            out += lvl[CY] * s.cy.gain * 1.4f * v * (0.5f * s.cy.e1.tick() + 0.5f * s.cy.e2.tick());
        }

        // ---- hats (shared metal bank)
        if (s.ch.env.v > 0.0f || s.oh.env.v > 0.0f)
        {
            const float m = s.hatMetal.tick (sr, 1.0f);
            if (s.ch.env.v > 0.0f)
            {
                s.hatHp.tick (m, lp, bp, hp);
                out += lvl[CH] * s.ch.gain * 1.3f * hp * s.ch.env.tick();
            }
            if (s.oh.env.v > 0.0f)
            {
                s.hatHpO.tick (m, lp, bp, hp);
                out += lvl[OH] * s.oh.gain * 1.3f * hp * s.oh.env.tick();
            }
        }

        // ---- sampled voices
        for (int i = 0; i < kNumInst; ++i)
        {
            auto& v = s.sv[i];
            if (! v.on) continue;
            const int idx = (int) v.pos;
            if (idx + 1 >= (int) v.slot->data.size()) { v.on = false; continue; }
            const float frac = (float) (v.pos - (double) idx);
            const float x = v.slot->data[(size_t) idx] + frac * (v.slot->data[(size_t) idx + 1] - v.slot->data[(size_t) idx]);
            v.pos += v.inc;
            const float e = v.env.tick();
            if (e < 1.0e-4f) { v.on = false; continue; }
            out += lvl[i] * v.gain * e * x;
        }

        return std::tanh (out * 0.8f);
    }

private:
    float p_cyTone() const noexcept { return params.cyTone; }

    // Start the sample for a voice (honouring the conga / claves / maracas switches); false = no sample, use the synth.
    bool startSample (int inst, float gain) noexcept
    {
        const Params& p = params;
        int slot = inst;
        switch (inst)
        {
            case LT: case MT: case HT: if (p.alt[inst - LT]) slot = SampleSet::kAlt + (inst - LT); break;
            case RS:                   if (p.alt[3]) slot = SampleSet::kAlt + 3; break;
            case CP:                   if (p.alt[4]) slot = SampleSet::kAlt + 4; break;
            default: break;
        }
        if (samples->slot[slot].empty()) slot = inst;                             // no alternate in this kit: use the main sample
        const auto& variants = samples->slot[slot];
        if (variants.empty()) return false;
        const int count = (int) variants.size();
        int pick = p.variant[inst] > 0 ? std::min (p.variant[inst], count) - 1 : rr[inst] % count;
        if (p.variant[inst] <= 0) rr[inst] = (rr[inst] + 1) % count;
        const auto& smp = variants[(size_t) pick];
        if (smp.data.size() < 2) return false;

        auto& v = st.sv[inst];
        v.slot = &smp;
        v.pos = 0.0;
        v.gain = gain;
        double ratio = (double) smp.rate / (double) sr;
        if (inst >= LT && inst <= HT)
            ratio *= std::pow (2.0, (double) (p.tune[inst - LT] - 0.5f));        // tuning knob: +/- half an octave
        v.inc = ratio;

        // decay knob shortens the sample with an exponential fade; fully clockwise leaves it untouched
        const float d = inst == BD ? p.bdDecay : inst == CY ? p.cyDecay : inst == OH ? p.ohDecay : 1.0f;
        v.env.trig (d >= 0.98f ? 1.0e5f : 0.03f + d * d * 2.5f, sr);
        v.on = true;
        return true;
    }

    struct State
    {
        detail::Noise noise;
        struct { float gain = 0, ph = 0; detail::Decay amp, pitch, click; } bd;
        struct { float gain = 0, ph1 = 0, ph2 = 0, f1 = 180, noiseLevel = 0; detail::Decay tone, pitch, noise; detail::Svf flt; } sd;
        struct { float gain = 0, ph = 0, f = 100, pitchAmt = 0; detail::Decay amp, pitch, click; } tom[3];
        struct { float gain = 0, ph1 = 0, ph2 = 0; bool claves = false; detail::Decay amp, tick; } rs;
        struct { float gain = 0, t = 0; bool on = false, maracas = false; detail::Svf flt; detail::Decay env; } cp;
        struct { float gain = 0, ph1 = 0, ph2 = 0; detail::Decay e1, e2; detail::Svf flt; } cb;
        struct { float gain = 0; detail::Decay e1, e2; detail::Svf lo, hi; detail::Metal metal; } cy;
        struct { float gain = 0; detail::Decay env; } oh, ch;
        detail::Metal hatMetal;
        detail::Svf hatHp, hatHpO;
        struct SV { const SampleSlot* slot = nullptr; double pos = 0.0, inc = 1.0; float gain = 0.0f; detail::Decay env; bool on = false; } sv[kNumInst];
    } st;

    const SampleSet* samples = nullptr;
    int rr[kNumInst] {};                       // round-robin position per voice
    float sr = 44100.0f;
    float lvl[kNumInst] {};
};
} // namespace tr08
