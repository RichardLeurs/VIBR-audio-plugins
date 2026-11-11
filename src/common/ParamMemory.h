// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

// Four panel memories ("MANUAL" + slots 1-4) shared by plugins that have the MEMORY strip.
//
// A slot is a snapshot of every parameter of the processor except the ones in `skipIds` (for example "power").
// The snapshots live in the AudioProcessorValueTreeState's state (child "memory"), so they are saved and restored with the
// plugin state and with the host session. Everything here runs on the message thread (editor), never on the audio thread.
//
//   current()      0 = MANUAL (the knobs are live), 1-4 = the selected slot
//   select (n)     select a slot; recalls its snapshot when it has one
//   stepNext()     MANUAL -> 1 -> 2 -> 3 -> 4 -> MANUAL (short press of the MEMORY button)
//   write()        store the current knob positions in the selected slot (hold the MEMORY button; MANUAL writes slot 1)
//   revertIfEdited()  a knob moved after a recall: go back to MANUAL
#include <JuceHeader.h>

class ParamMemory
{
public:
    static constexpr int kSlots = 4;

    ParamMemory (juce::AudioProcessorValueTreeState& s, juce::StringArray skip) : apvts (s), skipIds (std::move (skip)) {}

    int current() const { return juce::jlimit (0, kSlots, (int) node().getProperty ("current", 0)); }
    bool hasSnapshot (int slot) const { return slot >= 1 && node().getChildWithName (slotName (slot)).isValid(); }

    void select (int slot)
    {
        slot = juce::jlimit (0, kSlots, slot);
        node().setProperty ("current", slot, nullptr);
        if (hasSnapshot (slot))
            recall (slot);
    }

    void stepNext() { select ((current() + 1) % (kSlots + 1)); }

    void write()
    {
        const int slot = current() == 0 ? 1 : current();
        juce::ValueTree snap (slotName (slot));
        forEachParam ([&] (juce::AudioProcessorParameterWithID& p) { snap.setProperty (p.paramID, p.getValue(), nullptr); });
        auto n = node();
        n.removeChild (n.getChildWithName (slotName (slot)), nullptr);
        n.addChild (snap, -1, nullptr);
        n.setProperty ("current", slot, nullptr);
    }

    // true if the selected slot has a snapshot and the knobs no longer match it
    bool edited() const
    {
        const int slot = current();
        if (! hasSnapshot (slot))
            return false;
        const auto snap = node().getChildWithName (slotName (slot));
        bool differs = false;
        forEachParam ([&] (juce::AudioProcessorParameterWithID& p)
        {
            if (snap.hasProperty (p.paramID) && std::abs ((float) snap[p.paramID] - p.getValue()) > 0.002f)
                differs = true;
        });
        return differs;
    }

    void revertIfEdited()
    {
        if (edited())
            node().setProperty ("current", 0, nullptr);
    }

private:
    juce::ValueTree node() const { return apvts.state.getOrCreateChildWithName ("memory", nullptr); }
    static juce::Identifier slotName (int slot) { return juce::Identifier ("slot" + juce::String (slot)); }

    void recall (int slot)
    {
        const auto snap = node().getChildWithName (slotName (slot));
        forEachParam ([&] (juce::AudioProcessorParameterWithID& p)
        {
            if (snap.hasProperty (p.paramID))
            {
                p.beginChangeGesture();
                p.setValueNotifyingHost ((float) snap[p.paramID]);
                p.endChangeGesture();
            }
        });
    }

    template <typename Fn>
    void forEachParam (Fn&& fn) const
    {
        for (auto* p : apvts.processor.getParameters())
            if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
                if (! skipIds.contains (withId->paramID))
                    fn (*withId);
    }

    juce::AudioProcessorValueTreeState& apvts;
    juce::StringArray skipIds;
};
