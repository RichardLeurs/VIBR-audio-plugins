// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once
#include "dsp/SoftClip.h"
#include "PhantomEngine.h"
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

// SF-1 Spectral Fold: a phase-vocoder engine. Each channel is cut into overlapping frames (2048 samples, hop 512, Hann), turned into magnitude and true
// frequency per bin, folded in the spectral domain and put back together with accumulated phases. Per frame, in this order:
//   FEEDBACK (the previous output spectrum is added back in) -> CAPTURE (the snapshot for MORPH) -> FREEZE (4 bands, each holds its bins) -> MORPH (live towards
//   the snapshot) -> SMEAR (magnitudes ring on from frame to frame) -> BLUR (magnitudes spread over neighbouring bins) -> WINDOW (a moving window in log
//   frequency isolates and emphasises) -> SHIFT / DRIFT (every partial moves by a number of Hz, opposite in L and R by SPREAD) -> soft limit -> synthesis.
// After the frames: RING (a sine multiplies the wet sound), then MIX and LEVEL. The latency is one frame (kN samples), the same in every setting; the dry
// path is delayed by it, also with the power switch off, so the host's delay compensation always lines up.
// No allocation after prepare(), no locks; the display data is published in atomics.
namespace sf1
{
constexpr int kOrder = 11, kN = 1 << kOrder, kH = kN / 4, kBins = kN / 2 + 1, kRingSize = 2 * kN, kDryRing = 2 * kN, kBands = 4, kDisplayBands = 96;
constexpr float kRef = (float) kN / 4.0f;                 // |X| of a full-scale sine in a Hann frame: the 0 dBFS reference of the magnitudes
constexpr float kBandEdgesHz[kBands - 1] = { 150.0f, 800.0f, 4000.0f };
constexpr float kDisplayLoHz = 30.0f, kDisplayHiHz = 18000.0f;

inline const char* bandName (int b) { static const char* n[kBands] = { "LOW", "LOW MID", "HIGH MID", "HIGH" }; return n[juce::jlimit (0, kBands - 1, b)]; }

struct Params
{
    bool freeze[kBands] { false, false, false, false };    // per band (the global FREEZE sets all four)
    float smear = 0.0f, blur = 0.0f;                        // 0 .. 1
    float shiftHz = 0.0f;                                   // -1000 .. 1000
    float spread = 0.0f;                                    // 0 .. 1: opposite shift / window / scan in L and R
    float drift = 0.0f;                                     // 0 .. 1: slow random detune of every partial
    float ring = 0.0f, ringHz = 200.0f;                     // 0 .. 1, 20 .. 2000 Hz
    float feedback = 0.0f;                                  // 0 .. 1 (0.95 at the top)
    float morph = 0.0f;                                     // 0 .. 1 live -> snapshot
    float window = 0.0f;                                    // 0 .. 1 amount of the moving window
    float windowHz = 1000.0f;                               // centre, 40 .. 12000 Hz
    float windowWidthOct = 1.0f;                            // 0.15 .. 3 octaves (full width at half height)
    float scanHz = 0.0f;                                    // 0 = still, else the window sweeps +-2 octaves around its centre at this rate
    int mode = 0;                                           // 0 FOLD (the fields above), 1 HARMONY (the PT-1 engine with `harm`)
    pt1::Params harm;
};

// frequency (Hz) of the display band edges: 96 log bands 30 Hz .. 18 kHz
inline float displayBandHz (float position01) { return kDisplayLoHz * std::pow (kDisplayHiHz / kDisplayLoHz, position01); }

class Engine
{
public:
    Engine()
    {
        for (int i = 0; i < kN; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) kN);
        for (auto& c : chan) c.reset();
        reset();
    }

    int latency() const noexcept { return kN; }

    void prepare (float sampleRate)
    {
        fs = sampleRate;
        binHz = fs / (float) kN;
        for (int k = 0; k < kBins; ++k) { omega[(size_t) k] = juce::MathConstants<float>::twoPi * (float) k / (float) kN; log2f[(size_t) k] = std::log2 (std::max (0.5f, (float) k) * binHz); }
        for (int k = 0; k < kBins; ++k)
        {
            const float hz = (float) k * binHz;
            int b = 0; while (b < kBands - 1 && hz >= kBandEdgesHz[b]) ++b;
            bandOf[(size_t) k] = b;
        }
        for (int d = 0; d < kDisplayBands; ++d)                                    // which bins feed each display band
        {
            const float lo = displayBandHz ((float) d / kDisplayBands), hi = displayBandHz ((float) (d + 1) / kDisplayBands);
            dispLo[(size_t) d] = juce::jlimit (1, kBins - 1, (int) std::floor (lo / binHz));
            dispHi[(size_t) d] = juce::jlimit (dispLo[(size_t) d], kBins - 1, std::max (dispLo[(size_t) d], (int) std::floor (hi / binHz)));
        }
        {   // measure the scale of the complex FFT pair once (forward is unscaled, the inverse may or may not divide by N)
            std::array<juce::dsp::Complex<float>, kN> a {}, b {};
            a[3] = { 1.0f, 0.0f };
            fft.perform (a.data(), b.data(), false);
            fft.perform (b.data(), a.data(), true);
            ifftGain = std::abs (a[3].real()) > 1.0e-9f ? 1.0f / a[3].real() : 1.0f;
        }
        harmonizer.prepare (fs);
        mixSmooth.reset ((double) fs, 0.02);  levelSmooth.reset ((double) fs, 0.02);  powerSmooth.reset ((double) fs, 0.02);
        reset();
    }

    void reset()
    {
        for (auto& c : chan) c.reset();
        std::fill (dryRing.begin(), dryRing.end(), std::array<float, 2> { 0.0f, 0.0f });
        std::fill (harmRing.begin(), harmRing.end(), std::array<float, 2> { 0.0f, 0.0f });
        writeCount = 0; frameCount = 0; scanPhase = 0.0; ringPhase = 0.0;
        captureRequested.store (false);
        memoryValid.store (false);
        smoothersFresh = true;
    }

    // the snapshot of the next frame becomes the MORPH target (the editor's CAPTURE button; safe from any thread)
    void requestCapture() noexcept { captureRequested.store (true); }
    bool hasMemory() const noexcept { return memoryValid.load(); }

    // in place on l / r. mix 0 .. 1, levelGain linear, power: true = effect on (the dry sound is delayed by the latency in every case)
    // Mode FOLD is the phase vocoder below. Mode HARMONY runs the PT-1 harmonizer unchanged (its own MIX law and LEVEL) and delays its output by the same kN samples, so the
    // latency is the same in both modes and the dry sound stays lined up.
    void process (float* l, float* r, int n, const Params& p, float mix, float levelGain, bool power)
    {
        if (p.mode != lastMode) { reset(); harmonizer.reset(); lastMode = p.mode; }                       // a mode change starts from silence
        if (p.mode == 0) processFold (l, r, n, p, mix, levelGain, power);
        else processHarmony (l, r, n, p, mix, levelGain, power);
    }

    const pt1::Engine& harmony() const noexcept { return harmonizer; }                                // the editor's played and heard notes

private:
    void processHarmony (float* l, float* r, int n, const Params& p, float mix, float levelGain, bool power)
    {
        if (smoothersFresh) { powerSmooth.setCurrentAndTargetValue (power ? 1.0f : 0.0f); smoothersFresh = false; }
        powerSmooth.setTargetValue (power ? 1.0f : 0.0f);
        for (int pos = 0; pos < n; pos += 512)
        {
            const int m = std::min (512, n - pos);
            float* io[2] = { l + pos, r + pos };
            for (int i = 0; i < m; ++i) dryRing[(size_t) ((writeCount + i) & (kDryRing - 1))] = { io[0][i], io[1][i] };
            harmonizer.process (io[0], io[1], m, p.harm, mix, levelGain);
            for (int i = 0; i < m; ++i)
            {
                harmRing[(size_t) ((writeCount + i) & (kDryRing - 1))] = { io[0][i], io[1][i] };
                const size_t at = (size_t) ((writeCount + i - kN) & (kDryRing - 1));                          // both rings read kN samples back
                const float pw = powerSmooth.getNextValue();                                                // the power switch crossfades to the delayed dry sound
                for (int c = 0; c < 2; ++c) io[c][i] = dryRing[at][(size_t) c] + (harmRing[at][(size_t) c] - dryRing[at][(size_t) c]) * pw;
            }
            writeCount += m;
        }
    }

    void processFold (float* l, float* r, int n, const Params& p, float mix, float levelGain, bool power)
    {
        if (smoothersFresh) { mixSmooth.setCurrentAndTargetValue (mix); levelSmooth.setCurrentAndTargetValue (levelGain); powerSmooth.setCurrentAndTargetValue (power ? 1.0f : 0.0f); smoothersFresh = false; }   // no fade in at the start
        mixSmooth.setTargetValue (mix);  levelSmooth.setTargetValue (levelGain);  powerSmooth.setTargetValue (power ? 1.0f : 0.0f);
        const double ringStep = juce::MathConstants<double>::twoPi * (double) juce::jlimit (20.0f, 2000.0f, p.ringHz) / (double) fs;
        for (int i = 0; i < n; ++i)
        {
            const float inL = l[i], inR = r[i];
            for (int c = 0; c < 2; ++c) chan[(size_t) c].in[(size_t) (writeCount & (kN - 1))] = c == 0 ? inL : inR;
            dryRing[(size_t) (writeCount & (kDryRing - 1))] = { inL, inR };
            ++writeCount;
            if ((writeCount % kH) == 0) processFrames (p);

            const std::array<float, 2> dry = dryRing[(size_t) ((writeCount - 1 - kN) & (kDryRing - 1))];   // the input of kN samples ago (zeros while the ring fills)
            float wet[2];
            for (int c = 0; c < 2; ++c)
            {
                auto& ch = chan[(size_t) c];
                const size_t idx = (size_t) ((writeCount - 1 - kN) & (kRingSize - 1));                          // the sample that is complete now
                wet[c] = ch.acc[idx];
                ch.acc[idx] = 0.0f;
            }
            if (p.ring > 0.0f)                                                                               // RING: the wet sound times a sine
            {
                const float s = (float) std::sin (ringPhase);
                for (int c = 0; c < 2; ++c) wet[c] = wet[c] * (1.0f - p.ring) + wet[c] * s * p.ring * 1.4f;
                ringPhase += ringStep; if (ringPhase > juce::MathConstants<double>::twoPi) ringPhase -= juce::MathConstants<double>::twoPi;
            }
            const float m = mixSmooth.getNextValue() * powerSmooth.getNextValue(), lv = levelSmooth.getNextValue(), pw = powerSmooth.getCurrentValue();
            const float g = 1.0f + (lv - 1.0f) * pw;                                                         // LEVEL only counts while the effect is on
            for (int c = 0; c < 2; ++c) { const float w = softClip (wet[c]); (c == 0 ? l : r)[i] = (dry[(size_t) c] * (1.0f - m) + w * m) * g; }
        }
    }

public:
    // display data (message thread): input / output spectrum in dB re full scale per display band, the frozen weight per band, the window centre, memory
    float displayIn (int d) const noexcept { return specIn[(size_t) d].load (std::memory_order_relaxed); }
    float displayOut (int d) const noexcept { return specOut[(size_t) d].load (std::memory_order_relaxed); }
    float displayMemory (int d) const noexcept { return specMem[(size_t) d].load (std::memory_order_relaxed); }
    float bandFrozen (int b) const noexcept { return bandWeightPub[(size_t) b].load (std::memory_order_relaxed); }
    float windowCentreNow (int ch) const noexcept { return winCentrePub[(size_t) ch].load (std::memory_order_relaxed); }

private:
    struct Chan
    {
        std::array<float, kN> in {};                                    // the last kN input samples (ring)
        std::array<float, kRingSize> acc {};                            // the overlap-add output (ring)
        std::array<float, kBins> prevPhase {}, synPhase {};             // analysis and synthesis phases (rad)
        std::array<float, kBins> prevOutMag {}, prevOutFreq {};         // the previous frame's output (feedback)
        std::array<float, kBins> holdMag {}, holdFreq {};               // frozen bins
        std::array<float, kBins> memMag {}, memFreq {};                 // the MORPH snapshot
        std::array<float, kBins> smMag {}, smFreq {};                   // SMEAR state
        std::array<float, kBins> detune {};                             // DRIFT state, -1 .. 1
        std::array<float, kBands> bandWeight {};                        // 0 live .. 1 frozen, ramped
        std::array<bool, kBands> wasFrozen {};
        bool started = false;
        juce::Random rng { 12345 };
        void reset()
        {
            in.fill (0.0f); acc.fill (0.0f); prevPhase.fill (0.0f); synPhase.fill (0.0f); prevOutMag.fill (0.0f); prevOutFreq.fill (0.0f);
            holdMag.fill (0.0f); holdFreq.fill (0.0f); memMag.fill (0.0f); memFreq.fill (0.0f); smMag.fill (0.0f); smFreq.fill (0.0f); detune.fill (0.0f);
            bandWeight.fill (0.0f); wasFrozen.fill (false); started = false;
        }
    };

    static float softClip (float x) noexcept { return vibr::dsp::softClip (x); }
    static float wrapPi (float x) noexcept
    {
        constexpr float twoPi = juce::MathConstants<float>::twoPi;
        x -= twoPi * std::floor (x / twoPi + 0.5f);
        return x;
    }

    void processFrames (const Params& p)
    {
        const bool doCapture = captureRequested.exchange (false);
        ++frameCount;
        float inAcc[kDisplayBands] {}, outAcc[kDisplayBands] {}, memAcc[kDisplayBands] {};
        scanPhase += juce::MathConstants<double>::twoPi * (double) p.scanHz * (double) kH / (double) fs;
        if (scanPhase > 1.0e4) scanPhase = std::fmod (scanPhase, juce::MathConstants<double>::twoPi);

        for (int c = 0; c < 2; ++c)
        {
            auto& ch = chan[(size_t) c];
            const float side = c == 0 ? 1.0f : -1.0f;

            // ---- analysis
            for (int i = 0; i < kN; ++i) fftIn[(size_t) i] = { ch.in[(size_t) ((writeCount - kN + i) & (kN - 1))] * window[(size_t) i], 0.0f };
            fft.perform (fftIn.data(), fftOut.data(), false);
            for (int k = 0; k < kBins; ++k)
            {
                const float re = fftOut[(size_t) k].real(), im = fftOut[(size_t) k].imag();
                const float amp = std::sqrt (re * re + im * im), ph = std::atan2 (im, re);
                const float dev = wrapPi (ph - ch.prevPhase[(size_t) k] - omega[(size_t) k] * (float) kH);
                ch.prevPhase[(size_t) k] = ph;
                aMag[(size_t) k] = amp;
                aFreq[(size_t) k] = ch.started ? omega[(size_t) k] + dev / (float) kH : omega[(size_t) k];
                aPhase[(size_t) k] = ph;
            }
            const bool firstFrame = ! ch.started;
            if (firstFrame) { ch.synPhase = aPhase; ch.started = true; }       // the synthesis phase starts on the analysis phase (and is not advanced in this frame)
            for (int k = 0; k < kBins; ++k) { mag[(size_t) k] = aMag[(size_t) k]; freq[(size_t) k] = aFreq[(size_t) k]; }
            publish (inAcc, mag, c);

            // ---- FEEDBACK: the previous output comes back in (magnitude added, frequency weighted by the magnitudes)
            if (p.feedback > 0.0f)
            {
                const float fb = std::min (0.95f, p.feedback * 0.95f);
                for (int k = 0; k < kBins; ++k)
                {
                    const float add = fb * ch.prevOutMag[(size_t) k], sum = mag[(size_t) k] + add;
                    if (sum > 1.0e-9f) freq[(size_t) k] = (mag[(size_t) k] * freq[(size_t) k] + add * ch.prevOutFreq[(size_t) k]) / sum;
                    mag[(size_t) k] = sum;
                }
            }

            // ---- CAPTURE (the snapshot for MORPH)
            if (doCapture) { ch.memMag = mag; ch.memFreq = freq; }

            // ---- FREEZE: each band holds its bins; the weight ramps over a few frames so a band does not jump
            for (int b = 0; b < kBands; ++b)
            {
                if (p.freeze[b] && ! ch.wasFrozen[(size_t) b])
                    for (int k = 0; k < kBins; ++k) if (bandOf[(size_t) k] == b) { ch.holdMag[(size_t) k] = mag[(size_t) k]; ch.holdFreq[(size_t) k] = freq[(size_t) k]; }
                ch.wasFrozen[(size_t) b] = p.freeze[b];
                float& w = ch.bandWeight[(size_t) b];
                w = p.freeze[b] ? std::min (1.0f, w + 0.25f) : std::max (0.0f, w - 0.25f);
            }
            for (int k = 0; k < kBins; ++k)
            {
                const float w = ch.bandWeight[(size_t) bandOf[(size_t) k]];
                if (w <= 0.0f) continue;
                mag[(size_t) k] += w * (ch.holdMag[(size_t) k] - mag[(size_t) k]);
                freq[(size_t) k] += w * (ch.holdFreq[(size_t) k] - freq[(size_t) k]);
            }

            // ---- MORPH: live towards the snapshot
            if (p.morph > 0.0f && memoryValid.load (std::memory_order_relaxed))
            {
                const float m = p.morph;
                for (int k = 0; k < kBins; ++k)
                {
                    const float wl = mag[(size_t) k] * (1.0f - m), wm = ch.memMag[(size_t) k] * m, sum = wl + wm;
                    if (sum > 1.0e-9f) freq[(size_t) k] = (wl * freq[(size_t) k] + wm * ch.memFreq[(size_t) k]) / sum;
                    mag[(size_t) k] = sum;
                }
            }

            // ---- SMEAR: the magnitudes ring on from frame to frame (a quick rise, a slow fall)
            if (p.smear > 0.0f)
            {
                const float rel = 0.55f + 0.43f * p.smear, atk = 0.5f * p.smear * p.smear;
                for (int k = 0; k < kBins; ++k)
                {
                    float& s = ch.smMag[(size_t) k];
                    if (mag[(size_t) k] >= s) { s = s * atk + mag[(size_t) k] * (1.0f - atk); ch.smFreq[(size_t) k] = freq[(size_t) k]; }
                    else s = s * rel + mag[(size_t) k] * (1.0f - rel);
                    mag[(size_t) k] = s; freq[(size_t) k] = ch.smFreq[(size_t) k];
                }
            }
            else { ch.smMag = mag; ch.smFreq = freq; }

            // ---- BLUR: the magnitudes spread over neighbouring bins
            if (p.blur > 0.0f)
            {
                const int bw = (int) std::lround (p.blur * 20.0f);
                if (bw > 0)
                {
                    prefix[0] = 0.0f;
                    for (int k = 0; k < kBins; ++k) prefix[(size_t) k + 1] = prefix[(size_t) k] + mag[(size_t) k];
                    const float comp = std::sqrt (std::max (1.0f, (float) (2 * bw + 1) * 0.5f));
                    for (int k = 0; k < kBins; ++k)
                    {
                        const int lo = std::max (0, k - bw), hi = std::min (kBins - 1, k + bw);
                        tmp[(size_t) k] = (prefix[(size_t) hi + 1] - prefix[(size_t) lo]) / (float) (hi - lo + 1) * comp;
                    }
                    for (int k = 0; k < kBins; ++k) mag[(size_t) k] = tmp[(size_t) k];
                }
            }

            // ---- WINDOW: a Gaussian window in log frequency isolates (the amount at the top: nothing outside) and emphasises (+6 dB inside)
            float centreNow = p.windowHz;
            if (p.window > 0.0f)
            {
                const float scanOct = p.scanHz > 0.0f ? 2.0f * (float) std::sin (scanPhase + (double) (c == 0 ? 0.0f : p.spread * juce::MathConstants<float>::pi)) : 0.0f;
                const float oct = scanOct + side * p.spread * 0.5f;
                centreNow = juce::jlimit (30.0f, 20000.0f, p.windowHz * std::exp2 (oct));
                const float c2 = std::log2 (centreNow), sigma = std::max (0.05f, p.windowWidthOct) / 2.355f, a = p.window;
                for (int k = 1; k < kBins; ++k)
                {
                    const float d = (log2f[(size_t) k] - c2) / sigma, wv = std::exp (-0.5f * d * d);
                    mag[(size_t) k] *= (1.0f - a * (1.0f - wv)) * (1.0f + a * wv);
                }
            }
            winCentrePub[(size_t) c].store (centreNow, std::memory_order_relaxed);

            // ---- SHIFT and DRIFT: every partial moves; the bin it is stored in follows by whole bins, the frequency by the exact amount
            const float shiftHz = p.shiftHz + side * p.spread * 40.0f, shiftRad = juce::MathConstants<float>::twoPi * shiftHz / fs;
            const int dj = (int) std::lround (shiftHz / binHz);
            const float driftCents = p.drift * 40.0f;
            if (p.drift > 0.0f)
                for (int k = 0; k < kBins; ++k) ch.detune[(size_t) k] = juce::jlimit (-1.0f, 1.0f, ch.detune[(size_t) k] * 0.97f + 0.12f * (ch.rng.nextFloat() * 2.0f - 1.0f));
            for (int k = 0; k < kBins; ++k) { outMag[(size_t) k] = 0.0f; outFreq[(size_t) k] = omega[(size_t) k]; }
            for (int k = 0; k < kBins; ++k)
            {
                const int j = k + dj;
                if (j < 1 || j >= kBins - 1) continue;
                float f = freq[(size_t) k];
                if (p.drift > 0.0f) f *= std::exp2 (driftCents * ch.detune[(size_t) k] / 1200.0f);
                f += shiftRad;
                if (f < 0.0f) continue;
                if (mag[(size_t) k] > outMag[(size_t) j]) { outMag[(size_t) j] = mag[(size_t) k]; outFreq[(size_t) j] = f; }
            }
            if (dj == 0 && p.drift <= 0.0f && shiftRad == 0.0f)                                                // nothing moves: every bin keeps its own frequency
                for (int k = 0; k < kBins; ++k) { outMag[(size_t) k] = mag[(size_t) k]; outFreq[(size_t) k] = freq[(size_t) k]; }

            // ---- soft limit (about 1.6 times full scale per bin) and the memory for FEEDBACK
            constexpr float T = kRef, K = 1.6f * kRef;
            for (int k = 0; k < kBins; ++k)
            {
                float m = outMag[(size_t) k];
                if (m > T) m = T + (K - T) * std::tanh ((m - T) / (K - T));
                outMag[(size_t) k] = m;
                ch.prevOutMag[(size_t) k] = m; ch.prevOutFreq[(size_t) k] = outFreq[(size_t) k];
            }
            publish (outAcc, outMag, c);
            if (c == 0) publish (memAcc, ch.memMag, c);

            // ---- synthesis: accumulate the phase, build the symmetric spectrum, inverse FFT, window, overlap-add
            for (int k = 0; k < kBins; ++k)
            {
                if (! firstFrame) ch.synPhase[(size_t) k] = wrapPi (ch.synPhase[(size_t) k] + outFreq[(size_t) k] * (float) kH);
                const float m = outMag[(size_t) k], ph = ch.synPhase[(size_t) k];
                float re = m * std::cos (ph), im = m * std::sin (ph);
                if (k == 0 || k == kBins - 1) im = 0.0f;
                fftIn[(size_t) k] = { re, im };
                if (k > 0 && k < kBins - 1) fftIn[(size_t) (kN - k)] = { re, -im };
            }
            fft.perform (fftIn.data(), fftOut.data(), true);
            const float norm = ifftGain / 1.5f;                                                                // Hann squared at 75 % overlap adds up to 1.5
            for (int i = 0; i < kN; ++i)
                ch.acc[(size_t) ((writeCount - kN + i) & (kRingSize - 1))] += fftOut[(size_t) i].real() * window[(size_t) i] * norm;
        }

        if (doCapture) memoryValid.store (true, std::memory_order_relaxed);
        for (int d = 0; d < kDisplayBands; ++d)
        {
            const auto toDb = [] (float m) { return juce::Decibels::gainToDecibels (m / kRef, -100.0f); };
            const auto smooth = [] (std::atomic<float>& a, float v) { const float o = a.load (std::memory_order_relaxed); a.store (v > o ? v : o + (v - o) * 0.35f, std::memory_order_relaxed); };
            smooth (specIn[(size_t) d], toDb (inAcc[d])); smooth (specOut[(size_t) d], toDb (outAcc[d])); specMem[(size_t) d].store (toDb (memAcc[d]), std::memory_order_relaxed);
        }
        for (int b = 0; b < kBands; ++b) bandWeightPub[(size_t) b].store (chan[0].bandWeight[(size_t) b], std::memory_order_relaxed);
    }

    // max of the magnitudes per display band (the loudest channel wins)
    void publish (float* acc, const std::array<float, kBins>& m, int) const
    {
        for (int d = 0; d < kDisplayBands; ++d)
            for (int k = dispLo[(size_t) d]; k <= dispHi[(size_t) d]; ++k) acc[d] = std::max (acc[d], m[(size_t) k]);
    }

    float fs = 48000.0f, binHz = 48000.0f / (float) kN, ifftGain = 1.0f;
    juce::dsp::FFT fft { kOrder };
    std::array<float, kN> window {};
    std::array<float, kBins> omega {}, log2f {};
    std::array<int, kBins> bandOf {};
    std::array<int, kDisplayBands> dispLo {}, dispHi {};
    std::array<Chan, 2> chan;
    std::array<std::array<float, 2>, kDryRing> dryRing {}, harmRing {};
    pt1::Engine harmonizer;
    int lastMode = -1;
    long long writeCount = 0, frameCount = 0;
    double scanPhase = 0.0, ringPhase = 0.0;

    // scratch (no allocation in process)
    std::array<juce::dsp::Complex<float>, kN> fftIn {}, fftOut {};
    std::array<float, kBins> aMag {}, aFreq {}, aPhase {}, mag {}, freq {}, outMag {}, outFreq {}, tmp {};
    std::array<float, kBins + 1> prefix {};

    juce::SmoothedValue<float> mixSmooth, levelSmooth, powerSmooth;
    bool smoothersFresh = true;
    std::atomic<bool> captureRequested { false }, memoryValid { false };
    std::array<std::atomic<float>, kDisplayBands> specIn {}, specOut {}, specMem {};
    std::array<std::atomic<float>, kBands> bandWeightPub {};
    std::array<std::atomic<float>, 2> winCentrePub {};
};
} // namespace sf1
