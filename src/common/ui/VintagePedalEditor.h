// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include <JuceHeader.h>
#include "VintageUI.h"

namespace vibr::ui::vintage
{
// Base of the pedal editors in the default (Vintage Hardware) look: a resizable window with a fixed aspect ratio drawn in code in a design space (`designW` x `designH`,
// 1120 x 660 by default), the bakelite knob look-and-feel, and a click on the power switch (`powerRect()`) = the bypass parameter. The pedal keeps its sliders, its panels
// and its timer. Paint with `g.addTransform (juce::AffineTransform::scale (scale()))`, then `ui::paintBody`, `ui::paintHeaderNoPresets`, panels, knobs, `ui::paintFooter`.
// The old base class (rendered background image) is vibr::ui::PedalEditor in PedalEditor.h, kept as the backup.
class PedalEditor : public juce::AudioProcessorEditor
{
public:
    explicit PedalEditor (juce::AudioProcessor& p, float designW = kW, float designH = kH, int minWidth = 560)
        : AudioProcessorEditor (&p), dw (designW), dh (designH)
    {
        setLookAndFeel (&laf);
        constrainer.setSizeLimits (minWidth, (int) std::lround ((float) minWidth * dh / dw), 2400, (int) std::lround (2400.0f * dh / dw));
        constrainer.setFixedAspectRatio ((double) dw / (double) dh);
        setConstrainer (&constrainer);
        setResizable (true, true);
        setSize ((int) dw, (int) dh);
    }
    ~PedalEditor() override { setLookAndFeel (nullptr); }

    float scale() const { return (float) getWidth() / dw; }

    bool powerOn() const
    {
        auto* prm = getAudioProcessor()->getBypassParameter();
        return prm == nullptr || prm->getValue() < 0.5f;                     // the host bypass parameter is 1 when bypassed
    }
    // a click (in window pixels) on the power switch toggles the bypass parameter; true if it hit
    bool toggleBypassAt (juce::Point<float> position) const
    {
        if (! powerRect().expanded (3.0f).contains (position / scale())) return false;
        auto* prm = getAudioProcessor()->getBypassParameter();
        if (prm == nullptr) return false;
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->getValue() > 0.5f ? 0.0f : 1.0f);
        prm->endChangeGesture();
        return true;
    }
    // the slider square around a knob centre given in design units
    void placeKnob (juce::Slider& s, juce::Point<float> centre, float knobR = kKnobR) const
    {
        const float d = 2.0f * (knobR + kKnobPad) * scale();
        s.setBounds (juce::Rectangle<float> (d, d).withCentre (centre * scale()).toNearestInt());
    }
    void repaintDesign (juce::Rectangle<float> r) { repaint (r.transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer()); }

protected:
    const float dw, dh;
    KnobLookAndFeel laf;
    juce::ComponentBoundsConstrainer constrainer;
};
}
