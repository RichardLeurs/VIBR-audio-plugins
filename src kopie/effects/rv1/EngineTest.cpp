// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
// Offline check of the RV-1 engine: every mode at extreme / mid settings (and with freeze) must stay finite and bounded,
// produce sound from a tone burst, and REVERSE must really play the burst backwards (its envelope starts quiet and rises).
#include <cstdio>
#include "ReverseEngine.h"

int main()
{
    const float fs = 48000.0f;
    int problems = 0;
    for (int mode = 0; mode < rv1::kNumModes; ++mode)
        for (int setting = 0; setting < 4; ++setting)
        {
            rv1::Engine e;
            e.prepare (fs);
            rv1::Params p;
            p.mode = mode;
            const float v = setting == 0 ? 0.0f : setting == 1 ? 1.0f : 0.5f;
            p.length = p.speed = p.smooth = p.random = p.repeats = v;
            float peak = 0.0f, energy = 0.0f;
            bool finite = true;
            const int total = (int) (fs * 6.0f);
            std::vector<float> l (512), r (512);
            for (int pos = 0; pos < total; pos += 512)
            {
                if (setting == 3 && pos > (int) (fs * 0.9f)) p.freeze = true;
                for (int i = 0; i < 512; ++i)
                {
                    const float t = (float) (pos + i) / fs;
                    const float x = t < 1.0f ? 0.5f * std::sin (6.2831853f * 220.0f * t) * (t / 1.0f) : 0.0f;      // a swelling tone for 1 s
                    l[(size_t) i] = x; r[(size_t) i] = x;
                }
                e.process (l.data(), r.data(), 512, p, 1.0f, 1.0f);
                for (int i = 0; i < 512; ++i)
                {
                    if (! std::isfinite (l[(size_t) i]) || ! std::isfinite (r[(size_t) i])) finite = false;
                    peak = std::max ({ peak, std::fabs (l[(size_t) i]), std::fabs (r[(size_t) i]) });
                    if (pos > (int) (fs * 3.0f)) energy += l[(size_t) i] * l[(size_t) i];
                }
            }
            const bool bad = ! finite || peak > 2.0f || peak < 0.003f;
            if (bad) ++problems;
            std::printf ("%-8s setting %d  peak %.3f  energy(3-6s) %.2f %s\n", rv1::modeName (mode), setting, peak, energy, bad ? "  <-- PROBLEM" : "");
            if (setting == 3 && energy < 1.0f) { ++problems; std::printf ("  freeze did not keep playing the captured audio\n"); }
        }

    {   // REVERSE: a tone that decays should come back growing
        rv1::Engine e; e.prepare (fs);
        rv1::Params p; p.mode = rv1::Reverse; p.length = 0.62f; p.speed = 0.5f; p.smooth = 0.3f; p.random = 0.0f; p.repeats = 0.0f;
        std::vector<float> l (512), r (512), out;
        for (int pos = 0; pos < (int) (fs * 2.0f); pos += 512)
        {
            for (int i = 0; i < 512; ++i)
            {
                const float t = (float) (pos + i) / fs;
                const float x = t < 0.5f ? std::sin (6.2831853f * 440.0f * t) * std::exp (-6.0f * t) : 0.0f;        // decaying burst
                l[(size_t) i] = r[(size_t) i] = x;
            }
            e.process (l.data(), r.data(), 512, p, 1.0f, 1.0f);
            for (int i = 0; i < 512; ++i) out.push_back (l[(size_t) i]);
        }
        const auto rms = [&] (int a, int b) { double s = 0; for (int i = a; i < b; ++i) s += out[(size_t) i] * out[(size_t) i]; return std::sqrt (s / (b - a)); };
        // the reversed burst is replayed within about the next slice length (~0.25 s): the later part of the slice is louder than the earlier part
        const double early = rms ((int) (0.35f * fs), (int) (0.45f * fs)), late = rms ((int) (0.52f * fs), (int) (0.62f * fs));
        std::printf ("REVERSE shape: early %.4f  late %.4f\n", early, late);
        if (! (late > early)) { ++problems; std::printf ("  reverse does not grow\n"); }
    }
    std::printf ("%d problems\n", problems);
    return problems == 0 ? 0 : 1;
}
