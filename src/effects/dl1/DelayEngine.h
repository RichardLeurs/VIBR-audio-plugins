// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

// DL-1 Delay Lab: one delay engine with the voicings of the VIBR delays as modes (see .claude/lab_specs.md). Built so far:
//   DIGITAL  the SD-1 Digital Delay: clean, exact repeats, a two-pole hi-cut in the feedback path, a soft-clipped input, DIRECT and EFFECT levels, TIME FACTOR
//            (x0.25 .. x1); type DUCKED lets the repeats duck under the playing (new)
//   ANALOG   the MM-1 Analog Delay: a bucket-brigade style three-pole reconstruction filter that darkens longer delays, a soft input stage and soft regeneration;
//            types DELAY (as the MM-1), CHORUS (a short delay, the dry sound added) and VIBRATO (a short delay, wet only)
// Shared by the modes: TIME (ms, or a note length of the host tempo), FEEDBACK, LOW CUT and HI CUT in the loop, a modulation of the delay time (rate, depth),
// ping-pong SPREAD, HOLD (an endless loop), MIX and LEVEL. Header-only, no JUCE dependency, no allocation after prepare().
#include <algorithm>
#include <atomic>
#include <cmath>
#include <vector>
#include "../dm1/DM1DSP.h"

namespace dl1
{
constexpr float kPi = 3.14159265358979f, kTwoPi = 6.28318530717959f;
constexpr int kNumModes = 5, kNumVoices = dm1::kNumModes, kNumDivisions = 12;
enum Mode { Digital, Analog, Tape, Dual, Modulated };

inline int clampMode (int m) { return std::clamp (m, 0, kNumModes - 1); }
inline const char* modeName (int m) { static const char* n[kNumModes] = { "DIGITAL", "ANALOG", "TAPE", "DUAL", "MODULATED" }; return n[clampMode (m)]; }
inline int variantCount (int mode) { return mode == Analog || mode == Tape ? 3 : mode == Modulated ? 1 : 2; }
inline const char* voiceName (int v)
{
    static const char* n[kNumVoices] = { "CLASSIC", "VINTAGE", "MODERN", "MULTI-HEAD", "NON-LINEAR", "AMBIENCE", "REFLECT", "DOUBLING + DELAY", "WIDE", "DUAL MOD", "PAN", "PATTERN" };
    return n[std::clamp (v, 0, kNumVoices - 1)];
}
inline const char* variantTitle (int mode) { static const char* n[kNumModes] = { "TYPE", "TYPE", "HEADS", "ROUTING", "VOICING" }; return n[clampMode (mode)]; }
inline const char* variantName (int mode, int v)
{
    static const char* n[kNumModes][3] = { { "PLAIN", "DUCKED", "" }, { "DELAY", "CHORUS", "VIBRATO" }, { "1 HEAD", "2 HEADS", "3 HEADS" }, { "PARALLEL", "SERIES", "" }, { "DM-1", "", "" } };
    return n[clampMode (mode)][std::clamp (v, 0, 2)];
}

// tempo sync: the time is one of these note lengths (a beat = a quarter note)
inline const char* divisionName (int i)
{
    static const char* n[kNumDivisions] = { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4 DOT", "1/8 DOT", "1/16 DOT", "1/4 TRIP", "1/8 TRIP", "1/16 TRIP" };
    return n[std::clamp (i, 0, kNumDivisions - 1)];
}
inline float divisionBeats (int i)
{
    static const float b[kNumDivisions] = { 4.0f, 2.0f, 1.0f, 0.5f, 0.25f, 0.125f, 1.5f, 0.75f, 0.375f, 2.0f / 3.0f, 1.0f / 3.0f, 1.0f / 6.0f };
    return b[std::clamp (i, 0, kNumDivisions - 1)];
}
inline float divisionMs (int i, float bpm) { return 60000.0f / std::max (20.0f, bpm) * divisionBeats (i); }

// a delay line with 4-point Hermite interpolation for the modulated read (as in the SD-1 and MM-1)
class DelayLine
{
public:
    void prepare (float sampleRate, float seconds)
    {
        int n = 1; while ((float) n < sampleRate * seconds) n <<= 1;
        buffer.assign ((size_t) n, 0.0f); mask = n - 1; writePos = 0; maxDelay = (float) (n - 8);
    }
    void reset() { std::fill (buffer.begin(), buffer.end(), 0.0f); }
    void write (float x) { buffer[(size_t) writePos] = x; writePos = (writePos + 1) & mask; }
    float getMaxDelay() const { return maxDelay; }
    // The same interpolation as the SD-1 and MM-1 (the position is `write - 1 - delay`), but the whole and the fractional part are kept apart: the originals add them in
    // a float, which past 65536 samples of buffer rounds the fraction to 1/128 of a sample and adds noise to a modulated delay.
    float read (float delaySamples) const                                                        // 2 <= delaySamples <= maxDelay
    {
        const int di = (int) std::floor (delaySamples);
        const float df = delaySamples - (float) di;
        const int i1 = df > 0.0f ? writePos - 2 - di : writePos - 1 - di;
        const float f = df > 0.0f ? 1.0f - df : 0.0f;
        const float y0 = buffer[(size_t) ((i1 - 1) & mask)], y1 = buffer[(size_t) (i1 & mask)], y2 = buffer[(size_t) ((i1 + 1) & mask)], y3 = buffer[(size_t) ((i1 + 2) & mask)];
        const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }
private:
    std::vector<float> buffer;
    int mask = 0, writePos = 0;
    float maxDelay = 0.0f;
};

struct Params
{
    int mode = Digital, variant = 0;
    bool sync = false, hold = false;
    int division = 2;                                  // with sync: the note length
    float bpm = 120.0f;
    float timeMs = 350.0f, feedback = 0.4f;            // feedback 0 .. 1 (above 0.95 it self-oscillates, limited)
    float lowHz = 20.0f, highHz = 8000.0f;             // the loop filters; lowHz <= 20 = off
    float modRate = 1.0f, modDepth = 0.1f;             // Hz, 0 .. 1
    float spread = 0.0f;                               // 0 = two separate delays, 1 = ping-pong
    float mix = 1.0f;                                  // the whole effect against the input
    // DIGITAL
    float digFactor = 1.0f, digDirect = 0.7f, digEffect = 0.5f, digInput = 0.5f;           // all 0 .. 1 (the SD-1's knobs / 10)
    // ANALOG
    float anaBlend = 0.5f, anaDepth = 0.2f, anaGrit = 0.0f;                                // all 0 .. 1
    // TAPE (all 0 .. 1)
    float tapeWow = 0.3f, tapeSat = 0.3f, tapeAge = 0.0f, tapeSpacing = 0.5f;
    // DUAL: B is a ratio of A (0.25 .. 4); A uses the shared feedback and hi cut
    float dualRatio = 0.75f, dualFbB = 0.4f, dualLevelA = 0.8f, dualLevelB = 0.8f, dualHighB = 8000.0f;
    bool dualPingA = false, dualPingB = true;
    // MODULATED: the DM-1 voicing (0 .. 11) and its VARIATION macro and delay volume (0 .. 1); FEEDBACK is the INTENSITY, TIME, MOD RATE and MOD DEPTH are the DM-1's
    int modVoice = 0;
    float modVariation = 0.5f, modVolume = 0.5f;
};

// the delay time of tape head k (0 .. 2) as a multiple of TIME: 1, then 1 + step, 1 + 2 step with step = 2 * spacing (spacing 50 % = x1, x2, x3)
inline float tapeHeadRatio (int head, float spacing) { return 1.0f + (float) head * std::max (0.1f, 2.0f * std::clamp (spacing, 0.0f, 1.0f)); }

class Engine
{
public:
    std::atomic<float> inLevel { 0.0f }, outLevel { 0.0f };           // what the editor shows (peak, with a decay), written by the audio thread
    std::atomic<float> shownTimeMs { 350.0f }, shownTimeBMs { 262.0f };

    void prepare (float sampleRate)
    {
        fs = sampleRate;
        for (auto& c : ch) { c.line.prepare (fs, 2.4f); c.lineB.prepare (fs, 2.4f); }
        dm.prepare ((double) fs);
        reset();
    }

    void reset()
    {
        for (auto& c : ch) c.reset();
        dm.reset(); dmSnap = true;
        phase = 0.0f; wowPh = flPh = wow2Ph = 0.0f; drift = 0.0f; dropEnv = 0.0f; started = false; env = 0.0f; fade = 1.0f; lastMode = -1;
        inLevel.store (0.0f); outLevel.store (0.0f);
    }

    // `levelGain` is the output gain (linear)
    void process (float* l, float* r, int n, const Params& p, float levelGain)
    {
        if (p.mode != lastMode) { if (lastMode >= 0) { fade = 0.0f; for (auto& c : ch) c.reset(); dm.reset(); dmSnap = true; } lastMode = p.mode; }
        if (p.mode == Modulated) { processModulated (l, r, n, p, levelGain); return; }
        const float sm = 1.0f - std::exp (-1.0f / (0.03f * fs)), smSlow = 1.0f - std::exp (-1.0f / (0.05f * fs)), fadeInc = 1.0f / (0.03f * fs);
        const bool digital = p.mode == Digital, analog = p.mode == Analog, tape = p.mode == Tape, dual = p.mode == Dual;
        const bool chorus = analog && p.variant == 1, vibrato = analog && p.variant == 2;

        // the delay time the modes work with
        float timeMs = p.sync ? divisionMs (p.division, p.bpm) : p.timeMs;
        if (digital) timeMs *= 0.25f * std::pow (4.0f, std::clamp (p.digFactor, 0.0f, 1.0f));
        if (analog) timeMs = (chorus || vibrato) ? std::clamp (timeMs * 0.08f, 8.0f, 35.0f) : std::clamp (timeMs, 20.0f, 600.0f);
        timeMs = std::clamp (timeMs, 3.0f, 2048.0f);
        shownTimeMs.store (timeMs, std::memory_order_relaxed);
        const float timeBMs = std::clamp (timeMs * std::clamp (p.dualRatio, 0.25f, 4.0f), 3.0f, 2048.0f);
        shownTimeBMs.store (timeBMs, std::memory_order_relaxed);
        const int heads = tape ? 1 + std::clamp (p.variant, 0, 2) : 1;
        const float headRatio[3] = { 1.0f, tapeHeadRatio (1, p.tapeSpacing), tapeHeadRatio (2, p.tapeSpacing) };
        const float tWow = std::clamp (p.tapeWow, 0.0f, 1.0f), tSat = std::clamp (p.tapeSat, 0.0f, 1.0f), tAge = std::clamp (p.tapeAge, 0.0f, 1.0f);

        // the gains of the modes
        float fbT = std::clamp (p.feedback, 0.0f, 1.0f) * (digital ? 1.02f : 1.03f), fbBT = std::clamp (p.dualFbB, 0.0f, 1.0f) * 1.03f, inGainT = 1.0f, dryT, wetT, outT = 1.0f, modDepthT = std::clamp (p.modDepth, 0.0f, 1.0f);
        if (chorus) fbT *= 0.25f;
        if (vibrato) fbT = 0.0f;
        if (p.hold) { fbT = 1.0f; fbBT = 1.0f; }
        if (digital)
        {
            inGainT = std::clamp (p.digInput, 0.0f, 1.0f) * 2.0f;
            dryT = std::pow (std::clamp (p.digDirect, 0.0f, 1.0f), 1.6f) * 1.1f;
            wetT = std::pow (std::clamp (p.digEffect, 0.0f, 1.0f), 1.4f) * 1.3f;
        }
        else if (tape) { dryT = 1.0f; wetT = 0.9f; }
        else if (dual) { dryT = 1.0f; wetT = 1.0f; }
        else
        {
            const float blend = vibrato ? 1.0f : chorus ? 0.5f : std::clamp (p.anaBlend, 0.0f, 1.0f);
            dryT = std::cos (blend * 0.5f * kPi); wetT = std::sin (blend * 0.5f * kPi) * 1.25f; outT = std::pow (0.5f, 1.5f) * 2.2f;      // the MM-1 output stage at its default level (its LEVEL knob at 5)
            modDepthT = std::clamp (p.anaDepth, 0.0f, 1.0f);
        }
        const float holdT = p.hold ? 0.0f : 1.0f;                                                                 // HOLD: the input is muted, the loop runs
        const float rateT = std::clamp (p.modRate, 0.05f, 10.0f);
        const float highT = std::clamp (p.highHz, 500.0f, 20000.0f) * (tape ? 1.0f - 0.55f * tAge : 1.0f), highBT = std::clamp (p.dualHighB, 500.0f, 20000.0f);
        const float lowCoef = p.lowHz > 21.0f ? 1.0f - std::exp (-kTwoPi * p.lowHz / fs) : 0.0f;
        if (! started) { delayS = timeMs; fbS = fbT; inS = inGainT; dryS = dryT; wetS = wetT; outS = outT; depthS = modDepthT; rateS = rateT; holdS = holdT; cutS = highT; delayBS = timeBMs; fbBS = fbBT; highBS = highBT; mixS = p.mix; levelS = levelGain; started = true; }
        const float spread = std::clamp (p.spread, 0.0f, 1.0f);

        float peakIn = 0.0f, peakOut = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            delayS += (timeMs - delayS) * smSlow; fbS += (fbT - fbS) * sm; inS += (inGainT - inS) * sm; dryS += (dryT - dryS) * sm; wetS += (wetT - wetS) * sm;
            outS += (outT - outS) * sm; depthS += (modDepthT - depthS) * smSlow; rateS += (rateT - rateS) * smSlow; holdS += (holdT - holdS) * sm; cutS += (highT - cutS) * smSlow; delayBS += (timeBMs - delayBS) * smSlow; fbBS += (fbBT - fbBS) * sm; highBS += (highBT - highBS) * smSlow;
            mixS += (p.mix - mixS) * sm; levelS += (levelGain - levelS) * sm;
            fade = std::min (1.0f, fade + fadeInc);

            phase += kTwoPi * rateS / fs; if (phase > kTwoPi) phase -= kTwoPi;
            float cutHz = cutS, depthMs;
            float tapeEps = 0.0f, dropG = 1.0f;
            if (tape)
            {
                // the tape transport: wow (slow, about 0.5 Hz), flutter (about 7 Hz) and a drifting speed; head wear makes all of it worse. eps is the relative speed error
                wowPh += kTwoPi * 0.55f / fs; if (wowPh > kTwoPi) wowPh -= kTwoPi;
                wow2Ph += kTwoPi * 0.29f / fs; if (wow2Ph > kTwoPi) wow2Ph -= kTwoPi;
                flPh += kTwoPi * 7.1f / fs; if (flPh > kTwoPi) flPh -= kTwoPi;
                drift += (noise() - drift) * (kTwoPi * 0.4f / fs);
                const float wearK = 1.0f + 1.5f * tAge;
                tapeEps = tWow * wearK * (0.0030f * std::sin (wowPh) + 0.0016f * std::sin (wow2Ph) + 0.0009f * std::sin (flPh) + 0.012f * drift);
                if (tAge > 0.0f)                                                                              // drop-outs: now and then the level dips for a moment
                {
                    if (noise() > 0.9999f - 0.00005f * tAge * tAge) dropEnv = 1.0f;
                    dropEnv *= 1.0f - 1.0f / (0.03f * fs);
                    dropG = 1.0f - 0.7f * tAge * dropEnv;
                }
            }
            if (digital || dual || tape) depthMs = depthS * std::min (6.0f, 0.25f * delayS);
            else
            {
                const float bbd = std::clamp (14000.0f * std::sqrt (45.0f / delayS), 2200.0f, 9000.0f);            // longer delays = a lower clock = darker repeats
                cutHz = std::min (bbd, cutS);
                depthMs = depthS * depthS * std::min (9.0f, 0.30f * delayS);
                if (chorus || vibrato) depthMs = std::max (depthMs, 0.5f + 3.5f * std::clamp (p.anaDepth, 0.0f, 1.0f));
            }
            const float cutCoef = 1.0f - std::exp (-kTwoPi * std::min (cutHz, 0.45f * fs) / fs);

            const float xin[2] = { l[i], r[i] };
            float x[2], wet[2], loop[2], echoRaw[2], regen[2], y[2];
            const float mono = 0.5f * (xin[0] + xin[1]);
            peakIn = std::max (peakIn, std::max (std::abs (xin[0]), std::abs (xin[1])));
            const float satDrive = 1.0f + 3.0f * tSat;
            const float bumpCoef = 1.0f - std::exp (-kTwoPi * 90.0f / fs);

            // ducking of the repeats (type DUCKED): they dip while the input plays
            const float a = std::abs (mono);
            env += (a > env ? 1.0f - std::exp (-1.0f / (0.005f * fs)) : 1.0f - std::exp (-1.0f / (0.15f * fs))) * (a - env);
            const float duck = digital && p.variant == 1 ? 1.0f - 0.85f * std::clamp (env * 4.0f, 0.0f, 1.0f) : 1.0f;

            if (dual)
            {
                // two delays: A (TIME, FEEDBACK, HI CUT) and B (TIME B as a ratio of A, FEEDBACK B, HI CUT B); PARALLEL feeds both from the input, SERIES feeds B from A
                const float spreadA = p.dualPingA ? 1.0f : spread, spreadB = p.dualPingB ? 1.0f : spread;
                const float cutCoefB = 1.0f - std::exp (-kTwoPi * std::min (highBS, 0.45f * fs) / fs);
                const float depthB = depthS * std::min (6.0f, 0.25f * delayBS);
                float wa[2], wb[2];
                for (int c = 0; c < 2; ++c)
                {
                    auto& s = ch[c];
                    const float raw = std::isfinite (xin[c]) ? xin[c] : 0.0f;
                    x[c] = std::tanh (raw);
                    const float lfo = std::sin (phase + (float) c * 0.5f * kPi), lfoB = std::sin (phase + (float) c * 0.5f * kPi + 1.3f);
                    const float eA = s.line.read (std::clamp ((delayS + depthMs * lfo) * 0.001f * fs, 2.0f, s.line.getMaxDelay()));
                    const float eB = s.lineB.read (std::clamp ((delayBS + depthB * lfoB) * 0.001f * fs, 2.0f, s.lineB.getMaxDelay()));
                    s.cut1 += cutCoef * (eA - s.cut1); s.cut2 += cutCoef * (s.cut1 - s.cut2);
                    s.b1 += cutCoefB * (eB - s.b1); s.b2 += cutCoefB * (s.b1 - s.b2);
                    float fa = s.cut2, fb = s.b2;
                    if (lowCoef > 0.0f) { s.hp += lowCoef * (fa - s.hp); fa -= s.hp; s.bhp += lowCoef * (fb - s.bhp); fb -= s.bhp; }
                    wa[c] = fa; wb[c] = fb;
                }
                for (int c = 0; c < 2; ++c)
                {
                    const float raw = std::isfinite (xin[c]) ? xin[c] : 0.0f, rawO = std::isfinite (xin[1 - c]) ? xin[1 - c] : 0.0f, mn = 0.5f * (raw + rawO);
                    const auto pingIn = [&] (float sp) { return c == 0 ? raw + sp * (mn - raw) : raw * (1.0f - sp); };           // ping-pong: the input goes to the left line only
                    const float inA = std::tanh (pingIn (spreadA));
                    const float inB = p.variant == 1 ? wa[c] * holdS + 0.0f : std::tanh (pingIn (spreadB));              // SERIES: B hears A's repeats
                    const float fa = wa[c] + spreadA * (wa[1 - c] - wa[c]), fb = wb[c] + spreadB * (wb[1 - c] - wb[c]);
                    const float rA = p.hold ? softLimit (fa * fbS) : std::tanh (fa * fbS), rB = p.hold ? softLimit (fb * fbBS) : std::tanh (fb * fbBS);
                    ch[c].line.write (inA * holdS + rA);
                    ch[c].lineB.write (inB * (p.variant == 1 ? 1.0f : holdS) + rB);
                    y[c] = x[c] + std::clamp (p.dualLevelA, 0.0f, 1.0f) * wa[c] + std::clamp (p.dualLevelB, 0.0f, 1.0f) * wb[c];
                }
            }
            else
            {
                for (int c = 0; c < 2; ++c)
                {
                    auto& s = ch[c];
                    const float raw = std::isfinite (xin[c]) ? xin[c] : 0.0f;
                    // ping-pong: the input goes to the left line, what the left line repeats goes to the right line
                    const float src = c == 0 ? raw + spread * (mono - raw) * 1.0f : raw * (1.0f - spread);
                    x[c] = digital ? std::tanh (src * inS) : tape ? std::tanh (src * satDrive) / (0.6f + 0.4f * satDrive) : std::tanh (src * 1.3f) / 1.3f;
                    if (tape && tAge > 0.0f) x[c] += 0.0006f * tAge * noise();                                   // hiss of worn tape
                    const float lfo = std::sin (phase + (float) c * 0.5f * kPi);
                    const float base = tape ? (delayS * (1.0f + tapeEps) + depthMs * lfo) : delayS + depthMs * lfo;
                    const float echo = s.line.read (std::clamp (base * 0.001f * fs, 2.0f, s.line.getMaxDelay()));
                    echoRaw[c] = echo;
                    s.cut1 += cutCoef * (echo - s.cut1); s.cut2 += cutCoef * (s.cut1 - s.cut2);
                    float filtered = digital || tape ? s.cut2 : 0.0f;
                    if (analog) { s.cut3 += cutCoef * (s.cut2 - s.cut3); filtered = s.cut3; }
                    if (tape) { s.bump += bumpCoef * (filtered - s.bump); filtered += 0.35f * s.bump; filtered *= dropG; }       // the head bump: the lows swell a little
                    if (lowCoef > 0.0f) { s.hp += lowCoef * (filtered - s.hp); filtered -= s.hp; }                      // the low cut in the loop
                    wet[c] = loop[c] = filtered;
                    if (tape && heads > 1)                                                                                // the other heads: more taps of the same tape, each a bit softer and darker
                    {
                        float more = 0.75f * s.line.read (std::clamp (base * headRatio[1] * 0.001f * fs, 2.0f, s.line.getMaxDelay()));
                        if (heads > 2) more += 0.55f * s.line.read (std::clamp (base * headRatio[2] * 0.001f * fs, 2.0f, s.line.getMaxDelay()));
                        s.x1 += cutCoef * (more - s.x1); s.x2 += cutCoef * (s.x1 - s.x2);
                        wet[c] += s.x2 * dropG;
                    }
                }
                // feedback: each line gets its own repeats, or (ping-pong) the other line's (tape: from the first head)
                for (int c = 0; c < 2; ++c)
                {
                    const float own = loop[c], other = loop[1 - c];                                                    // tape: the loop closes round the first head only
                    const float fbIn = own + spread * (other - own);
                    if (p.hold) { regen[c] = softLimit (fbIn * fbS); continue; }                                       // HOLD: unity gain, only the peaks are limited (tanh would lose level on every pass)
                    if (tape) regen[c] = std::tanh (fbIn * fbS * satDrive) / satDrive;
                    else regen[c] = digital ? std::tanh (fbIn * fbS) : std::tanh (fbIn * fbS * (1.0f + 1.6f * std::clamp (p.anaGrit, 0.0f, 1.0f)));
                    if (analog && p.anaGrit > 0.0f) regen[c] *= 1.0f / (1.0f + 0.6f * p.anaGrit);
                }
                for (int c = 0; c < 2; ++c)
                {
                    ch[c].line.write (x[c] * holdS + regen[c]);
                    y[c] = digital ? x[c] * dryS + echoRaw[c] * wetS * duck : (x[c] * dryS + wet[c] * wetS) * outS;       // the SD-1 hears the raw echo, the MM-1 the filtered one
                }
            }
            for (int c = 0; c < 2; ++c)
            {
                const float in = xin[c];
                const float m = mixS * fade;
                float o = (in + (y[c] - in) * m) * levelS;
                if (! std::isfinite (o)) o = 0.0f;
                (c == 0 ? l : r)[i] = o;
                peakOut = std::max (peakOut, std::abs (o));
            }
        }
        const float decay = std::pow (0.88f, (float) n / 512.0f);
        inLevel.store (std::max (peakIn, inLevel.load (std::memory_order_relaxed) * decay), std::memory_order_relaxed);
        outLevel.store (std::max (peakOut, outLevel.load (std::memory_order_relaxed) * decay), std::memory_order_relaxed);
    }

private:
    // MODULATED: the DM-1 engine (12 voicings, see src/effects/dm1) between the shared TIME, FEEDBACK (the DM-1's INTENSITY), MOD RATE / DEPTH, MIX and LEVEL;
    // the loop filters of the other modes are the DM-1's own tone stage here
    void processModulated (float* l, float* r, int n, const Params& p, float levelGain)
    {
        const float timeMs = std::clamp (p.sync ? divisionMs (p.division, p.bpm) : p.timeMs, 20.0f, 2000.0f);
        shownTimeMs.store (timeMs, std::memory_order_relaxed);
        dm1::Params d;
        d.mode = std::clamp (p.modVoice, 0, kNumVoices - 1) + 1;
        d.delaySec = timeMs * 0.001f;
        d.feedback = p.hold ? 1.0f : 1.12f * std::pow (std::clamp (p.feedback, 0.0f, 1.0f), 1.2f);
        d.wetGain = 1.25f * std::pow (std::clamp (p.modVolume, 0.0f, 1.0f), 1.5f);
        d.modRateHz = std::clamp (p.modRate, 0.05f, 10.0f);
        d.modDepthSec = 0.008f * std::pow (std::clamp (p.modDepth, 0.0f, 1.0f), 2.0f);
        d.variation = std::clamp (p.modVariation, 0.0f, 1.0f);
        dm.setParams (d, dmSnap);
        if (dmSnap) { mixS = p.mix; levelS = levelGain; dmSnap = false; }
        float dry[2][kMaxBlock];
        float peakIn = 0.0f, peakOut = 0.0f;
        for (int pos = 0; pos < n; pos += kMaxBlock)
        {
            const int m = std::min (kMaxBlock, n - pos);
            float* a = l + pos; float* b = r + pos;
            for (int i = 0; i < m; ++i) { dry[0][i] = std::isfinite (a[i]) ? a[i] : 0.0f; dry[1][i] = std::isfinite (b[i]) ? b[i] : 0.0f; a[i] = dry[0][i]; b[i] = dry[1][i]; }
            dm.process (a, b, m);
            for (int i = 0; i < m; ++i)
            {
                fade = std::min (1.0f, fade + 1.0f / (0.03f * fs));
                mixS += (p.mix - mixS) * 0.001f; levelS += (levelGain - levelS) * 0.001f;
                peakIn = std::max (peakIn, std::max (std::abs (dry[0][i]), std::abs (dry[1][i])));
                const float mm = mixS * fade;
                a[i] = (dry[0][i] + (a[i] - dry[0][i]) * mm) * levelS; b[i] = (dry[1][i] + (b[i] - dry[1][i]) * mm) * levelS;
                if (! std::isfinite (a[i])) a[i] = 0.0f;
                if (! std::isfinite (b[i])) b[i] = 0.0f;
                peakOut = std::max (peakOut, std::max (std::abs (a[i]), std::abs (b[i])));
            }
        }
        const float decay = std::pow (0.88f, (float) n / 512.0f);
        inLevel.store (std::max (peakIn, inLevel.load (std::memory_order_relaxed) * decay), std::memory_order_relaxed);
        outLevel.store (std::max (peakOut, outLevel.load (std::memory_order_relaxed) * decay), std::memory_order_relaxed);
    }
    static constexpr int kMaxBlock = 512;
    dm1::Engine dm;
    bool dmSnap = true;
    static float softLimit (float v) { const float a = std::abs (v); return a < 0.8f ? v : std::copysign (0.8f + 0.2f * std::tanh ((a - 0.8f) / 0.2f), v); }
    struct Chan { DelayLine line, lineB; float cut1 = 0.0f, cut2 = 0.0f, cut3 = 0.0f, hp = 0.0f, bump = 0.0f, x1 = 0.0f, x2 = 0.0f, b1 = 0.0f, b2 = 0.0f, bhp = 0.0f;
                  void reset() { line.reset(); lineB.reset(); cut1 = cut2 = cut3 = hp = bump = x1 = x2 = b1 = b2 = bhp = 0.0f; } };
    float noise() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return (float) (int) rng * (1.0f / 2147483648.0f); }       // white noise -1 .. 1
    float fs = 48000.0f, phase = 0.0f, env = 0.0f, fade = 1.0f;
    float delayS = 350.0f, fbS = 0.0f, inS = 1.0f, dryS = 0.7f, wetS = 0.5f, outS = 1.0f, depthS = 0.0f, rateS = 1.0f, holdS = 1.0f, cutS = 8000.0f, mixS = 1.0f, levelS = 1.0f;
    float delayBS = 262.0f, fbBS = 0.0f, highBS = 8000.0f, wowPh = 0.0f, wow2Ph = 0.0f, flPh = 0.0f, drift = 0.0f, dropEnv = 0.0f;
    unsigned rng = 22222u;
    int lastMode = -1;
    bool started = false;
    Chan ch[2];
};
} // namespace dl1
