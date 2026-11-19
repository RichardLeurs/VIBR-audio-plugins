// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include <JuceHeader.h>
#include "VintageUI.h"
#include "../plugin/LabProcessor.h"

namespace vibr::ui::vintage
{
// Base of the Lab editors in the default (Vintage Hardware) look: the window (fixed aspect ratio, resizable, scale() = width / design width, 1120 x 660 by default),
// the bakelite knob look-and-feel, the power attachment, the 30 Hz timer and the helpers every Lab editor needs (read a parameter, toggle / set a choice as one host
// gesture, attach and place a knob, repaint a design-space rectangle). The plugin keeps its own panels, table of knobs, paint, mouse handling and timerCallback.
// The old base class (grey knob, satin grain) is vibr::ui::LabEditor in LabEditor.h, kept as the backup.
class LabEditor : public juce::AudioProcessorEditor,
                  protected juce::Timer
{
public:
    LabEditor (vibr::LabProcessor& p, float designW = kW, float designH = kH, int minWidth = 720)
        : LabEditor (p, p.apvts, p.undoManager, designW, designH, minWidth) {}

    // for a processor that is not a LabProcessor (MS-1): the parameter tree and the undo manager are passed on their own
    LabEditor (juce::AudioProcessor& p, juce::AudioProcessorValueTreeState& tree, juce::UndoManager& undoMgr, float designW = kW, float designH = kH, int minWidth = 720)
        : AudioProcessorEditor (&p), apvts (tree), undo (undoMgr), dw (designW), dh (designH),
          powerAtt (*tree.getParameter ("power"), [this] (float) { repaint(); }, nullptr)
    {
        setLookAndFeel (&laf);
        powerAtt.sendInitialUpdate();
        constrainer.setSizeLimits (minWidth, (int) std::lround ((float) minWidth * dh / dw), 2400, (int) std::lround (2400.0f * dh / dw));
        constrainer.setFixedAspectRatio ((double) dw / (double) dh);
        setConstrainer (&constrainer);
        setResizable (true, true);
        setSize ((int) dw, (int) dh);
    }

    ~LabEditor() override { stopTimer(); setLookAndFeel (nullptr); }

    float scale() const { return (float) getWidth() / dw; }

    // parameter access by id
    bool isOn (const char* id) const { return apvts.getRawParameterValue (id)->load() > 0.5f; }
    float value (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    int choice (const char* id) const { return (int) std::lround (value (id)); }
    void toggle (const char* id)
    {
        auto* par = apvts.getParameter (id);
        par->beginChangeGesture(); par->setValueNotifyingHost (par->getValue() > 0.5f ? 0.0f : 1.0f); par->endChangeGesture();
    }
    void setChoice (const char* id, int index)
    {
        auto* par = apvts.getParameter (id);
        par->beginChangeGesture(); par->setValueNotifyingHost (par->convertTo0to1 ((float) index)); par->endChangeGesture();
    }

    // a rotary knob on a parameter: look, double-click default, undo transaction per drag, repaint on change
    void attachKnob (juce::Slider& s, std::unique_ptr<juce::SliderParameterAttachment>& att, const juce::String& id)
    {
        vibr::ui::setupKnob (s, *this);
        auto* par = apvts.getParameter (id);
        s.setDoubleClickReturnValue (true, par->convertFrom0to1 (par->getDefaultValue()));
        s.onDragStart = [this] { undo.beginNewTransaction(); };
        addAndMakeVisible (s);
        att = std::make_unique<juce::SliderParameterAttachment> (*dynamic_cast<juce::RangedAudioParameter*> (par), s, nullptr);
        s.onValueChange = [this] { repaint(); };
    }
    // the slider square around a knob centre given in design units (`knobR` = the black body's radius)
    void placeKnob (juce::Slider& s, juce::Point<float> centre, float knobR = kKnobR) const
    {
        const float d = 2.0f * (knobR + kKnobPad) * scale();
        s.setBounds (juce::Rectangle<float> (d, d).withCentre (centre * scale()).toNearestInt());
    }

    // repaint a rectangle given in design units
    void repaintDesign (juce::Rectangle<float> r) { repaint (r.transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer()); }

protected:
    juce::AudioProcessorValueTreeState& apvts;
    juce::UndoManager& undo;
    const float dw, dh;
    KnobLookAndFeel laf;
    juce::ComponentBoundsConstrainer constrainer;
    juce::ParameterAttachment powerAtt;
};
}
