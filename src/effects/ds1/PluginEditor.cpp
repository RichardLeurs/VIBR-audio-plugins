// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "DS1Data.h"

namespace
{
// Positions are in the 1560 x 890 coordinates of the rendered background (make_background.py). The editor is resizable at a
// fixed aspect ratio; paint() scales image pixels to the window.
constexpr float kImgW = 1560.0f, kImgH = 890.0f;
constexpr int kBaseW = 780;

constexpr float kKnobRadius = 100.0f;                      // black knob; the printed scale sits outside it
constexpr float kKnobY = 532.0f;
struct KnobSpec { const char* id; float x; };
const KnobSpec kTone { "tone", 280.0f }, kLevel { "level", 780.0f }, kDist { "dist", 1280.0f };
constexpr float kStartDeg = -154.0f, kEndDeg = 154.0f;     // 11 ticks over 308 degrees

const juce::Point<float> kLed { 1485.0f, 87.0f };          // power slide switch (top right, ball left = off) = bypass

} // namespace

//==============================================================================
//==============================================================================
DS1AudioProcessorEditor::DS1AudioProcessorEditor (DS1AudioProcessor& p)
    : PedalEditor (p, DS1Data::background_png, (size_t) DS1Data::background_pngSize, kImgW, kImgH, kBaseW, 390), proc (p)
{
    setLookAndFeel (&laf);
    
    const auto add = [this] (Knob& k, const KnobSpec& spec)
    {
        k.slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        k.slider.setRotaryParameters (juce::degreesToRadians (kStartDeg), juce::degreesToRadians (kEndDeg), true);
        k.slider.setMouseDragSensitivity (180);
        k.slider.setScrollWheelEnabled (false);
        addAndMakeVisible (k.slider);
        midiUI.add (k.slider, spec.id);
        k.attachment = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter (spec.id), k.slider, nullptr);
    };
    add (tone, kTone);
    add (level, kLevel);
    add (dist, kDist);

    resized();                                                       // place the knobs (the base class sized the window before they existed)
    startTimerHz (20);
}

DS1AudioProcessorEditor::~DS1AudioProcessorEditor() { setLookAndFeel (nullptr); }

void DS1AudioProcessorEditor::resized()
{
    const float S = (float) getWidth() / kImgW;
    const auto place = [S] (Knob& k, const KnobSpec& spec)
    {
        k.slider.setBounds (juce::Rectangle<float> (2.0f * kKnobRadius * S, 2.0f * kKnobRadius * S)
                                .withCentre ({ spec.x * S, kKnobY * S }).toNearestInt());
    };
    place (tone, kTone);
    place (level, kLevel);
    place (dist, kDist);
}

void DS1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    // the power slide switch is the bypass switch
    toggleBypassAt (e.position, kLed);
}

void DS1AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    midiUI.paintMarks (g, juce::Colour(230, 83, 60));
}

void DS1AudioProcessorEditor::timerCallback()
{
    if (midiUI.needsRepaint()) { midiUI.tick(); repaint(); }
    const bool on = proc.apvts.getRawParameterValue ("bypass")->load() < 0.5f;
    if (on != lastOn) { lastOn = on; repaint(); }
}

void DS1AudioProcessorEditor::paint (juce::Graphics& g)
{
    paintBackground (g);       // the image, then image pixels

    // power slide switch: chrome ball left = off, right = on; the slot lights up green when on
    const bool on = proc.apvts.getRawParameterValue ("bypass")->load() < 0.5f;
    vibr::ui::paintSlideSwitch (g, kLed, on);
}
