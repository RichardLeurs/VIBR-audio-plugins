// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "SD1Data.h"

namespace
{
// Positions are in the 2800 x 818 coordinates of the rendered background (make_background.py). The editor is resizable at a
// fixed aspect ratio; paint() scales image pixels to the window.
constexpr float kImgW = 2800.0f, kImgH = 818.0f;
constexpr int kBaseW = 1400;

constexpr float kKnobRadius = 60.0f;                       // black knob; the printed scale sits outside it
constexpr float kPad = 3.0f;
constexpr float kKnobY = 500.0f;
struct KnobSpec { const char* id; float x; float startDeg, endDeg; };
const KnobSpec kKnobs[9] = {
    { "level",        360.0f, -154.0f, 154.0f }, { "delayTime",    635.0f, -130.0f, 50.0f },     // TIME only turns from SHORT to LONG
    { "factor",       845.0f, -154.0f, 154.0f }, { "feedback",    1135.0f, -154.0f, 154.0f },
    { "hiCut",       1345.0f, -154.0f, 154.0f }, { "modIntensity", 1635.0f, -154.0f, 154.0f },
    { "modFrequency", 1845.0f, -154.0f, 154.0f }, { "direct",     2135.0f, -154.0f, 154.0f },
    { "effect",      2345.0f, -154.0f, 154.0f } };

const juce::Point<float> kLed { 2725.0f, 87.0f };          // power slide switch (top right, ball left = off) = the `power` parameter
const juce::Point<float> kHoldLamp { 2560.0f, 500.0f }, kHoldButton { 2680.0f, 500.0f };
const juce::Rectangle<float> kMeter { 94.0f, 356.0f, 36.0f, 262.0f };      // headroom meter frame

juce::Point<float> polar (juce::Point<float> c, float r, float a)
{
    return { c.x + r * std::sin (a), c.y - r * std::cos (a) };
}

// lamp sitting in a chrome bezel that is painted in the background (image pixels)
void drawLamp (juce::Graphics& g, juce::Point<float> c, bool on, juce::Colour colour)
{
    const auto circle = [&] (float r) { return juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c); };
    if (on)
    {
        g.setColour (colour.withAlpha (0.20f));
        g.fillEllipse (circle (32.0f));
        g.setColour (colour.withAlpha (0.25f));
        g.fillEllipse (circle (21.0f));
    }
    g.setGradientFill (juce::ColourGradient (on ? colour.brighter (0.8f) : colour.darker (1.8f), c.x - 5.0f, c.y - 7.0f,
                                             on ? colour.darker (0.2f) : colour.darker (3.0f), c.x + 8.0f, c.y + 10.0f, true));
    g.fillEllipse (circle (11.5f));
    g.setColour (juce::Colours::white.withAlpha (on ? 0.55f : 0.18f));
    g.fillEllipse (juce::Rectangle<float> (6.0f, 4.0f).withCentre (c.translated (-4.0f, -6.0f)));
}
} // namespace

//==============================================================================
void SDD1000LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float,
                                         float startAngle, float endAngle, juce::Slider& s)
{
    const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const auto c = b.getCentre();
    const float R = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f - kPad;
    const double lo = s.getMinimum(), hi = s.getMaximum();
    const float pos = (float) ((s.getValue() - lo) / (hi - lo));
    const float angle = startAngle + pos * (endAngle - startAngle);
    const auto circle = [&] (float r) { return juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c); };

    // soft drop shadow
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (circle (R).translated (R * 0.05f, R * 0.09f));

    // matt black body with a knurled grip edge
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4c51), c.x - R * 0.7f, c.y - R * 0.8f,
                                             juce::Colour (0xff050506), c.x + R * 0.7f, c.y + R * 0.9f, false));
    g.fillEllipse (circle (R));
    constexpr int ridges = 60;
    for (int i = 0; i < ridges; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / (float) ridges;
        g.setColour (juce::Colours::white.withAlpha (0.10f + 0.10f * std::max (0.0f, std::cos (a + 0.8f))));
        g.drawLine (polar (c, R * 0.90f, a).x, polar (c, R * 0.90f, a).y, polar (c, R * 0.99f, a).x, polar (c, R * 0.99f, a).y, 1.0f);
    }
    g.setColour (juce::Colours::black);
    g.drawEllipse (circle (R), 1.2f);

    // raised cap
    const float capR = R * 0.80f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3b3f), c.x - capR * 0.6f, c.y - capR * 0.7f,
                                             juce::Colour (0xff0b0b0c), c.x + capR * 0.6f, c.y + capR * 0.8f, false));
    g.fillEllipse (circle (capR));
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.14f), c.x - capR * 0.5f, c.y - capR * 0.6f,
                                             juce::Colours::transparentWhite, c.x + capR * 0.1f, c.y + capR * 0.1f, true));
    g.fillEllipse (circle (capR));
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.35f), c.x - capR, c.y - capR,
                                             juce::Colour (0xff000000), c.x + capR, c.y + capR, false));
    g.drawEllipse (circle (capR), 1.2f);

    // pointer: white bar from the rim toward the centre
    juce::Path p;
    p.addRoundedRectangle (-capR * 0.055f, -capR * 0.94f, capR * 0.11f, capR * 0.50f, capR * 0.05f);
    g.setColour (juce::Colour (0xfff2f2ee));
    g.fillPath (p, juce::AffineTransform::rotation (angle).translated (c));
}

//==============================================================================
SDD1000AudioProcessorEditor::SDD1000AudioProcessorEditor (SDD1000AudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&laf);
    background = juce::ImageFileFormat::loadFrom (SD1Data::background_png, (size_t) SD1Data::background_pngSize);

    for (int i = 0; i < 9; ++i)
    {
        auto& k = knobs[i];
        k.slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        k.slider.setRotaryParameters (juce::degreesToRadians (kKnobs[i].startDeg), juce::degreesToRadians (kKnobs[i].endDeg), true);
        k.slider.setMouseDragSensitivity (180);
        k.slider.setScrollWheelEnabled (false);
        addAndMakeVisible (k.slider);
        k.attachment = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter (kKnobs[i].id), k.slider, nullptr);
    }

    constrainer.setFixedAspectRatio ((double) kImgW / (double) kImgH);
    constrainer.setSizeLimits (700, 205, 2800, 818);
    setConstrainer (&constrainer);
    setResizable (true, true);
    setSize (kBaseW, (int) std::round ((float) kBaseW * kImgH / kImgW));
    startTimerHz (30);
}

SDD1000AudioProcessorEditor::~SDD1000AudioProcessorEditor() { setLookAndFeel (nullptr); }

void SDD1000AudioProcessorEditor::resized()
{
    const float S = (float) getWidth() / kImgW;
    for (int i = 0; i < 9; ++i)
        knobs[i].slider.setBounds (juce::Rectangle<float> (2.0f * kKnobRadius * S, 2.0f * kKnobRadius * S)
                                       .withCentre ({ kKnobs[i].x * S, kKnobY * S }).toNearestInt());
}

void SDD1000AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    // power slide switch (top right) = power, and the HOLD push button toggles the hold parameter
    const float S = (float) getWidth() / kImgW;
    const auto toggle = [this] (const char* id)
    {
        auto* prm = proc.apvts.getParameter (id);
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->getValue() > 0.5f ? 0.0f : 1.0f);
        prm->endChangeGesture();
    };
    if (juce::Rectangle<float> (110.0f * S, 70.0f * S).withCentre (kLed * S).contains (e.position)) toggle ("power");
    else if (juce::Rectangle<float> (130.0f * S, 80.0f * S).withCentre (kHoldButton * S).contains (e.position)) toggle ("hold");
}

void SDD1000AudioProcessorEditor::timerCallback()
{
    const float db = juce::Decibels::gainToDecibels (proc.meterLevel.load(), -60.0f);
    meterDb += (db - meterDb) * 0.5f;
    const bool hold = proc.apvts.getRawParameterValue ("hold")->load() > 0.5f;
    const bool pwr = proc.apvts.getRawParameterValue ("power")->load() > 0.5f;
    const int segs = (meterDb >= -20.0f) + (meterDb >= -10.0f) + (meterDb >= 0.0f) + (meterDb >= 6.0f);
    if (segs != lastSegs || hold != lastHold || pwr != lastPower)
    {
        lastSegs = segs; lastHold = hold; lastPower = pwr;
        repaint();
    }
}

void SDD1000AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (background, 0, 0, getWidth(), getHeight(), 0, 0, background.getWidth(), background.getHeight());
    g.addTransform (juce::AffineTransform::scale ((float) getWidth() / kImgW));      // image pixels from here on

    // ---- headroom meter: 4 LED segments (-20, -10, 0, +6 dB), bottom to top
    {
        constexpr int segs = 4;
        const float thresholds[segs] = { -20.0f, -10.0f, 0.0f, 6.0f };
        const float segH = kMeter.getHeight() / (float) segs;
        for (int i = 0; i < segs; ++i)
        {
            const bool lit = meterDb >= thresholds[i];
            const juce::Colour base = i == 3 ? juce::Colour (0xffe03a2a) : (i == 2 ? juce::Colour (0xffe8b820) : juce::Colour (0xff35c46a));
            const juce::Rectangle<float> seg (kMeter.getX() + 6.0f, kMeter.getBottom() - (float) (i + 1) * segH + 5.0f, kMeter.getWidth() - 12.0f, segH - 10.0f);
            g.setColour (lit ? base : base.darker (2.6f).withAlpha (0.55f));
            g.fillRoundedRectangle (seg, 3.0f);
            if (lit)
            {
                g.setColour (base.withAlpha (0.28f));
                g.fillRoundedRectangle (seg.expanded (3.0f), 5.0f);
            }
        }
    }

    // ---- HOLD lamp
    drawLamp (g, kHoldLamp, proc.apvts.getRawParameterValue ("hold")->load() > 0.5f, juce::Colour (0xffff2a1a));

    // ---- power slide switch: chrome ball left = off, right = on; the slot lights up green when on
    const bool on = proc.apvts.getRawParameterValue ("power")->load() > 0.5f;
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
    const juce::Point<float> c (kLed.x + (on ? 17.0f : -17.0f), kLed.y);
    const auto circle = [&] (float r) { return juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c); };
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (circle (15.0f).translated (2.0f, 4.0f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffafbfd), c.x - 6.0f, c.y - 8.0f,
                                             juce::Colour (0xff6b6f7a), c.x + 9.0f, c.y + 11.0f, true));
    g.fillEllipse (circle (14.0f));
}
