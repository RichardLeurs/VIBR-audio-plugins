// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "EH2Data.h"

namespace
{
// Positions are in the 1840 x 890 coordinates of the rendered background (make_background.py). The editor is resizable at a
// fixed aspect ratio; paint() scales image pixels to the window.
constexpr float kImgW = 1840.0f, kImgH = 890.0f;
constexpr int kBaseW = 920;

constexpr float kKnobRadius = 100.0f;                      // black knob; the printed scale sits outside it
constexpr float kKnobY = 532.0f;
struct KnobSpec { const char* id; float x; float startDeg, endDeg; };
const KnobSpec kMix { "mix", 326.0f, -154.0f, 154.0f }, kFreq { "freq", 920.0f, -154.0f, 154.0f }, kSens { "sens", 1514.0f, -154.0f, 154.0f };
const juce::Point<float> kSensLamp { 1722.0f, 348.0f };    // SENS lamp: lit while the effect is open
const juce::Point<float> kLed { 1765.0f, 87.0f };          // power slide switch (top right, ball left = off) = bypass

} // namespace

//==============================================================================
//==============================================================================
EH2AudioProcessorEditor::EH2AudioProcessorEditor (EH2AudioProcessor& p)
    : PedalEditor (p, EH2Data::background_png, (size_t) EH2Data::background_pngSize, kImgW, kImgH, kBaseW, 460), proc (p)
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
        k.attachment = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter (spec.id), k.slider, nullptr);
    };
    add (mix, kMix);
    add (freq, kFreq);
    add (sens, kSens);

    resized();                                                       // place the knobs (the base class sized the window before they existed)
    startTimerHz (20);
}

EH2AudioProcessorEditor::~EH2AudioProcessorEditor() { setLookAndFeel (nullptr); }

void EH2AudioProcessorEditor::resized()
{
    const float S = (float) getWidth() / kImgW;
    const auto place = [S] (Knob& k, const KnobSpec& spec)
    {
        k.slider.setBounds (juce::Rectangle<float> (2.0f * kKnobRadius * S, 2.0f * kKnobRadius * S)
                                .withCentre ({ spec.x * S, kKnobY * S }).toNearestInt());
    };
    place (mix, kMix);
    place (freq, kFreq);
    place (sens, kSens);
}

void EH2AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    // the power slide switch is the bypass switch
    toggleBypassAt (e.position, kLed);
}

void EH2AudioProcessorEditor::timerCallback()
{
    const bool on = proc.apvts.getRawParameterValue ("bypass")->load() < 0.5f;
    const bool lit = on && proc.getOpen() > 0.5f;
    if (on != lastOn || lit != lastSens) { lastOn = on; lastSens = lit; repaint(); }
}

void EH2AudioProcessorEditor::paint (juce::Graphics& g)
{
    paintBackground (g);       // the image, then image pixels

    // power slide switch: chrome ball left = off, right = on; the slot lights up green when on
    const bool on = proc.apvts.getRawParameterValue ("bypass")->load() < 0.5f;
    if (on)
    {
        const auto slot = juce::Rectangle<float> (66.0f, 34.0f).withCentre (kLed);
        g.setColour (juce::Colour (0xff40d070).withAlpha (0.05f));
        g.fillRoundedRectangle (slot.expanded (16.0f), 30.0f);
        g.setColour (juce::Colour (0xff40d070).withAlpha (0.08f));
        g.fillRoundedRectangle (slot.expanded (8.0f), 24.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2f8a4c), slot.getX(), slot.getY(),
                                                 juce::Colour (0xff17512b), slot.getX(), slot.getBottom(), false));
        g.fillRoundedRectangle (slot, 17.0f);
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.fillRoundedRectangle (slot.withHeight (slot.getHeight() * 0.40f).reduced (8.0f, 3.0f), 6.0f);
    }
    // SENS lamp
    {
        const auto lamp = [&] (float r) { return juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (kSensLamp); };
        g.setColour (juce::Colours::black.withAlpha (0.8f)); g.fillEllipse (lamp (17.0f));
        g.setColour (juce::Colour (0xff808590).withAlpha (0.5f)); g.drawEllipse (lamp (17.0f), 2.0f);
        if (lastSens)
        {
            g.setColour (juce::Colour (0xffff2a1c).withAlpha (0.10f)); g.fillEllipse (lamp (34.0f));
            g.setColour (juce::Colour (0xffff2a1c).withAlpha (0.18f)); g.fillEllipse (lamp (25.0f));
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffb0a0), kSensLamp.x - 4.0f, kSensLamp.y - 5.0f,
                                                     juce::Colour (0xffd8160c), kSensLamp.x + 6.0f, kSensLamp.y + 8.0f, true));
            g.fillEllipse (lamp (12.5f));
        }
        else { g.setColour (juce::Colour (0xff4a1612)); g.fillEllipse (lamp (12.5f)); }
    }
    const juce::Point<float> c (kLed.x + (on ? 17.0f : -17.0f), kLed.y);
    const auto circle = [&] (float r) { return juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c); };
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (circle (15.0f).translated (2.0f, 4.0f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffafbfd), c.x - 6.0f, c.y - 8.0f,
                                             juce::Colour (0xff6b6f7a), c.x + 9.0f, c.y + 11.0f, true));
    g.fillEllipse (circle (14.0f));
}
