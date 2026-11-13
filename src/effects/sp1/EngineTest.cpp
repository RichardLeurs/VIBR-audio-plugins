// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
// Offline check of the SP-1 engine: every mode, extreme knob settings and freeze must stay finite and bounded,
// and a tail must exist (and, in freeze, hold).
#include <cstdio>
#include "SpaceEngine.h"

int main()
{
    const float fs = 48000.0f;
    int problems = 0;
    for (int mode = 0; mode < sp1::kNumModes; ++mode)
        for (int setting = 0; setting < 4; ++setting)
        {
            sp1::Engine e;
            e.prepare (fs);
            sp1::Params p;
            p.mode = mode;
            const float v = setting == 0 ? 0.0f : setting == 1 ? 1.0f : 0.5f;
            p.size = p.decay = p.diffusion = p.tone = p.motion = p.texture = v;
            p.freeze = false;
            float peak = 0.0f, tailAfter = 0.0f;
            bool finite = true;
            const int total = (int) (fs * 6.0f);
            std::vector<float> l (512), r (512);
            for (int pos = 0; pos < total; pos += 512)
            {
                if (setting == 3 && pos > (int) (fs * 3.0f)) p.freeze = true;
                for (int i = 0; i < 512; ++i)
                {
                    const float t = (float) (pos + i) / fs;
                    const float x = t < 0.5f ? 0.5f * std::sin (6.2831853f * 220.0f * t) * (t < 0.4f ? 1.0f : 0.0f) : 0.0f;
                    l[(size_t) i] = x; r[(size_t) i] = x;
                }
                e.process (l.data(), r.data(), 512, p, 1.0f, 1.0f);
                for (int i = 0; i < 512; ++i)
                {
                    if (! std::isfinite (l[(size_t) i]) || ! std::isfinite (r[(size_t) i])) finite = false;
                    peak = std::max ({ peak, std::fabs (l[(size_t) i]), std::fabs (r[(size_t) i]) });
                    if (pos > (int) (fs * 5.0f)) tailAfter = std::max (tailAfter, std::fabs (l[(size_t) i]));
                }
            }
            const bool bad = ! finite || peak > 2.0f;
            if (bad) ++problems;
            std::printf ("%-10s setting %d  peak %.3f  tail(5-6s) %.4f %s\n", sp1::modeName (mode), setting, peak, tailAfter, bad ? "  <-- PROBLEM" : "");
            if (setting == 3 && tailAfter < 1.0e-4f) { ++problems; std::printf ("  freeze did not hold the tail\n"); }
        }
    std::printf ("%d problems\n", problems);
    return problems == 0 ? 0 : 1;
}
