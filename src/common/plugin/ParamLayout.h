// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once
#include <JuceHeader.h>
#include <functional>
#include "../BypassLink.h"

// Helpers for the parameter layout of a processor (replaces the copies of `unitAttrs`, the `pct` / `skewed` lambdas and the power / bypass lines that every plugin
// carried). The ids, names, ranges, defaults, version hints and meta flags are exactly what the copies produced: a refactor to these helpers must leave the parameter
// list (id, name, range, default, steps, flags) unchanged, which `Shot-<PLUGIN> --dump` checks.
namespace vibr
{
// host label and readable text for a float parameter (the stored value is the shown value, the host shows the label next to it: the number only)
inline juce::AudioParameterFloatAttributes unitAttrs (const juce::String& unit, int decimals)
{
    return juce::AudioParameterFloatAttributes()
        .withLabel (unit)
        .withStringFromValueFunction ([decimals] (float v, int) { return juce::String (v, decimals); })
        .withValueFromStringFunction ([] (const juce::String& s) { return s.retainCharacters ("0123456789.-+").getFloatValue(); });
}

// the same with the text made by a function (a note length, a time with its unit, ...)
inline juce::AudioParameterFloatAttributes unitAttrs (const juce::String& unit, std::function<juce::String (float)> text)
{
    return juce::AudioParameterFloatAttributes()
        .withLabel (unit)
        .withStringFromValueFunction ([text] (float v, int) { return text (v); })
        .withValueFromStringFunction ([] (const juce::String& s) { return s.retainCharacters ("0123456789.-+").getFloatValue(); });
}

// a range with the middle of the knob at `centre`
inline juce::NormalisableRange<float> skewedRange (float lo, float hi, float step, float centre)
{
    juce::NormalisableRange<float> r (lo, hi, step);
    r.setSkewForCentre (centre);
    return r;
}

using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;

// "power" (true = on, a meta parameter: the OFF / ON switch in the window) and "bypass" (the host's bypass button, see BypassLink.h)
inline void addPower (Layout& layout)
{
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "power", 1 }, "Power", true, juce::AudioParameterBoolAttributes().withMeta (true)));
    layout.add (makeBypassParam());
}

// a float parameter in percent, 0 .. 100 in steps of 1
inline void addPercent (Layout& layout, const char* id, const juce::String& name, float def)
{
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), def, unitAttrs ("%", 0)));
}

// a float parameter with its own range, unit and decimals
inline void addFloat (Layout& layout, const char* id, const juce::String& name, juce::NormalisableRange<float> range, float def, const juce::String& unit, int decimals)
{
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range, def, unitAttrs (unit, decimals)));
}

// the output level, -24 .. +12 dB in steps of 1 (every Lab ends with it)
inline void addLevel (Layout& layout, const char* id = "level", float def = 0.0f)
{
    addFloat (layout, id, "Level", juce::NormalisableRange<float> (-24.0f, 12.0f, 1.0f), def, "dB", 0);
}

inline void addChoice (Layout& layout, const char* id, const juce::String& name, const juce::StringArray& choices, int def)
{
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, choices, def));
}

inline void addToggle (Layout& layout, const char* id, const juce::String& name, bool def)
{
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def));
}
} // namespace vibr
