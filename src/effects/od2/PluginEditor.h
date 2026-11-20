// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/VintagePedalEditor.h"
#include "ClickZone.h"
#include "MidiTarget.h"

// OD-2 in the default (Vintage Hardware) look: TYPE (a phosphor display and 12 flat buttons), DRIVE and TONE with their lower strips, drawn in code (no background image).
class OD2AudioProcessorEditor : public vibr::ui::vintage::PedalEditor,
                                private juce::Timer
{
public:
    explicit OD2AudioProcessorEditor (OD2AudioProcessor&);
    ~OD2AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void selectSlot (int s);
    void setType (int t);
    void toggleParam (const char* id);

    struct Knob
    {
        const char* id;
        const char* name;
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
    };

    OD2AudioProcessor& proc;
    MidiLearnUI midiUI { proc.midiMap };           // right-click a knob: MIDI learn
    Knob param { "param", "PARAM", {}, {} }, level { "level", "LEVEL", {}, {} }, drive { "drive", "DRIVE", {}, {} },
         low { "low", "LOW", {}, {} }, middle { "middle", "MIDDLE", {}, {} }, high { "high", "HIGH", {}, {} };
    MidiTarget typeTarget;                         // over the type display: click = next type; right-click = MIDI learn of TYPE
    MidiTarget boostTarget;                        // over the BOOST button (MIDI learn target)
    ClickZone paramZone;
    ClickZone paramLedZone[5];                     // click a lamp to select it

    int slot = 0;                                  // what the PARAM knob edits: TYPE param, POST, PRE, DECAY, THRESH
    bool lastOn = true, lastBoost = false, valueShown = false;
    int lastType = -1;
    juce::uint32 shownUntil = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OD2AudioProcessorEditor)
};
