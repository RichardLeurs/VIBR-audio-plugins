// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
// Offline check of the SR-1 engine: every texture / scale / setting stays finite and bounded; a tone on a string's note makes that string
// ring much longer than a tone that is not on any string; the ring dies out faster with a short DECAY.
#include <cstdio>
#include <vector>
#include "ResonatorEngine.h"

static float noise (uint32_t& s) { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s & 0xffffff) / 8388608.0f - 1.0f; }

// ring after the input stopped: rms in 0.5 .. 1.0 s after a 0.5 s burst of a sine at f
static double ring (const sr1::Params& p, float f)
{
    const float fs = 48000.0f;
    sr1::Engine e; e.prepare (fs);
    std::vector<float> l (512), r (512);
    double en = 0; int cnt = 0;
    for (int pos = 0; pos < (int) (fs * 1.5f); pos += 512)
    {
        for (int i = 0; i < 512; ++i) { const float t = (float) (pos + i) / fs; l[(size_t) i] = r[(size_t) i] = t < 0.5f ? 0.5f * std::sin (6.2831853f * f * t) : 0.0f; }
        e.process (l.data(), r.data(), 512, p, 1.0f, 1.0f);
        if (pos >= (int) (fs * 1.0f)) for (int i = 0; i < 512; ++i) { en += (double) l[(size_t) i] * l[(size_t) i]; ++cnt; }
    }
    return std::sqrt (en / cnt);
}

int main()
{
    int problems = 0;
    for (int tex = 0; tex < sr1::kNumTextures; ++tex)
        for (int sc = 0; sc < sr1::kNumScales; ++sc)
            for (int s = 0; s < 3; ++s)
            {
                sr1::Params p; p.texture = tex; p.scale = sc; p.root = sc * 2;
                const float v = s == 0 ? 0.0f : s == 1 ? 1.0f : 0.5f;
                p.strings = p.decay = p.damping = p.tension = p.motion = v;
                sr1::Engine e; e.prepare (48000.0f);
                uint32_t seed = 5u;
                float peak = 0; bool fin = true;
                std::vector<float> l (512), r (512);
                for (int pos = 0; pos < 48000 * 2; pos += 512)
                {
                    for (int i = 0; i < 512; ++i) l[(size_t) i] = r[(size_t) i] = 0.4f * noise (seed);
                    e.process (l.data(), r.data(), 512, p, 1.0f, 1.0f);
                    for (int i = 0; i < 512; ++i) { if (! std::isfinite (l[(size_t) i]) || ! std::isfinite (r[(size_t) i])) fin = false; peak = std::max ({ peak, std::fabs (l[(size_t) i]), std::fabs (r[(size_t) i]) }); }
                }
                if (! fin || peak > 2.0f) { ++problems; std::printf ("texture %d scale %d setting %d peak %.3f <-- PROBLEM\n", tex, sc, s, peak); }
            }

    sr1::Params p; p.root = 0; p.scale = 0; p.texture = 1; p.strings = 1.0f; p.decay = 0.7f; p.damping = 0.2f; p.tension = 0.0f; p.motion = 0.0f; p.fine = 0.5f;
    const float onNote = sr1::noteHz (sr1::stringNote (2, 0, 0), 0.0f);            // string 3: E3 in C major
    const float offNote = onNote * 1.0595f * 1.0f;                                  // a semitone higher: on no string of C major? (F is a string: use F#)
    const float offTone = sr1::noteHz (sr1::stringNote (2, 0, 0) + 2, 0.0f) * 1.0f;   // G is a string too; use a note a quarter tone off instead
    const float quarter = onNote * std::pow (2.0f, 0.5f / 12.0f);
    const double rOn = ring (p, onNote), rOff = ring (p, quarter);
    (void) offNote; (void) offTone;
    std::printf ("ring on a string's note %.4f, a quarter tone off %.4f\n", rOn, rOff);
    if (! (rOn > 3.0 * rOff)) { ++problems; std::printf ("  a tone on the string should ring much longer\n"); }
    sr1::Params shortP = p; shortP.decay = 0.0f;
    const double rShort = ring (shortP, onNote);
    std::printf ("ring with short decay %.4f\n", rShort);
    if (! (rOn > 3.0 * rShort)) { ++problems; std::printf ("  decay does not shorten the ring\n"); }
    std::printf ("%d problems\n", problems);
    return problems == 0 ? 0 : 1;
}
