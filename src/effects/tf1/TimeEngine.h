// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once
#include "dsp/Interp.h"
#include "dsp/SoftClip.h"
#include "ReverseEngine.h"
#include "GrainEngine.h"
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

// TF-1 Time Fracture: a rhythmic instrument that rearranges, repeats and destroys the playback of what you play. The input is recorded all the time into a ring
// buffer (BUFFER CAPTURE, about 12 s; HOLD stops the recording and keeps the buffer). A 16-step pattern, locked to the host tempo and position (or free running at
// the TEMPO knob), decides on every step what happens in that step:
//   PLAY     the live input passes
//   REPEAT   the last LOOP-sized piece loops (stutter / retrigger / micro-loop)
//   REVERSE  the step before is played backwards
//   SCRAMBLE a random slice of the last SCRAMBLE steps replaces the step (random buffer scrambling, rearrangement)
//   PITCH    the step before is played again with a random pitch jump (up to +-PITCH semitones, the speed changes with it)
//   STRETCH  the step before is stretched or compressed without changing its pitch (granular, 60 ms grains)
//   MUTE     a gap
// The PROBABILITY engine (CHANCE) replaces the programmed operation of a step by a random one. CRUSH reduces the bit depth and the sample rate of every fragment.
// Fragments crossfade over 64 samples, so steps never click. No latency, no allocation after prepare(), no locks; the display data is published in atomics.
namespace tf1
{
constexpr int kSteps = 16, kOps = 7, kPeaks = 1024, kPeakBlock = 1024, kFade = 64, kGrain = 2880;     // the grain: 60 ms at 48 kHz (scaled with the sample rate)
enum Mode { Steps = 0, Scrub, Grains };                                  // the Time Lab: the TF-1 step sequencer, the RV-1 (SCRUB) and the GR-1 (GRAINS) engines
enum Op { Play = 0, Repeat, Reverse, Scramble, Pitch, Stretch, Mute };
inline const char* opName (int o) { static const char* n[kOps] = { "PLAY", "REPEAT", "REVERSE", "SCRAMBLE", "PITCH", "STRETCH", "MUTE" }; return n[juce::jlimit (0, kOps - 1, o)]; }
inline const char* opShort (int o) { static const char* n[kOps] = { "--", "REP", "REV", "SCR", "PIT", "STR", "MUT" }; return n[juce::jlimit (0, kOps - 1, o)]; }
constexpr float kDivisionBeats[6] = { 1.0f, 0.5f, 0.25f, 0.125f, 1.0f / 3.0f, 1.0f / 6.0f };            // 1/4, 1/8, 1/16, 1/32, 1/8 triplet, 1/16 triplet (in quarter notes)
inline const char* divisionName (int d) { static const char* n[6] = { "1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T" }; return n[juce::jlimit (0, 5, d)]; }

struct Params
{
    int ops[kSteps] {};                  // the pattern: an Op per step
    int length = 16;                     // 1 .. 16 steps
    int division = 2;                    // index into kDivisionBeats
    float chance = 0.0f;                 // 0 .. 1 probability that a step gets a random operation
    float loop = 0.5f;                   // 0 .. 1: REPEAT loop length, 1/32 of a step .. a whole step
    float scramble = 0.5f;               // 0 .. 1: how many past steps SCRAMBLE draws from (1 .. 16)
    float pitch = 0.5f;                  // 0 .. 1: the largest pitch jump, 0 .. 24 semitones
    float stretch = 0.5f;                // 0 .. 1: how far STRETCH goes (0 = not at all)
    float crush = 0.0f;                  // 0 .. 1
    bool hold = false;                   // stop recording, keep the buffer
    int mode = Steps;                    // Steps (the fields above), Scrub (the RV-1 engine with `scrub`) or Grains (the GR-1 engine with `grain`)
    rv1::Params scrub;
    gr1::Params grain;
};
struct Host { bool playing = false; double bpm = 120.0; double ppq = 0.0; };

class Engine
{
public:
    void prepare (float sampleRate)
    {
        fs = sampleRate;
        ringSize = 1; while (ringSize < (long long) (12.0f * fs)) ringSize <<= 1;
        for (auto& r : ring) r.assign ((size_t) ringSize, 0.0f);
        grainLen = std::max (256, (int) (0.06f * fs)); grainLen &= ~1;
        scrubEngine.prepare (fs); grainEngine.prepare (fs);
        mixSmooth.reset ((double) fs, 0.02); levelSmooth.reset ((double) fs, 0.02); powerSmooth.reset ((double) fs, 0.02);
        reset();
    }
    void reset()
    {
        for (auto& r : ring) std::fill (r.begin(), r.end(), 0.0f);
        writeIdx = 0; voices[0] = {}; voices[1] = {};
        clockInit = false; lastStepNo = -1; freeIdx = 0; samplesToNext = 0.0; peakAcc = 0.0f; peakBlockCount = 0; peakCountPub.store (0);
        for (auto& p : peaks) p.store (0.0f);
        stepAge = 0; elapsedPub.store (0); stepPub.store (0); opPub.store (0); backPub.store (0); lenPub.store (0); stepLenPub.store (0.0f);
        rng.setSeed (4242);
        smoothersFresh = true;                                                                          // the first block starts the smoothers at their targets (no fade in)
    }

    // in place on l / r. mix 0 .. 1, levelGain linear, power true = on. The dry sound passes with no latency.
    // Steps runs the sequencer below; Scrub and Grains run the RV-1 and the GR-1 engine unchanged (each records the input itself), with the shared MIX, LEVEL and power.
    void process (float* l, float* r, int n, const Params& p, float mix, float levelGain, bool power, const Host& host)
    {
        if (p.mode != lastMode) { reset(); scrubEngine.reset(); grainEngine.reset(); lastMode = p.mode; }       // a new mode starts from an empty buffer
        if (p.mode == Steps) processSteps (l, r, n, p, mix, levelGain, power, host);
        else processHosted (l, r, n, p, mix, levelGain, power, host);
    }

    const gr1::Engine& grains() const noexcept { return grainEngine; }                                  // the grain cloud display

private:
    void processHosted (float* l, float* r, int n, const Params& p, float mix, float levelGain, bool power, const Host& host)
    {
        if (smoothersFresh) { powerSmooth.setCurrentAndTargetValue (power ? 1.0f : 0.0f); smoothersFresh = false; }
        powerSmooth.setTargetValue (power ? 1.0f : 0.0f);
        const float bpm = (float) juce::jlimit (20.0, 400.0, host.bpm);
        for (int pos = 0; pos < n; pos += kHostChunk)
        {
            const int m = std::min (kHostChunk, n - pos);
            float* io[2] = { l + pos, r + pos };
            for (int c = 0; c < 2; ++c) std::copy (io[c], io[c] + m, dry[c]);
            if (p.mode == Scrub) { auto q = p.scrub; q.bpm = bpm; scrubEngine.process (io[0], io[1], m, q, mix, levelGain); }
            else { auto q = p.grain; q.bpm = bpm; grainEngine.process (io[0], io[1], m, q, mix, levelGain); }
            for (int i = 0; i < m; ++i)
            {
                const float pw = powerSmooth.getNextValue();                                              // the power switch crossfades to the untouched input
                for (int c = 0; c < 2; ++c) io[c][i] = dry[c][i] + (io[c][i] - dry[c][i]) * pw;
            }
        }
    }

    void processSteps (float* l, float* r, int n, const Params& p, float mix, float levelGain, bool power, const Host& host)
    {
        if (smoothersFresh) { mixSmooth.setCurrentAndTargetValue (mix); levelSmooth.setCurrentAndTargetValue (levelGain); powerSmooth.setCurrentAndTargetValue (power ? 1.0f : 0.0f); smoothersFresh = false; }
        mixSmooth.setTargetValue (mix); levelSmooth.setTargetValue (levelGain); powerSmooth.setTargetValue (power ? 1.0f : 0.0f);
        const int len = juce::jlimit (1, kSteps, p.length);
        const double bpm = juce::jlimit (20.0, 400.0, host.bpm), stepBeats = (double) kDivisionBeats[juce::jlimit (0, 5, p.division)];
        const double stepLen = 60.0 / bpm * stepBeats * (double) fs, ppqPerSample = bpm / 60.0 / (double) fs;
        stepLenPub.store ((float) stepLen, std::memory_order_relaxed);
        if (! host.playing) clockInit = false;                                                        // a stopped transport: the pattern runs free
        for (int i = 0; i < n; ++i)
        {
            if ((i & 63) == 0) elapsedPub.store (stepAge, std::memory_order_relaxed);
            if (host.playing)
            {
                const long long stepNo = (long long) std::floor ((host.ppq + (double) i * ppqPerSample) / stepBeats);
                if (stepNo != lastStepNo || ! clockInit)
                {
                    clockInit = true; lastStepNo = stepNo;
                    startStep ((int) (((stepNo % len) + len) % len), p, stepLen);
                }
            }
            else
            {
                if (samplesToNext <= 0.0) { startStep (freeIdx % len, p, stepLen); freeIdx = (freeIdx + 1) % len; samplesToNext += stepLen; }
                samplesToNext -= 1.0;
            }

            const float inL = l[i], inR = r[i];
            float wetL = 0.0f, wetR = 0.0f;
            for (auto& v : voices) if (v.active) { float a, b; renderVoice (v, inL, inR, p.crush, a, b); wetL += a; wetR += b; }

            if (! p.hold)                                                                              // BUFFER CAPTURE: the input goes into the ring
            {
                const size_t idx = (size_t) (writeIdx & (ringSize - 1));
                ring[0][idx] = inL; ring[1][idx] = inR;
                ++writeIdx;
                peakAcc = std::max (peakAcc, std::max (std::fabs (inL), std::fabs (inR)));
                if (++peakBlockCount >= kPeakBlock)
                {
                    const int c = peakCountPub.load (std::memory_order_relaxed);
                    peaks[(size_t) (c % kPeaks)].store (peakAcc, std::memory_order_relaxed);
                    peakCountPub.store (c + 1, std::memory_order_release);
                    peakAcc = 0.0f; peakBlockCount = 0;
                }
            }
            holdPub.store (p.hold, std::memory_order_relaxed);
            ++stepAge;

            const float m = mixSmooth.getNextValue() * powerSmooth.getNextValue(), lv = levelSmooth.getNextValue(), pw = powerSmooth.getCurrentValue(), g = 1.0f + (lv - 1.0f) * pw;
            l[i] = (inL * (1.0f - m) + softClip (wetL) * m) * g;
            r[i] = (inR * (1.0f - m) + softClip (wetR) * m) * g;
        }
    }

public:
    // display data (message thread)
    int currentStep() const noexcept { return stepPub.load (std::memory_order_relaxed); }
    int currentOp() const noexcept { return opPub.load (std::memory_order_relaxed); }
    float stepLengthSamples() const noexcept { return stepLenPub.load (std::memory_order_relaxed); }
    long long stepElapsed() const noexcept { return elapsedPub.load (std::memory_order_relaxed); }   // samples since the current step began
    long long sourceBack() const noexcept { return backPub.load (std::memory_order_relaxed); }       // the fragment's source starts this many samples before now
    long long sourceLength() const noexcept { return lenPub.load (std::memory_order_relaxed); }
    bool held() const noexcept { return holdPub.load (std::memory_order_relaxed); }
    float sampleRate() const noexcept { return fs; }
    int peakCount() const noexcept { return peakCountPub.load (std::memory_order_acquire); }
    float peak (int absoluteIndex) const noexcept { return peaks[(size_t) (((absoluteIndex % kPeaks) + kPeaks) % kPeaks)].load (std::memory_order_relaxed); }
    float capacitySeconds() const noexcept { return fs > 0.0f ? (float) ringSize / fs : 0.0f; }

private:
    struct Voice
    {
        bool active = false, releasing = false, live = false, mute = false, grains = false;
        double pos = 0.0, rate = 1.0;                       // read position in the ring (absolute) and speed (negative = backwards)
        double loopStart = 0.0, loopLen = 0.0;              // REPEAT: the region that loops
        bool looping = false;
        int age = 0, relAge = 0;
        // STRETCH (two grains)
        double head = 0.0, speed = 1.0; double gPos[2] {}; int gAge[2] { 0, 0 }; bool gOn[2] { false, false };
        // CRUSH state
        float hold[2] { 0.0f, 0.0f }; int holdCount = 0;
    };

    static float softClip (float x) noexcept { return vibr::dsp::softClip (x); }
    float readRing (int ch, double pos) const noexcept
    {
        const long long i0 = (long long) std::floor (pos);
        const float f = (float) (pos - (double) i0);
        const auto& r = ring[(size_t) ch];
        const float a = r[(size_t) (i0 & (ringSize - 1))], b = r[(size_t) ((i0 + 1) & (ringSize - 1))];
        return vibr::dsp::lerp (a, b, f);
    }

    void startStep (int stepIdx, const Params& p, double stepLen)
    {
        int op = juce::jlimit (0, kOps - 1, p.ops[stepIdx]);
        if (p.chance > 0.0f && rng.nextFloat() < p.chance)                                              // the probability engine
        {
            const float w[6] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f };                                  // REPEAT, REVERSE, SCRAMBLE, PITCH, STRETCH, MUTE
            float sum = 0.0f; for (float x : w) sum += x;
            float pick = rng.nextFloat() * sum; int k = 0;
            for (; k < 5; ++k) { if (pick < w[k]) break; pick -= w[k]; }
            op = 1 + k;
        }
        const long long S = std::max<long long> (32, (long long) std::llround (stepLen));
        const double now = (double) writeIdx;                                                           // the history is everything before this
        const long long maxBack = ringSize - 2 * (long long) kFade - 4096;

        Voice v; v.active = true;
        long long back = 0, length = 0;
        switch (op)
        {
            case Play:    v.live = true; break;
            case Mute:    v.mute = true; break;
            case Repeat:
            {
                const double frac = std::pow (2.0, -5.0 * (1.0 - (double) p.loop));                      // 1/32 of a step .. a whole step
                const long long loopLen = juce::jlimit<long long> (std::min<long long> (S, (long long) (0.002f * fs)), std::min (S, maxBack), (long long) std::llround ((double) S * frac));
                v.looping = true; v.loopStart = now - (double) loopLen; v.loopLen = (double) loopLen; v.pos = v.loopStart; v.rate = 1.0;
                back = loopLen; length = loopLen; break;
            }
            case Reverse:
            {
                const long long span = std::min (S, maxBack);
                v.pos = now - 1.0; v.rate = -1.0; back = span; length = span; break;
            }
            case Scramble:
            {
                const int steps = juce::jlimit (1, kSteps, 1 + (int) std::lround ((double) p.scramble * (kSteps - 1)));
                const long long filled = std::min<long long> (writeIdx, ringSize);
                const long long avail = std::max<long long> (1, std::min<long long> ({ (long long) steps, maxBack / S, filled / S }));
                const long long r = (long long) rng.nextInt ((int) avail);                                // 0 = the step before
                v.pos = now - (double) ((r + 1) * S); v.rate = 1.0; back = (r + 1) * S; length = S; break;
            }
            case Pitch:
            {
                static const int jumps[10] = { -12, -7, -5, -3, 3, 5, 7, 12, 19, 24 };
                const int range = (int) std::lround ((double) p.pitch * 24.0);
                int st = 0, tries = 0;
                if (range > 0) do { st = jumps[rng.nextInt (10)]; } while (std::abs (st) > range && ++tries < 40);
                if (std::abs (st) > range) st = 0;
                v.rate = std::pow (2.0, (double) st / 12.0);
                const double span = std::min ((double) maxBack, (double) S * v.rate);
                v.pos = now - span; back = (long long) span; length = (long long) span; break;
            }
            case Stretch:
            {
                static const double factors[5] = { 0.5, 0.67, 1.5, 2.0, 3.0 };
                v.speed = std::pow (factors[rng.nextInt (5)], (double) p.stretch);                        // source speed: below 1 stretches, above 1 compresses
                const double span = std::min ((double) maxBack, std::max ((double) S, (double) S * v.speed) + (double) grainLen + 600.0);        // the grains stay inside the recording
                v.head = now - span; v.grains = true; back = (long long) span; length = (long long) span; break;
            }
        }
        stepPub.store (stepIdx, std::memory_order_relaxed); opPub.store (op, std::memory_order_relaxed);
        backPub.store (back, std::memory_order_relaxed); lenPub.store (length, std::memory_order_relaxed);
        stepAge = 0;
        if ((v.live && voices[0].active && voices[0].live && ! voices[0].releasing) || (v.mute && voices[0].active && voices[0].mute && ! voices[0].releasing)) return;   // the same again: no seam
        if (voices[0].active) { voices[1] = voices[0]; voices[1].releasing = true; voices[1].relAge = 0; }
        else { voices[1] = {}; v.age = kFade; }                                                         // the first sound needs no fade in
        voices[0] = v;
    }

    void renderVoice (Voice& v, float inL, float inR, float crush, float& outL, float& outR)
    {
        float a = 0.0f, b = 0.0f;
        if (v.live) { a = inL; b = inR; }
        else if (v.mute) { a = b = 0.0f; }
        else if (v.grains)
        {
            const int G = grainLen, H = G / 2;
            if (v.age % H == 0)                                                                       // a new grain every half grain
            {
                const int slot = (v.age / H) & 1, other = 1 - slot;
                double start = v.head;
                if (v.gOn[other])                                                                       // WSOLA: slide the new grain (+-10 ms) to where it continues the running one best
                {
                    const double cont = v.gPos[other] + (double) v.gAge[other];
                    const int D = (int) (0.01f * fs), L = 384;
                    double bestScore = -1.0e30; int bestK = 0;
                    const double hiLimit = (double) writeIdx - (double) G - 2.0;
                    for (int k = -D; k <= D; k += 2)
                    {
                        const double cand = v.head + k;
                        if (cand + (double) G > hiLimit + (double) G || cand > hiLimit || cand < (double) (writeIdx - ringSize + 8192)) continue;
                        double score = 0.0;
                        for (int q = 0; q < L; q += 2) score += (double) readRing (0, cont + q) * (double) readRing (0, cand + q);
                        if (score > bestScore) { bestScore = score; bestK = k; }
                    }
                    start = v.head + bestK;
                }
                v.gOn[slot] = true; v.gPos[slot] = start; v.gAge[slot] = 0;
                v.head += v.speed * (double) H;
            }
            for (int s = 0; s < 2; ++s)
                if (v.gOn[s])
                {
                    const float w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) v.gAge[s] / (float) G);
                    const double pos = v.gPos[s] + (double) v.gAge[s];
                    a += w * readRing (0, pos); b += w * readRing (1, pos);
                    if (++v.gAge[s] >= G) v.gOn[s] = false;
                }
        }
        else
        {
            if (v.looping && v.pos >= v.loopStart + v.loopLen) v.pos -= v.loopLen;
            a = readRing (0, v.pos); b = readRing (1, v.pos);
            if (v.looping)                                                                            // a short fade at both ends of the loop hides the seam
            {
                const double fromStart = v.pos - v.loopStart, toEnd = v.loopStart + v.loopLen - v.pos;
                const double lim = std::min (32.0, v.loopLen * 0.25), e = std::min (fromStart, toEnd);
                if (e < lim) { const float g = (float) (e / lim); a *= g; b *= g; }
            }
            v.pos += v.rate;
        }
        if (crush > 0.0f && ! v.live)                                                                  // CRUSH: fewer bits, a lower sample rate
        {
            const float levels = std::exp2 (std::round (15.0f - 12.0f * crush));                        // 2^15 .. 2^3
            const int df = 1 + (int) (crush * crush * 14.0f);
            if (v.holdCount++ % df == 0) { v.hold[0] = std::round (a * levels) / levels; v.hold[1] = std::round (b * levels) / levels; }
            a = v.hold[0]; b = v.hold[1];
        }
        float g = std::min (1.0f, (float) v.age / (float) kFade);
        if (v.releasing) { g *= 1.0f - (float) v.relAge / (float) kFade; if (++v.relAge >= kFade) v.active = false; }
        ++v.age;
        outL = a * g; outR = b * g;
    }

    static constexpr int kHostChunk = 512;
    rv1::Engine scrubEngine;
    gr1::Engine grainEngine;
    float dry[2][kHostChunk] {};
    int lastMode = -1;
    float fs = 48000.0f;
    long long ringSize = 1 << 19, writeIdx = 0;
    int grainLen = kGrain;
    std::array<std::vector<float>, 2> ring;
    std::array<Voice, 2> voices {};
    bool clockInit = false; long long lastStepNo = -1; int freeIdx = 0; double samplesToNext = 0.0;
    float peakAcc = 0.0f; int peakBlockCount = 0;
    juce::Random rng { 4242 };
    juce::SmoothedValue<float> mixSmooth, levelSmooth, powerSmooth;
    bool smoothersFresh = true;
    std::atomic<int> stepPub { 0 }, opPub { 0 }, peakCountPub { 0 };
    std::atomic<long long> backPub { 0 }, lenPub { 0 }, elapsedPub { 0 };
    long long stepAge = 0;
    std::atomic<float> stepLenPub { 0.0f };
    std::atomic<bool> holdPub { false };
    std::array<std::atomic<float>, kPeaks> peaks {};
};
} // namespace tf1
