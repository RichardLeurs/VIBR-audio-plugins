// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include <JuceHeader.h>

// Invisible click target over a switch drawn in code, so that MidiLearnUI can attach to it (right-click: MIDI learn).
// A normal press calls onPress (the same action the editor ran from its mouseDown); a right-click only reaches the learn menu.
class MidiTarget : public juce::Component
{
public:
    std::function<void()> onPress;
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) return;
        if (onPress) onPress();
        if (auto* p = getParentComponent()) p->repaint();
    }
};
