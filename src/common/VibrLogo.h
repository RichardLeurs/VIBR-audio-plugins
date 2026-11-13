// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once
#include <JuceHeader.h>

// The small VIBR logo (the rainbow V of assets/icon.svg) on a transparent background; `box` is its bounding square.
inline void drawVibrLogo (juce::Graphics& g, juce::Rectangle<float> box, juce::Colour outline)
{
    const float k = box.getWidth() / 59.2f;   // + 1.6 outline each side                               // the V spans 4 .. 60 of the 64 x 64 viewBox (stroke included)
    const auto pt = [&] (float x, float y) { return juce::Point<float> (box.getX() + (x - 2.4f) * k, box.getY() + (y - 2.4f) * k); };
    const auto arm = [&] (juce::Point<float> a, juce::Point<float> b, std::initializer_list<std::pair<float, juce::uint32>> stops)
    {
        juce::ColourGradient grad (juce::Colour (stops.begin()->second), a, juce::Colour (stops.begin()->second), b, false);
        for (auto& s : stops) grad.addColour (s.first, juce::Colour (s.second));
        g.setGradientFill (grad);
        g.strokePath ([&] { juce::Path p; p.startNewSubPath (a); p.lineTo (b); return p; }(),
                      juce::PathStrokeType (12.0f * k, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };
    const auto line = [&] (juce::Point<float> a, juce::Point<float> b)
    {
        juce::Path p; p.startNewSubPath (a); p.lineTo (b);
        g.setColour (outline);
        g.strokePath (p, juce::PathStrokeType (15.2f * k, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };
    line (pt (10, 12), pt (32, 54)); line (pt (32, 54), pt (54, 12));
    arm (pt (10, 12), pt (32, 54), { { 0.0f, 0xffff2d55 }, { 0.34f, 0xffff8a00 }, { 0.67f, 0xffffd60a }, { 1.0f, 0xff30e060 } });
    arm (pt (32, 54), pt (54, 12), { { 0.0f, 0xff30e060 }, { 0.34f, 0xff00d4ff }, { 0.67f, 0xff3d7bff }, { 1.0f, 0xffa259ff } });
}
