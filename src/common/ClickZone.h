// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include <JuceHeader.h>

// Invisible click target; the editor paints the hardware under it.
class ClickZone : public juce::Component
{
public:
    std::function<void()> onDown;    // mouse pressed
    std::function<void()> onClick;   // mouse released inside
    std::function<void()> onUp;      // mouse released anywhere
    bool isDown() const { return down; }

    void mouseDown (const juce::MouseEvent&) override { down = true; if (onDown) onDown(); if (auto* p = getParentComponent()) p->repaint(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        down = false;
        if (onUp) onUp();
        if (onClick && contains (e.getPosition().toFloat())) onClick();
        if (auto* p = getParentComponent()) p->repaint();
    }

private:
    bool down = false;
};
