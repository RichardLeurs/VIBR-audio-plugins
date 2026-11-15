// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

// MS-1 AUTO, stage 1 (see .claude/mastering_specs.md, "AUTO: analyse the track and propose settings"): the plain data and the RULES.
//   AnalysisSnapshot  what the audio thread measured on the chain input (filled by AutoAnalyzer.h, copied to the editor through a sequence counter)
//   AutoContext       the current settings the proposal is compared with, plus the loudness target and the style
//   Proposal          the result: one row per stage with a summary and a reason, and the plain values to write
//   propose()         the rules: a pure function (no JUCE, no state, no randomness), so the same measurements always give the same proposal
//   paramWrites()     the (parameter id, plain value) pairs of the checked rows; the processor writes them in one undo transaction
// Stage 1 has the rules for the input trim, the bus compressor and the limiter; stage 2 adds the EQ (style curves in AutoTargets.h, MATCH EQ's fit when a
// reference is loaded, narrow cuts from the narrow-peak statistics); stage 3 adds the multiband (from the per band level histograms) and the imager (from
// the measured per band correlation and side / mid level), and the bus and limiter rules now count the multiband's predicted reduction.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include "AutoTargets.h"
#include "MasteringStages.h"

namespace ms1::autom
{
constexpr int kHistBins = 100;                      // level histograms: 1 dB bins from -100 to 0 dB
constexpr float kHistLo = -100.0f;
constexpr int kSpecBands = 128;                     // the long-term spectrum: the 128 log bands of ms1::Spectrum (20 Hz .. 20 kHz)
constexpr int kMbBands = 4;
constexpr float kMinProgrammeSeconds = 10.0f;       // a proposal needs at least this much counted programme
constexpr float kMaxProgrammeSeconds = 120.0f;      // learning stops here
constexpr float kProgrammeGateLufs = -60.0f;        // a block counts when its momentary loudness is above this

inline int histBin (float db) { return std::clamp ((int) std::floor (db - kHistLo), 0, kHistBins - 1); }
inline float histCentre (int bin) { return kHistLo + (float) bin + 0.5f; }

// Everything the audio thread measures. Only 4-byte members (it is published word by word under a sequence counter), no pointers, no padding.
struct AnalysisSnapshot
{
    float sampleRate = 0.0f;
    float programmeSeconds = 0.0f;                  // counted audio (momentary loudness above -60 LUFS, plugin on)
    uint32_t learning = 0;                          // 1 while the analyser is running
    float samplePeakDb = -120.0f, truePeakDb = -120.0f;   // the largest over the programme, both channels (true peak: the 8x detector)
    uint32_t clippedSamples = 0;                    // |x| >= 0.999
    float clippedFraction = 0.0f;                   // clipped samples / counted samples (both channels)
    float integratedLufs = -120.0f, shortTermMaxLufs = -120.0f, momentaryMaxLufs = -120.0f, loudnessRange = 0.0f;
    float rmsDb = -120.0f, crestDb = 0.0f;          // programme RMS (both channels, true RMS) and sample peak - RMS
    float psrDb = 0.0f;                             // mean over the programme (every 100 ms) of the true peak of the last 3 s - the short-term loudness
    float drDb = 0.0f;                              // the DR value of the input (same meter as the output's)
    float correlation = 1.0f;                       // phase correlation over the programme
    float lowEnergyDb = -120.0f, midBassDb = -120.0f, dcDb = -120.0f;     // mid signal: below 25 Hz, 40 .. 60 Hz (dBFS RMS), and the DC offset (largest channel mean, dBFS)
    float momentaryNow = -120.0f, correlationNow = 1.0f;                   // live values for the picture
    float xover[3] { 120.0f, 1200.0f, 6000.0f };    // the multiband crossovers the band histograms were measured with
    uint32_t shortTermHist[kHistBins] {};           // short-term loudness, one count per 100 ms of programme
    uint32_t bandHist[kMbBands][kHistBins] {};      // per multiband band, the level of each 400 ms of programme (dB RMS, both channels), pre compression
    float spectrumDb[kSpecBands] {};                // long-term spectrum (power average over the programme), dB as the spectrum display reads it
    uint32_t spectrumFrames = 0;
    uint32_t narrowBlocks = 0;                      // 3 s blocks of programme that were checked for narrow peaks
    uint32_t narrowHits[kSpecBands] {};             // per band, in how many of them the band was more than 6 dB above the mean of its neighbours (4 .. 8 bands away)
    float imgX[2] { 200.0f, 4000.0f };              // the imager crossovers the next two arrays were measured with
    float imgCorr[3] { 2.0f, 2.0f, 2.0f };          // per imager band, the phase correlation of L / R over the programme; 2 = no signal in that band (below -70 dBFS)
    float imgSideMidDb[3] { -120.0f, -120.0f, -120.0f };   // per imager band, the side energy against the mid energy (dB), before the width
    uint32_t check = 0;                             // a hash of all the words above; a torn copy would not match
};

inline uint32_t snapshotCheck (const AnalysisSnapshot& s)
{
    const auto* w = reinterpret_cast<const uint32_t*> (&s);
    const size_t n = sizeof (AnalysisSnapshot) / 4 - 1;           // everything but `check`, which is the last member
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; ++i) { h ^= w[i]; h *= 16777619u; }
    return h;
}

//==============================================================================
struct AutoContext
{
    float targetLufs = -14.0f, ceilingDb = -1.0f;   // from the selected target preset (never changed by AUTO)
    int style = 0;                                  // 0 Neutral, 1 Warm, 2 Bright, 3 Club (the style curve the EQ rule aims at, see AutoTargets.h)
    bool hasRef = false;                            // a reference track is loaded: its long-term spectrum is the target and the EQ step is MATCH EQ
    std::array<float, kSpecBands> refDb {};         // the reference's long-term spectrum (the same 128 bands, dB)
    bool curEqOn = true;                            // the current EQ: eqOn and the 8 bands
    struct EqCur { bool on = true; float freq = 1000.0f, gainDb = 0.0f, q = 1.0f; int mode = 0; bool dyn = false; };
    std::array<EqCur, kEqBands> curEq {};
    // the current settings, to tell "no change needed" from a change
    float curInGain = 0.0f, curHpf = 0.0f;
    float curOutGain = 0.0f; int curDither = 0;     // the OUTPUT stage: outGain (dB) and ditherBits (0 off .. 3 = 24 bit)
    bool curBusOn = false; int curBusRatio = 1, curBusAttack = 3, curBusRelease = 2; float curBusThresh = -12.0f;
    bool curLimAuto = false, curLimRelAuto = false; int curLimChar = 0;
    bool curMbOn = false; float curMbMix = 100.0f;
    struct MbCur { bool on = true, autoRel = false; float thresh = -18.0f, ratio = 2.0f, attack = 10.0f, make = 0.0f; };
    std::array<MbCur, kMbBands> curMb {};
    bool curImgOn = false, curImgSafe = false; float curWidth[3] { 100.0f, 100.0f, 100.0f }, curMonoBelow = 0.0f;
};

struct EqProp { float freq = 1000.0f, gainDb = 0.0f, q = 1.0f; bool on = false, narrow = false; };

struct Row
{
    bool available = true;                          // false: the stage is not part of this version of AUTO
    bool needed = false;                            // the rule proposes a change of this stage
    std::string summary, reason;
};

struct Proposal
{
    bool valid = false;                             // enough programme was heard
    std::string message;                            // when not valid: why
    Row input, eq, multiband, imager, bus, limiter, output;
    float outGainDb = 0.0f; int ditherBits = 0;     // the OUTPUT row: the output gain proposed and the dither setting (kept: AUTO cannot know the delivery format)
    // the values (what is written when the row is applied)
    float inGainDb = 0.0f, hpfHz = 0.0f;
    bool clipWarning = false;
    std::array<EqProp, kEqBands> eqBands {};        // the 8 EQ bands (band 1 = low shelf, 8 = high shelf, 2 .. 7 bells)
    bool eqSet = false, eqFromRef = false;          // at least one band moves (the row writes the 8 bands) / the target was the reference
    int eqNarrow = 0;                               // the number of narrow cuts among the bands
    bool busOn = false; int busRatio = 1, busAttack = 4, busRelease = 4; float busThreshDb = -12.0f, busReductionDb = 0.0f;
    bool limAuto = true, limRelAuto = true; int limChar = 0; float neededGainDb = 0.0f; bool heavyWarning = false;
    // multiband: per band the rule's result (on = the band compresses, else it is left off), the loudness the bands lose together
    struct MbProp { bool on = false; float spreadDb = 0.0f, offsetDb = 0.0f, ratio = 2.0f, threshDb = -18.0f, attackMs = 10.0f, reductionDb = 0.0f; };
    std::array<MbProp, kMbBands> mb {};
    bool mbSet = false; float mbLoudnessRedDb = 0.0f;
    // imager: the three widths (%), mono-below (Hz, 0 = off), auto safe width switched on by the rule
    float widthPct[3] { 100.0f, 100.0f, 100.0f }, monoBelowHz = 0.0f; bool imgSafeOn = false, imgSet = false;
};

struct ParamWrite { char id[20]; float value; ParamWrite (const char* name, float v) : value (v) { std::snprintf (id, sizeof (id), "%s", name); } };

//==============================================================================
inline std::string fmt (const char* f, double a = 0.0, double b = 0.0, double c = 0.0, double d = 0.0)
{
    char buf[256];
    std::snprintf (buf, sizeof (buf), f, a, b, c, d);
    return buf;
}
inline float round01 (float v) { return std::round (v * 10.0f) / 10.0f; }

// the static curve of ms1::BusCompressor (6 dB soft knee): the reduction in dB (>= 0) for a level `over` dB above the threshold
inline float busReduction (float over, float ratio)
{
    const float slope = 1.0f - 1.0f / ratio, knee = 6.0f;
    if (2.0f * over > knee) return slope * over;
    if (2.0f * over > -knee) { const float x = over + knee * 0.5f; return slope * x * x / (2.0f * knee); }
    return 0.0f;
}

// the mean reduction of a level histogram (counts per 1 dB bin) pushed through the curve; `shiftDb` moves the levels (the input trim comes before the bus)
inline double meanReduction (const uint32_t* hist, float shiftDb, float thresholdDb, float ratio)
{
    double sum = 0.0, n = 0.0;
    for (int b = 0; b < kHistBins; ++b)
    {
        if (hist[b] == 0) continue;
        sum += (double) hist[b] * (double) busReduction (histCentre (b) + shiftDb - thresholdDb, ratio);
        n += (double) hist[b];
    }
    return n > 0.0 ? sum / n : 0.0;
}

// the threshold (-30 .. 0 dB, 0.1 dB steps) at which the mean reduction is `wanted`, by bisection (the same 40 steps every time: deterministic)
inline float findBusThreshold (const uint32_t* hist, float shiftDb, float ratio, float wanted, float lowest = -30.0f)
{
    float lo = lowest, hi = 0.0f;                                  // the reduction falls as the threshold rises
    if (meanReduction (hist, shiftDb, lo, ratio) <= (double) wanted) return lo;
    if (meanReduction (hist, shiftDb, hi, ratio) >= (double) wanted) return hi;
    for (int i = 0; i < 40; ++i)
    {
        const float mid = 0.5f * (lo + hi);
        if (meanReduction (hist, shiftDb, mid, ratio) > (double) wanted) lo = mid; else hi = mid;
    }
    return std::clamp (round01 (0.5f * (lo + hi)), lowest, 0.0f);
}

// the level (dB) below which the share `q` (0 .. 1) of the histogram lies, interpolated inside the 1 dB bin (bin edges, so differences of percentiles are exact)
inline float histPercentile (const uint32_t* hist, float q)
{
    double n = 0.0;
    for (int b = 0; b < kHistBins; ++b) n += (double) hist[b];
    if (n <= 0.0) return kHistLo;
    const double target = (double) q * n;
    double cum = 0.0;
    for (int b = 0; b < kHistBins; ++b)
    {
        if (hist[b] == 0) continue;
        if (cum + (double) hist[b] >= target) return kHistLo + (float) b + (float) ((target - cum) / (double) hist[b]);
        cum += (double) hist[b];
    }
    return 0.0f;
}
// the mean power of a histogram of dB levels (linear, relative), for the energy share of a band
inline double histPower (const uint32_t* hist)
{
    double sum = 0.0, n = 0.0;
    for (int b = 0; b < kHistBins; ++b) { sum += (double) hist[b] * std::pow (10.0, (double) histCentre (b) / 10.0); n += (double) hist[b]; }
    return n > 0.0 ? sum / n : 0.0;
}

// Detector offsets (dB), see the spec: the rules predict the reduction from a level distribution (the 400 ms RMS of a band, the 3 s short-term loudness of
// the programme) while the real compressors detect the instantaneous peak of the louder channel. The distribution is moved up by this offset before it is
// pushed through the static curve; the values were measured against the real compressors on synthetic signals (see AutoTest.cpp, "calibration").
// multiband: the offset depends on the band's attack (a faster attack follows the peaks more closely, so the same threshold reduces more) and a little on the
// ratio; measured on stereo pink noise whose level in one band steps every second (spreads 7 .. 15 dB), the offset at which the real compressor's mean is 1.5 dB:
// ratio 2: 6.5 / 7.3 / 7.95 / 8.1 dB for the bands with attack 30 / 15 / 8 / 4 ms; ratio 1.5 about 0.5 dB lower, ratio 3 about 0.4 dB higher
inline float mbDetectorOffsetDb (int band, float ratio)
{
    static const float base[4] = { 6.5f, 7.3f, 7.95f, 8.1f };
    return base[std::clamp (band, 0, 3)] + (ratio < 1.75f ? -0.5f : ratio > 2.5f ? 0.4f : 0.0f);
}
constexpr float kBusDetectorOffsetDb = 5.0f;

inline const char* busRatioText (int i) { static const char* t[4] = { "1.5:1", "2:1", "4:1", "10:1" }; return t[std::clamp (i, 0, 3)]; }
inline const char* limCharText (int i) { static const char* t[3] = { "TRANSPARENT", "PUNCHY", "LOUD" }; return t[std::clamp (i, 0, 2)]; }


//==============================================================================
// Rule 2, EQ. The target is the reference's long-term spectrum (then the result IS matchEqFit, the same code as the MATCH EQ button) or the style curve.
// The fit (level removed, one octave of smoothing, at most +-4 dB, shelves Q 0.71 and bells Q 1.1) is the existing matchEqFit; AUTO drops the bands under 1 dB
// and lets the narrow-peak statistics replace up to two bells with narrow cuts.
inline float specBandHz (int b) { return 20.0f * std::pow (1000.0f, ((float) b + 0.5f) / (float) kSpecBands); }

// the mean (dB) of the bands 4 .. 8 away on both sides; false when fewer than 4 of them have a level
inline bool neighbourMean (const float* db, int b, float& mean)
{
    double acc = 0.0; int n = 0;
    for (int o = 4; o <= 8; ++o)
        for (int sgn = -1; sgn <= 1; sgn += 2)
        {
            const int k = b + sgn * o;
            if (k >= 0 && k < kSpecBands && db[k] > -100.0f) { acc += (double) db[k]; ++n; }
        }
    mean = n > 0 ? (float) (acc / n) : 0.0f;
    return n >= 4;
}

inline std::string hzText (float hz) { return hz >= 1000.0f ? fmt ("%.1f kHz", (double) hz / 1000.0) : fmt ("%.0f Hz", (double) hz); }

inline void proposeEq (const AnalysisSnapshot& s, const AutoContext& c, Proposal& p)
{
    p.eq.available = true;
    p.eqFromRef = c.hasRef;
    const std::string target = c.hasRef ? std::string ("the reference track's long-term spectrum (the same fit as the MATCH EQ button; the style is not used)")
                                        : std::string ("the ") + styleName (c.style) + " style curve";
    if (s.spectrumFrames < 20)
    {
        p.eq.summary = "no change needed (not enough spectrum heard yet)";
        p.eq.reason = "The long-term spectrum needs about 2 s of programme.";
        return;
    }
    std::array<float, kSpecBands> tgt {};
    if (c.hasRef) tgt = c.refDb; else styleTarget<kSpecBands> (c.style, tgt.data());
    const auto fit = matchEqFit (tgt.data(), s.spectrumDb);
    for (int i = 0; i < kEqBands; ++i)
    {
        auto& e = p.eqBands[(size_t) i];
        e.freq = fit[(size_t) i].freq; e.q = fit[(size_t) i].q; e.gainDb = fit[(size_t) i].gainDb; e.on = true; e.narrow = false;
        if (! c.hasRef && std::abs (e.gainDb) < 0.995f) { e.gainDb = 0.0f; e.on = false; }       // AUTO: no band under 1 dB (MATCH EQ keeps its 0.3 dB limit)
    }

    // narrow cuts: bands that are 6 dB above their neighbours in more than half of the 3 s blocks (and are narrow: a run of at most 6 bands)
    std::string narrowText;
    if (! c.hasRef && s.narrowBlocks >= 2)
    {
        struct Cand { int b; float excess; };
        std::vector<Cand> cands;
        std::array<float, kSpecBands> exc {};
        std::array<bool, kSpecBands> hit {};
        for (int b = 0; b < kSpecBands; ++b)
        {
            float m = 0.0f;
            if (s.spectrumDb[b] > -100.0f && neighbourMean (s.spectrumDb, b, m)) exc[(size_t) b] = s.spectrumDb[b] - m;
            hit[(size_t) b] = exc[(size_t) b] > 0.0f && 2u * s.narrowHits[b] > s.narrowBlocks;
        }
        for (int b = 0; b < kSpecBands;)
        {
            if (! hit[(size_t) b]) { ++b; continue; }
            int e = b; while (e + 1 < kSpecBands && hit[(size_t) (e + 1)]) ++e;
            if (e - b + 1 <= 6)
            {
                int pk = b; for (int k = b; k <= e; ++k) if (exc[(size_t) k] > exc[(size_t) pk]) pk = k;
                cands.push_back ({ pk, exc[(size_t) pk] });
            }
            b = e + 1;
        }
        std::stable_sort (cands.begin(), cands.end(), [] (const Cand& x, const Cand& y) { return x.excess > y.excess; });
        if (cands.size() > 2) cands.resize (2);
        std::array<bool, kEqBands> used {};
        for (const auto& cd : cands)
        {
            const float hz = std::round (specBandHz (cd.b));
            int best = -1; float bestG = 1.0e9f, bestD = 1.0e9f;
            for (int i = 1; i < kEqBands - 1; ++i)                                           // the bells only
            {
                if (used[(size_t) i]) continue;
                const float g = std::abs (p.eqBands[(size_t) i].gainDb), d = std::abs (std::log2 (hz / p.eqBands[(size_t) i].freq));
                if (g < bestG - 1.0e-6f || (std::abs (g - bestG) <= 1.0e-6f && d < bestD)) { best = i; bestG = g; bestD = d; }
            }
            if (best < 0) continue;
            used[(size_t) best] = true;
            auto& e = p.eqBands[(size_t) best];
            e.freq = hz; e.gainDb = -std::clamp (std::round (0.5f * cd.excess * 10.0f) / 10.0f, 1.0f, 4.0f);
            e.q = std::clamp (std::round ((2.0f + cd.excess / 3.0f) * 10.0f) / 10.0f, 3.0f, 5.0f);
            e.on = true; e.narrow = true; ++p.eqNarrow;
            narrowText += "\n" + fmt ("Narrow cut %.1f dB, Q %.1f at ", (double) e.gainDb, (double) e.q) + hzText (e.freq)
                        + fmt (": that band is %.1f dB above its neighbours (more than 6 dB in %.0f of %.0f blocks of 3 s).", (double) cd.excess, (double) s.narrowHits[cd.b], (double) s.narrowBlocks);
        }
    }

    bool any = false;
    for (const auto& e : p.eqBands) if (e.on && e.gainDb != 0.0f) any = true;
    p.eqSet = any;
    std::string list;
    for (int i = 0; i < kEqBands; ++i)
    {
        const auto& e = p.eqBands[(size_t) i];
        if (! e.on || e.gainDb == 0.0f) continue;
        if (! list.empty()) list += ", ";
        list += fmt ("%+.1f dB at ", (double) e.gainDb) + hzText (e.freq) + (i == 0 ? " (low shelf)" : i == kEqBands - 1 ? " (high shelf)" : e.narrow ? fmt (" (Q %.1f)", (double) e.q) : std::string());
    }
    if (! any)
    {
        p.eq.needed = false;
        p.eq.summary = c.hasRef ? "no change needed (the spectrum already matches the reference)" : std::string ("no change needed (within 1 dB of the ") + styleName (c.style) + " curve)";
        p.eq.reason = "Target: " + target + ". The long-term spectrum of the input (level removed, smoothed to one octave) deviates by less than " + (c.hasRef ? "0.3" : "1") + " dB everywhere, so the EQ is left alone.";
        return;
    }
    bool differs = ! c.curEqOn;
    for (int i = 0; i < kEqBands; ++i)
    {
        const auto& e = p.eqBands[(size_t) i]; const auto& cu = c.curEq[(size_t) i];
        const float g = e.on ? e.gainDb : 0.0f, cg = cu.on ? cu.gainDb : 0.0f;
        if (std::abs (g - cg) > 0.05f || cu.mode != 0 || cu.dyn) differs = true;
        else if (g != 0.0f && (std::abs (e.freq - cu.freq) > 1.0f + 0.005f * e.freq || std::abs (e.q - cu.q) > 0.02f)) differs = true;
    }
    p.eq.needed = differs;
    const std::string sum = std::string (c.hasRef ? "MATCH EQ: " : (std::string (styleName (c.style)) + ": ").c_str()) + list;
    p.eq.summary = differs ? sum : "no change needed (" + sum + ")";
    p.eq.reason = "Target: " + target + ". The long-term spectrum of the input (" + fmt ("%.0f", (double) s.programmeSeconds) + " s heard) is compared with it, the overall level is removed, the difference is smoothed to one octave and written as a low shelf, six bells and a high shelf (at most +-4 dB; bands under "
                + (c.hasRef ? "0.3" : "1") + " dB are left flat).\nProposed: " + list + "." + narrowText + "\nThe EQ phase mode is never changed.";
}

//==============================================================================
// Rule 3, multiband. Per band: spread = P95 - P10 of the 400 ms level histogram (pre compression, after the proposed trim). Under 6 dB the band is left off;
// else ratio 1.5 / 2 / 3 by the spread, the threshold by bisection so that the mean reduction through the static curve (6 dB knee) is 1.5 dB.
inline const char* mbBandName (int k) { static const char* t[4] = { "LOW", "LOW-MID", "HIGH-MID", "HIGH" }; return t[std::clamp (k, 0, 3)]; }

inline void proposeMultiband (const AnalysisSnapshot& s, const AutoContext& c, Proposal& p)
{
    p.multiband.available = true;
    static const float attack[4] = { 30.0f, 15.0f, 8.0f, 4.0f };
    std::string list, reason = fmt ("The multiband bands are %.0f / %.0f / %.0f Hz (the crossovers the levels were measured with). The level of every 400 ms of programme is counted per band; spread = level of the loudest 5 %% minus the level of the quietest 10 %%.\n",
                                    (double) s.xover[0], (double) s.xover[1], (double) s.xover[2]);
    double power[kMbBands], powerSum = 0.0;
    for (int k = 0; k < kMbBands; ++k) { power[k] = histPower (s.bandHist[k]); powerSum += power[k]; }
    double weighted = 0.0;                                                  // power share x the power the reduction leaves
    for (int k = 0; k < kMbBands; ++k)
    {
        auto& b = p.mb[(size_t) k];
        const uint32_t* h = s.bandHist[k];
        uint32_t n = 0; for (int i = 0; i < kHistBins; ++i) n += h[i];
        const float hi = histPercentile (h, 0.95f), lo = histPercentile (h, 0.10f), med = histPercentile (h, 0.5f);
        b.spreadDb = n > 0 ? hi - lo : 0.0f;
        b.attackMs = attack[k];
        float red = 0.0f;
        if (n == 0 || med < -70.0f) reason += std::string (mbBandName (k)) + ": no signal, left off.\n";
        if (n > 0 && med >= -70.0f && b.spreadDb >= 6.0f)
        {
            b.on = true;
            b.ratio = b.spreadDb < 8.0f ? 1.5f : b.spreadDb <= 12.0f ? 2.0f : 3.0f;
            b.offsetDb = mbDetectorOffsetDb (k, b.ratio);
            b.threshDb = findBusThreshold (h, p.inGainDb + b.offsetDb, b.ratio, 1.5f, -60.0f);
            b.reductionDb = red = (float) meanReduction (h, p.inGainDb + b.offsetDb, b.threshDb, b.ratio);
            reason += std::string (mbBandName (k)) + fmt (": level spread %.1f dB, ratio %.1f:1, threshold %.1f dB", (double) b.spreadDb, (double) b.ratio, (double) b.threshDb)
                    + fmt (", attack %.0f ms, predicted mean reduction %.2f dB.\n", (double) b.attackMs, (double) b.reductionDb);
            list += (list.empty() ? "" : ", ") + std::string (mbBandName (k)) + fmt (" %.1f:1 at %.1f dB", (double) b.ratio, (double) b.threshDb);
            p.mbSet = true;
        }
        else if (n > 0 && med >= -70.0f) reason += std::string (mbBandName (k)) + fmt (": level spread %.1f dB (under 6 dB: already steady), left off.\n", (double) b.spreadDb);
        weighted += powerSum > 0.0 ? power[k] / powerSum * std::pow (10.0, -(double) red / 10.0) : 0.0;
    }
    p.mbLoudnessRedDb = p.mbSet && weighted > 0.0 ? (float) std::max (0.0, -10.0 * std::log10 (weighted)) : 0.0f;
    if (! p.mbSet)
    {
        p.multiband.needed = false;
        p.multiband.summary = "no change needed (every band is steady, level spread under 6 dB)";
        p.multiband.reason = reason + "No band varies enough to gain from compression, so the multiband is left alone.";
        return;
    }
    // does the multiband already do this? (a band that is off or a multiband that is off counts as off)
    bool differs = ! c.curMbOn || std::abs (c.curMbMix - 100.0f) > 0.5f;
    for (int k = 0; k < kMbBands; ++k)
    {
        const auto& b = p.mb[(size_t) k]; const auto& cu = c.curMb[(size_t) k];
        if (b.on != cu.on) differs = true;
        else if (b.on && (std::abs (b.ratio - cu.ratio) > 0.05f || std::abs (b.threshDb - cu.thresh) > 0.05f || std::abs (b.attackMs - cu.attack) > 0.05f || ! cu.autoRel || std::abs (cu.make) > 0.05f)) differs = true;
    }
    p.multiband.needed = differs;
    const std::string sum = list + fmt (" (about %.1f dB reduction each, release auto)", 1.5);
    p.multiband.summary = differs ? sum : "no change needed (" + sum + ")";
    p.multiband.reason = reason + fmt ("Attack 30 / 15 / 8 / 4 ms from low to high, release auto, make-up 0 dB (the limiter's auto gain makes up the level), mix 100 %%. The multiband's phase mode and the crossovers are not changed.\n"
                                       "The threshold is where the level distribution (after the trim, plus a detector offset of about 6.5 .. 8.5 dB per band, because the compressor detects peaks and the histogram holds 400 ms RMS levels) is predicted to be reduced by 1.5 dB on average through the 6 dB knee. Together the bands lose about %.1f dB of loudness.",
                                       (double) p.mbLoudnessRedDb);
}

//==============================================================================
// Rule 4, imager. Per band from the measured correlation r of L / R (before the imager) and, for the low band, the side-to-mid energy. Mid is never touched.
inline void proposeImager (const AnalysisSnapshot& s, const AutoContext& c, Proposal& p)
{
    p.imager.available = true;
    static const char* names[3] = { "LOW", "MID", "HIGH" };
    std::string reason = fmt ("The imager bands are 0 .. %.0f Hz, %.0f Hz .. %.1f kHz and above (the crossovers the correlation was measured with).\n", (double) s.imgX[0], (double) s.imgX[0], (double) s.imgX[1] / 1000.0);
    bool measured = false;
    for (int k = 0; k < 3; ++k)
    {
        const float r = s.imgCorr[k];
        if (r > 1.5f) { p.widthPct[k] = 100.0f; reason += std::string (names[k]) + ": no signal in this band, width left at 100 %.\n"; continue; }
        measured = true;
        if (r > 0.8f) p.widthPct[k] = 115.0f;
        else if (r < 0.3f) { p.widthPct[k] = 90.0f; p.imgSafeOn = true; }
        else p.widthPct[k] = 100.0f;
        reason += std::string (names[k]) + fmt (": correlation %+.2f, side / mid %.1f dB: ", (double) r, (double) s.imgSideMidDb[k])
                + (r > 0.8f ? "close to mono, width 115 %.\n" : r < 0.3f ? "wide or out of phase, width 90 % and auto safe width on.\n" : "in between, width 100 %.\n");
    }
    const bool lowMono = s.imgCorr[0] <= 1.5f && s.imgSideMidDb[0] > -20.0f;
    p.monoBelowHz = lowMono ? 120.0f : 0.0f;
    reason += lowMono ? fmt ("The low band's side is %.1f dB against its mid (above -20 dB): mono below 120 Hz.\n", (double) s.imgSideMidDb[0]) : std::string ("The low band is nearly mono (side below -20 dB against mid): mono below stays off.\n");
    reason += "The mid level, the balance, the crossovers and the safe-width threshold are never changed; auto safe width is only switched on, never off.";
    if (! measured)
    {
        p.imager.needed = false;
        p.imager.summary = "no change needed (no signal in the imager bands)";
        p.imager.reason = reason;
        return;
    }
    // compared with the current settings (an imager that is off counts as neutral: 100 %, no mono below, safe off)
    const bool curOn = c.curImgOn;
    bool differs = false;
    for (int k = 0; k < 3; ++k) if (std::abs (p.widthPct[k] - (curOn ? c.curWidth[k] : 100.0f)) > 0.5f) differs = true;
    if (std::abs (p.monoBelowHz - (curOn ? c.curMonoBelow : 0.0f)) > 0.5f) differs = true;
    if (p.imgSafeOn && ! (curOn && c.curImgSafe)) differs = true;
    p.imgSet = differs;
    std::string sum = fmt ("WIDTH %.0f / ", (double) p.widthPct[0]) + fmt ("%.0f / ", (double) p.widthPct[1]) + fmt ("%.0f %%", (double) p.widthPct[2]);
    sum += p.monoBelowHz > 0.0f ? ", MONO BELOW 120 Hz" : ", MONO BELOW off";
    if (p.imgSafeOn) sum += ", SAFE WIDTH on";
    p.imager.needed = differs;
    p.imager.summary = differs ? sum : "no change needed (" + sum + ")";
    p.imager.reason = reason;
}

//==============================================================================
// The rules (stage 1: input trim, bus compressor, limiter; stage 2: EQ). All results are inside the parameter ranges.
inline Proposal propose (const AnalysisSnapshot& s, const AutoContext& c)
{
    Proposal p;
    if (s.programmeSeconds < kMinProgrammeSeconds || s.integratedLufs < -100.0f)
    {
        p.message = s.programmeSeconds <= 0.0f ? "Play the track through MS-1, then press ANALYZE." : fmt ("Heard %.0f s of programme; at least %.0f s are needed.", s.programmeSeconds, kMinProgrammeSeconds);
        return p;
    }
    p.valid = true;

    // 1. input trim: headroom, clipping warning, rumble high-pass
    {
        const float tp = s.truePeakDb;
        p.inGainDb = tp > -1.0f ? std::clamp (round01 (-(tp + 3.0f)), -24.0f, 0.0f) : 0.0f;
        p.clipWarning = s.clippedFraction > 1.0e-4f;
        const bool rumble = s.lowEnergyDb > -60.0f && s.lowEnergyDb >= s.midBassDb - 6.0f, dc = s.dcDb > -60.0f;
        p.hpfHz = (rumble || dc) ? 20.0f : 0.0f;
        p.input.needed = std::abs (p.inGainDb - c.curInGain) > 0.05f || std::abs (p.hpfHz - c.curHpf) > 0.5f;
        std::string sum = p.inGainDb < 0.0f ? fmt ("TRIM %+.1f dB", p.inGainDb) : std::string ("TRIM 0 dB");
        sum += p.hpfHz > 0.0f ? ", HIGH-PASS 20 Hz" : ", HIGH-PASS off";
        p.input.summary = p.input.needed ? sum : "no change needed (" + sum + ")";
        std::string r = fmt ("True peak %.1f dBTP (sample peak %.1f dBFS). ", tp, s.samplePeakDb);
        r += tp > -1.0f ? fmt ("Above -1 dBFS, so the trim is -(peak + 3 dB) = %.1f dB: the chain sees about -3 dBTP of headroom.\n", p.inGainDb)
                        : std::string ("At or below -1 dBFS: no trim needed.\n");
        r += fmt ("Below 25 Hz %.1f dBFS, 40 .. 60 Hz %.1f dBFS, DC offset %.1f dBFS: ", s.lowEnergyDb, s.midBassDb, s.dcDb);
        r += p.hpfHz > 0.0f ? (dc ? "the DC offset is above -60 dBFS, so the 20 Hz high-pass is proposed." : "the rumble is within 6 dB of the bass, so the 20 Hz high-pass is proposed.")
                            : "no rumble, the high-pass stays off.";
        if (p.clipWarning) r += fmt ("\nWARNING: the input is clipped (%.3f %% of the samples are at full scale); the limiter cannot repair it.", (double) s.clippedFraction * 100.0);
        p.input.reason = r;
        if (p.clipWarning) p.input.summary += "  (clipped input!)";
    }

    // 2. EQ: the style curve or the reference, the narrow cuts
    proposeEq (s, c, p);

    // 3. multiband, 4. imager (the bus and the limiter below count the multiband's predicted reduction)
    proposeMultiband (s, c, p);
    proposeImager (s, c, p);

    // 5. bus compressor: only when the material still breathes (PSR above 9 dB)
    float busRed = 0.0f;
    {
        const bool breathes = s.psrDb > 9.0f;
        if (breathes)
        {
            const float ratio = 2.0f, bshift = p.inGainDb - p.mbLoudnessRedDb + kBusDetectorOffsetDb;        // the trim, the multiband's reduction, the detector offset
            p.busOn = true; p.busRatio = 1; p.busAttack = 4; p.busRelease = 4;
            p.busThreshDb = findBusThreshold (s.shortTermHist, bshift, ratio, 1.5f);
            p.busReductionDb = (float) meanReduction (s.shortTermHist, bshift, p.busThreshDb, ratio);
            busRed = p.busReductionDb;
            p.bus.needed = ! (c.curBusOn && c.curBusRatio == p.busRatio && c.curBusAttack == p.busAttack && c.curBusRelease == p.busRelease && std::abs (c.curBusThresh - p.busThreshDb) < 0.05f);
            const std::string sum = fmt ("%.1f dB threshold, 2:1, attack 10 ms, release auto (about %.1f dB reduction)", p.busThreshDb, p.busReductionDb);
            p.bus.summary = p.bus.needed ? sum : "no change needed (" + sum + ")";
            p.bus.reason = fmt ("Input PSR %.1f dB (above 9 dB: the material still breathes). Ratio 2:1, attack 10 ms, release auto.\n", s.psrDb)
                         + fmt ("The threshold %.1f dB is where the short-term loudness distribution of the input (after the trim", p.busThreshDb) + (p.mbSet ? fmt (" and the multiband's %.1f dB", (double) p.mbLoudnessRedDb) : std::string()) + fmt (", plus a %.1f dB detector offset) is predicted to be reduced by 1.5 dB on average; ", (double) kBusDetectorOffsetDb)
                         + fmt ("with that threshold the predicted mean reduction is %.2f dB. Short-term loudness stands in for the detector level.", p.busReductionDb);
        }
        else
        {
            p.bus.needed = false;
            p.bus.summary = "no change needed (PSR " + fmt ("%.1f", s.psrDb) + " dB, already dense)";
            p.bus.reason = fmt ("Input PSR %.1f dB: at or below 9 dB the material has little dynamics left to glue, so the bus compressor is left alone.", s.psrDb);
        }
    }

    // 6. limiter: auto gain to the target, auto release, the character from the gain the target needs
    {
        p.limAuto = true; p.limRelAuto = true;
        const float after = s.integratedLufs + p.inGainDb - p.mbLoudnessRedDb - busRed;                  // the loudness the limiter sees
        p.neededGainDb = c.targetLufs - after;
        p.limChar = p.neededGainDb <= 4.0f ? 0 : p.neededGainDb <= 8.0f ? 1 : 2;
        p.heavyWarning = p.neededGainDb > 9.0f;
        p.limiter.needed = ! (c.curLimAuto && c.curLimRelAuto && c.curLimChar == p.limChar);
        std::string sum = std::string ("AUTO TO TARGET, AUTO REL., ") + limCharText (p.limChar) + fmt (" (needs %+.1f dB)", p.neededGainDb);
        if (p.heavyWarning) sum += "  heavy limiting!";
        p.limiter.summary = p.limiter.needed ? sum : "no change needed (" + sum + ")";
        std::string r = fmt ("Input integrated loudness %.1f LUFS; after the trim, the multiband's (%.1f dB) and the bus reduction ", (double) s.integratedLufs, (double) p.mbLoudnessRedDb)
                        + fmt ("%.1f LUFS; the target is %.1f LUFS, so the limiter needs %+.1f dB.\n", (double) after, (double) c.targetLufs, (double) p.neededGainDb);
        r += fmt ("Up to 4 dB: TRANSPARENT, up to 8 dB: PUNCHY, above: LOUD. Auto gain to the target and auto release are switched on; the target and the ceiling (%.1f dBTP) come from the target preset and are not changed.", c.ceilingDb);
        if (p.neededGainDb < 0.0f) r += "\nThe input is already louder than the target; the limiter's auto gain cannot turn it down.";
        if (p.heavyWarning) r += fmt ("\nWARNING: needs +%.1f dB: heavy limiting, check the dynamics.", p.neededGainDb);
        p.limiter.reason = r;
    }

    // 7. output: the gain after the limiter must not push the signal past the ceiling, so a boost is taken back to 0 dB (a cut is the user's choice and is kept);
    // the dither depends on the delivery format, which the measurements cannot tell: the current setting is shown and kept
    {
        p.outGainDb = c.curOutGain > 0.05f ? 0.0f : c.curOutGain;
        p.ditherBits = std::clamp (c.curDither, 0, 3);
        static const char* const bits[4] = { "OFF", "16 BIT", "20 BIT", "24 BIT" };
        p.output.needed = std::abs (p.outGainDb - c.curOutGain) > 0.05f;
        const std::string sum = fmt ("GAIN %+.1f dB", (double) p.outGainDb) + ", DITHER " + bits[p.ditherBits] + " (kept)";
        p.output.summary = p.output.needed ? sum : "no change needed (" + sum + ")";
        std::string r = fmt ("The output gain sits after the limiter: %.1f dB now. ", (double) c.curOutGain);
        r += c.curOutGain > 0.05f ? "A boost there goes past the true-peak ceiling, so 0 dB is proposed (the limiter's auto gain sets the level)." : "It does not push past the ceiling, so it is left as it is.";
        r += std::string ("\nDither: ") + bits[p.ditherBits] + " now. It depends on the bit depth you deliver (16 bit for CD, usually off for 24 bit or float files), which AUTO cannot measure from the audio, so the setting is not changed.";
        p.output.reason = r;
    }
    return p;
}

// the parameter writes of the checked rows
inline std::vector<ParamWrite> paramWrites (const Proposal& p, bool input, bool eq, bool multiband, bool imager, bool bus, bool limiter, bool output = false)
{
    std::vector<ParamWrite> w;
    if (! p.valid) return w;
    if (input) { w.push_back ({ "inGain", p.inGainDb }); w.push_back ({ "trimHpf", p.hpfHz }); }
    if (eq && p.eqSet)                                                  // all 8 bands, like MATCH EQ; eqPhase is never written
    {
        w.push_back ({ "eqOn", 1.0f });
        for (int i = 0; i < kEqBands; ++i)
        {
            const auto& e = p.eqBands[(size_t) i];
            const std::string id = "eq" + std::to_string (i + 1);
            w.push_back ({ (id + "On").c_str(), e.on ? 1.0f : 0.0f }); w.push_back ({ (id + "Freq").c_str(), e.freq });
            w.push_back ({ (id + "Gain").c_str(), e.on ? e.gainDb : 0.0f }); w.push_back ({ (id + "Q").c_str(), e.q });
            w.push_back ({ (id + "Mode").c_str(), 0.0f }); w.push_back ({ (id + "Dyn").c_str(), 0.0f });
        }
    }
    if (multiband && p.mbSet)                                           // mbPhase and the crossovers are never written
    {
        w.push_back ({ "mbOn", 1.0f }); w.push_back ({ "mbMix", 100.0f });
        for (int k = 0; k < kMbBands; ++k)
        {
            const auto& b = p.mb[(size_t) k];
            const std::string id = "mb" + std::to_string (k + 1);
            w.push_back ({ (id + "On").c_str(), b.on ? 1.0f : 0.0f });
            if (! b.on) continue;
            w.push_back ({ (id + "Thresh").c_str(), b.threshDb }); w.push_back ({ (id + "Ratio").c_str(), b.ratio }); w.push_back ({ (id + "Attack").c_str(), b.attackMs });
            w.push_back ({ (id + "Auto").c_str(), 1.0f }); w.push_back ({ (id + "Make").c_str(), 0.0f });
        }
    }
    if (imager && p.imgSet)                                             // the mid level (imgBalance), the crossovers and the safe-width threshold are never written
    {
        w.push_back ({ "imgOn", 1.0f });
        for (int k = 0; k < 3; ++k) w.push_back ({ ("width" + std::to_string (k + 1)).c_str(), p.widthPct[k] });
        w.push_back ({ "monoBelow", p.monoBelowHz });
        if (p.imgSafeOn) w.push_back ({ "imgSafe", 1.0f });
    }
    if (bus && p.busOn)
    {
        w.push_back ({ "busOn", 1.0f }); w.push_back ({ "busRatio", (float) p.busRatio }); w.push_back ({ "busAttack", (float) p.busAttack });
        w.push_back ({ "busRelease", (float) p.busRelease }); w.push_back ({ "busThresh", p.busThreshDb });
    }
    if (limiter) { w.push_back ({ "limAuto", p.limAuto ? 1.0f : 0.0f }); w.push_back ({ "limRelAuto", p.limRelAuto ? 1.0f : 0.0f }); w.push_back ({ "limChar", (float) p.limChar }); }
    if (output) { w.push_back ({ "outGain", p.outGainDb }); w.push_back ({ "ditherBits", (float) p.ditherBits }); }
    return w;
}
} // namespace ms1::autom
