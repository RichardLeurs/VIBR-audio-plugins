// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "OC5Data.h"

namespace
{
// Positions are in the 1840 x 890 coordinates of the rendered background (make_background.py). The editor is resizable at a
// fixed aspect ratio; paint() scales image pixels to the window.
constexpr float kImgW = 1840.0f, kImgH = 890.0f;
constexpr int kBaseW = 920;

constexpr float kKnobRadius = 100.0f;                      // black knob; the printed scale sits outside it
constexpr float kKnobY = 532.0f;
struct KnobSpec { const char* id; float x; };
const KnobSpec kDirect { "direct", 230.0f }, kUp1 { "up1", 520.0f }, kDown1 { "down1", 810.0f }, kDown2 { "down2", 1210.0f };
constexpr float kStartDeg = -154.0f, kEndDeg = 154.0f;     // 11 ticks over 308 degrees

const juce::Point<float> kLed { 1765.0f, 87.0f };          // power slide switch (top right, ball left = off) = bypass
const juce::Point<float> kSwitch { 1610.0f, 532.0f };      // VINTAGE / POLY slide switch (vertical): ball up = VINTAGE, down = POLY

} // namespace

//==============================================================================
//==============================================================================
OC5AudioProcessorEditor::OC5AudioProcessorEditor (OC5AudioProcessor& p)
    : PedalEditor (p, OC5Data::background_png, (size_t) OC5Data::background_pngSize, kImgW, kImgH, kBaseW, 460), proc (p)
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
        k.attachment = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter (spec.id), k.slider, nullptr);
        midiUI.add (k.slider, spec.id);
    };
    add (direct, kDirect);
    add (up1, kUp1);
    add (down1, kDown1);
    add (down2, kDown2);

    resized();                                                       // place the knobs (the base class sized the window before they existed)
    startTimerHz (20);
}

OC5AudioProcessorEditor::~OC5AudioProcessorEditor() { setLookAndFeel (nullptr); }

void OC5AudioProcessorEditor::resized()
{
    const float S = (float) getWidth() / kImgW;
    const auto place = [S] (Knob& k, const KnobSpec& spec)
    {
        k.slider.setBounds (juce::Rectangle<float> (2.0f * kKnobRadius * S, 2.0f * kKnobRadius * S)
                                .withCentre ({ spec.x * S, kKnobY * S }).toNearestInt());
    };
    place (direct, kDirect);
    place (up1, kUp1);
    place (down1, kDown1);
    place (down2, kDown2);
}

void OC5AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    // the power slide switch is the bypass switch; the slide switch in the MODE panel picks VINTAGE (up) or POLY (down)
    const float S = (float) getWidth() / kImgW;
    const auto setParam = [this] (const char* id, float v)
    {
        auto* prm = proc.apvts.getParameter (id);
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (v);
        prm->endChangeGesture();
    };
    if (juce::Rectangle<float> (110.0f * S, 70.0f * S).withCentre (kLed * S).contains (e.position))
        setParam ("bypass", proc.apvts.getParameter ("bypass")->getValue() > 0.5f ? 0.0f : 1.0f);
    else if (juce::Rectangle<float> (240.0f * S, 340.0f * S).withCentre (kSwitch * S).contains (e.position))
        setParam ("poly", e.position.y / S > kSwitch.y ? 1.0f : 0.0f);
}

void OC5AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    midiUI.paintMarks (g, juce::Colour (0xffe84b91));
}

void OC5AudioProcessorEditor::timerCallback()
{
    if (midiUI.needsRepaint()) { midiUI.tick(); repaint(); }
    const bool on = proc.apvts.getRawParameterValue ("bypass")->load() < 0.5f;
    const bool poly = proc.apvts.getRawParameterValue ("poly")->load() > 0.5f;
    if (on != lastOn || poly != lastPoly) { lastOn = on; lastPoly = poly; repaint(); }
}

void OC5AudioProcessorEditor::paint (juce::Graphics& g)
{
    paintBackground (g);       // the image, then image pixels

    // power slide switch: chrome ball left = off, right = on; the slot lights up green when on
    const bool on = proc.apvts.getRawParameterValue ("bypass")->load() < 0.5f;
    vibr::ui::paintSlideSwitch (g, kLed, on);

    // VINTAGE / POLY: chrome ball in the slot, up = VINTAGE, down = POLY
    const bool poly = proc.apvts.getRawParameterValue ("poly")->load() > 0.5f;
    const juce::Point<float> ball (kSwitch.x, kSwitch.y + (poly ? 40.0f : -40.0f));
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (juce::Rectangle<float> (30.0f, 30.0f).withCentre (ball + juce::Point<float> (2.0f, 4.0f)));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffafbfd), ball.x - 6.0f, ball.y - 8.0f,
                                             juce::Colour (0xff6b6f7a), ball.x + 9.0f, ball.y + 11.0f, true));
    g.fillEllipse (juce::Rectangle<float> (28.0f, 28.0f).withCentre (ball));
}
