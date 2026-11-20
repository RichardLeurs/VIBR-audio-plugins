// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once
#include <JuceHeader.h>
#include "../ui/Presets.h"

// Preset support for the pedals (which keep their own AudioProcessor): the preset manager plus the factory presets as the host's programs, the same as vibr::LabProcessor
// does for the Labs. How a pedal processor uses it:
//   juce::UndoManager undoManager { 30000, 100 };                      // declared BEFORE apvts, and apvts is constructed with  &undoManager
//   juce::AudioProcessorValueTreeState apvts;
//   vibr::PedalPresets presets { *this, apvts, undoManager, "OD-2 Presets", factoryPresets(), { "bypass" } };      // declared AFTER apvts
//   int getNumPrograms() override { return presets.numPrograms(); }   int getCurrentProgram() override { return presets.currentProgram(); }
//   void setCurrentProgram (int i) override { presets.setProgram (i); }  const juce::String getProgramName (int i) override { return presets.programName (i); }
// The preset name is stored in the state tree (property "presetName"), so it is saved with the session. A preset never touches the ids in `neverReset` (the bypass).
namespace vibr
{
class PedalPresets : public PresetManager,
                     private juce::AsyncUpdater
{
public:
    PedalPresets (juce::AudioProcessor& p, juce::AudioProcessorValueTreeState& s, juce::UndoManager& u, const juce::String& folderName,
                  std::vector<Factory> factoryList, juce::StringArray neverReset = { "bypass" })
        : PresetManager (p, s, u, folderName, std::move (factoryList), std::move (neverReset)) {}
    ~PedalPresets() override { cancelPendingUpdate(); }

    int numPrograms() const { return juce::jmax (1, (int) factory().size()); }
    int currentProgram() const { return program.load(); }
    juce::String programName (int index) const
    {
        const auto& list = factory();
        return index >= 0 && index < (int) list.size() ? list[(size_t) index].name : juce::String();
    }
    void setProgram (int index)
    {
        index = juce::jlimit (0, numPrograms() - 1, index);
        if (index == program.load()) return;                // a host re-sending the current program must not reset the user's edits
        program.store (index);
        if (juce::MessageManager::getInstance()->isThisTheMessageThread()) applyFactory (index);
        else { pending.store (index); triggerAsyncUpdate(); }
    }

private:
    void handleAsyncUpdate() override { const int i = pending.exchange (-1); if (i >= 0) applyFactory (i); }
    std::atomic<int> program { 0 }, pending { -1 };
};
} // namespace vibr
