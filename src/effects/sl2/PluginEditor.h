// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/VintagePedalEditor.h"

// SL-2 in the default (Vintage Hardware) look: OUTPUT (BALANCE, TEMPO), SLICE (ATTACK, DUTY), MODE (5 buttons), VARIATION (11 numbered buttons) and TEMPO SOURCE (KNOB / HOST),
// drawn in code (no background image).
class SL2AudioProcessorEditor : public vibr::ui::vintage::PedalEditor,
                                private juce::Timer
{
public:
    explicit SL2AudioProcessorEditor (SL2AudioProcessor&);
    ~SL2AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void setParam (const char* id, float value01);

    struct Knob
    {
        const char* id;
        const char* name;
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
    };

    SL2AudioProcessor& proc;
    Knob balance { "balance", "BALANCE", {}, {} }, tempo { "tempo", "TEMPO", {}, {} }, attack { "attack", "ATTACK", {}, {} }, duty { "duty", "DUTY", {}, {} };
    int lastVariation = -1, lastMode = -1;
    bool lastOn = true, lastHost = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SL2AudioProcessorEditor)
};
