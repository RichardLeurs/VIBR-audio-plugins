// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

// DM-1 style multi-mode delay engine (header-only, no JUCE dependency).
//
// What is modelled vs. approximated
// ---------------------------------
// The hardware that inspired it is an analog BBD delay machine with 12 modes. Its exact
// circuits (BBD clocking, compander, per-mode voicing) are NOT reproduced here.
// This engine is a digital approximation built from generic blocks:
//   * one fractional delay line per channel (4-point Hermite interpolation),
//   * an in-loop tone stage (one-pole HP + two one-pole LPs) and a tanh
//     saturator that bounds every feedback path,
//   * per-mode tap / routing / envelope logic (see Mode comments below).
// Nothing here has been compared against recordings of the hardware; the
// mode voicings are informed guesses at the character described by the panel.
//
// Real-time safety: all buffers are allocated in prepare(). process() does no
// allocation, locking or I/O. Filter coefficients are recomputed only when the
// target cutoff changes (checked once per block in setParams()).

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace dm1
{
inline constexpr float kPi    = 3.14159265358979f;
inline constexpr float kTwoPi = 2.0f * kPi;

inline constexpr float kMinDelaySec = 0.02f;   // Delay Time knob 0
inline constexpr float kMaxDelaySec = 2.0f;    // Delay Time knob 10
inline constexpr float kMaxModSec   = 0.010f;  // headroom for Mod Depth (max 8 ms used)

// Mode numbers match the "mode" AudioParameterInt (1..12), i.e. the 12 positions of
// the panel's Mode Selector clockwise from the top.
enum Mode : int
{
    Classic = 1,     // clean digital delay, Variation = repeat tone (dark -> bright)
    Vintage,         // BBD-ish: LP falls with delay time, Variation = age/grit
    Modern,          // clean and bright, Variation = low cut on the repeats
    MultiHead,       // 3 taps at d/3, 2d/3, d (tape-echo style), Variation = head combination
    NonLinear,       // per-repeat envelope, Variation: 0 gate .. 5 plain .. 10 reverse-ish swell
    Ambience,        // delay + allpass diffusion in the loop, Variation = diffusion
    Reflect,         // early-reflection multi-tap + main repeat, Variation = room size
    DoublingDelay,   // short doubler + main delay, Variation = doubler/delay balance
    Wide,            // ping-pong, Variation = stereo width (cross-feed)
    DualMod,         // two independently modulated delays (L/R), Variation = 2nd delay time ratio
    Pan,             // mono delay, repeats auto-panned L <-> R, Variation = pan speed (repeats per sweep)
    Pattern          // rhythmic multi-tap pattern inside one delay period, Variation = pattern select
};
inline constexpr int kNumModes = 12;
static_assert (Pattern == kNumModes, "Mode enum and kNumModes out of sync");

inline float clampf (float x, float lo, float hi) { return std::min (hi, std::max (lo, x)); }
inline float onePoleCoef (float seconds, double sr) { return 1.0f - std::exp (-1.0f / (seconds * (float) sr)); }

//==============================================================================
// Circular delay line, 4-point Hermite fractional read. Read BEFORE write each sample.
class DelayLine
{
public:
    void prepare (int minSamples)
    {
        int n = 1;
        while (n < minSamples) n <<= 1;
        buffer.assign ((size_t) n, 0.0f);
        mask = n - 1;
        writePos = 0;
        maxDelay = (float) (n - 4);
    }

    void reset() { std::fill (buffer.begin(), buffer.end(), 0.0f); }

    void write (float x)
    {
        buffer[(size_t) writePos] = x;
        writePos = (writePos + 1) & mask;
    }

    // delay in samples, clamped to [2, size-4] so all 4 Hermite points are valid
    float read (float delay) const
    {
        delay = clampf (delay, 2.0f, maxDelay);
        const float pos = (float) writePos - 1.0f - delay;
        const float fl = std::floor (pos);
        const float f = pos - fl;
        const int i1 = (int) fl;
        const float y0 = buffer[(size_t) ((i1 - 1) & mask)];
        const float y1 = buffer[(size_t) (i1 & mask)];
        const float y2 = buffer[(size_t) ((i1 + 1) & mask)];
        const float y3 = buffer[(size_t) ((i1 + 2) & mask)];
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }

private:
    std::vector<float> buffer;
    int mask = 0, writePos = 0;
    float maxDelay = 4.0f;
};

//==============================================================================
struct OnePoleLP
{
    float a = 1.0f, y = 0.0f;
    void setCutoff (float hz, double sr) { a = 1.0f - std::exp (-kTwoPi * hz / (float) sr); }
    float process (float x) { y += a * (x - y); return y; }
    void reset() { y = 0.0f; }
};

struct OnePoleHP
{
    OnePoleLP lp;
    void setCutoff (float hz, double sr) { lp.setCutoff (hz, sr); }
    float process (float x) { return x - lp.process (x); }
    void reset() { lp.reset(); }
};

// Schroeder allpass with fixed length; gain passed per sample (|g| < 1 => stable).
class Allpass
{
public:
    void prepare (int length)
    {
        buf.assign ((size_t) std::max (1, length), 0.0f);
        idx = 0;
    }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); }

    float process (float x, float g)
    {
        const float d = buf[(size_t) idx];
        const float v = x + g * d;
        buf[(size_t) idx] = v;
        if (++idx >= (int) buf.size()) idx = 0;
        return d - g * v;
    }

private:
    std::vector<float> buf;
    int idx = 0;
};

// Per-sample one-pole parameter smoother.
struct Smoother
{
    float y = 0.0f, a = 0.001f;
    void setTime (float seconds, double sr) { a = onePoleCoef (seconds, sr); }
    float next (float target) { y += a * (target - y); return y; }
    void snap (float v) { y = v; }
};

//==============================================================================
// Control values in physical units. The processor maps the 0-10 knobs to these.
struct Params
{
    int   mode        = Classic;
    float delaySec    = 0.2f;   // 0.02 .. 2 s
    float feedback    = 0.4f;   // loop gain 0 .. ~1.12 (>1 = self-oscillation, bounded by tanh)
    float wetGain     = 0.5f;   // linear
    float modRateHz   = 0.6f;
    float modDepthSec = 0.0f;   // 0 .. 0.008 s
    float variation   = 0.5f;   // 0..1, per-mode macro
    bool  power       = true;
};

//==============================================================================
class Engine
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        const int lineLen = (int) std::ceil ((kMaxDelaySec + kMaxModSec + 0.05f) * (float) sr) + 8;
        for (auto& ch : chans)
            ch.line.prepare (lineLen);

        // Ambience diffuser lengths in ms (L/R detuned for decorrelation)
        static constexpr float apMs[2][kNumAP] = { { 3.1f, 4.7f, 7.9f, 11.3f },
                                                   { 3.4f, 5.1f, 8.6f, 12.1f } };
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < kNumAP; ++k)
                chans[c].ap[k].prepare ((int) std::round (apMs[c][k] * 0.001f * (float) sr));

        powerSm.setTime (0.015f, sr);   // power switch fade
        fbSm.setTime (0.03f, sr);
        wetSm.setTime (0.03f, sr);
        varSm.setTime (0.05f, sr);
        depthSm.setTime (0.05f, sr);
        satSm.setTime (0.05f, sr);
        compSm.setTime (0.02f, sr);
        panCoef   = onePoleCoef (0.005f, sr);  // pan gain smoothing (rounds the triangle corners)
        slotCoef  = onePoleCoef (0.02f, sr);   // tap gain changes when Variation steps
        delayCoef = onePoleCoef (0.12f, sr);   // delay-time glide (pitch bends like a BBD clock sweep)
        fadeStep  = 1.0f / (0.010f * (float) sr); // 10 ms out + 10 ms in on mode change
        dblInc    = dblIncHz / (float) sr;        // doubler LFO

        // Non-linear onset detector
        fastAtt = onePoleCoef (0.001f, sr); fastRel = onePoleCoef (0.03f, sr);
        slowAtt = onePoleCoef (0.04f, sr);  slowRel = onePoleCoef (0.25f, sr);

        lastLP = lastHP = -1.0f;
        reset();
    }

    void reset()
    {
        for (auto& ch : chans)
        {
            ch.line.reset();
            ch.hp.reset(); ch.lp1.reset(); ch.lp2.reset();
            for (auto& a : ch.ap) a.reset();
        }
        lfo1 = lfo2 = lfoD = 0.0f;
        lfo1R = 0.25f;
        beatPhase = nlPhase = 0.0f;
        panPhase = 0.0f;
        panGSm[0] = panGSm[1] = 1.0f;
        envFast = envSlow = 0.0f;
        holdoff = 0;
    }

    // Call once per block before process(). snap = jump smoothers to targets (after prepare).
    void setParams (const Params& p, bool snap = false)
    {
        const int m = std::clamp (p.mode, 1, kNumModes);
        targetMode = m;
        if (snap) { activeMode = m; modeFade = 1.0f; }

        const float v = clampf (p.variation, 0.0f, 1.0f);
        const float dSec = clampf (p.delaySec, kMinDelaySec, kMaxDelaySec);

        varTarget   = v;
        powerTarget = p.power ? 1.0f : 0.0f;
        fbTarget    = clampf (p.feedback, 0.0f, 1.15f);
        wetTarget   = std::max (0.0f, p.wetGain);
        depthTarget = clampf (p.modDepthSec, 0.0f, kMaxModSec) * (float) sr;
        lfoInc      = std::max (0.0f, p.modRateHz) / (float) sr;

        dTarget[0] = dSec * (float) sr;
        // Dual Mod: second delay = d * (1 - 0.5 v)  (v=0 same time, v=1 half time)
        dTarget[1] = m == DualMod ? dTarget[0] * (1.0f - 0.5f * v) : dTarget[0];

        // ---- per-mode voicing: loop filters + saturator headroom ----------------
        // (cutoffs in Hz; all approximations, not measured from hardware)
        float lp = 9000.0f, hp = 30.0f, sat = 1.2f;
        switch (m)
        {
            case Classic:   lp = 1500.0f * std::pow (10.0f, v); hp = 30.0f; sat = 1.2f; break; // 1.5k..15k
            case Vintage:
            {
                // BBD: longer delay = lower clock = darker. ~6 kHz @ 20 ms .. ~2.2 kHz @ 2 s,
                // then Variation darkens further (x1 .. x0.45) and lowers the clip ceiling.
                lp  = 6000.0f * std::pow (kMinDelaySec / dSec, 0.22f) * (1.0f - 0.55f * v);
                hp  = 80.0f;
                sat = 1.0f / (1.0f + 2.0f * v);   // headroom 1.0 .. 0.33 => grit on louder repeats
                break;
            }
            case Modern:    lp = 16000.0f; hp = 20.0f * std::pow (40.0f, v); sat = 1.4f; break; // low cut 20..800 Hz
            case MultiHead: lp = 5000.0f;  hp = 70.0f; sat = 0.9f; break;                      // tape-ish
            case NonLinear: lp = 9000.0f;  hp = 30.0f; break;
            case Ambience:  lp = 7000.0f * (1.0f - 0.4f * v); hp = 60.0f; break;
            case Reflect:   lp = 8000.0f;  hp = 40.0f; break;
            case DoublingDelay: lp = 10000.0f; hp = 30.0f; break;
            case Wide:      lp = 9000.0f;  hp = 30.0f; break;
            case DualMod:   lp = 7500.0f;  hp = 40.0f; break;
            case Pan:       lp = 9000.0f;  hp = 30.0f; break;
            case Pattern:   lp = 9000.0f;  hp = 40.0f; break;
            default: break;
        }
        lp = clampf (lp, 200.0f, 0.45f * (float) sr);
        satTarget = sat;

        if (std::abs (lp - lastLP) > 0.005f * lp || std::abs (hp - lastHP) > 0.005f * hp)
        {
            for (auto& ch : chans)
            {
                ch.lp1.setCutoff (lp, sr);
                ch.lp2.setCutoff (lp, sr);
                ch.hp.setCutoff (hp, sr);
            }
            lastLP = lp; lastHP = hp;
        }

        // ---- Multi-head: Variation selects 1 of 7 head combinations ----------
        // 0:H1  1:H2  2:H3  3:H1+H2  4:H2+H3  5:H1+H3  6:H1+H2+H3   (H1=d/3, H2=2d/3, H3=d)
        static constexpr int combos[7] = { 0b001, 0b010, 0b100, 0b011, 0b110, 0b101, 0b111 };
        const int mask = combos[std::clamp ((int) std::lround (v * 6.0f), 0, 6)];
        const int n = (mask & 1) + ((mask >> 1) & 1) + ((mask >> 2) & 1);
        for (int k = 0; k < 3; ++k)
            headTarget[k] = (mask >> k) & 1 ? 1.0f / (float) n : 0.0f;  // loop weights sum to 1 => stable
        compTarget = std::sqrt ((float) n);                              // output loudness compensation

        // ---- Pan: Variation = sweep speed, expressed in repeats per full L->R->L cycle ----
        // Variation knob 0..10 (v 0..1) -> cycle length N = 16 * 8^(-v) repeats, exponential:
        //   knob 0 = 16 repeats per cycle (slow drift), knob 5 = ~5.7, knob 10 = 2 (repeats
        //   alternate hard L / hard R like a ping-pong). The sweep is tied to the delay time,
        //   so the pan rate in Hz = 1 / (N * delaySec); width is always full (L to R).
        panRepeats = 16.0f * std::pow (8.0f, -v);

        // ---- Pattern: Variation selects 1 of 7 patterns ---------------------
        const int pat = std::clamp ((int) std::lround (v * 6.0f), 0, 6);
        for (int k = 0; k < kNumSlots; ++k)
            slotTarget[k] = kPatterns[pat][k];

        if (snap)
        {
            powerSm.snap (powerTarget); fbSm.snap (fbTarget); wetSm.snap (wetTarget);
            varSm.snap (varTarget); depthSm.snap (depthTarget); satSm.snap (satTarget);
            compSm.snap (compTarget);
            for (int k = 0; k < 3; ++k) headSm[k] = headTarget[k];
            for (int k = 0; k < kNumSlots; ++k) slotSm[k] = slotTarget[k];
            dSm[0] = dTarget[0]; dSm[1] = dTarget[1];
        }
    }

    // In-place. right == nullptr => mono (internal stereo engine summed to L).
    void process (float* left, float* right, int numSamples)
    {
        const bool stereo = right != nullptr;

        for (int i = 0; i < numSamples; ++i)
        {
            const float inL = left[i];
            const float inR = stereo ? right[i] : inL;
            const float mono = 0.5f * (inL + inR);

            // ---- mode crossfade (wet + loop dip 10 ms, switch, 10 ms back) ----
            if (activeMode != targetMode)
            {
                modeFade -= fadeStep;
                if (modeFade <= 0.0f) { modeFade = 0.0f; activeMode = targetMode; }
            }
            else if (modeFade < 1.0f)
                modeFade = std::min (1.0f, modeFade + fadeStep);

            // ---- smoothed controls ----
            const float power = powerSm.next (powerTarget);
            const float fb    = fbSm.next (fbTarget) * power * modeFade;   // power off => loop drains
            const float wetG  = wetSm.next (wetTarget) * power * modeFade;
            const float v     = varSm.next (varTarget);
            const float depth = depthSm.next (depthTarget);
            const float satH  = satSm.next (satTarget);
            const float comp  = compSm.next (compTarget);
            dSm[0] += delayCoef * (dTarget[0] - dSm[0]);
            dSm[1] += delayCoef * (dTarget[1] - dSm[1]);
            for (int k = 0; k < 3; ++k)         headSm[k] += slotCoef * (headTarget[k] - headSm[k]);
            for (int k = 0; k < kNumSlots; ++k) slotSm[k] += slotCoef * (slotTarget[k] - slotSm[k]);

            // ---- LFOs (sine; R in quadrature, Dual Mod uses an independent 1.37x LFO) ----
            lfo1 += lfoInc;         if (lfo1 >= 1.0f) lfo1 -= 1.0f;
            lfo1R = lfo1 + 0.25f;   if (lfo1R >= 1.0f) lfo1R -= 1.0f;
            lfo2 += lfoInc * 1.37f; if (lfo2 >= 1.0f) lfo2 -= 1.0f;
            lfoD += dblInc;         if (lfoD >= 1.0f) lfoD -= 1.0f;

            // delay modulation is unipolar (0..depth) so the read point never gets shorter than d
            const float mod0 = depth * 0.5f * (1.0f + std::sin (kTwoPi * lfo1));
            const float mod1 = depth * 0.5f * (1.0f + std::sin (kTwoPi * (activeMode == DualMod ? lfo2 : lfo1R)));
            const float dd[2] = { dSm[0] + mod0, dSm[1] + mod1 };

            // ---- beat clock (tap LED) and Non-linear envelope phase ----
            const float phaseInc = 1.0f / std::max (1.0f, dSm[0]);
            beatPhase += phaseInc; if (beatPhase >= 1.0f) beatPhase -= 1.0f;
            nlPhase   += phaseInc; if (nlPhase   >= 1.0f) nlPhase   -= 1.0f;
            panPhase  += phaseInc / panRepeats; if (panPhase >= 1.0f) panPhase -= 1.0f;
            {
                // onset detector: a transient restarts the envelope so it lines up with
                // each repeat of that note (repeats arrive at t0 + k*d)
                const float a = std::abs (mono);
                envFast += (a > envFast ? fastAtt : fastRel) * (a - envFast);
                envSlow += (a > envSlow ? slowAtt : slowRel) * (a - envSlow);
                if (holdoff > 0) --holdoff;
                else if (envFast > 0.003f && envFast > 1.8f * envSlow)
                {
                    nlPhase = 0.0f;
                    holdoff = (int) (0.08 * sr);
                }
            }

            // ---- read taps + loop tone stage ----
            float proc[2];
            for (int c = 0; c < 2; ++c)
            {
                auto& ch = chans[c];
                float t;
                if (activeMode == MultiHead)
                    t = headSm[0] * ch.line.read (dd[c] * (1.0f / 3.0f))
                      + headSm[1] * ch.line.read (dd[c] * (2.0f / 3.0f))
                      + headSm[2] * ch.line.read (dd[c]);
                else
                    t = ch.line.read (dd[c]);

                float x = ch.lp2.process (ch.lp1.process (ch.hp.process (t)));
                if (activeMode == Ambience)
                {
                    const float g = 0.35f + 0.4f * v;   // diffusion 0.35 .. 0.75
                    for (auto& ap : ch.ap) x = ap.process (x, g);
                }
                proc[c] = satH * std::tanh (x / satH);  // bounds every feedback path
            }

            // ---- per-mode routing ----
            float wet[2]  = { proc[0], proc[1] };
            float loop[2] = { proc[0], proc[1] };
            float feed[2] = { inL, inR };

            switch (activeMode)
            {
                case MultiHead:
                    wet[0] *= comp; wet[1] *= comp;
                    break;

                case NonLinear:
                {
                    const float e = nonLinearEnv (nlPhase, v);
                    wet[0] *= e; wet[1] *= e;   // shape output only, loop untouched => each repeat shaped once
                    break;
                }

                case Reflect:
                {
                    // 6 early reflections spread over min(d, 15..150 ms) (room size = Variation)
                    for (int c = 0; c < 2; ++c)
                    {
                        const float span = std::min (dSm[c], 0.015f * std::pow (10.0f, v) * (float) sr);
                        float er = 0.0f;
                        for (int k = 0; k < kNumER; ++k)
                            er += kERGain[k] * chans[c].line.read (kERFrac[c][k] * span);
                        wet[c] = 0.7f * proc[c] + 0.35f * er;
                    }
                    break;
                }

                case DoublingDelay:
                {
                    // doubler: 17 ms (L) / 23 ms (R) +-1.2 ms at 0.7 Hz, read from the same line
                    const float dm = 0.0012f * (float) sr * std::sin (kTwoPi * lfoD);
                    const float dbl0 = chans[0].line.read (0.017f * (float) sr + dm);
                    const float dbl1 = chans[1].line.read (0.023f * (float) sr - dm);
                    const float gDel = std::sin (0.5f * kPi * v);  // equal-power balance
                    const float gDbl = std::cos (0.5f * kPi * v);
                    wet[0] = gDel * proc[0] + gDbl * dbl0;
                    wet[1] = gDel * proc[1] + gDbl * dbl1;
                    break;
                }

                case Wide:
                {
                    // v=0: two parallel mono delays; v=1: full ping-pong (input to L only, cross feedback).
                    // Loop matrix [[1-v, v],[v, 1-v]] has eigenvalues 1 and 1-2v => no extra gain.
                    feed[0] = mono;
                    feed[1] = mono * (1.0f - v);
                    loop[0] = (1.0f - v) * proc[0] + v * proc[1];
                    loop[1] = (1.0f - v) * proc[1] + v * proc[0];
                    break;
                }

                case DualMod:
                    feed[0] = feed[1] = mono;
                    wet[0] = (proc[0] + 0.35f * proc[1]) * 0.8f;
                    wet[1] = (proc[1] + 0.35f * proc[0]) * 0.8f;
                    break;

                case Pan:
                {
                    // Mono-summed delay with mono feedback (same loop as Classic => same tanh bound).
                    // Both lines are fed so a later mode switch finds them warm; only line 0 is heard.
                    // Pan position: triangle LFO -1 (L) .. +1 (R); equal-power law
                    // gL = cos(th), gR = sin(th), th = (pos + 1) * pi/4, scaled by sqrt(2) so the
                    // total power matches the other (two-channel) modes. Gains smoothed over 5 ms.
                    feed[0] = feed[1] = mono;
                    const float pos = 1.0f - 4.0f * std::abs (panPhase - 0.5f);   // -1 .. +1 .. -1
                    const float th  = (pos + 1.0f) * (0.25f * kPi);
                    panGSm[0] += panCoef * (kSqrt2 * std::cos (th) - panGSm[0]);
                    panGSm[1] += panCoef * (kSqrt2 * std::sin (th) - panGSm[1]);
                    if (stereo) { wet[0] = panGSm[0] * proc[0]; wet[1] = panGSm[1] * proc[0]; }
                    else        { wet[0] = wet[1] = proc[0]; }   // mono bus: just the summed delay
                    break;
                }

                case Pattern:
                {
                    // loudness normalisation: more taps => each a bit quieter
                    float slotSum = 0.0f;
                    for (int k = 0; k < kNumSlots; ++k) slotSum += slotSm[k];
                    const float norm = 1.0f / (1.0f + 0.35f * slotSum);
                    for (int c = 0; c < 2; ++c)
                    {
                        float acc = proc[c];   // main tap at d (also the feedback tap)
                        for (int k = 0; k < kNumSlots; ++k)
                            if (slotSm[k] > 1.0e-4f)
                                acc += slotSm[k] * kSlotPan[c][k] * chans[c].line.read (dd[c] * kSlotFrac[k]);
                        wet[c] = acc * norm;
                    }
                    break;
                }

                default: break;   // Classic / Vintage / Modern / Ambience: plain repeat
            }

            // ---- record ----
            for (int c = 0; c < 2; ++c)
                chans[c].line.write (feed[c] * power + fb * loop[c]);

            // ---- output: dry always at unity (buffered bypass), wet faded by power ----
            if (stereo)
            {
                left[i]  = inL + wetG * wet[0];
                right[i] = inR + wetG * wet[1];
            }
            else
                left[i] = inL + wetG * 0.5f * (wet[0] + wet[1]);
        }
    }

    float getBeatPhase() const { return beatPhase; }

private:
    // Non-linear envelope over one repeat (p = 0..1 of the delay period).
    // v 0..0.5: blend from gate (open 35 % of the period) to flat; v 0.5..1: flat to swell.
    // Both shapes are 0 at p=0 and p=1, so the envelope is continuous at the wrap.
    // Approximation: a "reverse-ish" swell, not true reversed playback.
    static float nonLinearEnv (float p, float v)
    {
        if (v < 0.5f)
        {
            const float depth = 1.0f - 2.0f * v;
            const float gate = std::min (1.0f, p / 0.01f) * clampf ((0.40f - p) / 0.05f, 0.0f, 1.0f);
            return 1.0f + depth * (gate - 1.0f);
        }
        const float depth = 2.0f * v - 1.0f;
        const float swell = p * p * std::min (1.0f, (1.0f - p) / 0.04f);
        return 1.0f + depth * (swell - 1.0f);
    }

    static constexpr int kNumAP = 4;
    static constexpr int kNumER = 6;
    static constexpr int kNumSlots = 9;

    // Reflect: tap positions as fraction of the span (L/R differ) and gains
    static constexpr float kERFrac[2][kNumER] = { { 0.11f, 0.21f, 0.33f, 0.48f, 0.67f, 0.90f },
                                                  { 0.14f, 0.25f, 0.38f, 0.56f, 0.75f, 1.00f } };
    static constexpr float kERGain[kNumER] = { 0.62f, 0.55f, 0.48f, 0.42f, 0.36f, 0.30f };

    // Pattern: extra tap slots as fraction of d (the main tap at d is always on)
    static constexpr float kSlotFrac[kNumSlots] = { 0.125f, 0.25f, 0.375f, 0.5f, 0.625f, 0.75f, 0.875f,
                                                    1.0f / 3.0f, 2.0f / 3.0f };
    static constexpr float kSlotPan[2][kNumSlots] = { { 1.0f, 0.55f, 1.0f, 0.55f, 1.0f, 0.55f, 1.0f, 1.0f, 0.55f },
                                                      { 0.55f, 1.0f, 0.55f, 1.0f, 0.55f, 1.0f, 0.55f, 0.55f, 1.0f } };
    //                                                1/8   1/4   3/8   1/2   5/8   3/4   7/8   1/3   2/3
    static constexpr float kPatterns[7][kNumSlots] = {
        { 0.0f, 0.0f, 0.0f, 0.8f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },   // 0: straight 8ths
        { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.8f, 0.0f, 0.0f, 0.0f },   // 1: dotted feel
        { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.7f, 0.8f },   // 2: triplets
        { 0.0f, 0.6f, 0.0f, 0.75f, 0.0f, 0.6f, 0.0f, 0.0f, 0.0f },  // 3: 16ths
        { 0.0f, 0.0f, 0.7f, 0.0f, 0.0f, 0.7f, 0.0f, 0.0f, 0.0f },   // 4: 3-3-2 syncopation
        { 0.0f, 0.0f, 0.0f, 0.7f, 0.55f, 0.7f, 0.0f, 0.0f, 0.0f },  // 5: gallop
        { 0.25f, 0.3f, 0.35f, 0.4f, 0.45f, 0.5f, 0.55f, 0.0f, 0.0f } // 6: rising 32nd roll
    };

    struct Channel
    {
        DelayLine line;
        OnePoleHP hp;
        OnePoleLP lp1, lp2;
        Allpass ap[kNumAP];
    };

    double sr = 44100.0;
    Channel chans[2];

    int targetMode = Classic, activeMode = Classic;
    float modeFade = 1.0f, fadeStep = 0.001f;

    Smoother powerSm, fbSm, wetSm, varSm, depthSm, satSm, compSm;
    float powerTarget = 1.0f, fbTarget = 0.0f, wetTarget = 0.0f, varTarget = 0.5f,
          depthTarget = 0.0f, satTarget = 1.2f, compTarget = 1.0f;

    float dTarget[2] = { 4410.0f, 4410.0f }, dSm[2] = { 4410.0f, 4410.0f };
    float delayCoef = 0.001f, slotCoef = 0.001f;

    float headTarget[3] = { 0.0f, 0.0f, 1.0f }, headSm[3] = { 0.0f, 0.0f, 1.0f };
    float slotTarget[kNumSlots] {}, slotSm[kNumSlots] {};

    float lfo1 = 0.0f, lfo1R = 0.25f, lfo2 = 0.0f, lfoD = 0.0f, lfoInc = 0.0f;
    static constexpr float dblIncHz = 0.7f;
    float dblInc = 0.0f;

    float beatPhase = 0.0f, nlPhase = 0.0f;

    static constexpr float kSqrt2 = 1.41421356f;
    float panPhase = 0.0f, panRepeats = 5.66f, panCoef = 0.005f;
    float panGSm[2] = { 1.0f, 1.0f };
    float envFast = 0.0f, envSlow = 0.0f, fastAtt = 0.1f, fastRel = 0.01f, slowAtt = 0.01f, slowRel = 0.001f;
    int holdoff = 0;

    float lastLP = -1.0f, lastHP = -1.0f;
};

} // namespace dm1
