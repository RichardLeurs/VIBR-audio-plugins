// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once
#include <JuceHeader.h>

// What the vintage pedal editors share: the black knurled knob with a raised cap and a white pointer (the look of the DM-1 and 14 more pedals), drawn over the
// scales printed in the background image. Each pedal aliases it as its own `XX1LookAndFeel`. The drawing must stay pixel identical (the pedals are frozen).
namespace vibr::ui
{
class PedalKnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        constexpr float kPedalKnobPad = 3.0f;
        const auto polar = [] (juce::Point<float> c, float r, float a) { return juce::Point<float> { c.x + r * std::sin (a), c.y - r * std::cos (a) }; };
        const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        const auto c = b.getCentre();
        const float R = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f - kPedalKnobPad;
        const double lo = s.getMinimum(), hi = s.getMaximum();
        const float pos = (float) ((s.getValue() - lo) / (hi - lo));
        const float angle = startAngle + pos * (endAngle - startAngle);
        const auto circle = [&] (float r) { return juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c); };

        // soft drop shadow
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillEllipse (circle (R).translated (R * 0.05f, R * 0.09f));

        // matt black body with a knurled grip edge
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4c51), c.x - R * 0.7f, c.y - R * 0.8f,
                                                 juce::Colour (0xff050506), c.x + R * 0.7f, c.y + R * 0.9f, false));
        g.fillEllipse (circle (R));
        constexpr int ridges = 60;
        for (int i = 0; i < ridges; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * (float) i / (float) ridges;
            g.setColour (juce::Colours::white.withAlpha (0.10f + 0.10f * std::max (0.0f, std::cos (a + 0.8f))));
            g.drawLine (polar (c, R * 0.90f, a).x, polar (c, R * 0.90f, a).y, polar (c, R * 0.99f, a).x, polar (c, R * 0.99f, a).y, 1.0f);
        }
        g.setColour (juce::Colours::black);
        g.drawEllipse (circle (R), 1.2f);

        // raised cap
        const float capR = R * 0.80f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3b3f), c.x - capR * 0.6f, c.y - capR * 0.7f,
                                                 juce::Colour (0xff0b0b0c), c.x + capR * 0.6f, c.y + capR * 0.8f, false));
        g.fillEllipse (circle (capR));
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.14f), c.x - capR * 0.5f, c.y - capR * 0.6f,
                                                 juce::Colours::transparentWhite, c.x + capR * 0.1f, c.y + capR * 0.1f, true));
        g.fillEllipse (circle (capR));
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.35f), c.x - capR, c.y - capR,
                                                 juce::Colour (0xff000000), c.x + capR, c.y + capR, false));
        g.drawEllipse (circle (capR), 1.2f);

        // pointer: white bar from the rim toward the centre
        juce::Path p;
        p.addRoundedRectangle (-capR * 0.055f, -capR * 0.94f, capR * 0.11f, capR * 0.50f, capR * 0.05f);
        g.setColour (juce::Colour (0xfff2f2ee));
        g.fillPath (p, juce::AffineTransform::rotation (angle).translated (c));
    }
};

// the power slide switch printed on the pedal's top right: a chrome ball (left = off, right = on); the slot lights up green when on. `led` is the switch centre
// in image pixels (the caller has already scaled the graphics to the background image).
inline void paintSlideSwitch (juce::Graphics& g, juce::Point<float> led, bool on)
{
    if (on)
    {
        const auto slot = juce::Rectangle<float> (66.0f, 34.0f).withCentre (led);
        g.setColour (juce::Colour (0xff40d070).withAlpha (0.05f));
        g.fillRoundedRectangle (slot.expanded (16.0f), 30.0f);
        g.setColour (juce::Colour (0xff40d070).withAlpha (0.08f));
        g.fillRoundedRectangle (slot.expanded (8.0f), 24.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2f8a4c), slot.getX(), slot.getY(),
                                                 juce::Colour (0xff17512b), slot.getX(), slot.getBottom(), false));
        g.fillRoundedRectangle (slot, 17.0f);
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.fillRoundedRectangle (slot.withHeight (slot.getHeight() * 0.40f).reduced (8.0f, 3.0f), 6.0f);
    }
    const juce::Point<float> c (led.x + (on ? 17.0f : -17.0f), led.y);
    const auto circle = [&] (float r) { return juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c); };
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (circle (15.0f).translated (2.0f, 4.0f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffafbfd), c.x - 6.0f, c.y - 8.0f,
                                             juce::Colour (0xff6b6f7a), c.x + 9.0f, c.y + 11.0f, true));
    g.fillEllipse (circle (14.0f));
}
}
