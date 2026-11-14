// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include <JuceHeader.h>

// Links the host's bypass button to the plugin's own OFF / ON switch.
// The plugin keeps its "power" parameter (true = on; the OFF / ON switch in the window, saved in old sessions) and gets a second
// parameter "bypass" (true = bypassed) that getBypassParameter() returns. The two always hold opposite values: when the host
// bypasses the plugin the switch in the window goes OFF, and the other way round. The sound only depends on "power".
// Usage in a processor: add vibr::makeBypassParam() to the layout, declare `vibr::BypassLink bypassLink { apvts };` right after the
// apvts member and return apvts.getParameter ("bypass") from getBypassParameter(). A plugin whose switch has another id
// (the MM-1 footswitch "effect") passes that id.
namespace vibr
{
inline std::unique_ptr<juce::AudioParameterBool> makeBypassParam()
{
    return std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "bypass", 1 }, "Bypass", false);
}

class BypassLink : private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit BypassLink (juce::AudioProcessorValueTreeState& s, const juce::String& onId = "power") : apvts (s), powerId (onId)
    {
        apvts.addParameterListener (powerId, this);
        apvts.addParameterListener ("bypass", this);
    }
    ~BypassLink() override
    {
        apvts.removeParameterListener (powerId, this);
        apvts.removeParameterListener ("bypass", this);
    }

    // Call on a saved state tree before replaceState (setStateInformation): "power" (the id saved in every session) is the truth, the
    // "bypass" entry is rebuilt from it. This covers sessions saved before the bypass parameter existed and a bypass entry without a value.
    static void normalise (juce::ValueTree& state, const juce::String& onId = "power")
    {
        float on = 1.0f;
        for (int i = 0; i < state.getNumChildren(); ++i)
            if (state.getChild (i).getProperty ("id").toString() == onId) on = (float) state.getChild (i).getProperty ("value", 1.0f);
        for (int i = state.getNumChildren(); --i >= 0;)
            if (state.getChild (i).getProperty ("id").toString() == "bypass") state.removeChild (i, nullptr);
        juce::ValueTree b ("PARAM");
        b.setProperty ("id", "bypass", nullptr);
        b.setProperty ("value", on >= 0.5f ? 0.0f : 1.0f, nullptr);
        state.appendChild (b, nullptr);
    }

private:
    void parameterChanged (const juce::String& id, float value) override
    {
        const bool fromBypass = id == "bypass";
        auto* other = apvts.getParameter (fromBypass ? powerId : juce::String ("bypass"));
        if (other == nullptr) return;
        const float target = value >= 0.5f ? 0.0f : 1.0f;                // the other parameter holds the opposite value
        if (std::abs (other->getValue() - target) > 0.5f) other->setValueNotifyingHost (target);      // equal values end the ping-pong
    }

    juce::AudioProcessorValueTreeState& apvts;
    juce::String powerId;
};
} // namespace vibr
