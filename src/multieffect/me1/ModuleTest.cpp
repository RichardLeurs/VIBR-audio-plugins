// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
// Offline check of every ME-1 module: runs it over a sine and a noise burst at the knob extremes and reports non-finite or huge output.
// Build: cmake --build <build dir> --target ME1ModuleTest ; run the produced binary.
#include <JuceHeader.h>
#include "Modules.h"
#include <cstring>

int main()
{
    int bad = 0;
    juce::Random rng (3);
    for (int m = 0; m < me1::numModules(); ++m)
    {
        const auto& info = me1::moduleInfo (m);
        for (int variant = 0; variant < 4; ++variant)                    // defaults, all min, all max, all middle
        {
            auto mod = info.make();
            mod->prepare (48000.0);
            float p[me1::kMaxParams] {};
            for (int k = 0; k < info.numParams; ++k)
            {
                const auto& s = info.params[k];
                p[k] = variant == 0 ? s.def : variant == 1 ? s.min : variant == 2 ? s.max : 0.5f * (s.min + s.max);
            }
            float peak = 0.0f, rms = 0.0f, inRms = 0.0f; bool finite = true; long count = 0;
            std::vector<float> l (512), r (512);
            for (int b = 0; b < 400; ++b)
            {
                for (int i = 0; i < 512; ++i)
                {
                    const float t = (float) (b * 512 + i) / 48000.0f;
                    const float x = (b % 100 < 60 ? 0.4f * std::sin (6.2831853f * 220.0f * t) : 0.0f) + (b % 100 >= 60 && b % 100 < 70 ? 0.3f * (rng.nextFloat() - 0.5f) : 0.0f);
                    l[(size_t) i] = x; r[(size_t) i] = x * 0.8f;
                    inRms += x * x;
                }
                mod->process (l.data(), r.data(), 512, p);
                for (int i = 0; i < 512; ++i)
                {
                    if (! std::isfinite (l[(size_t) i]) || ! std::isfinite (r[(size_t) i])) finite = false;
                    peak = std::max ({ peak, std::abs (l[(size_t) i]), std::abs (r[(size_t) i]) });
                    rms += l[(size_t) i] * l[(size_t) i]; ++count;
                }
            }
            const bool ok = finite && peak < (std::strcmp (info.id, "BOOST") == 0 || std::strcmp (info.id, "TRANSIENT") == 0 ? 16.0f : 8.0f);
            if (! ok) ++bad;
            if (variant == 0 || ! ok)
                std::printf ("%-11s v%d  peak %6.3f  gain %+6.1f dB  %s\n", info.id, variant, peak,
                             10.0f * std::log10 ((rms + 1e-12f) / (inRms + 1e-12f)), ok ? "ok" : "PROBLEM");
        }
    }
    // behaviour checks
    {
        const int idx = me1::indexOfModule ("PINGPONG");
        auto mod = me1::moduleInfo (idx).make(); mod->prepare (48000.0);
        const float p[3] = { 100.0f, 50.0f, 100.0f };                           // 100 ms, mix 100 %
        std::vector<float> l (48000, 0.0f), r (48000, 0.0f);
        l[0] = r[0] = 1.0f;
        mod->process (l.data(), r.data(), 48000, p);
        const auto energy = [] (const std::vector<float>& v, int centre) { float e = 0; for (int i = centre - 50; i < centre + 50; ++i) e += v[(size_t) i] * v[(size_t) i]; return e; };
        const bool ok = energy (l, 4800) > 0.1f && energy (r, 4800) < 0.01f && energy (r, 9600) > 0.01f && energy (l, 14400) > 0.01f;
        std::printf ("ping-pong alternates L R L: %s\n", ok ? "ok" : "PROBLEM"); if (! ok) ++bad;
    }
    {
        const int idx = me1::indexOfModule ("REVERSE");
        auto mod = me1::moduleInfo (idx).make(); mod->prepare (48000.0);
        const float p[3] = { 500.0f, 0.0f, 100.0f };                            // 500 ms segments
        std::vector<float> l (96000, 0.0f), r (96000, 0.0f);
        for (int i = 0; i < 24000; ++i) l[(size_t) i] = r[(size_t) i] = std::exp (-(float) i / 6000.0f) * std::sin (6.2831853f * 440.0f * (float) i / 48000.0f);   // decaying note
        auto in = l;
        mod->process (l.data(), r.data(), 96000, p);
        const auto rms = [&] (int a, int b) { float e = 0; for (int i = a; i < b; ++i) { const float w = l[(size_t) i] - in[(size_t) i]; e += w * w; } return e; };
        // the reversed note must grow: more energy late in the segment than early
        const bool ok = rms (36000, 48000) > 3.0f * rms (24000, 36000);
        std::printf ("reverse plays backwards (late %.1f vs early %.1f): %s\n", rms (36000, 48000), rms (24000, 36000), ok ? "ok" : "PROBLEM"); if (! ok) ++bad;
    }
    for (const float shift : { 12.0f, -12.0f })
    {
        const int idx = me1::indexOfModule ("PITCH");
        auto mod = me1::moduleInfo (idx).make(); mod->prepare (48000.0);
        const float p[3] = { shift, 0.0f, 100.0f };                             // wet only
        std::vector<float> l (48000), r (48000);
        for (int i = 0; i < 48000; ++i) l[(size_t) i] = r[(size_t) i] = 0.5f * std::sin (6.2831853f * 440.0f * (float) i / 48000.0f);
        mod->process (l.data(), r.data(), 48000, p);
        int crossings = 0;
        for (int i = 24001; i < 48000; ++i) if (l[(size_t) i - 1] < 0.0f && l[(size_t) i] >= 0.0f) ++crossings;
        const float hz = (float) crossings / 0.5f;                              // half a second
        const float want = 440.0f * std::pow (2.0f, shift / 12.0f);
        const bool ok = hz > want * 0.9f && hz < want * 1.1f;
        std::printf ("pitch %+.0f st turns 440 Hz into %.0f Hz: %s\n", shift, hz, ok ? "ok" : "PROBLEM"); if (! ok) ++bad;
    }
    {   // ROTARY must pulse in level; TUBE must add a second harmonic
        auto run = [] (const char* id, const float* p, std::vector<float>& l)
        {
            auto mod = me1::moduleInfo (me1::indexOfModule (id)).make(); mod->prepare (48000.0);
            l.assign (48000, 0.0f);
            for (int i = 0; i < 48000; ++i) l[(size_t) i] = 0.4f * std::sin (6.2831853f * 440.0f * (float) i / 48000.0f);
            auto r = l;
            mod->process (l.data(), r.data(), 48000, p);
        };
        std::vector<float> l;
        const float rot[3] = { 100.0f, 100.0f, 100.0f };
        run ("ROTARY", rot, l);
        float lo = 1e9f, hi = 0.0f;
        for (int w = 30000; w + 480 < 48000; w += 480) { float e = 0; for (int i = 0; i < 480; ++i) e += l[(size_t) (w + i)] * l[(size_t) (w + i)]; lo = std::min (lo, e); hi = std::max (hi, e); }
        const bool okR = hi > 1.5f * lo;
        std::printf ("rotary pulses (loud %.2f vs quiet %.2f): %s\n", hi, lo, okR ? "ok" : "PROBLEM"); if (! okR) ++bad;
        const float tube[3] = { 70.0f, 60.0f, 0.0f };
        run ("TUBE", tube, l);
        double re = 0, im = 0;
        for (int i = 24000; i < 48000; ++i) { const double a = 6.283185307 * 880.0 * i / 48000.0; re += l[(size_t) i] * std::cos (a); im += l[(size_t) i] * std::sin (a); }
        const double h2 = std::sqrt (re * re + im * im) / 12000.0;
        const bool okT = h2 > 0.005;
        std::printf ("tube second harmonic %.4f: %s\n", h2, okT ? "ok" : "PROBLEM"); if (! okT) ++bad;
    }
    {   // TRANSIENT SHAPER: ATTACK + raises the start of a note, SUSTAIN - lowers the tail; EXPANDER lowers quiet sounds only
        const auto tone = [] (float amp, float tau, std::vector<float>& l)
        {
            l.assign (48000, 0.0f);
            for (int i = 0; i < 48000; ++i) l[(size_t) i] = amp * std::exp (-(float) i / (tau * 48000.0f)) * std::sin (6.2831853f * 220.0f * (float) i / 48000.0f);
        };
        const auto run = [&] (const char* id, const float* p, std::vector<float>& l)
        {
            auto mod = me1::moduleInfo (me1::indexOfModule (id)).make(); mod->prepare (48000.0);
            auto r = l;
            mod->process (l.data(), r.data(), 48000, p);
        };
        const auto peak = [] (const std::vector<float>& v, int a, int b) { float m = 0; for (int i = a; i < b; ++i) m = std::max (m, std::abs (v[(size_t) i])); return m; };
        std::vector<float> flat, up, down;
        tone (0.5f, 0.15f, flat); up = flat; down = flat;
        const float neutral[3] = { 0.0f, 0.0f, 0.0f }, attackUp[3] = { 100.0f, 0.0f, 0.0f }, sustainDown[3] = { 0.0f, -100.0f, 0.0f };
        run ("TRANSIENT", neutral, flat); run ("TRANSIENT", attackUp, up); run ("TRANSIENT", sustainDown, down);
        const bool okA = peak (up, 0, 960) > 1.25f * peak (flat, 0, 960);
        const bool okS = peak (down, 14400, 19200) < 0.7f * peak (flat, 14400, 19200);
        std::printf ("transient: attack +100 raises the start (%.2f vs %.2f): %s; sustain -100 lowers the tail (%.3f vs %.3f): %s\n",
                     peak (up, 0, 960), peak (flat, 0, 960), okA ? "ok" : "PROBLEM", peak (down, 14400, 19200), peak (flat, 14400, 19200), okS ? "ok" : "PROBLEM");
        if (! okA) ++bad;
        if (! okS) ++bad;

        std::vector<float> quiet (48000), loud (48000);
        for (int i = 0; i < 48000; ++i) { const float s = std::sin (6.2831853f * 440.0f * (float) i / 48000.0f); quiet[(size_t) i] = 0.01f * s; loud[(size_t) i] = 0.3f * s; }
        const float exp1[3] = { -30.0f, 4.0f, 100.0f };
        run ("EXPANDER", exp1, quiet); run ("EXPANDER", exp1, loud);
        const float q = peak (quiet, 24000, 48000) / 0.01f, lo = peak (loud, 24000, 48000) / 0.3f;
        const bool okE = q < 0.3f && lo > 0.95f;
        std::printf ("expander: quiet sound x%.2f, loud sound x%.2f: %s\n", q, lo, okE ? "ok" : "PROBLEM"); if (! okE) ++bad;
    }
    std::printf ("%d problems\n", bad);
    return bad == 0 ? 0 : 1;
}
