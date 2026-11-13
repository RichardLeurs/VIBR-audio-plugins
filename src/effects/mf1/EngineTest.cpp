// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
// Offline check of the MF-1 engine: every filter type with every source at extreme and mid settings must stay finite and bounded,
// and the movement must be audible: the output level over time has to change (LFO, steps) or follow the playing (envelope).
#include <cstdio>
#include <vector>
#include "MotionEngine.h"

static float noise (uint32_t& s) { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s & 0xffffff) / 8388608.0f - 1.0f; }

int main()
{
    const float fs = 48000.0f;
    int problems = 0;
    for (int type = 0; type < mf1::kNumTypes; ++type)
        for (int source = 0; source < mf1::kNumSources; ++source)
            for (int setting = 0; setting < 3; ++setting)
            {
                mf1::Engine e; e.prepare (fs);
                mf1::Params p;
                p.type = type; p.source = source;
                const float v = setting == 0 ? 0.0f : setting == 1 ? 1.0f : 0.5f;
                p.cutoff = p.res = p.depth = p.m1 = p.m2 = v;
                p.sync = setting == 2;
                uint32_t seed = 99u;
                float peak = 0.0f;
                bool finite = true;
                std::vector<float> l (512), r (512);
                for (int pos = 0; pos < (int) (fs * 3.0f); pos += 512)
                {
                    for (int i = 0; i < 512; ++i) l[(size_t) i] = r[(size_t) i] = 0.4f * noise (seed);
                    e.process (l.data(), r.data(), 512, p, 1.0f, 1.0f);
                    for (int i = 0; i < 512; ++i)
                    {
                        if (! std::isfinite (l[(size_t) i]) || ! std::isfinite (r[(size_t) i])) finite = false;
                        peak = std::max ({ peak, std::fabs (l[(size_t) i]), std::fabs (r[(size_t) i]) });
                    }
                }
                const bool bad = ! finite || peak > 2.0f;
                if (bad) { ++problems; std::printf ("type %d source %d setting %d  peak %.3f  <-- PROBLEM\n", type, source, setting, peak); }
            }

    // movement: a low-pass filter with deep LFO / steps on white noise: the level in 50 ms windows must vary a lot
    for (int source = 0; source < 3; ++source)
    {
        mf1::Engine e; e.prepare (fs);
        mf1::Params p; p.type = mf1::LowPass; p.source = source; p.cutoff = 0.35f; p.res = 0.4f; p.depth = 0.9f; p.m1 = 0.45f; p.m2 = 0.0f;
        uint32_t seed = 7u;
        std::vector<float> l (480), r (480);
        float lo = 1e9f, hi = 0.0f;
        for (int w = 0; w < 80; ++w)               // 80 windows of 10 ms = 0.8 s; the envelope source gets a gate: loud for 0.4 s, then quiet
        {
            double en = 0.0;
            for (int i = 0; i < 480; ++i) { const float g = (source == mf1::Envelope && w >= 40) ? 0.01f : 0.4f; l[(size_t) i] = r[(size_t) i] = g * noise (seed); }
            e.process (l.data(), r.data(), 480, p, 1.0f, 1.0f);
            for (int i = 0; i < 480; ++i) en += l[(size_t) i] * l[(size_t) i];
            const float rms = (float) std::sqrt (en / 480.0);
            if (w >= 5) { lo = std::min (lo, rms); hi = std::max (hi, rms); }
        }
        const bool ok = hi > 2.0f * lo;
        std::printf ("%-9s movement: level %.4f .. %.4f %s\n", mf1::sourceName (source), lo, hi, ok ? "" : "  <-- does not move");
        if (! ok) ++problems;
    }
    std::printf ("%d problems\n", problems);
    return problems == 0 ? 0 : 1;
}
