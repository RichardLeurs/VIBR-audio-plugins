// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

// AX-1 Amp Lab engine (see .claude/ampcabinet_spec.md). The amp types, tone stack, cabinet filter model and AMBIENCE room come from IR-2, the gate, tight boost,
// extra amp controls, IR cabinet, EQ and limiter from MA-1 (copied, not shared, so the old plugins can be deleted). Chain:
//   input trim -> gate -> boost -> [4x oversampling: amp stages] -> tone stack + presence / resonance -> master
//   -> cabinet (IR A + IR B, or the filter model) -> low / high cut -> EQ -> ambience -> output -> limiter
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>
#include <complex>

namespace ax1
{
constexpr int kNumTypes = 14;
constexpr int kOsFactor = 2;                       // 4x oversampling around the boost and amp stages

struct AmpType
{
    const char* name;
    float gainLoDb, gainHiDb;      // first-stage gain range of the GAIN knob
    float hpHz;                    // input high-pass (tightness / brightness)
    float midHz, midDb;            // mid emphasis in front of the gain stages
    int stages;                    // clipping stages (1 .. 4)
    float asym;                    // asymmetry of the first stage (even harmonics)
    float interHpHz;               // coupling high-pass between the stages
    float cabLpHz;                 // cabinet / fizz roll-off of the filter model
    float trimDb;                  // loudness match at gain 5, master 5 (see EngineTest)
};

// order = selector order. The first 11 are the IR-2 table, the last three the MA-1 voicings.
constexpr AmpType kTypes[kNumTypes] = {
    { "CLEAN",   -6.0f, 14.0f,  60.0f,  600.0f, -1.0f, 1, 0.00f, 140.0f, 6500.0f,  14.4f },
    { "TWN",     -4.0f, 18.0f,  50.0f,  500.0f, -3.0f, 1, 0.05f, 140.0f, 6200.0f,  11.7f },
    { "TWEED",    2.0f, 24.0f,  70.0f,  800.0f,  3.0f, 1, 0.25f, 140.0f, 4800.0f,  6.5f },
    { "DIAMOND",  8.0f, 28.0f, 150.0f, 1500.0f,  3.0f, 1, 0.10f, 140.0f, 6000.0f,   4.3f },
    { "CRUNCH",  10.0f, 30.0f,  90.0f, 1000.0f,  2.0f, 2, 0.15f, 140.0f, 5200.0f,   -3.0f },
    { "BRIT",    14.0f, 36.0f, 100.0f, 1000.0f,  4.0f, 2, 0.10f, 140.0f, 5000.0f,   -4.9f },
    { "BROWN",   16.0f, 38.0f,  80.0f,  700.0f,  3.0f, 2, 0.20f, 140.0f, 4800.0f,   -5.4f },
    { "HI-GAIN", 22.0f, 46.0f, 120.0f, 1600.0f,  2.0f, 3, 0.05f, 140.0f, 5200.0f,   -7.1f },
    { "SLDN",    24.0f, 48.0f,  90.0f,  900.0f,  5.0f, 3, 0.10f, 140.0f, 4600.0f,   -7.0f },
    { "MODDED",  22.0f, 46.0f, 110.0f, 1200.0f,  3.0f, 3, 0.15f, 140.0f, 5000.0f,   -7.0f },
    { "RFIER",   24.0f, 48.0f,  70.0f,  500.0f, -3.0f, 3, 0.05f, 140.0f, 5000.0f,   -6.8f },
    { "MODERN",  14.0f, 40.0f, 130.0f, 1400.0f,  3.0f, 3, 0.08f, 160.0f, 6000.0f,  -6.6f },
    { "BRUTAL",  20.0f, 46.0f, 140.0f,  900.0f,  4.0f, 4, 0.05f, 180.0f, 5600.0f,  -7.3f },
    { "ULTRA",   26.0f, 52.0f, 170.0f, 1800.0f,  5.0f, 4, 0.00f, 200.0f, 5200.0f,  -7.5f },
};

inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
inline float shape (float x, float asym) { return std::tanh (x + asym) - std::tanh (asym); }

// ---- parameter mappings (knobs run 0 .. 10) ----
inline float gateThresholdDb (float k)  { return -70.0f + 4.0f * k; }                        // -70 .. -30 dB
inline float gateReleaseMs (float k)    { return 30.0f * std::pow (13.0f, k * 0.1f); }       // 30 .. 400 ms
inline float tightHz (float k)          { return 40.0f * std::pow (22.5f, k * 0.1f); }       // 40 .. 900 Hz
inline float bipolarDb (float k)        { return (k - 5.0f) * 2.4f; }                        // +-12 dB
inline float eqFreqHz (float k)         { return 200.0f * std::pow (15.0f, k * 0.1f); }      // 200 Hz .. 3 kHz
inline float lowCutHz (float k)         { return 40.0f * std::pow (10.0f, k * 0.1f); }       // 40 .. 400 Hz
inline float highCutHz (float k)        { return 3000.0f * std::pow (5.3f, k * 0.1f); }      // 3 kHz .. 16 kHz
inline float irLevelGain (float k)      { return k <= 0.01f ? 0.0f : dbToGain ((k - 7.0f) * 3.0f); }
inline float presenceDb (float k)       { return (k - 5.0f) * 1.2f; }                        // +-6 dB
inline float masterDb (float k)         { return -24.0f + 3.0f * k; }                        // -24 .. +6 dB (before the type trim)
inline float outputDb (float k)         { return k <= 0.01f ? -100.0f : k >= 5.0f ? (k - 5.0f) * 1.2f : -60.0f * std::pow ((5.0f - k) / 5.0f, 2.0f); }   // -inf .. +6 dB, 5 = 0 dB

// RBJ biquad, transposed direct form II
class Biquad
{
public:
    void lowpass (double sr, float f, float q)   { prep (sr, f, q); set ((1 - cw) / 2, 1 - cw, (1 - cw) / 2, 1 + alpha, -2 * cw, 1 - alpha); }
    void highpass (double sr, float f, float q)  { prep (sr, f, q); set ((1 + cw) / 2, -(1 + cw), (1 + cw) / 2, 1 + alpha, -2 * cw, 1 - alpha); }
    void peak (double sr, float f, float q, float db)
    {
        prep (sr, f, q); const double A = std::pow (10.0, db / 40.0);
        set (1 + alpha * A, -2 * cw, 1 - alpha * A, 1 + alpha / A, -2 * cw, 1 - alpha / A);
    }
    void lowShelf (double sr, float f, float db)
    {
        prep (sr, f, 0.7071f); const double A = std::pow (10.0, db / 40.0), sq = 2 * std::sqrt (A) * alpha;
        set (A * ((A + 1) - (A - 1) * cw + sq), 2 * A * ((A - 1) - (A + 1) * cw), A * ((A + 1) - (A - 1) * cw - sq),
             (A + 1) + (A - 1) * cw + sq, -2 * ((A - 1) + (A + 1) * cw), (A + 1) + (A - 1) * cw - sq);
    }
    void highShelf (double sr, float f, float db)
    {
        prep (sr, f, 0.7071f); const double A = std::pow (10.0, db / 40.0), sq = 2 * std::sqrt (A) * alpha;
        set (A * ((A + 1) + (A - 1) * cw + sq), -2 * A * ((A - 1) + (A + 1) * cw), A * ((A + 1) + (A - 1) * cw - sq),
             (A + 1) - (A - 1) * cw + sq, 2 * ((A - 1) - (A + 1) * cw), (A + 1) - (A - 1) * cw - sq);
    }
    void reset() { z1 = z2 = 0.0; }
    float process (float x)
    {
        const double y = c0 * (double) x + z1;
        z1 = c1 * (double) x - d1 * y + z2;
        z2 = c2 * (double) x - d2 * y;
        return (float) y;
    }
    // magnitude of the transfer function at f (for the display and the tests)
    double magnitude (double sr, double f) const
    {
        const double w = 2.0 * juce::MathConstants<double>::pi * f / sr;
        const std::complex<double> z1i = std::polar (1.0, -w), z2i = std::polar (1.0, -2.0 * w);
        const auto num = c0 + c1 * z1i + c2 * z2i, den = 1.0 + d1 * z1i + d2 * z2i;
        return std::abs (num / den);
    }
private:
    void prep (double sr, float f, float q)
    {
        const double w = 2.0 * juce::MathConstants<double>::pi * juce::jmin ((double) f, 0.45 * sr) / sr;
        cw = std::cos (w); alpha = std::sin (w) / (2.0 * (double) q);
    }
    void set (double b0, double b1, double b2, double a0, double a1, double a2)
    {
        c0 = b0 / a0; c1 = b1 / a0; c2 = b2 / a0; d1 = a1 / a0; d2 = a2 / a0;
    }
    double cw = 1.0, alpha = 0.0;
    double c0 = 1, c1 = 0, c2 = 0, d1 = 0, d2 = 0, z1 = 0, z2 = 0;      // double: the low high-passes at the oversampled rate need it
};

// Noise gate with a high-passed detector, hold and hysteresis.
class Gate
{
public:
    void prepare (double sampleRate) { sr = sampleRate; hp.highpass (sr, 150.0f, 0.7071f); reset(); }
    void reset() { hp.reset(); env = 0.0f; gain = 0.0f; open = false; holdLeft = 0; }
    float process (float peakIn, float thresholdDb, float releaseMs)
    {
        const float x = std::fabs (hp.process (peakIn));
        const float aC = std::exp (-1.0f / (0.0005f * (float) sr)), rC = std::exp (-1.0f / (0.030f * (float) sr));
        env = x > env ? aC * env + (1.0f - aC) * x : rC * env + (1.0f - rC) * x;
        const float db = 20.0f * std::log10 (env + 1.0e-9f);
        if (db > thresholdDb) { open = true; holdLeft = (int) (0.015 * sr); }
        else if (open && db < thresholdDb - 4.0f)
        {
            if (holdLeft > 0) --holdLeft; else open = false;
        }
        const float target = open ? 1.0f : 0.0f;
        const float c = target > gain ? std::exp (-1.0f / (0.001f * (float) sr)) : std::exp (-1.0f / (0.001f * releaseMs * (float) sr));
        gain = c * gain + (1.0f - c) * target;
        return gain;
    }
    bool isOpen() const { return open; }
private:
    double sr = 44100.0;
    Biquad hp;
    float env = 0.0f, gain = 0.0f;
    bool open = false;
    int holdLeft = 0;
};

// tanh clipper with first order antiderivative anti-aliasing (ADAA): the output is the average of tanh over the step from the last input to this one
struct Clipper
{
    double xPrev = 0.0;
    void reset() { xPrev = 0.0; }
    static double logCosh (double x) { const double a = std::fabs (x); return a + std::log1p (std::exp (-2.0 * a)) - 0.6931471805599453; }
    float process (float xIn, float asym)
    {
        const double x = (double) xIn + (double) asym, xp = xPrev + (double) asym, dx = x - xp;
        double y;
        if (std::fabs (dx) < 1.0e-5) y = std::tanh (0.5 * (x + xp));
        else y = (logCosh (x) - logCosh (xp)) / dx;
        xPrev = (double) xIn;
        return (float) (y - std::tanh ((double) asym));
    }
};

constexpr float kStageLpHz = 9000.0f, kOutLpHz = 14000.0f;

// the part of the signal path that runs at the oversampled rate: boost + amp stages
struct AmpPre
{
    Biquad tight, boostMid, hp, mid, bright, inter1, inter2, inter3, lp1, lp2, lp3, lpOut;      // lp*: the bandwidth of each stage, so that the hard clipped edges do not alias
    Clipper clip1, clip2, clip3, clip4;
    float sagEnv = 0.0f;
    void reset() { clip1.reset(); clip2.reset(); clip3.reset(); clip4.reset(); tight.reset(); boostMid.reset(); hp.reset(); mid.reset(); bright.reset(); inter1.reset(); inter2.reset(); inter3.reset(); lp1.reset(); lp2.reset(); lp3.reset(); lpOut.reset(); sagEnv = 0.0f; }

    void setBoost (double osRate, float tightK)
    {
        tight.highpass (osRate, tightHz (tightK), 0.7071f);
        boostMid.peak (osRate, 720.0f, 0.7f, 5.0f);
    }
    void setAmp (double osRate, const AmpType& t, bool brightOn)
    {
        hp.highpass (osRate, t.hpHz, 0.7071f);
        mid.peak (osRate, t.midHz, 0.9f, t.midDb);
        bright.highShelf (osRate, 2800.0f, brightOn ? 6.0f : 0.0f);
        inter1.highpass (osRate, t.interHpHz, 0.7071f);
        inter2.highpass (osRate, t.interHpHz, 0.7071f);
        inter3.highpass (osRate, t.interHpHz, 0.7071f);
        lp1.lowpass (osRate, kStageLpHz, 0.7071f); lp2.lowpass (osRate, kStageLpHz, 0.7071f); lp3.lowpass (osRate, kStageLpHz, 0.7071f); lpOut.lowpass (osRate, kOutLpHz, 0.7071f);
    }
    float processBoost (float x, float drive, float levelGain)
    {
        float y = boostMid.process (tight.process (x));
        y = shape (y * drive, 0.0f) / std::sqrt (drive);                // drive 1 = clean, higher = softly clipped, level kept in check
        return y * levelGain;
    }
    float processAmp (float x, float g1, const AmpType& t, float sag, float sagCoefAtk, float sagCoefRel)
    {
        float y = clip1.process (bright.process (mid.process (hp.process (x))) * g1, t.asym);
        if (t.stages < 2) return y;
        const float a = std::fabs (y);
        sagEnv = a > sagEnv ? sagCoefAtk * sagEnv + (1.0f - sagCoefAtk) * a : sagCoefRel * sagEnv + (1.0f - sagCoefRel) * a;
        const float sagGain = 1.0f - sag * 0.35f * juce::jmin (1.0f, sagEnv);       // the supply sags under load: later stages get less gain
        y = clip2.process (inter1.process (lp1.process (y)) * 3.0f * sagGain, t.asym * 0.5f);
        if (t.stages >= 3) y = clip3.process (inter2.process (lp2.process (y)) * 3.0f * sagGain, 0.0f);
        if (t.stages >= 4) y = clip4.process (inter3.process (lp3.process (y)) * 2.5f * sagGain, 0.0f);
        return lpOut.process (y);
    }
};

// the cabinet path after the amp: the filter model of a 4x12 (used while no impulse response is loaded), low / high cut and the 3 band EQ
struct CabModel
{
    Biquad hp, low, pres, lp1, lp2, lowCut1, lowCut2, highCut1, highCut2, eqLow, eqMid, eqHigh;
    void reset() { for (auto* b : { &hp, &low, &pres, &lp1, &lp2, &lowCut1, &lowCut2, &highCut1, &highCut2, &eqLow, &eqMid, &eqHigh }) b->reset(); }
    // mic01: 0 = centre of the speaker (bright, forward), 1 = edge (dark, soft)
    void setModel (double sr, const AmpType& t, float mic01)
    {
        const float m = juce::jlimit (0.0f, 1.0f, mic01), lpHz = t.cabLpHz * (1.25f - 0.5f * m);
        hp.highpass (sr, 85.0f, 0.7071f);
        low.peak (sr, 110.0f, 1.2f, 2.0f + 2.0f * m);
        pres.peak (sr, 2800.0f - 500.0f * m, 1.0f, 5.0f - 4.5f * m);
        lp1.lowpass (sr, lpHz, 0.7071f);
        lp2.lowpass (sr, lpHz * 1.15f, 0.7071f);
    }
    void setCuts (double sr, float lowCutK, float highCutK)
    {
        lowCut1.highpass (sr, lowCutHz (lowCutK), 0.7071f);  lowCut2.highpass (sr, lowCutHz (lowCutK), 0.7071f);
        highCut1.lowpass (sr, highCutHz (highCutK), 0.7071f); highCut2.lowpass (sr, highCutHz (highCutK), 0.7071f);
    }
    void setEq (double sr, float lowK, float midK, float freqK, float highK)
    {
        eqLow.lowShelf (sr, 100.0f, bipolarDb (lowK));
        eqMid.peak (sr, eqFreqHz (freqK), 1.2f, bipolarDb (midK));
        eqHigh.highShelf (sr, 6000.0f, bipolarDb (highK));
    }
    float model (float x) { return lp2.process (lp1.process (pres.process (low.process (hp.process (x))))); }
    float cuts (float x)  { return highCut2.process (highCut1.process (lowCut2.process (lowCut1.process (x)))); }
    float eq (float x)    { return eqHigh.process (eqMid.process (eqLow.process (x))); }
    double modelMag (double sr, double f) const { return hp.magnitude (sr, f) * low.magnitude (sr, f) * pres.magnitude (sr, f) * lp1.magnitude (sr, f) * lp2.magnitude (sr, f); }
    double cutsMag (double sr, double f) const  { return lowCut1.magnitude (sr, f) * lowCut2.magnitude (sr, f) * highCut1.magnitude (sr, f) * highCut2.magnitude (sr, f); }
    double eqMag (double sr, double f) const    { return eqLow.magnitude (sr, f) * eqMid.magnitude (sr, f) * eqHigh.magnitude (sr, f); }
};

// peak limiter at the end of the chain: instant attack (the ceiling is never exceeded), 80 ms release
struct Limiter
{
    float gain = 1.0f;
    void reset() { gain = 1.0f; }
    float process (float peak, double sr, float ceiling)
    {
        const float target = peak > ceiling ? ceiling / peak : 1.0f;
        if (target < gain) gain = target;
        else { const float c = std::exp (-1.0f / (0.080f * (float) sr)); gain = c * gain + (1.0f - c) * target; }
        return gain;
    }
};

// small stereo room (Freeverb-like: 4 damped combs and 2 all-passes per side), the AMBIENCE
class Room
{
public:
    void prepare (double sr)
    {
        static constexpr int combLen[4] = { 1116, 1188, 1277, 1356 }, apLen[2] = { 556, 441 };
        const double k = sr / 44100.0;
        for (int c = 0; c < 2; ++c)
        {
            const int spread = c * 23;
            for (int i = 0; i < 4; ++i) { comb[c][i].buf.assign ((size_t) ((combLen[i] + spread) * k) + 4, 0.0f); comb[c][i].pos = 0; comb[c][i].lp = 0.0f; }
            for (int i = 0; i < 2; ++i) { ap[c][i].buf.assign ((size_t) ((apLen[i] + spread) * k) + 4, 0.0f); ap[c][i].pos = 0; }
        }
    }
    void clear()
    {
        for (auto& c : comb) for (auto& cb : c) { std::fill (cb.buf.begin(), cb.buf.end(), 0.0f); cb.lp = 0.0f; }
        for (auto& c : ap) for (auto& a : c) std::fill (a.buf.begin(), a.buf.end(), 0.0f);
    }
    float process (int c, float x)
    {
        float sum = 0.0f;
        for (auto& cb : comb[c])
        {
            const float o = cb.buf[(size_t) cb.pos];
            cb.lp = o * 0.7f + cb.lp * 0.3f;                          // damping
            cb.buf[(size_t) cb.pos] = x * 0.03f + cb.lp * 0.80f;
            if (++cb.pos >= (int) cb.buf.size()) cb.pos = 0;
            sum += o;
        }
        for (auto& a : ap[c])
        {
            const float b = a.buf[(size_t) a.pos];
            const float y = -sum + b;
            a.buf[(size_t) a.pos] = sum + b * 0.5f;
            if (++a.pos >= (int) a.buf.size()) a.pos = 0;
            sum = y;
        }
        return sum;
    }
private:
    struct Comb { std::vector<float> buf; int pos = 0; float lp = 0.0f; };
    struct Allpass { std::vector<float> buf; int pos = 0; };
    Comb comb[2][4];
    Allpass ap[2][2];
};

//==============================================================================
struct Params
{
    float inGainDb = 0.0f;
    bool gateOn = true;       float gateThresh = 4.0f, gateRelease = 3.0f;
    bool boostOn = false;     float boostDrive = 2.0f, boostLevel = 5.0f, boostTight = 5.0f;
    int type = 5;
    float gain = 5.0f, bass = 5.0f, middle = 5.0f, treble = 5.0f, presence = 5.0f, resonance = 5.0f, sag = 3.0f, master = 5.0f;
    bool bright = false, deep = false;
    bool cabOn = true;        float irALevel = 7.0f, irBLevel = 7.0f, lowCut = 2.0f, highCut = 7.0f, mic = 3.0f;
    float ambience = 0.0f;
    bool eqOn = false;        float eqLow = 5.0f, eqMid = 5.0f, eqFreq = 5.0f, eqHigh = 5.0f;
    float out = 5.0f;
    bool limiter = true;
};

class Engine
{
public:
    static constexpr int kMaxChannels = 2;

    void prepare (double sampleRate, int maxBlockSize)
    {
        sr = sampleRate;
        maxBlock = juce::jmax (1, maxBlockSize);
        oversampling = std::make_unique<juce::dsp::Oversampling<float>> ((size_t) kMaxChannels, (size_t) kOsFactor,
                                                                          juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
        oversampling->initProcessing ((size_t) maxBlock);
        oversampling->reset();
        latencySamples = (int) std::ceil (oversampling->getLatencyInSamples());
        room.prepare (sampleRate);
        const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlock, (juce::uint32) kMaxChannels };
        for (int i = 0; i < 2; ++i) { conv[i].prepare (spec); irBuf[i].setSize (kMaxChannels, maxBlock, false, false, true); }
        reset();
    }
    void reset()
    {
        gate.prepare (sr);
        limiter.reset();
        for (auto& a : amp) a.reset();
        for (auto& m : cab) m.reset();
        for (auto* b : { &bass, &middle, &treble, &presence, &resonance, &deep })
            for (auto& x : *b) x.reset();
        room.clear();
        primed = false; curType = -1; fadeMode = 0; typeGain = 1.0f; roomDirty = false;
        if (oversampling != nullptr) oversampling->reset();
    }
    int latency() const { return latencySamples; }

    // message thread: the impulse response in a file's bytes (WAV / AIFF), or none
    void loadIr (int slot, const void* data, size_t size)
    {
        conv[slot].loadImpulseResponse (data, size, juce::dsp::Convolution::Stereo::yes, juce::dsp::Convolution::Trim::yes, 0, juce::dsp::Convolution::Normalise::yes);
        irLoaded[slot] = size > 0;
    }
    void clearIr (int slot) { irLoaded[slot] = false; conv[slot].reset(); }
    bool hasIr (int slot) const { return irLoaded[slot].load(); }

    // what the amp stages do to a static input level x (base rate, after the gate), without the filters: the CURVE display
    static float staticCurve (const Params& p, float x)
    {
        const auto& t = kTypes[juce::jlimit (0, kNumTypes - 1, p.type)];
        float y = x;
        if (p.boostOn) { const float d = 1.0f + p.boostDrive * 0.1f * 15.0f; y = shape (y * d, 0.0f) / std::sqrt (d) * dbToGain (bipolarDb (p.boostLevel)); }
        y = shape (y * dbToGain (t.gainLoDb + (t.gainHiDb - t.gainLoDb) * p.gain * 0.1f), t.asym);
        if (t.stages >= 2)
        {
            const float sagGain = 1.0f - p.sag * 0.1f * 0.35f * juce::jmin (1.0f, std::fabs (y));
            y = shape (y * 3.0f * sagGain, t.asym * 0.5f);
            if (t.stages >= 3) y = shape (y * 3.0f * sagGain, 0.0f);
            if (t.stages >= 4) y = shape (y * 2.5f * sagGain, 0.0f);
        }
        return y;
    }

    // process in place; channels = 1 or 2
    void process (float* const* chans, int channels, int total, const Params& p)
    {
        channels = juce::jmin (channels, kMaxChannels);
        if (total <= 0 || channels <= 0 || oversampling == nullptr) return;
        setTargets (p);

        const int type = juce::jlimit (0, kNumTypes - 1, p.type);
        const double osRate = sr * (double) (1 << kOsFactor);
        const float sagAtk = std::exp (-1.0f / (0.005f * (float) osRate)), sagRel = std::exp (-1.0f / (0.120f * (float) osRate));
        const int chunk = juce::jmin (maxBlock, 4096);
        for (int start = 0, n = 0; start < total; start += n)
        {
            // a new amp type fades the output out (512 samples), switches the stages and fades in again (2048 samples); short blocks while it fades
            if (curType < 0) { curType = type; for (auto& a : amp) a.reset(); }
            else if (fadeMode == 3) { curType = type; for (auto& a : amp) a.reset(); fadeMode = 2; }
            else if (type != curType && fadeMode != 1) fadeMode = 1;
            const auto& t = kTypes[curType];
            n = juce::jmin (fadeMode != 0 ? 64 : chunk, total - start);
            float* ch[kMaxChannels] = {};
            for (int c = 0; c < channels; ++c) ch[c] = chans[c] + start;

            // ---- input trim and gate (base rate)
            float inP = 0.0f;
            const float gateTh = gateThresholdDb (p.gateThresh), gateRel = gateReleaseMs (sm[sGateRel].getCurrentValue());
            for (int i = 0; i < n; ++i)
            {
                const float g = dbToGain (sm[sIn].getNextValue());
                float peak = 0.0f;
                for (int c = 0; c < channels; ++c) { inP = juce::jmax (inP, std::fabs (ch[c][i])); ch[c][i] *= g; peak = juce::jmax (peak, std::fabs (ch[c][i])); }
                if (p.gateOn)
                {
                    gateGainNow = gate.process (peak, gateTh, gateRel);
                    for (int c = 0; c < channels; ++c) ch[c][i] *= gateGainNow;
                }
            }
            raise (inPeak, inP);
            gateLamp.store (p.gateOn ? gateGainNow : -1.0f);
            dotIn.store (ch[0][n - 1]);

            // ---- boost + amp stages (oversampled)
            juce::dsp::AudioBlock<float> block (ch, (size_t) channels, (size_t) n);
            const float g1 = dbToGain (t.gainLoDb + (t.gainHiDb - t.gainLoDb) * sm[sGain].skip (n) * 0.1f);
            const float drive = 1.0f + sm[sDrive].skip (n) * 0.1f * 15.0f;
            const float boostLevel = dbToGain (bipolarDb (sm[sBoostLevel].skip (n)));
            const float tightK = sm[sTight].skip (n), sag = sm[sSag].skip (n) * 0.1f;
            for (int c = 0; c < channels; ++c)
            {
                amp[c].setBoost (osRate, tightK);
                amp[c].setAmp (osRate, t, p.bright);
            }
            auto up = oversampling->processSamplesUp (block);
            for (int c = 0; c < channels; ++c)
            {
                auto* x = up.getChannelPointer ((size_t) c);
                for (size_t i = 0; i < up.getNumSamples(); ++i)
                {
                    float y = x[i];
                    if (p.boostOn) y = amp[c].processBoost (y, drive, boostLevel);
                    x[i] = amp[c].processAmp (y, g1, t, sag, sagAtk, sagRel);
                }
            }
            oversampling->processSamplesDown (block);

            // ---- tone stack, presence, resonance, master (base rate)
            const float bassK = sm[sBass].skip (n) * 0.1f, midK = sm[sMid].skip (n) * 0.1f, trebK = sm[sTreb].skip (n) * 0.1f;
            const float presDb = presenceDb (sm[sPres].skip (n)), resDb = presenceDb (sm[sRes].skip (n));
            const float mic01 = sm[sMic].skip (n) * 0.1f, lowCutK = sm[sLowCut].skip (n), highCutK = sm[sHighCut].skip (n);
            for (int c = 0; c < channels; ++c)
            {
                bass[c].lowShelf (sr, 120.0f, (bassK - 0.5f) * 24.0f);
                middle[c].peak (sr, 700.0f, 0.8f, (midK - 0.5f) * 20.0f);
                treble[c].highShelf (sr, 3200.0f, (trebK - 0.5f) * 24.0f);
                presence[c].highShelf (sr, 3500.0f, presDb);
                resonance[c].lowShelf (sr, 100.0f, resDb);
                deep[c].lowShelf (sr, 90.0f, p.deep ? 4.0f : 0.0f);
                cab[c].setModel (sr, t, mic01);
                cab[c].setCuts (sr, lowCutK, highCutK);
            }
            const float masterGain = dbToGain (masterDb (sm[sMaster].skip (n)) + t.trimDb);
            for (int c = 0; c < channels; ++c)
                for (int i = 0; i < n; ++i)
                    ch[c][i] = deep[c].process (resonance[c].process (presence[c].process (treble[c].process (middle[c].process (bass[c].process (ch[c][i])))))) * masterGain;

            // ---- cabinet: impulse responses A + B, or the filter model when none is loaded
            const float irLa = irLevelGain (sm[sIrA].skip (n)), irLb = irLevelGain (sm[sIrB].skip (n));
            if (p.cabOn)
            {
                const bool a = irLoaded[0].load(), b = irLoaded[1].load();
                if (a || b)
                {
                    for (int s = 0; s < 2; ++s)
                        if (irLoaded[s].load())
                        {
                            for (int c = 0; c < channels; ++c) irBuf[s].copyFrom (c, 0, ch[c], n);
                            juce::dsp::AudioBlock<float> ib (irBuf[s]);
                            auto sub = ib.getSubBlock (0, (size_t) n).getSubsetChannelBlock (0, (size_t) channels);
                            juce::dsp::ProcessContextReplacing<float> ctx (sub);
                            conv[s].process (ctx);
                        }
                    for (int c = 0; c < channels; ++c)
                        for (int i = 0; i < n; ++i)
                            ch[c][i] = (a ? irBuf[0].getSample (c, i) * irLa : 0.0f) + (b ? irBuf[1].getSample (c, i) * irLb : 0.0f);
                }
                else
                    for (int c = 0; c < channels; ++c)
                        for (int i = 0; i < n; ++i)
                            ch[c][i] = cab[c].model (ch[c][i]);
                for (int c = 0; c < channels; ++c)
                    for (int i = 0; i < n; ++i)
                        ch[c][i] = cab[c].cuts (ch[c][i]);
            }

            // ---- EQ (the coefficients follow the smoothed knobs every 32 samples)
            for (int sub = 0; sub < n; sub += 32)
            {
                const int m = juce::jmin (32, n - sub);
                const float eqL = sm[sEqLow].skip (m), eqM = sm[sEqMid].skip (m), eqF = sm[sEqFreq].skip (m), eqH = sm[sEqHigh].skip (m);
                if (! p.eqOn) continue;
                for (int c = 0; c < channels; ++c)
                {
                    cab[c].setEq (sr, eqL, eqM, eqF, eqH);
                    for (int i = sub; i < sub + m; ++i) ch[c][i] = cab[c].eq (ch[c][i]);
                }
            }

            // ---- ambience (a mono-summed send into the stereo room), output level, limiter
            const bool ambActive = p.ambience > 0.001f || sm[sAmb].isSmoothing();
            if (! ambActive && roomDirty) { room.clear(); roomDirty = false; }
            float limMin = 1.0f, outP = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                const float amb = sm[sAmb].getNextValue() * 0.1f * 1.35f;
                const float outGain = dbToGain (outputDb (sm[sOut].getNextValue())) * (sm[sOut].getCurrentValue() <= 0.01f ? 0.0f : 1.0f);
                if (fadeMode == 1) { typeGain -= 1.0f / 512.0f; if (typeGain <= 0.0f) { typeGain = 0.0f; fadeMode = 3; } }
                else if (fadeMode == 2) { typeGain += 1.0f / 2048.0f; if (typeGain >= 1.0f) { typeGain = 1.0f; fadeMode = 0; } }
                const float fade = typeGain;
                if (ambActive)
                {
                    const float send = channels == 1 ? ch[0][i] : 0.5f * (ch[0][i] + ch[1][i]);
                    for (int c = 0; c < channels; ++c) ch[c][i] += amb * room.process (c, send);
                    roomDirty = true;
                }
                float peak = 0.0f;
                for (int c = 0; c < channels; ++c) { ch[c][i] *= outGain * fade; peak = juce::jmax (peak, std::fabs (ch[c][i])); }
                if (p.limiter)
                {
                    const float lg = limiter.process (peak, sr, 0.944f);          // -0.5 dB ceiling
                    limMin = juce::jmin (limMin, lg);
                    for (int c = 0; c < channels; ++c) ch[c][i] *= lg;
                    peak *= lg;
                }
                outP = juce::jmax (outP, peak);
            }
            raise (outPeak, outP);
            lower (limiterMin, limMin);
        }
    }

    // read by the editor (atomics): peaks are the maximum since the editor last reset them with exchange (0 / 1)
    std::atomic<float> inPeak { 0.0f }, outPeak { 0.0f }, limiterMin { 1.0f }, gateLamp { -1.0f }, dotIn { 0.0f };

private:
    enum { sIn, sGain, sDrive, sBoostLevel, sTight, sMaster, sSag, sBass, sMid, sTreb, sPres, sRes, sLowCut, sHighCut, sMic, sIrA, sIrB,
           sEqLow, sEqMid, sEqFreq, sEqHigh, sAmb, sOut, sGateRel, sCount };

    static void raise (std::atomic<float>& a, float v) { if (v > a.load (std::memory_order_relaxed)) a.store (v, std::memory_order_relaxed); }
    static void lower (std::atomic<float>& a, float v) { if (v < a.load (std::memory_order_relaxed)) a.store (v, std::memory_order_relaxed); }

    void setTargets (const Params& p)
    {
        const float v[sCount] = { p.inGainDb, p.gain, p.boostDrive, p.boostLevel, p.boostTight, p.master, p.sag, p.bass, p.middle, p.treble, p.presence, p.resonance,
                                  p.lowCut, p.highCut, p.mic, p.irALevel, p.irBLevel, p.eqLow, p.eqMid, p.eqFreq, p.eqHigh, p.ambience, p.out, p.gateRelease };
        for (int i = 0; i < sCount; ++i)
        {
            if (! primed) { sm[(size_t) i].reset (sr, 0.03); sm[(size_t) i].setCurrentAndTargetValue (v[i]); }
            else sm[(size_t) i].setTargetValue (v[i]);
        }
        primed = true;
    }

    double sr = 44100.0;
    int maxBlock = 512, latencySamples = 0, curType = -1, fadeMode = 0;      // fadeMode: 0 steady, 1 fading out, 2 fading in, 3 faded out (switch the stages)
    float typeGain = 1.0f;
    bool primed = false, roomDirty = false;
    float gateGainNow = 0.0f;

    Gate gate;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    AmpPre amp[kMaxChannels];
    Biquad bass[kMaxChannels], middle[kMaxChannels], treble[kMaxChannels], presence[kMaxChannels], resonance[kMaxChannels], deep[kMaxChannels];
    CabModel cab[kMaxChannels];
    Limiter limiter;
    Room room;
    juce::dsp::Convolution conv[2];
    juce::AudioBuffer<float> irBuf[2];
    std::atomic<bool> irLoaded[2] { false, false };
    std::array<juce::SmoothedValue<float>, sCount> sm;
};
} // namespace ax1
