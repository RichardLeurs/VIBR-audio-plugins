// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include "PluginProcessor.h"
#include "ui/VibrUI.h"

// MS-1 window, drawn in code on a satin body: header (name, description, target preset, power), SPECTRUM (with the EQ: curve, 8 draggable band nodes,
// a strip with the selected band) and LOUDNESS panels on top, and under them
// INPUT (trim), LIMITER (six knobs, character, auto gain) and OUTPUT (gain, dither, delta), and a third row with the BUS COMPRESSOR (six knobs, BUS IN,
// sidechain high-pass) and its GAIN REDUCTION history. See .claude/mastering_specs.md.
class MS1AudioProcessorEditor : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit MS1AudioProcessorEditor (MS1AudioProcessor&);
    ~MS1AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    struct Knob
    {
        juce::String id;              // the parameter id (with perBand: the part after "mb<n>")
        int panel, index, count;      // panel number, position in the panel, knobs in the panel (or in the tab)
        const char* name;
        int kind;                     // 0 gain +- (dB), 1 gain + (dB), 2 ceiling (dBTP), 3 release (ms), 4 lookahead (ms), 5 percent, 6 target (LUFS), 7 dither bits
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> att;
        int tab = -1;                 // the stage tab the knob belongs to (row 3), -1 = always shown
        bool perBand = false;         // a multiband knob: follows the selected band
    };
    struct StageButton { juce::String label; bool on; std::function<void()> act; };
    void timerCallback() override;
    float scale() const { return (float) getWidth() / 1200.0f; }
    void layoutKnobs();
    bool isOn (const char* id) const { return proc.apvts.getRawParameterValue (id)->load() > 0.5f; }
    float value (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }
    int choice (const char* id) const { return (int) std::lround (value (id)); }
    void toggle (const char* id);
    void setChoice (const char* id, int index);
    void setFloat (const char* id, float v);
    void showTargetMenu();
    void applyTarget (int index);
    void drawSpectrum (juce::Graphics&);
    void drawEq (juce::Graphics&);
    ms1::EqBand eqBand (int band) const;
    int nodeAt (juce::Point<float> p, float radius) const;
    void setEqFreqGain (int band, float freq, float gain);
    void endEqDrag();
    void drawLoudness (juce::Graphics&);
    void drawChain (juce::Graphics&);
    void drawBusGr (juce::Graphics&);
    void drawStageDisplay (juce::Graphics&);
    void drawReference (juce::Graphics&);
    void showReferenceChooser();
    void setTab (int t);
    void selectMbBand (int b);
    void attachKnob (Knob&);
    bool knobVisible (const Knob& k) const { return k.tab < 0 || k.tab == tab; }
    juce::String knobParam (const Knob& k) const { return k.perBand ? "mb" + juce::String (selMb + 1) + k.id : k.id; }
    std::vector<StageButton> stageButtons();
    void endDivDrag();


    MS1AudioProcessor& proc;
    vibr::ui::KnobLookAndFeel laf;
    juce::ComponentBoundsConstrainer constrainer;
    juce::Image grain;
    std::array<Knob, 30> knobs;
    juce::String matchNote;                         // the answer to the MATCH EQ button, shown for a few seconds
    int matchNoteTicks = 0;
    bool specHold = false, specFreeze = false, frozenValid = false;      // spectrum: peak hold line, frozen picture
    std::array<float, ms1::kBands> holdPeak {}, frozenPre {}, frozenPost {};
    vibr::ui::PresetBarState barState;              // the COPIED flash of the A / B copy button
    int dragField = -1;                             // the dynamic EQ field being dragged (0 threshold, 1 ratio, 2 attack, 3 release)
    float dragStartY = 0.0f, dragStartNorm = 0.0f;
    int tab = 0, selMb = 0, dragDiv = -1;           // the stage tab of row 3 (0 multiband, 1 imager, 2 colour, 3 bus), the selected multiband band, the crossover being dragged
    std::unique_ptr<juce::FileChooser> chooser;
    std::array<float, 240> grHist {};               // the bus compressor's gain reduction, one value per timer tick (about 8 s)
    int grPos = 0;
    int selBand = 3, dragBand = -1;                 // the selected EQ band (0 .. 7) and the band being dragged
    juce::ParameterAttachment powerAtt, autoAtt, deltaAtt, charAtt, shapeAtt, targetAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MS1AudioProcessorEditor)
};
