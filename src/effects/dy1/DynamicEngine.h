// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

// DY-1 Dynamic Shaper: a noise gate, a downward expander and a transient shaper in one chain (gate -> expander -> transient shaper), each
// with its own on / off switch. Every stage listens to the loudest of the two channels, so the stereo image does not move.
//   GATE       THRESH (-80 .. -10 dB) opens and closes the sound (a little hysteresis so it does not chatter), RELEASE (10 .. 800 ms) is how
//              long the close takes.
//   EXPANDER   a gentler gate: below THRESH every dB is turned down by (RATIO - 1) more dB, over a 6 dB soft knee; RELEASE as above.
//   TRANSIENT  compares a fast and a slow level follower: where the fast one is higher (the start of a note) ATTACK raises or lowers the level,
//              where it is lower (the tail) SUSTAIN does. It works on the shape of the note, so it does not depend on how loud you play.
// MIX blends with the dry sound, LEVEL is the output gain. Header-only, no JUCE dependency, no allocation after prepare().
#include <algorithm>
#include <atomic>
#include <cmath>

namespace dy1
{
constexpr int kHistory = 256;                       // the display history: one entry per 20 ms (about 5 seconds)

inline float dbToGain (float db) { return std::exp (db * 0.11512925f); }
inline float gainToDb (float g) { return 8.68588964f * std::log (std::max (g, 1.0e-9f)); }
inline float timeCoef (float seconds, float fs) { return std::exp (-1.0f / std::max (1.0f, seconds * fs)); }

struct Params
{
    bool gateOn = true, expOn = false, transOn = true;
    float gateThresh = -50.0f, gateRelease = 120.0f;                    // dB, ms
    float expThresh = -45.0f, expRatio = 2.0f, expRelease = 150.0f;     // dB, :1, ms
    float attack = 0.0f, sustain = 0.0f;                                // -100 .. 100 %
    float mix = 1.0f;                                                   // 0 .. 1
};

class Engine
{
public:
    // what the editor shows: the history of the input level and of the gain of the whole chain, one entry per 20 ms
    std::atomic<float> histIn[kHistory], histGain[kHistory];            // dB
    std::atomic<int> histPos { 0 };                                     // the next entry that will be written

    void prepare (float sampleRate)
    {
        fs = sampleRate;
        histWindow = std::max (1, (int) (0.02f * fs));
        reset();
    }

    void reset()
    {
        envG = envE = 0.0f; gG = 1.0f; open = false; grE = 0.0f; fast = slow = 0.0f; gainDb = 0.0f;
        winCount = 0; winMax = 0.0f; winGain = 0.0f;
        for (int i = 0; i < kHistory; ++i) { histIn[i].store (-90.0f); histGain[i].store (0.0f); }
        histPos.store (0);
    }

    void process (float* l, float* r, int n, const Params& p, float levelGain)
    {
        const float thrG = dbToGain (p.gateThresh), closeThr = thrG * 0.7f;
        const float envRelG = timeCoef (0.03f, fs), openG = timeCoef (0.001f, fs), closeG = timeCoef (p.gateRelease * 0.001f, fs);
        const float slope = std::max (1.0f, p.expRatio) - 1.0f, knee = 6.0f;
        const float envRelE = timeCoef (0.04f, fs), openE = timeCoef (0.002f, fs), closeE = timeCoef (p.expRelease * 0.001f, fs);
        const float att = p.attack * 0.01f, sus = p.sustain * 0.01f;
        const float fa = timeCoef (0.0005f, fs), fr = timeCoef (0.012f, fs), sa = timeCoef (0.015f, fs), sr2 = timeCoef (0.14f, fs), gs = timeCoef (0.002f, fs);
        const float lev = levelGain;

        for (int i = 0; i < n; ++i)
        {
            const float dryL = l[i], dryR = r[i];
            const float inLevel = std::max (std::abs (dryL), std::abs (dryR));
            float xl = dryL, xr = dryR, g = 1.0f;

            if (p.gateOn)
            {
                envG = inLevel > envG ? inLevel : inLevel + (envG - inLevel) * envRelG;
                if (! open && envG > thrG) open = true; else if (open && envG < closeThr) open = false;
                const float target = open ? 1.0f : 0.0f;
                gG = target + (gG - target) * (open ? openG : closeG);
            }
            else { gG += (1.0f - gG) * (1.0f - openG); envG = inLevel; }       // off: glide to open so switching it off does not click
            g *= gG; xl *= gG; xr *= gG;

            if (p.expOn)
            {
                const float a = std::max (std::abs (xl), std::abs (xr));
                envE = a > envE ? a : a + (envE - a) * envRelE;
                const float under = p.expThresh - gainToDb (envE);                 // dB below the threshold
                float red = 0.0f;
                if (under > knee * 0.5f) red = slope * under;
                else if (under > -knee * 0.5f) red = slope * (under + knee * 0.5f) * (under + knee * 0.5f) / (2.0f * knee);
                red = std::min (red, 60.0f);
                grE = red + (grE - red) * (red > grE ? closeE : openE);
            }
            else grE *= openE;
            const float gE = dbToGain (-grE);
            xl *= gE; xr *= gE; g *= gE;

            if (p.transOn)
            {
                const float a = std::max (std::abs (xl), std::abs (xr));
                fast = a > fast ? a + (fast - a) * fa : a + (fast - a) * fr;
                slow = a > slow ? a + (slow - a) * sa : a + (slow - a) * sr2;
                const float d = gainToDb (fast + 1.0e-6f) - gainToDb (slow + 1.0e-6f);      // > 0 at an onset, < 0 in the tail
                float gt = d > 0.0f ? att * d : sus * -d;
                gt = std::clamp (gt, -15.0f, 10.0f);
                if (slow < 0.0005f) gt = 0.0f;                                              // leave the noise floor alone
                gainDb = gt + (gainDb - gt) * gs;
            }
            else gainDb *= gs;
            const float gT = dbToGain (gainDb);
            xl *= gT; xr *= gT; g *= gT;

            l[i] = (dryL + (xl - dryL) * p.mix) * lev;
            r[i] = (dryR + (xr - dryR) * p.mix) * lev;

            winMax = std::max (winMax, inLevel);
            winGain = g;
            if (++winCount >= histWindow)
            {
                const int pos = histPos.load (std::memory_order_relaxed);
                histIn[pos].store (gainToDb (winMax), std::memory_order_relaxed);
                histGain[pos].store (gainToDb (winGain), std::memory_order_relaxed);
                histPos.store ((pos + 1) % kHistory, std::memory_order_release);
                winCount = 0; winMax = 0.0f;
            }
        }
    }

private:
    float fs = 48000.0f;
    float envG = 0.0f, gG = 1.0f, envE = 0.0f, grE = 0.0f, fast = 0.0f, slow = 0.0f, gainDb = 0.0f;
    bool open = false;
    int histWindow = 960, winCount = 0;
    float winMax = 0.0f, winGain = 0.0f;
};
} // namespace dy1
