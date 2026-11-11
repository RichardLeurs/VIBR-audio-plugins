// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "RE1Data.h"

namespace
{
// Positions are in the coordinates of the rendered background (2610 x 1138, see make_background.py);
// the editor shows it at 1305 x 569 and paint() scales them with S.
constexpr int kW = 1305, kH = 569;
constexpr float S = (float) kW / 2610.0f;

juce::Point<float> at (float x, float y) { return { x * S, y * S }; }

const juce::Point<float> kWow { 210.0f, 790.0f }, kAge { 510.0f, 790.0f }, kInput { 810.0f, 790.0f },
                         kMode { 1320.0f, 634.0f },
                         kBass { 1815.0f, 458.0f }, kTreble { 2115.0f, 458.0f }, kReverb { 2415.0f, 458.0f },
                         kRate { 1815.0f, 790.0f }, kIntensity { 2115.0f, 790.0f }, kEcho { 2415.0f, 790.0f };
constexpr float kKnobRadius = 72.0f, kModeRadius = 100.0f;
constexpr float kPad = 3.0f;

const juce::Point<float> kPeakLed { 130.0f, 480.0f }, kPowerSwitch { 2535.0f, 87.0f };      // PEAK lamp; power slide switch (top right)
const juce::Rectangle<float> kVuFace { 250.0f, 342.0f, 520.0f, 277.0f };                    // live VU meter (drawn at 250 x 133 and scaled)

juce::Point<float> polar (juce::Point<float> c, float r, float a)
{
    return { c.x + r * std::sin (a), c.y - r * std::cos (a) };
}

// lamp sitting in a chrome bezel that is painted in the background
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
// Black knob: matt body with a knurled grip edge, raised cap, white pointer.
void SpaceEchoLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float,
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
SpaceEchoAudioProcessorEditor::SpaceEchoAudioProcessorEditor (SpaceEchoAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&laf);

    background = juce::ImageFileFormat::loadFrom (RE1Data::background_png, (size_t) RE1Data::background_pngSize);

    addKnob (wowFlutter, "wowFlutter");
    addKnob (tapeAge,    "tapeAge");
    addKnob (inputVol,   "inputVol");
    addKnob (mode,       "mode");
    addKnob (bass,       "bass");
    addKnob (treble,     "treble");
    addKnob (reverbVol,  "reverbVol");
    addKnob (repeatRate, "repeatRate");
    addKnob (intensity,  "intensity");
    addKnob (echoVol,    "echoVol");

    // the printed 1-11 + REVERB ONLY dial: pointer on "1" at the lower left, sweeping clockwise, 30 degrees per step
    mode.slider.setRotaryParameters (juce::degreesToRadians (-150.0f), juce::degreesToRadians (180.0f), true);
    mode.slider.setRange (1.0, 12.0, 1.0);

    addAndMakeVisible (powerButton);
    powerAttachment = std::make_unique<juce::ButtonParameterAttachment> (*proc.apvts.getParameter ("power"), powerButton);
    powerButton.onStateChange = [this] { repaint(); };

    setSize (kW, kH);
    startTimerHz (30);
}

SpaceEchoAudioProcessorEditor::~SpaceEchoAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void SpaceEchoAudioProcessorEditor::addKnob (Knob& k, const juce::String& paramId)
{
    k.slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    k.slider.setRotaryParameters (juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
    k.slider.setMouseDragSensitivity (180);
    k.slider.setScrollWheelEnabled (false);
    addAndMakeVisible (k.slider);
    k.attachment = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter (paramId), k.slider, nullptr);
}

void SpaceEchoAudioProcessorEditor::resized()
{
    auto place = [] (Knob& k, juce::Point<float> c, float r = kKnobRadius)
    {
        const float d = 2.0f * (r * S + kPad);
        k.slider.setBounds (juce::Rectangle<float> (d, d).withCentre (at (c.x, c.y)).toNearestInt());
    };
    place (wowFlutter, kWow);   place (tapeAge, kAge);        place (inputVol, kInput);
    place (mode, kMode, kModeRadius);
    place (bass, kBass);        place (treble, kTreble);      place (reverbVol, kReverb);
    place (repeatRate, kRate);  place (intensity, kIntensity); place (echoVol, kEcho);

    powerButton.setBounds (juce::Rectangle<float> (110.0f * S, 70.0f * S).withCentre (at (kPowerSwitch.x, kPowerSwitch.y)).toNearestInt());
}

void SpaceEchoAudioProcessorEditor::timerCallback()
{
    const float level = proc.outputLevel.load();
    const float target = juce::jlimit (0.0f, 1.0f, juce::Decibels::gainToDecibels (level, -40.0f) / 40.0f + 1.0f);
    vuValue += (target - vuValue) * 0.25f;   // needle ballistics

    if (proc.inputPeak.exchange (0.0f) > 0.89f) peakHold = 8;
    peakLit = peakHold > 0;
    if (peakHold > 0) --peakHold;

    repaint (juce::Rectangle<float> (kVuFace.getX() * S - 4.0f, kVuFace.getY() * S - 4.0f, kVuFace.getWidth() * S + 8.0f, kVuFace.getHeight() * S + 8.0f).toNearestInt());
    repaint (juce::Rectangle<float> (80.0f * S, 80.0f * S).withCentre (at (kPeakLed.x, kPeakLed.y)).toNearestInt());
}

//==============================================================================
void SpaceEchoAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (background, 0, 0, kW, kH, 0, 0, background.getWidth(), background.getHeight());
    g.addTransform (juce::AffineTransform::scale (S));      // from here on: photo pixels

    // ---- live VU meter, drawn at 250 x 133 and scaled into the recess painted in the background
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.addTransform (juce::AffineTransform::scale (kVuFace.getWidth() / 250.0f).translated (kVuFace.getX(), kVuFace.getY()));
        const juce::Rectangle<float> vu (0.0f, 0.0f, 250.0f, 133.0f);
        juce::Path clip; clip.addRoundedRectangle (vu, 3.0f);
        g.reduceClipRegion (clip);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a3135), vu.getX(), vu.getY(),
                                                 juce::Colour (0xff14181a), vu.getX(), vu.getBottom(), false));
        g.fillRect (vu);

        const juce::Point<float> pivot (vu.getCentreX(), vu.getBottom() + 28.0f);
        const float rad = 128.0f;
        const float a0 = juce::degreesToRadians (-50.0f), a1 = juce::degreesToRadians (48.0f);
        const float aRed = juce::degreesToRadians (8.0f);

        juce::Path greenArc, redArc;
        greenArc.addCentredArc (pivot.x, pivot.y, rad - 14.0f, rad - 14.0f, 0.0f, a0, aRed, true);
        redArc.addCentredArc (pivot.x, pivot.y, rad - 14.0f, rad - 14.0f, 0.0f, aRed, a1, true);
        g.setColour (juce::Colour (0xff2fb36a));
        g.strokePath (greenArc, juce::PathStrokeType (9.0f));
        g.setColour (juce::Colour (0xffe0603f));
        g.strokePath (redArc, juce::PathStrokeType (9.0f));
        for (int i = 0; i <= 14; ++i)
        {
            const float a = a0 + (float) i / 14.0f * (a1 - a0);
            g.setColour (a > aRed ? juce::Colour (0xffe0603f) : juce::Colour (0xffc9e6d4));
            g.drawLine ({ polar (pivot, rad - 26.0f, a), polar (pivot, rad - (i % 7 == 0 ? 36.0f : 31.0f), a) }, i % 7 == 0 ? 2.4f : 1.6f);
        }
        g.setColour (juce::Colour (0xff2fb36a));
        g.setFont (juce::Font (juce::FontOptions (30.0f, juce::Font::bold)));
        g.drawText ("VU", juce::Rectangle<float> (80.0f, 34.0f).withCentre ({ vu.getCentreX(), vu.getY() + 82.0f }), juce::Justification::centred);

        const float a = a0 + vuValue * (a1 - a0);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawLine ({ pivot.translated (2.0f, 2.0f), polar (pivot, rad + 6.0f, a).translated (2.0f, 2.0f) }, 2.6f);
        g.setColour (juce::Colour (0xffeeeeea));
        g.drawLine ({ pivot, polar (pivot, rad + 6.0f, a) }, 2.4f);

        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.16f), vu.getX(), vu.getY(),
                                                 juce::Colours::transparentWhite, vu.getX(), vu.getCentreY(), false));
        g.fillRect (vu.withHeight (vu.getHeight() * 0.5f));
    }

    drawLamp (g, kPeakLed, peakLit, juce::Colour (0xffff2a1a));

    // power slide switch: chrome ball left = off, right = on; the slot lights up green when on
    {
        const bool on = powerButton.getToggleState();
        if (on)
        {
            const auto slot = juce::Rectangle<float> (66.0f, 34.0f).withCentre (kPowerSwitch);
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
        const juce::Point<float> ball (kPowerSwitch.x + (on ? 17.0f : -17.0f), kPowerSwitch.y);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillEllipse (juce::Rectangle<float> (30.0f, 30.0f).withCentre (ball + juce::Point<float> (2.0f, 4.0f)));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffafbfd), ball.x - 6.0f, ball.y - 8.0f,
                                                 juce::Colour (0xff6b6f7a), ball.x + 9.0f, ball.y + 11.0f, true));
        g.fillEllipse (juce::Rectangle<float> (28.0f, 28.0f).withCentre (ball));
    }
}
