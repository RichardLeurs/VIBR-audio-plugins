// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include <JuceHeader.h>

// The pedals the DC-1 can chain. Each pedal's own PluginProcessor.cpp is compiled into the DC-1 with createPluginFilter renamed
// to createPluginFilter_<folder> (see CMakeLists.txt).
#define DC1_DECLARE(x) juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter_##x();
DC1_DECLARE (dd1) DC1_DECLARE (dm1) DC1_DECLARE (ce1) DC1_DECLARE (re1) DC1_DECLARE (re2) DC1_DECLARE (ds1) DC1_DECLARE (bf3)
DC1_DECLARE (cs3) DC1_DECLARE (lm2) DC1_DECLARE (ph2) DC1_DECLARE (ge7) DC1_DECLARE (eh2) DC1_DECLARE (ir2) DC1_DECLARE (sl2)
DC1_DECLARE (od2) DC1_DECLARE (oc5) DC1_DECLARE (bs1) DC1_DECLARE (mm1) DC1_DECLARE (sd1) DC1_DECLARE (sd3)
#undef DC1_DECLARE

namespace dc1
{
struct Group { const char* name; uint32_t colour; };
inline const Group* groups (int& n)
{
    static const Group g[] = { { "Delay & Echo", 0xff5276E5 }, { "Modulation", 0xff36B779 }, { "Drive", 0xffF07835 }, { "Dynamics", 0xffE9B93F },
                               { "EQ", 0xff28A9D5 }, { "Amp & Cabinet", 0xffF05B60 }, { "Pitch", 0xffE84B91 }, { "Reverb", 0xff8959D5 },
                               { "Rhythm", 0xffA5C943 } };
    n = (int) std::size (g);
    return g;
}

struct Effect
{
    const char* id;                 // stored in the session; never change
    const char* name;               // short name on the tile
    const char* description;
    int group;                      // index into groups()
    uint32_t colour;                // the pedal's palette colour
    juce::AudioProcessor* (*make)();
};

inline const Effect* effects (int& n)
{
    static const Effect e[] = {
        { "DD-1", "DD-1", "Dual Delay",             0, 0xff5276E5, createPluginFilter_dd1 },
        { "DM-1", "DM-1", "Delay Machine",          0, 0xff2588C9, createPluginFilter_dm1 },
        { "RE-1", "RE-1", "Space Echo",             0, 0xff28A9D5, createPluginFilter_re1 },
        { "RE-2", "RE-2", "Space Echo",             0, 0xff3155B5, createPluginFilter_re2 },
        { "MM-1", "MM-1", "Analog Delay",           0, 0xff4B91D1, createPluginFilter_mm1 },
        { "SD-1", "SD-1", "Digital Delay",          0, 0xff3979EF, createPluginFilter_sd1 },
        { "SD-3", "SD-3", "Digital Delay",          0, 0xff7048C8, createPluginFilter_sd3 },
        { "CE-1", "CE-1", "Chorus Ensemble",        1, 0xff20B6A5, createPluginFilter_ce1 },
        { "BF-3", "BF-3", "Flanger",                1, 0xff36B779, createPluginFilter_bf3 },
        { "PH-2", "PH-2", "Phaser",                 1, 0xff78C83C, createPluginFilter_ph2 },
        { "DS-1", "DS-1", "Distortion",             2, 0xffE6533C, createPluginFilter_ds1 },
        { "OD-2", "OD-2", "Hybrid Drive",           2, 0xffF07835, createPluginFilter_od2 },
        { "CS-3", "CS-3", "Compression Sustainer",  3, 0xffD8BE35, createPluginFilter_cs3 },
        { "LM-2", "LM-2", "Limiter",                3, 0xffE9B93F, createPluginFilter_lm2 },
        { "GE-7", "GE-7", "Equalizer",              4, 0xff28A9D5, createPluginFilter_ge7 },
        { "EH-2", "EH-2", "Enhancer",               4, 0xff20B6D0, createPluginFilter_eh2 },
        { "IR-2", "IR-2", "Amp & Cabinet",          5, 0xffF05B60, createPluginFilter_ir2 },
        { "OC-5", "OC-5", "Octave",                 6, 0xffE84B91, createPluginFilter_oc5 },
        { "BS-1", "BS-1", "Reverb",                 7, 0xff8959D5, createPluginFilter_bs1 },
        { "SL-2", "SL-2", "Slicer",                 8, 0xffA5C943, createPluginFilter_sl2 },
    };
    n = (int) std::size (e);
    return e;
}

inline int indexOf (const juce::String& id)
{
    int n = 0;
    const auto* e = effects (n);
    for (int i = 0; i < n; ++i)
        if (id == e[i].id) return i;
    return -1;
}
} // namespace dc1
