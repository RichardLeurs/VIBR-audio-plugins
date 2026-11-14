// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once
#include "VibrUI.h"

// Factory and user presets, A / B slots and undo / redo for a plugin built on an AudioProcessorValueTreeState, and the header bar that shows them (the preset chip,
// A, B, copy, UNDO, REDO). Moved out of the MS-1 so the Labs share it: the plugin owns an UndoManager (passed to the APVTS), a PresetManager and calls
// presetBarMouseDown / paintPresetBar from its editor.
namespace vibr
{
class PresetManager
{
public:
    struct Factory { juce::String name, note; std::vector<std::pair<juce::String, float>> values; };      // values are plain (host) values

    // folderName: the user presets live in Documents / VIBR / <folderName>. neverReset: parameter ids a preset does not touch (power, monitoring ...).
    // keepProps: properties of the state tree a preset or an A / B switch does not carry (the MS-1's reference path).
    PresetManager (juce::AudioProcessor& p, juce::AudioProcessorValueTreeState& s, juce::UndoManager& u, juce::String folderName, std::vector<Factory> factory,
                   juce::StringArray neverReset = {}, juce::StringArray keepProps = {})
        : proc (p), apvts (s), undo (u), folder (std::move (folderName)), presets (std::move (factory)), keepIds (std::move (neverReset)), keepProperties (std::move (keepProps)) {}

    const std::vector<Factory>& factory() const { return presets; }
    juce::File folderFile() const { return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("VIBR").getChildFile (folder); }
    juce::String name() const { return apvts.state.getProperty ("presetName", presets.empty() ? juce::String ("Init") : presets.front().name).toString(); }
    int abSlot() const { return abCurrent; }

    // every parameter back to its default (but the ones in neverReset)
    void resetToDefaults()
    {
        for (auto* par : proc.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (par))
            {
                if (keepIds.contains (rp->paramID)) continue;
                rp->beginChangeGesture(); rp->setValueNotifyingHost (rp->getDefaultValue()); rp->endChangeGesture();
            }
    }

    void applyFactory (int index)
    {
        if (index < 0 || index >= (int) presets.size()) return;
        undo.beginNewTransaction ("Preset " + presets[(size_t) index].name);
        resetToDefaults();
        for (const auto& [id, v] : presets[(size_t) index].values)
            if (auto* par = apvts.getParameter (id)) { par->beginChangeGesture(); par->setValueNotifyingHost (par->convertTo0to1 (v)); par->endChangeGesture(); }
        apvts.state.setProperty ("presetName", presets[(size_t) index].name, nullptr);
    }

    bool saveUser (const juce::String& nameIn)
    {
        const auto clean = juce::File::createLegalFileName (nameIn.trim());
        if (clean.isEmpty()) return false;
        auto dir = folderFile();
        if (! dir.createDirectory()) return false;
        apvts.state.setProperty ("presetName", clean, nullptr);
        if (auto xml = apvts.copyState().createXml()) return xml->writeTo (dir.getChildFile (clean + ".xml"));
        return false;
    }

    bool loadUser (const juce::File& file)
    {
        auto xml = juce::XmlDocument::parse (file);
        if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return false;
        undo.beginNewTransaction ("Preset " + file.getFileNameWithoutExtension());
        auto tree = juce::ValueTree::fromXml (*xml);
        for (const auto& p : keepProperties) tree.removeProperty (p, nullptr);
        resetToDefaults();
        apvts.replaceState (tree);
        apvts.state.setProperty ("presetName", file.getFileNameWithoutExtension(), nullptr);
        return true;
    }

    // 0 = A, 1 = B: the current settings go to the slot you leave
    void switchAB (int slot)
    {
        slot = juce::jlimit (0, 1, slot);
        if (slot == abCurrent) return;
        abState[abCurrent] = apvts.copyState();
        abCurrent = slot;
        if (abState[slot].isValid())
        {
            undo.beginNewTransaction ("A / B");
            auto tree = abState[slot].createCopy();
            for (const auto& p : keepProperties) tree.setProperty (p, apvts.state.getProperty (p), nullptr);
            apvts.replaceState (tree);
        }
    }
    void copyToOtherSlot() { abState[1 - abCurrent] = apvts.copyState(); }

private:
    juce::AudioProcessor& proc;
    juce::AudioProcessorValueTreeState& apvts;
    juce::UndoManager& undo;
    juce::String folder;
    std::vector<Factory> presets;
    juce::StringArray keepIds, keepProperties;
    juce::ValueTree abState[2];
    int abCurrent = 0;
};
} // namespace vibr

namespace vibr::ui
{
// the header bar: the preset chip, A / B / copy and UNDO / REDO, at height y (the top of the 32-high chips; the MS-1 puts it at 62 under its target chip, the Labs at 39)
// The two chips under the name are the same width (kChipW, from x = 210): the MS-1's TARGET chip above and the PRESET chip below. The preset buttons follow the
// chip: A, B, copy (TO B / TO A), UNDO and REDO.
constexpr float kBarX = 210.0f, kChipW = 340.0f;
inline juce::Rectangle<float> presetChipRect (float y) { return { kBarX, y, kChipW, 32.0f }; }
inline juce::Rectangle<float> abRect (int i, float y)
{
    static const float x[3] = { 0.0f, 40.0f, 80.0f }, w[3] = { 36.0f, 36.0f, 44.0f };
    return { kBarX + kChipW + 8.0f + x[i], y, w[i], 32.0f };
}
// showAB false (the Labs): no A / B / copy buttons, UNDO and REDO follow the chip directly
inline juce::Rectangle<float> undoRect (bool redo, float y, bool showAB = true) { return { kBarX + kChipW + 8.0f + (showAB ? (redo ? 188.0f : 132.0f) : (redo ? 56.0f : 0.0f)), y, 54.0f, 32.0f }; }

// state the bar needs between paints: when the copy button was pressed (it shows COPIED for a moment, the editor's timer repaints)
struct PresetBarState { juce::uint32 copiedAt = 0; bool copiedRecently() const { return juce::Time::getMillisecondCounter() - copiedAt < 1200u && copiedAt != 0; } };

inline void paintPresetBar (juce::Graphics& g, const Palette& pal, const vibr::PresetManager& mgr, const juce::UndoManager& undo, float y, const PresetBarState& state = {}, bool showAB = true)
{
    const auto chip = presetChipRect (y);
    g.setColour (pal.panel);
    g.fillRoundedRectangle (chip, 12.0f);
    g.setColour (pal.dark);
    g.drawRoundedRectangle (chip.reduced (0.9f), 12.0f, 1.8f);
    drawText (g, "PRESET", chip.getX() + 14.0f, chip.getCentreY(), 11.0f, pal.ink.withAlpha (0.5f), true, 0.2f, juce::Justification::centredLeft);
    drawText (g, mgr.name().toUpperCase(), chip.getX() + 78.0f, chip.getCentreY(), 13.0f, pal.body, true, 0.06f, juce::Justification::centredLeft);
    const int slot = mgr.abSlot();
    for (int i = 0; i < (showAB ? 3 : 0); ++i)
    {
        const auto b = abRect (i, y);
        const bool selected = i < 2 && slot == i;                                          // the selected slot is a lit pill: the plugin colour with a dark border
        const bool flash = i == 2 && state.copiedRecently();
        if (selected || flash)
        {
            g.setColour (pal.body);
            g.fillRoundedRectangle (b, 10.0f);
            g.setColour (pal.dark);
            g.drawRoundedRectangle (b.reduced (0.9f), 10.0f, 1.8f);
        }
        else
        {
            g.setColour (pal.panel);
            g.fillRoundedRectangle (b, 10.0f);
            g.setColour (pal.dark);
            g.drawRoundedRectangle (b.reduced (0.9f), 10.0f, 1.6f);
        }
        drawText (g, i == 0 ? "A" : i == 1 ? "B" : flash ? "COPIED" : (slot == 0 ? "TO B" : "TO A"), b.getCentreX(), b.getCentreY(), i < 2 ? 14.0f : flash ? 9.0f : 11.0f,
                  (selected || flash) ? pal.dark : pal.ink.withAlpha (0.8f), true, 0.08f);
    }
    for (int redo = 0; redo < 2; ++redo)
    {
        const auto b = undoRect (redo != 0, y, showAB);
        const bool can = redo ? undo.canRedo() : undo.canUndo();
        g.setColour (pal.panel);
        g.fillRoundedRectangle (b, 10.0f);
        g.setColour (pal.dark);
        g.drawRoundedRectangle (b.reduced (0.9f), 10.0f, 1.6f);
        drawText (g, redo ? "REDO" : "UNDO", b.getCentreX(), b.getCentreY(), 11.0f, pal.ink.withAlpha (can ? 0.9f : 0.3f), true, 0.1f);
    }
}

// The preset menu: factory presets, my presets, save as, show the folder.
inline void showPresetMenu (juce::Component& owner, vibr::PresetManager& mgr, float y, float scale, std::function<void()> done)
{
    juce::PopupMenu menu, factory, user;
    const auto& list = mgr.factory();
    for (int i = 0; i < (int) list.size(); ++i) factory.addItem (1000 + i, list[(size_t) i].name + (list[(size_t) i].note.isEmpty() ? juce::String() : "  -  " + list[(size_t) i].note), true, mgr.name() == list[(size_t) i].name);
    auto files = mgr.folderFile().findChildFiles (juce::File::findFiles, false, "*.xml");
    files.sort();
    for (int i = 0; i < files.size(); ++i) user.addItem (2000 + i, files[i].getFileNameWithoutExtension(), true, mgr.name() == files[i].getFileNameWithoutExtension());
    menu.addSubMenu ("Factory presets", factory);
    menu.addSubMenu ("My presets", user, files.size() > 0);
    menu.addSeparator();
    menu.addItem (1, "Save preset as...");
    menu.addItem (2, "Show the presets folder");
    juce::Component::SafePointer<juce::Component> safe (&owner);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&owner).withTargetScreenArea (presetChipRect (y).transformedBy (juce::AffineTransform::scale (scale)).toNearestInt().translated (owner.getScreenX(), owner.getScreenY())),
                        [safe, &mgr, files, done] (int result)
                        {
                            if (safe == nullptr) return;
                            if (result >= 2000) mgr.loadUser (files[result - 2000]);
                            else if (result >= 1000) mgr.applyFactory (result - 1000);
                            else if (result == 1)
                            {
                                auto* w = new juce::AlertWindow ("Save preset", "Name of the preset (saved in Documents / VIBR / " + mgr.folderFile().getFileName() + "):", juce::MessageBoxIconType::NoIcon, safe.getComponent());
                                w->addTextEditor ("name", mgr.name(), "Name");
                                w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
                                w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
                                w->enterModalState (true, juce::ModalCallbackFunction::create ([&mgr, w, done] (int r) { if (r == 1) mgr.saveUser (w->getTextEditorContents ("name")); done(); }), true);
                            }
                            else if (result == 2) { auto f = mgr.folderFile(); f.createDirectory(); f.revealToUser(); }
                            done();
                        });
}

// a click at `p` (layout units): true when the bar took it
inline bool presetBarMouseDown (juce::Point<float> p, juce::Component& owner, vibr::PresetManager& mgr, juce::UndoManager& undo, float y, float scale, PresetBarState& state,
                                std::function<void()> done, bool showAB = true)
{
    if (presetChipRect (y).contains (p)) { showPresetMenu (owner, mgr, y, scale, done); return true; }
    for (int i = 0; i < (showAB ? 3 : 0); ++i)
        if (abRect (i, y).contains (p)) { if (i < 2) mgr.switchAB (i); else { mgr.copyToOtherSlot(); state.copiedAt = juce::Time::getMillisecondCounter(); } done(); return true; }
    if (undoRect (false, y, showAB).contains (p)) { undo.undo(); done(); return true; }
    if (undoRect (true, y, showAB).contains (p)) { undo.redo(); done(); return true; }
    return false;
}
} // namespace vibr::ui
