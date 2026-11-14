// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>

// MIDI CC -> parameter map with MIDI learn, for audio effects that only read MIDI (the buffer is never changed).
//   - Audio thread: process (midi) reads the CC messages and writes plain atomics (no allocation, no locks).
//   - Message thread: a 60 Hz timer applies the new values to the parameters with a change gesture, so the host
//     records them as automation and the editor follows. It runs without an editor, so it also works with the window closed.
//   - The bindings live in a MIDIMAP child of the plugin state (not in presets): save with toTree(), restore with fromTree().
class MidiMap : private juce::Timer
{
public:
    static constexpr int kMaxSlots = 16;

    // ids: the parameters that can be bound (one slot each, at most 16)
    MidiMap (juce::AudioProcessorValueTreeState& s, std::vector<juce::String> ids) : apvts (s), paramIds (std::move (ids))
    {
        jassert ((int) paramIds.size() <= kMaxSlots);
        for (auto& c : cc) c.store (-1);
        for (auto& p : pending) p.store (-1.0f);
        startTimerHz (60);
    }

    ~MidiMap() override { stopTimer(); }

    // while sync is on, the rate knob edits the note division: a CC bound to rateId then moves divisionId instead
    void setSyncAlias (const juce::String& rateId, const juce::String& syncId, const juce::String& divisionId)
    {
        aliasSlot = indexOf (rateId);
        syncParam = apvts.getParameter (syncId);
        divisionParam = apvts.getParameter (divisionId);
    }

    void setDefault (const juce::String& id, int ccNumber)       // factory binding (any channel); replaced by a saved map
    {
        if (const int i = indexOf (id); i >= 0) cc[(size_t) i].store (ccNumber);
    }

    int numSlots() const noexcept { return (int) paramIds.size(); }
    int indexOf (const juce::String& id) const noexcept
    {
        for (size_t i = 0; i < paramIds.size(); ++i) if (paramIds[i] == id) return (int) i;
        return -1;
    }

    int  getCC (int slot) const noexcept        { return slot >= 0 ? cc[(size_t) slot].load() : -1; }
    int  getLearning() const noexcept           { return learnSlot.load(); }
    bool isSoftTakeover() const noexcept        { return takeover.load(); }
    void setSoftTakeover (bool b) noexcept      { takeover.store (b); for (auto& p : picked) p.store (false); }
    void startLearn (int slot) noexcept         { learnSlot.store (getLearning() == slot ? -1 : slot); }
    void stopLearn() noexcept                   { learnSlot.store (-1); }
    void clear (int slot) noexcept              { if (slot >= 0) { cc[(size_t) slot].store (-1); if (getLearning() == slot) stopLearn(); } }

    // tap tempo (DM-1, RE-2, MM-1, SD-1, DD-1, DL-1): enableTap() once; the plugin calls consumeTap() each block
    // (audio thread) and feeds its own tap function when it returns true. cc = -1: notes only.
    void enableTap (int ccNumber = -1) noexcept { tapEnabled.store (true); tapCC.store (ccNumber); }
    bool consumeTap() noexcept                   { return tapPending.exchange (false); }

    // audio thread
    void process (const juce::MidiBuffer& midi) noexcept
    {
        for (const auto meta : midi)
        {
            const auto m = meta.getMessage();
            if (tapEnabled.load() && m.isNoteOn()) { tapPending.store (true); continue; }              // any note-on taps the tempo
            if (m.isController() && m.getControllerNumber() == tapCC.load() && m.getControllerValue() >= 64)
            {
                if (! tapHigh.exchange (true)) tapPending.store (true);                                // rising edge only
                continue;
            }
            if (m.isController() && m.getControllerNumber() == tapCC.load()) { tapHigh.store (false); continue; }
            if (! m.isController()) continue;
            const int num = m.getControllerNumber();
            if (num >= 120) continue;                                    // channel mode messages (all notes off ...)

            if (const int l = learnSlot.load(); l >= 0)
            {
                for (int i = 0; i < numSlots(); ++i)                     // a CC drives one parameter only
                    if (cc[(size_t) i].load() == num) cc[(size_t) i].store (-1);
                cc[(size_t) l].store (num);
                picked[(size_t) l].store (false);
                learnSlot.store (-1);
            }

            for (int i = 0; i < numSlots(); ++i)
                if (cc[(size_t) i].load() == num)
                    pending[(size_t) i].store ((float) m.getControllerValue() / 127.0f);
        }
    }

    // state
    juce::ValueTree toTree() const
    {
        juce::ValueTree t ("MIDIMAP");
        t.setProperty ("takeover", takeover.load(), nullptr);
        for (int i = 0; i < numSlots(); ++i)
        {
            juce::ValueTree n ("MAP");
            n.setProperty ("param", paramIds[(size_t) i], nullptr);
            n.setProperty ("cc", cc[(size_t) i].load(), nullptr);
            t.appendChild (n, nullptr);
        }
        return t;
    }

    void fromTree (const juce::ValueTree& t)
    {
        if (! t.isValid()) return;                                       // older session: keep the factory map
        takeover.store ((bool) t.getProperty ("takeover", false));
        for (auto n : t)
            if (const int i = indexOf (n.getProperty ("param").toString()); i >= 0)
                { cc[(size_t) i].store ((int) n.getProperty ("cc", -1)); picked[(size_t) i].store (false); }
    }

private:
    void timerCallback() override
    {
        for (int i = 0; i < numSlots(); ++i)
        {
            const float v = pending[(size_t) i].exchange (-1.0f);
            if (v < 0.0f) continue;

            auto* p = apvts.getParameter (paramIds[(size_t) i]);
            if (i == aliasSlot && syncParam != nullptr && divisionParam != nullptr && syncParam->getValue() > 0.5f)
                p = divisionParam;
            if (p == nullptr) continue;

            const float now = p->getValue();
            if (takeover.load() && ! picked[(size_t) i].load())          // wait until the controller passes the knob
            {
                const float last = lastIn[(size_t) i];
                lastIn[(size_t) i] = v;
                if (std::abs (v - now) > 0.04f && (last < 0.0f || (v - now) * (last - now) > 0.0f)) continue;
                picked[(size_t) i].store (true);
            }

            if (std::abs (v - now) < 1.0e-4f) continue;
            p->beginChangeGesture();
            p->setValueNotifyingHost (v);
            p->endChangeGesture();
        }
    }

    juce::AudioProcessorValueTreeState& apvts;
    std::vector<juce::String> paramIds;
    std::array<std::atomic<int>, kMaxSlots> cc;
    std::array<std::atomic<float>, kMaxSlots> pending;
    std::array<std::atomic<bool>, kMaxSlots> picked {};
    std::array<float, kMaxSlots> lastIn { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
    std::atomic<int> learnSlot { -1 };
    std::atomic<bool> takeover { false }, tapEnabled { false }, tapPending { false }, tapHigh { false };
    std::atomic<int> tapCC { -1 };
    int aliasSlot = -1;
    juce::RangedAudioParameter *syncParam = nullptr, *divisionParam = nullptr;
};

// Right-click menu on a knob (MIDI learn, clear, soft takeover) and the marks painted over the knobs:
// a pulsing ring while the knob waits for a CC, a small flat dot (the plugin's body colour) under the knob when a CC is bound.
class MidiLearnUI : private juce::MouseListener
{
public:
    explicit MidiLearnUI (MidiMap& m) : map (m) {}
    ~MidiLearnUI() override { for (auto& k : knobs) if (k.comp != nullptr) k.comp->removeMouseListener (this); }   // comp is a SafePointer: safe even if the knobs are destroyed first

    void add (juce::Component& knob, const juce::String& paramId)
    {
        knobs.push_back ({ juce::Component::SafePointer<juce::Component> (&knob), map.indexOf (paramId) });
        knob.addMouseListener (this, false);
    }

    // call from the editor's paintOverChildren(); colour = the plugin's accent
    void paintMarks (juce::Graphics& g, juce::Colour accent) const
    {
        const float pulse = 0.5f + 0.5f * std::sin ((float) juce::Time::getMillisecondCounter() * 0.012f);
        for (const auto& k : knobs)
        {
            if (k.comp == nullptr || k.slot < 0) continue;
            const auto b = k.comp->getBounds().toFloat();
            if (map.getLearning() == k.slot)
            {
                g.setColour (accent.withAlpha (0.35f + 0.6f * pulse));
                g.drawEllipse (b.reduced (b.getWidth() * 0.02f), 4.0f);
            }
            else if (map.getCC (k.slot) >= 0)
            {
                // centred under the knob at 6 o'clock, in the gap between the minimum and the maximum of the scale
                const auto dot = juce::Rectangle<float> (14.0f, 14.0f).withCentre ({ b.getCentreX(), b.getBottom() + 10.0f });
                g.setColour (accent);
                g.fillEllipse (dot);
            }
        }
    }

    bool needsRepaint() const noexcept { return map.getLearning() >= 0 || learnWas != map.getLearning(); }
    void tick() noexcept { learnWas = map.getLearning(); }

private:
    struct K { juce::Component::SafePointer<juce::Component> comp; int slot; };

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! e.mods.isPopupMenu()) return;
        for (const auto& k : knobs)
            if (k.comp.getComponent() == e.eventComponent && k.slot >= 0) { showMenu (*k.comp, k.slot); return; }
    }

    void showMenu (juce::Component& c, int slot)
    {
        const int cc = map.getCC (slot);
        juce::PopupMenu m;
        m.addItem (1, "MIDI learn", true, map.getLearning() == slot);
        m.addItem (2, cc >= 0 ? "Clear MIDI (CC " + juce::String (cc) + ")" : juce::String ("Clear MIDI"), cc >= 0);
        m.addSeparator();
        m.addItem (3, "Soft takeover", true, map.isSoftTakeover());
        juce::Component::SafePointer<juce::Component> sp (&c);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&c), [this, sp, slot] (int r)
        {
            if (sp == nullptr) return;                                   // the editor was closed while the menu was open
            if (r == 1) map.startLearn (slot);
            else if (r == 2) map.clear (slot);
            else if (r == 3) map.setSoftTakeover (! map.isSoftTakeover());
            if (auto* parent = sp->getParentComponent()) parent->repaint();
        });
    }

    MidiMap& map;
    std::vector<K> knobs;
    int learnWas = -1;
};
