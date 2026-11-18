// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once
#include <JuceHeader.h>
#include "../BypassLink.h"
#include "../MidiMap.h"
#include "../ui/Presets.h"
#include "../ui/OutputMeter.h"

// The processor base of the Labs and the newer experimental plugins (replaces the same 150 lines in every processor): the undo manager, the value tree, the host
// bypass link, the MIDI map, the preset manager, the factory presets as host programs, the output meter and the state save / restore (the value tree plus the
// MIDIMAP child; `addExtraState` / `restoreExtraState` / `afterStateRestored` let a plugin store more, as AL-1 does with its impulse responses).
// The format of the saved state is the one all of these plugins always had, so a session saved before this base class existed loads after it, and the other way round.
// A plugin derives from this class, passes its bus layout, its state tag ("RL1", "ML1", ...), its parameter layout, the name of its preset folder, its factory presets
// and the ids the MIDI map can learn, and keeps its own prepareToPlay / processBlock / editor.
namespace vibr
{
class LabProcessor : public juce::AudioProcessor,
                     private juce::AsyncUpdater
{
public:
    LabProcessor (const BusesProperties& buses, const juce::String& stateTag, juce::AudioProcessorValueTreeState::ParameterLayout layout, const juce::String& presetFolder,
                  std::vector<PresetManager::Factory> factory, std::vector<juce::String> midiIds, juce::StringArray neverReset = { "power", "bypass" })
        : AudioProcessor (buses),
          apvts (*this, &undoManager, stateTag, std::move (layout)),
          midiMap (apvts, std::move (midiIds)),
          presets (*this, apvts, undoManager, presetFolder, std::move (factory), std::move (neverReset))
    {
    }
    ~LabProcessor() override { cancelPendingUpdate(); }

    juce::AudioProcessorParameter* getBypassParameter() const override { return apvts.getParameter ("bypass"); }

    // the factory presets are the host's programs (the preset menu in the editor stays in step through the preset name)
    int getNumPrograms() override { return juce::jmax (1, (int) presets.factory().size()); }
    int getCurrentProgram() override { return currentProgram.load(); }
    void setCurrentProgram (int index) override
    {
        index = juce::jlimit (0, getNumPrograms() - 1, index);
        if (index == currentProgram.load()) return;   // a host re-sending the current program must not reset the user's edits
        currentProgram.store (index);
        if (juce::MessageManager::getInstance()->isThisTheMessageThread()) presets.applyFactory (index);
        else { pendingProgram.store (index); triggerAsyncUpdate(); }
    }
    const juce::String getProgramName (int index) override
    {
        const auto& list = presets.factory();
        return index >= 0 && index < (int) list.size() ? list[(size_t) index].name : juce::String();
    }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& dest) override
    {
        auto state = apvts.copyState();
        state.appendChild (midiMap.toTree(), nullptr);
        addExtraState (state);
        if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
    }
    void setStateInformation (const void* data, int size) override
    {
        if (auto xml = getXmlFromBinary (data, size))
            if (xml->hasTagName (apvts.state.getType()))
            {
                auto st = juce::ValueTree::fromXml (*xml);
                midiMap.fromTree (st.getChildWithName ("MIDIMAP"));
                st.removeChild (st.getChildWithName ("MIDIMAP"), nullptr);
                restoreExtraState (st);
                vibr::BypassLink::normalise (st);
                apvts.replaceState (st);
                afterStateRestored();
            }
    }

    juce::UndoManager undoManager { 30000, 100 };
    juce::AudioProcessorValueTreeState apvts;
    vibr::BypassLink bypassLink { apvts };                // host bypass <-> OFF / ON switch
    MidiMap midiMap;                                      // CC -> knob, MIDI learn
    vibr::PresetManager presets;
    std::atomic<int> currentProgram { 0 };
    std::atomic<int> pendingProgram { -1 };
    vibr::ui::PeakTap outMeter;                           // sample peak of the output, read by the editor

protected:
    // extra children of the saved state: add them to `state` when saving, take them out of `state` (and use them) when restoring, before the value tree is replaced
    virtual void addExtraState (juce::ValueTree&) {}
    virtual void restoreExtraState (juce::ValueTree&) {}             // take your children out of the tree (it is then given to the value tree)
    virtual void afterStateRestored() {}                              // the value tree has been replaced: use what restoreExtraState took

private:
    void handleAsyncUpdate() override { const int i = pendingProgram.exchange (-1); if (i >= 0) presets.applyFactory (i); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LabProcessor)
};
} // namespace vibr
