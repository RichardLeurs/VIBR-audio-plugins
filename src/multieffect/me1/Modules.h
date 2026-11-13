// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include <JuceHeader.h>
#include <memory>

// ME-1 effect modules. Every module is a small, self-contained stereo effect with 1 to 3 knobs; the ME-1 chains up to 8 of them.
// The list, the knob ranges and the DSP notes are in "ME-1 specs.md" (keep that file and this one in step).
namespace me1
{
constexpr int kMaxParams = 3;

// One knob. Values are real units (ms, %, dB, Hz ...); `centre` > 0 puts that value at the middle of the knob travel (skew).
struct ParamSpec
{
    const char* name;
    float min, max, def, centre;
    const char* unit;
    int decimals;
    const char* labels = nullptr;   // "LOW|BAND|HIGH": a stepped knob (0, 1, 2 ...) shows these names
};

class Module
{
public:
    virtual ~Module() = default;
    virtual void prepare (double sampleRate) = 0;                          // allocates; message thread only
    virtual void process (float* left, float* right, int numSamples, const float* params) = 0;   // audio thread, in place
};

struct ModuleInfo
{
    const char* id;                 // stored in the session
    const char* name;               // panel title
    const char* description;
    int group;
    int numParams;
    ParamSpec params[kMaxParams];
    std::unique_ptr<Module> (*make)();
    float tailSeconds;
    int syncParam = -1;             // index of the TIME / RATE knob that can follow the host tempo (SYNC button), -1 = none
};

int numModules();
const ModuleInfo& moduleInfo (int index);
int indexOfModule (const juce::String& id);          // -1 when unknown
int numGroups();
const char* groupName (int group);
juce::uint32 groupColour (int group);     // the colour of a module group (tile bar, like the pedal palette)
juce::String formatValue (const ParamSpec&, float value);   // "350 ms", "+3.0 dB"

// tempo sync: with SYNC on, the TIME / RATE knob picks a note length instead of ms / Hz
constexpr int kNumDivisions = 12, kDefaultDivision = 6;      // 1/8
const char* divisionName (int index);                        // "1/4", "1/8.", "1/16T" ...
float syncedValue (const ParamSpec&, int division, double bpm);   // ms for a "ms" knob, Hz for a "Hz" knob
} // namespace me1
