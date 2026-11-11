// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include <JuceHeader.h>

// Slider that can turn endlessly round a stepped selector: dragging past the last position continues at the first (and back).
class WrapSlider : public juce::Slider
{
public:
    int wrapSteps = 0;                    // > 0: number of positions that wrap round (0 = normal slider)

    void mouseDown (const juce::MouseEvent& e) override
    {
        Slider::mouseDown (e);
        startValue = getValue();
        startY = e.position.y;
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (wrapSteps <= 0) { Slider::mouseDrag (e); return; }
        constexpr float pixelsPerStep = 22.0f;                                   // one drag of ~260 px goes all the way round
        double v = startValue + (double) (startY - e.position.y) / pixelsPerStep;
        v = std::fmod (v, (double) wrapSteps);
        if (v < 0.0) v += (double) wrapSteps;
        setValue (std::floor (v + 0.5), juce::sendNotificationSync);
    }

private:
    double startValue = 0.0;
    float startY = 0.0f;
};
