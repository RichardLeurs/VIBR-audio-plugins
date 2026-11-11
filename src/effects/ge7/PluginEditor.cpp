// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "GE7Data.h"

namespace
{
// Positions are in the 1840 x 960 coordinates of the rendered background (make_background.py). The editor is resizable at a
// fixed aspect ratio; paint() scales image pixels to the window.
constexpr float kImgW = 1840.0f, kImgH = 960.0f;
constexpr int kBaseW = 920;

constexpr float kTop = 410.0f, kBottom = 750.0f;           // slider travel (+15 dB at the top, -15 dB at the bottom), cap centre
constexpr float kCap = 30.0f;                              // half the cap height = padding of the slider component
constexpr float kBandX0 = 250.0f, kBandDx = 190.0f, kLevelX = 1640.0f;
const char* const kIds[7] = { "f100", "f200", "f400", "f800", "f1k6", "f3k2", "f6k4" };

const juce::Point<float> kCheck { 560.0f, 87.0f };         // CHECK lamp (red, lit when the effect is on)
const juce::Point<float> kLed { 1765.0f, 87.0f };          // power slide switch (top right, ball left = off) = bypass

} // namespace

//==============================================================================
void GE7LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float sliderPos, float, float,
                                       juce::Slider::SliderStyle, juce::Slider&)
{
    const float cx = (float) x + (float) w * 0.5f;
    const juce::Rectangle<float> cap (74.0f * scale, 52.0f * scale);
    const auto r = cap.withCentre ({ cx, sliderPos });
    juce::ignoreUnused (y, h);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (r.translated (3.0f * scale, 6.0f * scale), 16.0f * scale);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffbf8ee), r.getX(), r.getY(), juce::Colour (0xffc9c4b4), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 16.0f * scale);
    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.fillRoundedRectangle (r.reduced (6.0f * scale, 5.0f * scale).withHeight (r.getHeight() * 0.35f), 8.0f * scale);
    g.setColour (juce::Colour (0xff5a574c));
    g.drawRoundedRectangle (r, 16.0f * scale, 1.5f * scale);
    g.setColour (juce::Colour (0xff3a382f));                                    // centre line
    g.fillRect (juce::Rectangle<float> (r.getWidth() - 20.0f * scale, 3.0f * scale).withCentre (r.getCentre()));
}

//==============================================================================
GE7AudioProcessorEditor::GE7AudioProcessorEditor (GE7AudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&laf);
    background = juce::ImageFileFormat::loadFrom (GE7Data::background_png, (size_t) GE7Data::background_pngSize);

    const auto add = [this] (Knob& k, const char* id)
    {
        k.slider.setSliderStyle (juce::Slider::LinearVertical);
        k.slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        k.slider.setSliderSnapsToMousePosition (false);
        k.slider.setDoubleClickReturnValue (true, 0.0);
        k.slider.setScrollWheelEnabled (false);
        addAndMakeVisible (k.slider);
        k.attachment = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter (id), k.slider, nullptr);
    };
    for (int b = 0; b < ge7::kBands; ++b) add (band[b], kIds[b]);
    add (level, "level");

    constrainer.setFixedAspectRatio ((double) kImgW / (double) kImgH);
    constrainer.setSizeLimits (460, 240, 1840, 960);
    setConstrainer (&constrainer);
    setResizable (true, true);
    setSize (kBaseW, (int) std::round ((float) kBaseW * kImgH / kImgW));
    startTimerHz (20);
}

GE7AudioProcessorEditor::~GE7AudioProcessorEditor() { setLookAndFeel (nullptr); }

void GE7AudioProcessorEditor::resized()
{
    const float S = (float) getWidth() / kImgW;
    laf.scale = S;
    const auto place = [S] (Knob& k, float x)
    {
        k.slider.setBounds (juce::Rectangle<float> (100.0f * S, (kBottom - kTop + 2.0f * kCap) * S)
                                .withCentre ({ x * S, 0.5f * (kTop + kBottom) * S }).toNearestInt());
    };
    for (int b = 0; b < ge7::kBands; ++b) place (band[b], kBandX0 + kBandDx * (float) b);
    place (level, kLevelX);
}

void GE7AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    // the power slide switch is the bypass switch
    const float S = (float) getWidth() / kImgW;
    if (juce::Rectangle<float> (110.0f * S, 70.0f * S).withCentre (kLed * S).contains (e.position))
    {
        auto* prm = proc.apvts.getParameter ("bypass");
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->getValue() > 0.5f ? 0.0f : 1.0f);
        prm->endChangeGesture();
    }
}

void GE7AudioProcessorEditor::timerCallback()
{
    const bool on = proc.apvts.getRawParameterValue ("bypass")->load() < 0.5f;
    if (on != lastOn) { lastOn = on; repaint(); }
}

void GE7AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (background, 0, 0, getWidth(), getHeight(), 0, 0, background.getWidth(), background.getHeight());
    g.addTransform (juce::AffineTransform::scale ((float) getWidth() / kImgW));      // image pixels from here on

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
    // CHECK lamp: red, lit while the effect is on
    {
        const auto lamp = [&] (float r) { return juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (kCheck); };
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.fillEllipse (lamp (17.0f));
        g.setColour (juce::Colour (0xff808590).withAlpha (0.5f));
        g.drawEllipse (lamp (17.0f), 2.0f);
        if (on)
        {
            g.setColour (juce::Colour (0xffff2a1c).withAlpha (0.10f)); g.fillEllipse (lamp (34.0f));
            g.setColour (juce::Colour (0xffff2a1c).withAlpha (0.18f)); g.fillEllipse (lamp (25.0f));
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffb0a0), kCheck.x - 4.0f, kCheck.y - 5.0f,
                                                     juce::Colour (0xffd8160c), kCheck.x + 6.0f, kCheck.y + 8.0f, true));
            g.fillEllipse (lamp (12.5f));
        }
        else
        {
            g.setColour (juce::Colour (0xff4a1612)); g.fillEllipse (lamp (12.5f));
        }
    }
    const juce::Point<float> c (kLed.x + (on ? 17.0f : -17.0f), kLed.y);
    const auto circle = [&] (float r) { return juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c); };
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (circle (15.0f).translated (2.0f, 4.0f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffafbfd), c.x - 6.0f, c.y - 8.0f,
                                             juce::Colour (0xff6b6f7a), c.x + 9.0f, c.y + 11.0f, true));
    g.fillEllipse (circle (14.0f));
}
