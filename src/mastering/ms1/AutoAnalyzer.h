// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

// MS-1 AUTO, stages 1 to 3: the measurements on the chain INPUT (before INPUT TRIM), made on the audio thread (see AutoMaster.h for the rules and the spec).
// Header-only, no JUCE. Only reads the audio, so the output of the plugin is bit-exact with and without it. Nothing is allocated after prepare();
// idle (not learning) costs two atomic loads per block.
//
// What is counted: a block of the host's audio counts as programme when the momentary loudness of the input is above -60 LUFS and the plugin is on.
// Learning stops by itself after 120 s of programme. The measurements are plain members written by the audio thread; every ~50 ms and on a clear
// they are published into an array of atomic words under a sequence counter, which any thread reads with read() (a copy that is retried when the
// audio thread was writing).
#include <cstring>
#include <type_traits>
#include "AutoMaster.h"
#include "MasteringStages.h"

namespace ms1
{
static_assert (autom::kSpecBands == kBands, "the AUTO spectrum uses the display bands");
static_assert (std::is_trivially_copyable_v<autom::AnalysisSnapshot> && sizeof (autom::AnalysisSnapshot) % 4 == 0, "the snapshot is published word by word");

class AutoAnalyzer
{
public:
    std::atomic<bool> learning { false };               // ANALYZE / STOP (the audio thread also clears it when 120 s are full)
    std::atomic<bool> clearRequest { false };           // CLEAR (executed by the audio thread at the next block)

    // a new sample rate starts again; the same sample rate keeps everything (a host calling prepareToPlay on every start must not wipe the learning)
    void prepare (float sampleRate)
    {
        if (sampleRate == fs) return;
        fs = sampleRate;
        loud.prepare (fs); levels.prepare (fs); stereo.prepare (fs); spec.prepare (fs);
        for (auto& t : tp) t.reset();
        lowLp.set (25.0f, fs, false);                                         // below 25 Hz: two 2nd-order low-passes in series (4th order keeps the 40 .. 60 Hz bass out)
        bandHp.set (40.0f, fs, true); bandLp.set (60.0f, fs, false);          // 40 .. 60 Hz: a 2nd-order high-pass and low-pass
        lastX[0] = lastX[1] = lastX[2] = -1.0f; lastIx[0] = lastIx[1] = -1.0f;
        maxCounted = (long) std::lround ((double) fs * (double) kMaxSeconds);
        cadenceLen = std::max (1, (int) std::lround (0.1f * fs)); bandWinLen = std::max (1, (int) std::lround (0.4f * fs)); publishLen = std::max (1, (int) std::lround (0.05f * fs));
        clear();
    }

    // the audio thread: `l`, `r` are only read. `active` = the plugin is on (not bypassed). xover: the multiband crossovers (for the band level histograms),
    // imgXover: the two imager crossovers (for the per band correlation and side / mid level)
    void process (const float* l, const float* r, int n, bool active, const float* xover, const float* imgXover)
    {
        if (clearRequest.load (std::memory_order_relaxed) && clearRequest.exchange (false)) clear();
        if (! learning.load (std::memory_order_relaxed) || ! active || n <= 0 || fs <= 0.0f) return;
        if (counted >= maxCounted) { learning.store (false); publish(); return; }

        if (xover[0] != lastX[0] || xover[1] != lastX[1] || xover[2] != lastX[2])
        {
            for (int k = 0; k < 3; ++k) lastX[k] = xover[k];
            xo.set (lastX, fs);
        }
        if (imgXover[0] != lastIx[0] || imgXover[1] != lastIx[1])
        {
            lastIx[0] = imgXover[0]; lastIx[1] = imgXover[1];
            xi.set (lastIx, fs);
        }
        loud.process (l, r, n);                                                  // the gate: a second set of the loudness meter's objects, fed from the input
        const float mom = loud.momentary.load (std::memory_order_relaxed);
        const bool programme = mom > autom::kProgrammeGateLufs;
        float blockTp = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float a = l[i], b = r[i];
            const float tpl = tp[0].push (a), tpr = tp[1].push (b), tpMax = std::max (tpl, tpr);
            blockTp = std::max (blockTp, tpMax);
            const float mid = 0.5f * (a + b);
            spec.push (mid);
            const float low = lowLp.tick (mid), bass = bandLp.tick (bandHp.tick (mid));
            float bl[4], br[4];
            xo.split (0, a, bl); xo.split (1, b, br);
            float il[3], ir[3];
            xi.split (0, a, il); xi.split (1, b, ir);
            if (! programme) continue;
            const float pk = std::max (std::abs (a), std::abs (b));
            samplePeak = std::max (samplePeak, pk);
            truePeak = std::max (truePeak, tpMax);
            if (std::abs (a) >= 0.999f) ++clipped;
            if (std::abs (b) >= 0.999f) ++clipped;
            sumSq += (double) a * a + (double) b * b;
            sumL += a; sumR += b;
            cLL += (double) a * a; cRR += (double) b * b; cLR += (double) a * b;
            lowSq += (double) low * low; bassSq += (double) bass * bass;
            for (int k = 0; k < 4; ++k) bandSq[k] += (double) bl[k] * bl[k] + (double) br[k] * br[k];
            for (int k = 0; k < 3; ++k)                                          // the imager bands: L / R products, and the mid and side energy
            {
                const double x = il[k], y = ir[k], m = 0.5 * (x + y), sd = 0.5 * (x - y);
                iLL[k] += x * x; iRR[k] += y * y; iLR[k] += x * y; iMM[k] += m * m; iSS[k] += sd * sd;
            }
        }
        levels.process (l, r, n, blockTp);
        stereo.process (l, r, n);
        momentaryNow = mom;
        if (programme)
        {
            counted += n; bandWin += n; cadence += n;
            momentaryMax = std::max (momentaryMax, mom);
            while (bandWin >= bandWinLen)                                        // a 400 ms window of programme per band
            {
                for (int k = 0; k < 4; ++k)
                {
                    const float db = bandSq[k] > 0.0 ? (float) (10.0 * std::log10 (bandSq[k] / (2.0 * (double) bandWin))) : -120.0f;
                    ++bandHist[k][autom::histBin (db)];
                    bandSq[k] = 0.0;
                }
                bandWin = 0;
            }
            while (cadence >= cadenceLen)                                        // every 100 ms of programme
            {
                cadence -= cadenceLen;
                const float st = loud.shortTerm.load (std::memory_order_relaxed);
                if (st > -70.0f)
                {
                    ++stHist[autom::histBin (st)];
                    shortTermMax = std::max (shortTermMax, st);
                    const float wp = levels.windowPeakDb.load (std::memory_order_relaxed);
                    if (wp > -119.0f) { psrSum += (double) (wp - st); ++psrCount; }
                }
                bool warm = false;
                for (int b = 0; b < kBands; ++b) if (spec.bands[b].load (std::memory_order_relaxed) > -119.0f) { warm = true; break; }
                if (warm)
                {
                    for (int b = 0; b < kBands; ++b) specAcc[(size_t) b] += std::pow (10.0, (double) spec.bands[b].load (std::memory_order_relaxed) / 10.0);
                    ++specFrames;
                    for (int b = 0; b < kBands; ++b) blkAcc[(size_t) b] += std::pow (10.0, (double) spec.bands[b].load (std::memory_order_relaxed) / 10.0);
                    if (++blkFrames >= kNarrowBlockFrames) narrowBlock();
                }
            }
        }
        sincePublish += n;
        if (sincePublish >= publishLen || counted >= maxCounted) publish();
    }

    // any thread: the last published snapshot; false when the audio thread kept writing (the caller keeps what it had)
    bool read (autom::AnalysisSnapshot& out) const
    {
        constexpr size_t nWords = sizeof (autom::AnalysisSnapshot) / 4;
        uint32_t tmp[nWords];
        for (int attempt = 0; attempt < 16; ++attempt)
        {
            const uint32_t s1 = seq.load (std::memory_order_acquire);
            if (s1 & 1u) continue;
            for (size_t i = 0; i < nWords; ++i) tmp[i] = words[i].load (std::memory_order_relaxed);
            std::atomic_thread_fence (std::memory_order_acquire);
            if (seq.load (std::memory_order_relaxed) != s1) continue;
            std::memcpy (&out, tmp, sizeof (out));
            return true;
        }
        return false;
    }

    // the audio thread (or any thread while no audio runs): forget everything that was learned
    void clear()
    {
        counted = 0; bandWin = 0; cadence = 0; sincePublish = 0;
        samplePeak = truePeak = 0.0f; clipped = 0;
        sumSq = 0.0; sumL = sumR = 0.0; cLL = cRR = cLR = 0.0; lowSq = bassSq = 0.0;
        for (auto& b : bandSq) b = 0.0;
        for (auto& h : stHist) h = 0;
        for (auto& h : bandHist) for (auto& v : h) v = 0;
        specAcc.fill (0.0); specFrames = 0;
        blkAcc.fill (0.0); blkFrames = 0; narrowBlocks = 0; for (auto& h : narrowHits) h = 0;
        psrSum = 0.0; psrCount = 0; shortTermMax = momentaryMax = -120.0f; momentaryNow = -120.0f;
        loud.resetAll(); levels.reset(); stereo.reset(); spec.reset();
        for (auto& t : tp) t.reset();
        xo.clear();
        lowLp.clear();
        bandHp.clear(); bandLp.clear();
        xi.clear();
        for (int k = 0; k < 3; ++k) iLL[k] = iRR[k] = iLR[k] = iMM[k] = iSS[k] = 0.0;
        publish();
    }

private:
    static constexpr long kMaxSeconds = 120;
    static constexpr int kNarrowBlockFrames = 30;                                // a narrow-peak block = 30 spectrum frames = 3 s of programme

    // narrow-peak statistics: at the end of each 3 s block, the bands that are more than 6 dB above the mean (dB) of their neighbours (4 .. 8 bands away) count a hit
    void narrowBlock()
    {
        std::array<float, kBands> db {};
        for (int b = 0; b < kBands; ++b) db[(size_t) b] = blkAcc[(size_t) b] > 1.0e-12 ? (float) (10.0 * std::log10 (blkAcc[(size_t) b] / (double) blkFrames)) : -120.0f;
        for (int b = 0; b < kBands; ++b)
        {
            float m = 0.0f;
            if (db[(size_t) b] > -100.0f && autom::neighbourMean (db.data(), b, m) && db[(size_t) b] - m > 6.0f) ++narrowHits[b];
        }
        ++narrowBlocks;
        blkAcc.fill (0.0); blkFrames = 0;
    }
    long maxCounted = 1;                                                         // 120 s in samples (set with the sample rate)

    void publish()
    {
        sincePublish = 0;
        autom::AnalysisSnapshot s;
        const double c = (double) std::max<long> (1, counted);
        s.sampleRate = fs;
        s.programmeSeconds = fs > 0.0f ? (float) ((double) counted / (double) fs) : 0.0f;
        s.learning = learning.load (std::memory_order_relaxed) ? 1u : 0u;
        const auto db = [] (double v) { return v > 1.0e-12 ? (float) (10.0 * std::log10 (v)) : -120.0f; };
        s.samplePeakDb = samplePeak > 0.0f ? gainToDb (samplePeak) : -120.0f;
        s.truePeakDb = truePeak > 0.0f ? gainToDb (truePeak) : -120.0f;
        s.clippedSamples = clipped;
        s.clippedFraction = counted > 0 ? (float) ((double) clipped / (2.0 * c)) : 0.0f;
        s.integratedLufs = loud.integrated.load (std::memory_order_relaxed);
        s.shortTermMaxLufs = shortTermMax; s.momentaryMaxLufs = momentaryMax;
        s.loudnessRange = loud.range.load (std::memory_order_relaxed);
        s.rmsDb = counted > 0 ? db (sumSq / (2.0 * c)) : -120.0f;
        s.crestDb = counted > 0 && s.rmsDb > -119.0f && samplePeak > 0.0f ? s.samplePeakDb - s.rmsDb : 0.0f;
        s.psrDb = psrCount > 0 ? (float) (psrSum / (double) psrCount) : 0.0f;
        s.drDb = levels.dr.load (std::memory_order_relaxed);
        s.correlation = cLL * cRR > 1.0e-12 ? (float) std::clamp (cLR / std::sqrt (cLL * cRR), -1.0, 1.0) : 1.0f;
        s.lowEnergyDb = counted > 0 ? db (lowSq / c) : -120.0f;
        s.midBassDb = counted > 0 ? db (bassSq / c) : -120.0f;
        {
            const double dc = std::max (std::abs (sumL), std::abs (sumR)) / c;                                   // the largest channel mean (an amplitude)
            s.dcDb = counted > 0 && dc > 1.0e-9 ? (float) (20.0 * std::log10 (dc)) : -120.0f;
        }
        s.momentaryNow = momentaryNow;
        s.correlationNow = stereo.corrShort.load (std::memory_order_relaxed);
        for (int k = 0; k < 3; ++k) s.xover[k] = lastX[k] > 0.0f ? lastX[k] : s.xover[k];
        for (int k = 0; k < 2; ++k) s.imgX[k] = lastIx[k] > 0.0f ? lastIx[k] : s.imgX[k];
        for (int k = 0; k < 3; ++k)                                                // 2 = no signal in that band (below -70 dBFS RMS), else the correlation of its L / R
        {
            const double e = (iLL[k] + iRR[k]) / (2.0 * c);
            s.imgCorr[k] = counted > 0 && e > 1.0e-7 && iLL[k] * iRR[k] > 0.0 ? (float) std::clamp (iLR[k] / std::sqrt (iLL[k] * iRR[k]), -1.0, 1.0) : 2.0f;
            s.imgSideMidDb[k] = counted > 0 && e > 1.0e-7 ? std::clamp (db ((iSS[k] + 1.0e-18) / (iMM[k] + 1.0e-18)), -120.0f, 60.0f) : -120.0f;
        }
        for (int b = 0; b < autom::kHistBins; ++b) s.shortTermHist[b] = stHist[b];
        for (int k = 0; k < 4; ++k) for (int b = 0; b < autom::kHistBins; ++b) s.bandHist[k][b] = bandHist[k][b];
        for (int b = 0; b < kBands; ++b) s.spectrumDb[b] = specFrames > 0 ? db (specAcc[(size_t) b] / (double) specFrames) : -120.0f;
        s.spectrumFrames = specFrames;
        s.narrowBlocks = narrowBlocks;
        for (int b = 0; b < kBands; ++b) s.narrowHits[b] = narrowHits[b];
        s.check = autom::snapshotCheck (s);

        constexpr size_t nWords = sizeof (autom::AnalysisSnapshot) / 4;
        uint32_t tmp[nWords];
        std::memcpy (tmp, &s, sizeof (s));
        const uint32_t q = seq.load (std::memory_order_relaxed);
        seq.store (q + 1, std::memory_order_relaxed);
        std::atomic_thread_fence (std::memory_order_release);
        for (size_t i = 0; i < nWords; ++i) words[i].store (tmp[i], std::memory_order_relaxed);
        seq.store (q + 2, std::memory_order_release);
    }

    float fs = 0.0f;
    Loudness loud;                                                               // the second instance of the meter objects, fed from the input
    LevelMeter levels;
    StereoMeter stereo;
    Spectrum spec;                                                               // the mid of the input, the long-term spectrum accumulates from it
    TruePeak tp[2];
    Lr4 lowLp;                                                                   // the 4th-order low-pass at 25 Hz (two 2nd-order sections)
    Butter2 bandHp, bandLp;
    Crossover<3> xo;
    float lastX[3] { -1.0f, -1.0f, -1.0f };
    Crossover<2> xi;                                                             // the imager's crossovers
    float lastIx[2] { -1.0f, -1.0f };
    double iLL[3] {}, iRR[3] {}, iLR[3] {}, iMM[3] {}, iSS[3] {};
    int cadenceLen = 4800, bandWinLen = 19200, publishLen = 2400;

    long counted = 0;                                                            // counted samples
    int bandWin = 0, cadence = 0, sincePublish = 0;
    float samplePeak = 0.0f, truePeak = 0.0f, shortTermMax = -120.0f, momentaryMax = -120.0f, momentaryNow = -120.0f;
    uint32_t clipped = 0;
    double sumSq = 0.0, sumL = 0.0, sumR = 0.0, cLL = 0.0, cRR = 0.0, cLR = 0.0, lowSq = 0.0, bassSq = 0.0, bandSq[4] {};
    uint32_t stHist[autom::kHistBins] {}, bandHist[4][autom::kHistBins] {};
    std::array<double, kBands> specAcc {};
    uint32_t specFrames = 0;
    std::array<double, kBands> blkAcc {};                                        // the power sum of the current 3 s block
    int blkFrames = 0;
    uint32_t narrowBlocks = 0, narrowHits[kBands] {};
    double psrSum = 0.0; long psrCount = 0;

    std::atomic<uint32_t> seq { 0 };
    std::atomic<uint32_t> words[sizeof (autom::AnalysisSnapshot) / 4] {};
};
} // namespace ms1
