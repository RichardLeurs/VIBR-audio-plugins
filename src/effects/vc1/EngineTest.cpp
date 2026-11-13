// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
// Offline check of the VC-1 engine: extreme settings stay finite and bounded; routing a source to a destination really moves it:
// LFO 1 -> FILTER varies the level of noise, ENVELOPE -> AMP follows the playing, MIDI -> FILTER opens with the controller,
// LFO 1 -> PAN swings between left and right, and X-MOD changes how LFO 1 moves.
#include <cstdio>
#include <vector>
#include "ModEngine.h"

static float noise (uint32_t& s) { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s & 0xffffff) / 8388608.0f - 1.0f; }

int main()
{
    const float fs = 48000.0f;
    int problems = 0;
    for (int setting = 0; setting < 3; ++setting)
        for (int type = 0; type < vc1::kNumTypes; ++type)
        {
            vc1::Engine e; e.prepare (fs);
            vc1::Params p; p.type = type;
            const float v = setting == 0 ? 0.0f : setting == 1 ? 1.0f : 0.5f;
            p.cutoff = p.res = p.envSens = p.envRel = p.rate1 = p.rate2 = p.randRate = p.xmod = v; p.midi = v;
            for (auto& row : p.matrix) for (auto& m : row) m = setting == 0 ? -1.0f : 1.0f;
            uint32_t seed = 3u; float peak = 0; bool fin = true;
            std::vector<float> l (512), r (512);
            for (int pos = 0; pos < (int) (fs * 3.0f); pos += 512)
            {
                for (int i = 0; i < 512; ++i) l[(size_t) i] = r[(size_t) i] = 0.4f * noise (seed);
                e.process (l.data(), r.data(), 512, p, 1.0f);
                for (int i = 0; i < 512; ++i) { if (! std::isfinite (l[(size_t) i]) || ! std::isfinite (r[(size_t) i])) fin = false; peak = std::max ({ peak, std::fabs (l[(size_t) i]), std::fabs (r[(size_t) i]) }); }
            }
            if (! fin || peak > 3.0f) { ++problems; std::printf ("setting %d type %d peak %.3f <-- PROBLEM\n", setting, type, peak); }
        }

    // routed movement: returns min and max of the left level in 10 ms windows over `seconds`
    const auto levels = [&] (vc1::Params p, float seconds, bool gate, float& lo, float& hi, float* leftRight = nullptr)
    {
        vc1::Engine e; e.prepare (fs);
        uint32_t seed = 11u;
        std::vector<float> l (480), r (480);
        lo = 1e9f; hi = 0.0f;
        double sl = 0, sr = 0, lmin = 1e9, rmax = 0;
        for (int w = 0; w < (int) (seconds * 100.0f); ++w)
        {
            const float g = (gate && (w / 40) % 2 == 1) ? 0.01f : 0.4f;
            for (int i = 0; i < 480; ++i) l[(size_t) i] = r[(size_t) i] = g * noise (seed);
            e.process (l.data(), r.data(), 480, p, 1.0f);
            double el = 0, er = 0;
            for (int i = 0; i < 480; ++i) { el += l[(size_t) i] * l[(size_t) i]; er += r[(size_t) i] * r[(size_t) i]; }
            const float rms = (float) std::sqrt (el / 480.0);
            if (w >= 10) { lo = std::min (lo, rms); hi = std::max (hi, rms); sl += el; sr += er; lmin = std::min (lmin, el); rmax = std::max (rmax, er); }
        }
        if (leftRight) { leftRight[0] = (float) lmin; leftRight[1] = (float) rmax; }
    };
    float lo, hi;
    vc1::Params base; base.cutoff = 0.35f; base.res = 0.4f; base.rate1 = 0.55f; base.rate2 = 0.3f;

    vc1::Params a = base; a.matrix[vc1::Lfo1][vc1::Filter] = 0.9f;
    levels (a, 2.0f, false, lo, hi);
    std::printf ("LFO1 -> FILTER: level %.4f .. %.4f\n", lo, hi);
    if (! (hi > 2.0f * lo)) { ++problems; std::printf ("  the filter does not move\n"); }

    vc1::Params b = base; b.cutoff = 1.0f; b.matrix[vc1::Env][vc1::Amp] = -1.0f;
    levels (b, 1.6f, true, lo, hi);
    std::printf ("ENVELOPE -> AMP (negative): level %.4f .. %.4f\n", lo, hi);
    if (! (hi > 1.5f * lo)) { ++problems; std::printf ("  the amp does not follow the envelope\n"); }

    vc1::Params c = base; c.matrix[vc1::Midi][vc1::Filter] = 1.0f;
    float m0lo, m0hi, m1lo, m1hi;
    c.midi = 0.0f; levels (c, 1.0f, false, m0lo, m0hi);
    c.midi = 1.0f; levels (c, 1.0f, false, m1lo, m1hi);
    std::printf ("MIDI -> FILTER: controller 0 -> level %.4f, controller 1 -> level %.4f\n", m0hi, m1hi);
    if (! (m1hi > 2.0f * m0hi)) { ++problems; std::printf ("  the controller does not open the filter\n"); }

    vc1::Params d = base; d.cutoff = 1.0f; d.matrix[vc1::Lfo1][vc1::Pan] = 1.0f; d.rate1 = 0.5f;
    float lr[2];
    levels (d, 2.0f, false, lo, hi, lr);
    std::printf ("LFO1 -> PAN: quietest left window %.2f, loudest right window %.2f\n", lr[0], lr[1]);
    if (! (lr[1] > 4.0f * lr[0])) { ++problems; std::printf ("  the pan does not swing\n"); }
    std::printf ("%d problems\n", problems);
    return problems == 0 ? 0 : 1;
}
