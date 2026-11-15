// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

// SA-1 Lo-Fi Saturator: four colouring stages in one chain (BOOST -> TUBE -> TAPE -> CRUSH), each with its own on / off switch.
//   BOOST  clean gain (0 .. 24 dB) with a TONE tilt (a +-8 dB high shelf at 3 kHz): it pushes the stages after it harder.
//   TUBE   smooth asymmetric saturation (tanh round a bias point): DRIVE sets the gain into the curve, BIAS how lopsided it is. The lopsided
//          curve adds even harmonics (the "glow"); the DC it creates is removed.
//   TAPE   gentler saturation that keeps the level of quiet sounds (tanh divided by the gain): DRIVE, and WARMTH adds a low "head bump" (+4 dB at
//          90 Hz), rolls the highs off (16 kHz down to 5 kHz) and a little wow.
//   CRUSH  fewer bits (1 .. 16) and a lower sample rate (500 Hz .. 48 kHz, no filtering: the aliasing is the effect), blended by its own MIX.
// MIX blends the whole chain with the dry sound, LEVEL is the output gain. The curve functions (`shape`) are also used by the editor to draw
// the transfer curve and the waveform. Header-only, no JUCE dependency, no allocation after prepare().
#include <algorithm>
#include <cmath>
#include <vector>

namespace sa1
{
constexpr float kPi = 3.14159265358979f, kTwoPi = 6.28318530717959f;

inline float dbToGain (float db) { return std::exp (db * 0.11512925f); }
inline float timeCoef (float seconds, float fs) { return std::exp (-1.0f / std::max (1.0f, seconds * fs)); }

struct Params
{
    bool boostOn = false, tubeOn = true, tapeOn = true, crushOn = false;
    float boostGain = 10.0f, boostTone = 0.5f;          // dB (0 .. 24), 0 .. 1
    float tubeDrive = 0.4f, tubeBias = 0.4f;            // 0 .. 1
    float tapeDrive = 0.4f, tapeWarmth = 0.5f;          // 0 .. 1
    float bits = 8.0f, rate = 12000.0f, crushMix = 1.0f;     // bits 1 .. 16, Hz, 0 .. 1
    float mix = 1.0f;                                   // 0 .. 1
};

// the static curve of each stage, shared with the editor drawing (the filters and the sample-rate hold are not part of it)
inline float tubeGain (float drive) { return 1.0f + drive * 7.0f; }
inline float tapeGain (float drive) { return 1.0f + drive * 5.0f; }
inline float tubeCurve (float x, float drive, float bias)
{
    const float g = tubeGain (drive), b = bias * 0.6f;
    return (std::tanh (g * x + b) - std::tanh (b)) / g * (1.0f + 0.1f * (g - 1.0f));
}
inline float tapeCurve (float x, float drive)
{
    const float g = tapeGain (drive);
    return std::tanh (g * x) / g * (1.0f + 0.12f * (g - 1.0f));
}
inline float crushCurve (float x, float bits)
{
    const float levels = std::exp2 (std::round (bits) - 1.0f);
    return std::round (x * levels) / levels;
}
inline float shape (float x, const Params& p)       // the whole chain without filters, wow and sample-rate hold
{
    if (p.boostOn) x *= dbToGain (p.boostGain);
    if (p.tubeOn) x = tubeCurve (x, p.tubeDrive, p.tubeBias);
    if (p.tapeOn) x = tapeCurve (x, p.tapeDrive);
    if (p.crushOn) x += (crushCurve (x, p.bits) - x) * p.crushMix;
    return x;
}

class Engine
{
public:
    void prepare (float sampleRate)
    {
        fs = sampleRate;
        size = 1; while ((float) size < 0.02f * fs + 8.0f) size <<= 1;
        mask = size - 1;
        for (auto& b : line) b.assign ((size_t) size, 0.0f);
        reset();
    }

    void reset()
    {
        for (auto& b : line) std::fill (b.begin(), b.end(), 0.0f);
        for (int c = 0; c < 2; ++c)
        {
            hsX1[c] = hsX2[c] = hsY1[c] = hsY2[c] = 0.0f;
            dcX1[c] = dcY1[c] = 0.0f; lpT[c] = 0.0f;
            bX1[c] = bX2[c] = bY1[c] = bY2[c] = 0.0f; tl1[c] = tl2[c] = 0.0f; hold[c] = 0.0f;
        }
        w = 0; wow = 0.0f; counter = 1.0f;
        gBoost = -1.0f; stage[0] = stage[1] = stage[2] = stage[3] = -1.0f; mixS = levelS = -1.0f;
    }

    void process (float* l, float* r, int n, const Params& p, float levelGain)
    {
        const float sm = 1.0f - std::exp (-1.0f / (0.02f * fs));
        const bool on[4] = { p.boostOn, p.tubeOn, p.tapeOn, p.crushOn };
        if (stage[0] < 0.0f) { for (int s = 0; s < 4; ++s) stage[s] = on[s] ? 1.0f : 0.0f; mixS = p.mix; levelS = levelGain; gBoost = dbToGain (p.boostGain); }

        // coefficients (they only change when the knobs change)
        setHighShelf (3000.0f, (p.boostTone - 0.5f) * 16.0f);
        const float warm = p.tapeWarmth, fcTape = 16000.0f * std::pow (0.3125f, warm);
        setLowShelf (90.0f, 4.0f * warm);
        const float tapeA = std::exp (-kTwoPi * fcTape / fs), dcA = std::exp (-kTwoPi * 20.0f / fs), lpA = std::exp (-kTwoPi * 12000.0f / fs);
        const float inc = std::min (1.0f, p.rate / fs);
        const float levels = std::exp2 (std::round (p.bits) - 1.0f);
        const float tg = tubeGain (p.tubeDrive), tb = p.tubeBias * 0.6f, toff = std::tanh (tb), tubeMake = 1.0f + 0.1f * (tg - 1.0f);
        const float ag = tapeGain (p.tapeDrive), tapeMake = 1.0f + 0.12f * (ag - 1.0f);

        float* io[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
            for (int s = 0; s < 4; ++s) stage[s] += ((on[s] ? 1.0f : 0.0f) - stage[s]) * sm;
            mixS += (p.mix - mixS) * sm; levelS += (levelGain - levelS) * sm;
            gBoost += (dbToGain (p.boostGain) - gBoost) * sm;
            wow += 0.7f / fs; if (wow >= 1.0f) wow -= 1.0f;
            const float flutter = (1.5f + std::sin (kTwoPi * wow) * 0.8f) * 0.0001f * fs * (0.3f + warm);     // a little wow on the tape
            counter += inc;
            const bool newHold = counter >= 1.0f;
            if (newHold) counter -= 1.0f;

            float dry[2], out[2];
            for (int c = 0; c < 2; ++c)
            {
                const float x0 = io[c][i];
                dry[c] = x0;
                float x = x0, y;

                // BOOST: high shelf (tilt) and gain
                y = x * hsB0 + hsX1[c] * hsB1 + hsX2[c] * hsB2 - hsY1[c] * hsA1 - hsY2[c] * hsA2;
                hsX2[c] = hsX1[c]; hsX1[c] = x; hsY2[c] = hsY1[c]; hsY1[c] = y;
                x += (y * gBoost - x) * stage[0];

                // TUBE: asymmetric saturation, DC removed, 12 kHz low pass
                y = (std::tanh (tg * x + tb) - toff) / tg * tubeMake;
                const float yd = y - dcX1[c] + 0.9975f * dcY1[c]; dcX1[c] = y; dcY1[c] = yd;
                lpT[c] = yd * (1.0f - lpA) + lpT[c] * lpA;
                x += (lpT[c] - x) * stage[1];

                // TAPE: wow (a short modulated delay), head bump, saturation, high roll-off
                line[c][(size_t) w] = x;
                const float d = std::clamp (flutter + 2.0f, 1.0f, (float) mask - 3.0f), fi = std::floor (d), fr = d - fi;
                const int i0 = (w - (int) fi) & mask, i1 = (i0 - 1) & mask;
                const float del = line[c][(size_t) i0] + (line[c][(size_t) i1] - line[c][(size_t) i0]) * fr;
                y = del * bB0 + bX1[c] * bB1 + bX2[c] * bB2 - bY1[c] * bA1 - bY2[c] * bA2;
                bX2[c] = bX1[c]; bX1[c] = del; bY2[c] = bY1[c]; bY1[c] = y;
                y = std::tanh (ag * y) / ag * tapeMake;
                tl1[c] = y * (1.0f - tapeA) + tl1[c] * tapeA;
                tl2[c] = tl1[c] * (1.0f - tapeA) + tl2[c] * tapeA;
                x += (tl2[c] - x) * stage[2];

                // CRUSH: fewer bits, held samples
                if (newHold) hold[c] = std::round (x * levels) / levels;
                x += (x + (hold[c] - x) * p.crushMix - x) * stage[3] * 1.0f;

                out[c] = x;
            }
            w = (w + 1) & mask;
            for (int c = 0; c < 2; ++c) io[c][i] = (dry[c] + (out[c] - dry[c]) * mixS) * levelS;
        }
    }

private:
    void setHighShelf (float fc, float gainDb)
    {
        const float A = std::exp (gainDb * 0.0575646f), w0 = kTwoPi * fc / fs, c = std::cos (w0), sq = 2.0f * std::sqrt (A) * (std::sin (w0) * 0.70710678f);
        const float b0 = A * ((A + 1) + (A - 1) * c + sq), b1 = -2 * A * ((A - 1) + (A + 1) * c), b2 = A * ((A + 1) + (A - 1) * c - sq);
        const float a0 = (A + 1) - (A - 1) * c + sq, a1 = 2 * ((A - 1) - (A + 1) * c), a2 = (A + 1) - (A - 1) * c - sq;
        hsB0 = b0 / a0; hsB1 = b1 / a0; hsB2 = b2 / a0; hsA1 = a1 / a0; hsA2 = a2 / a0;
    }
    void setLowShelf (float fc, float gainDb)
    {
        const float A = std::exp (gainDb * 0.0575646f), w0 = kTwoPi * fc / fs, c = std::cos (w0), sq = 2.0f * std::sqrt (A) * (std::sin (w0) * 0.70710678f);
        const float b0 = A * ((A + 1) - (A - 1) * c + sq), b1 = 2 * A * ((A - 1) - (A + 1) * c), b2 = A * ((A + 1) - (A - 1) * c - sq);
        const float a0 = (A + 1) + (A - 1) * c + sq, a1 = -2 * ((A - 1) + (A + 1) * c), a2 = (A + 1) + (A - 1) * c - sq;
        bB0 = b0 / a0; bB1 = b1 / a0; bB2 = b2 / a0; bA1 = a1 / a0; bA2 = a2 / a0;
    }

    float fs = 48000.0f;
    float hsB0 = 1, hsB1 = 0, hsB2 = 0, hsA1 = 0, hsA2 = 0, bB0 = 1, bB1 = 0, bB2 = 0, bA1 = 0, bA2 = 0;
    float hsX1[2] {}, hsX2[2] {}, hsY1[2] {}, hsY2[2] {}, bX1[2] {}, bX2[2] {}, bY1[2] {}, bY2[2] {};
    float dcX1[2] {}, dcY1[2] {}, lpT[2] {}, tl1[2] {}, tl2[2] {}, hold[2] {};
    float stage[4] { -1, -1, -1, -1 }, gBoost = -1.0f, mixS = -1.0f, levelS = -1.0f, wow = 0.0f, counter = 1.0f;
    int size = 1, mask = 0, w = 0;
    std::vector<float> line[2];
};
} // namespace sa1
