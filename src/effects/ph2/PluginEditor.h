// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/VintagePedalEditor.h"

// PH-2 in the default (Vintage Hardware) look: SWEEP (RATE, DEPTH, SYNC), RESONANCE (RES) and MODE (I / II), drawn in code (no background image).
class PH2AudioProcessorEditor : public vibr::ui::vintage::PedalEditor,
                                private juce::Timer
{
public:
    explicit PH2AudioProcessorEditor (PH2AudioProcessor&);
    ~PH2AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void paintOverChildren (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void syncChanged (bool on);

    struct Knob
    {
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
    };

    // a transparent area over the I / II buttons: it takes the clicks and carries the MIDI learn menu of the MODE parameter
    struct ModeArea : juce::Component
    {
        std::function<void (juce::Point<float>)> onDown;
        void mouseDown (const juce::MouseEvent& e) override { if (! e.mods.isPopupMenu() && onDown) onDown (e.position); }
    };

    PH2AudioProcessor& proc;
    MidiLearnUI midiUI { proc.midiMap };       // right-click a knob: MIDI learn
    Knob res, depth, rate;
    ModeArea modeArea;
    bool lastOn = true;
    bool syncOn = false;
    int division = 3;
    int modeIndex = 0;                         // 0 = I (4 stages), 1 = II (8 stages)
    std::unique_ptr<juce::SliderParameterAttachment> divisionKnobAtt;      // the RATE knob while sync is on
    // declared last so they are destroyed first (their callbacks touch the members above)
    juce::ParameterAttachment syncAtt, divisionAtt, modeAtt;   // divisionAtt only tracks the value for the readout

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PH2AudioProcessorEditor)
};
