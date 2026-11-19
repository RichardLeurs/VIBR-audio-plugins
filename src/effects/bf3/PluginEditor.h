// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/VintagePedalEditor.h"

// BF-3 in the default Vintage Hardware look: cream plate, bakelite knobs, outlined panels FLANGE / SWEEP / MODE, all drawn in code.
class BF3AudioProcessorEditor : public vibr::ui::vintage::PedalEditor,
                                private juce::Timer
{
public:
    explicit BF3AudioProcessorEditor (BF3AudioProcessor&);
    ~BF3AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void paintOverChildren (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void syncChanged();
    int stateKey() const;

    struct Knob
    {
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
    };

    BF3AudioProcessor& proc;
    MidiLearnUI midiUI { proc.midiMap };       // right-click a knob: MIDI learn
    Knob manual, res, depth, rate;
    int lastKey = -1;

    bool syncOn = false;
    int division = 3, mode = 1;
    juce::ParameterAttachment syncAtt, divisionAtt, modeAtt;
    std::unique_ptr<juce::SliderParameterAttachment> divisionKnobAtt;      // the RATE knob while sync is on

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BF3AudioProcessorEditor)
};
