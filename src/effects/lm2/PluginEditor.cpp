// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "LM2Data.h"

namespace
{
// Positions are in the 1840 x 890 coordinates of the rendered background (make_background.py). The editor is resizable at a
// fixed aspect ratio; paint() scales image pixels to the window.
constexpr float kImgW = 1840.0f, kImgH = 890.0f;
constexpr int kBaseW = 920;

constexpr float kKnobRadius = 100.0f;                      // black knob; the printed scale sits outside it
constexpr float kKnobY = 532.0f;
struct KnobSpec { const char* id; float x; float startDeg, endDeg; };
const KnobSpec kLevel { "level", 305.0f, -154.0f, 154.0f }, kTone { "tone", 645.0f, -154.0f, 154.0f },
               kRelease { "release", 1195.0f, -154.0f, 154.0f }, kThreshold { "threshold", 1535.0f, -154.0f, 154.0f };

const juce::Point<float> kLed { 1765.0f, 87.0f };          // power slide switch (top right, ball left = off) = bypass

} // namespace

//==============================================================================
//==============================================================================
LM2AudioProcessorEditor::LM2AudioProcessorEditor (LM2AudioProcessor& p)
    : PedalEditor (p, LM2Data::background_png, (size_t) LM2Data::background_pngSize, kImgW, kImgH, kBaseW, 460), proc (p)
{
    setLookAndFeel (&laf);
    
    const auto add = [this] (Knob& k, const KnobSpec& spec)
    {
        k.slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        k.slider.setRotaryParameters (juce::degreesToRadians (spec.startDeg), juce::degreesToRadians (spec.endDeg), true);
        k.slider.setMouseDragSensitivity (180);
        k.slider.setScrollWheelEnabled (false);
        addAndMakeVisible (k.slider);
        midiUI.add (k.slider, spec.id);
        k.attachment = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter (spec.id), k.slider, nullptr);
    };
    add (level, kLevel);
    add (tone, kTone);
    add (release, kRelease);
    add (threshold, kThreshold);

    resized();                                                       // place the knobs (the base class sized the window before they existed)
    startTimerHz (20);
}

LM2AudioProcessorEditor::~LM2AudioProcessorEditor() { setLookAndFeel (nullptr); }

void LM2AudioProcessorEditor::resized()
{
    const float S = (float) getWidth() / kImgW;
    const auto place = [S] (Knob& k, const KnobSpec& spec)
    {
        k.slider.setBounds (juce::Rectangle<float> (2.0f * kKnobRadius * S, 2.0f * kKnobRadius * S)
                                .withCentre ({ spec.x * S, kKnobY * S }).toNearestInt());
    };
    place (level, kLevel);
    place (tone, kTone);
    place (release, kRelease);
    place (threshold, kThreshold);
}

void LM2AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    // the power slide switch is the bypass switch
    toggleBypassAt (e.position, kLed);
}

void LM2AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    midiUI.paintMarks (g, juce::Colour(233, 185, 63));
}

void LM2AudioProcessorEditor::timerCallback()
{
    if (midiUI.needsRepaint()) { midiUI.tick(); repaint(); }
    const bool on = proc.apvts.getRawParameterValue ("bypass")->load() < 0.5f;
    if (on != lastOn) { lastOn = on; repaint(); }
}

void LM2AudioProcessorEditor::paint (juce::Graphics& g)
{
    paintBackground (g);       // the image, then image pixels

    // power slide switch: chrome ball left = off, right = on; the slot lights up green when on
    const bool on = proc.apvts.getRawParameterValue ("bypass")->load() < 0.5f;
    vibr::ui::paintSlideSwitch (g, kLed, on);
}
