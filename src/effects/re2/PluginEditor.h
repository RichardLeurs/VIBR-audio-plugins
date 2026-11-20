// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/VintagePedalEditor.h"
#include "ClickZone.h"

// RE-2 in the default (Vintage Hardware) look: MODE (12-position dial with head lamps), ECHO, TAPE and MIXER panels, drawn in code (no background image).
class RE2AudioProcessorEditor : public vibr::ui::vintage::PedalEditor,
                                private juce::Timer
{
public:
    explicit RE2AudioProcessorEditor (RE2AudioProcessor&);
    ~RE2AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void syncChanged();                       // SYNC toggled: RATE switches between the repeat rate and the note division
    bool paramOn (const char* id) const { return proc.apvts.getRawParameterValue (id)->load() > 0.5f; }

    struct Knob
    {
        const char* id;
        const char* name;
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
    };

    RE2AudioProcessor& proc;
    MidiLearnUI midiUI { proc.midiMap };       // right-click a knob: MIDI learn
    Knob saturation { "saturation", "SATURATION", {}, {} }, wowFlutter { "wowFlutter", "WOW & FLUTTER", {}, {} }, mode { "mode", "MODE", {}, {} },
         bass { "bass", "BASS", {}, {} }, treble { "treble", "TREBLE", {}, {} }, reverbVol { "reverbVol", "REVERB", {}, {} },
         repeatRate { "repeatRate", "RATE", {}, {} }, intensity { "intensity", "INTENSITY", {}, {} }, echoVol { "echoVol", "ECHO VOL", {}, {} };

    int lastHeadMask = -1;

    // SYNC button under RATE; while it is on the RATE knob shows the note division
    juce::ParameterAttachment syncAtt, divisionAtt;
    bool syncOn = false;
    int division = 4;
    std::unique_ptr<juce::SliderParameterAttachment> divisionKnobAtt;      // RATE while sync is on

    ClickZone inputLedZone[2], tapeLedZone[2];       // GUITAR / LINE and NEW / AGED buttons
    int shownState = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RE2AudioProcessorEditor)
};
