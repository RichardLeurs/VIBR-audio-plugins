// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "SL2Data.h"

namespace
{
// Positions are in the 2380 x 890 coordinates of the rendered background (make_background.py). The editor is resizable at a
// fixed aspect ratio; paint() scales image pixels to the window.
constexpr float kImgW = 2380.0f, kImgH = 890.0f;
constexpr int kBaseW = 1190;

constexpr float kKnobRadius = 100.0f;                      // black knob; the printed scale sits outside it
constexpr float kKnobY = 532.0f;
struct KnobSpec { const char* id; float x; float startDeg, endDeg; };
const KnobSpec kBalance { "balance", 185.0f, -154.0f, 154.0f }, kTempo { "tempo", 475.0f, -154.0f, 154.0f },
               kAttack { "attack", 785.0f, -154.0f, 154.0f }, kDuty { "duty", 1075.0f, -154.0f, 154.0f },
               kVariation { "variation", 1460.0f, -150.0f, 150.0f },      // 11 patterns, 30 degrees apart
               kMode { "mode", 2020.0f, -90.0f, 90.0f };                 // 5 positions: SINGLE, DUAL, TREMOLO, SFX, HARMONIC

const juce::Point<float> kLed { 2305.0f, 87.0f };          // power slide switch (top right, ball left = off) = bypass
const juce::Point<float> kSync { 2020.0f, 702.0f };        // tempo source slide switch: ball left = KNOB, right = HOST

} // namespace

//==============================================================================
//==============================================================================
SL2AudioProcessorEditor::SL2AudioProcessorEditor (SL2AudioProcessor& p)
    : PedalEditor (p, SL2Data::background_png, (size_t) SL2Data::background_pngSize, kImgW, kImgH, kBaseW, 595), proc (p)
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
    add (balance, kBalance);
    add (tempo, kTempo);
    add (attack, kAttack);
    add (duty, kDuty);
    add (variation, kVariation);
    add (mode, kMode);

    resized();                                                       // place the knobs (the base class sized the window before they existed)
    startTimerHz (20);
}

SL2AudioProcessorEditor::~SL2AudioProcessorEditor() { setLookAndFeel (nullptr); }

void SL2AudioProcessorEditor::resized()
{
    const float S = (float) getWidth() / kImgW;
    const auto place = [S] (Knob& k, const KnobSpec& spec)
    {
        k.slider.setBounds (juce::Rectangle<float> (2.0f * kKnobRadius * S, 2.0f * kKnobRadius * S)
                                .withCentre ({ spec.x * S, kKnobY * S }).toNearestInt());
    };
    place (balance, kBalance);
    place (tempo, kTempo);
    place (attack, kAttack);
    place (duty, kDuty);
    place (variation, kVariation);
    place (mode, kMode);
}

void SL2AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    // the power slide switch is the bypass switch; the small slide switch under the mode knob chooses the tempo source
    const float S = (float) getWidth() / kImgW;
    const auto toggle = [this] (const char* id)
    {
        auto* prm = proc.apvts.getParameter (id);
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->getValue() > 0.5f ? 0.0f : 1.0f);
        prm->endChangeGesture();
    };
    if (juce::Rectangle<float> (110.0f * S, 70.0f * S).withCentre (kLed * S).contains (e.position)) toggle ("bypass");
    else if (juce::Rectangle<float> (100.0f * S, 60.0f * S).withCentre (kSync * S).contains (e.position)) toggle ("hostSync");
}

void SL2AudioProcessorEditor::timerCallback()
{
    const bool on = proc.apvts.getRawParameterValue ("bypass")->load() < 0.5f;
    const bool host = proc.apvts.getRawParameterValue ("hostSync")->load() > 0.5f;
    if (on != lastOn || host != lastHost) { lastOn = on; lastHost = host; repaint(); }
}

void SL2AudioProcessorEditor::paint (juce::Graphics& g)
{
    paintBackground (g);       // the image, then image pixels

    // power slide switch: chrome ball left = off, right = on; the slot lights up green when on
    const bool on = proc.apvts.getRawParameterValue ("bypass")->load() < 0.5f;
    vibr::ui::paintSlideSwitch (g, kLed, on);

    // tempo source: chrome ball in the slot, left = KNOB, right = HOST
    const bool host = proc.apvts.getRawParameterValue ("hostSync")->load() > 0.5f;
    const juce::Point<float> ball (kSync.x + (host ? 17.0f : -17.0f), kSync.y);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (juce::Rectangle<float> (30.0f, 30.0f).withCentre (ball + juce::Point<float> (2.0f, 4.0f)));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffafbfd), ball.x - 6.0f, ball.y - 8.0f,
                                             juce::Colour (0xff6b6f7a), ball.x + 9.0f, ball.y + 11.0f, true));
    g.fillEllipse (juce::Rectangle<float> (28.0f, 28.0f).withCentre (ball));
}
