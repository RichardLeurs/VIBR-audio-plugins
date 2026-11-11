// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "RE2Data.h"

namespace
{
// Positions are in the coordinates of the rendered background (2610 x 1138, see make_background.py);
// the editor shows it at 1305 x 569 and paint() scales them with S.
constexpr int kW = 1305, kH = 569;
constexpr float S = (float) kW / 2610.0f;

juce::Point<float> img (float x, float y) { return { x * S, y * S }; }

const juce::Point<float> kSaturation { 250.0f, 790.0f }, kWow { 690.0f, 790.0f }, kMode { 1240.0f, 700.0f },
                         kBass { 1775.0f, 458.0f }, kTreble { 2075.0f, 458.0f }, kReverb { 2375.0f, 458.0f },
                         kRate { 1775.0f, 790.0f }, kIntensity { 2075.0f, 790.0f }, kEcho { 2375.0f, 790.0f };
constexpr float kKnobRadius = 72.0f, kModeRadius = 100.0f;
constexpr float kPad = 3.0f;

const float kHeadLedX[4] = { 1075.0f, 1185.0f, 1295.0f, 1405.0f };
constexpr float kHeadLedY = 422.0f;
const juce::Point<float> kGuitarLed { 150.0f, 574.0f }, kLineLed { 260.0f, 574.0f },
                         kNewLed { 550.0f, 574.0f }, kAgedLed { 660.0f, 574.0f };
const juce::Point<float> kInputButton { 390.0f, 574.0f }, kTapeButton { 780.0f, 574.0f };
const float kMemLedX[5] = { 150.0f, 260.0f, 370.0f, 480.0f, 590.0f };
const juce::Point<float> kMemoryLed0 { 0.0f, 386.0f }, kMemoryButton { 780.0f, 386.0f };
constexpr juce::uint32 kHoldMs = 800;
const juce::Point<float> kPowerSwitch { 2535.0f, 87.0f };      // power slide switch (top right)

juce::Point<float> polar (juce::Point<float> c, float r, float a)
{
    return { c.x + r * std::sin (a), c.y - r * std::cos (a) };
}

// lamp sitting in a chrome bezel that is painted in the background (image pixels)
void drawLamp (juce::Graphics& g, juce::Point<float> c, bool on, juce::Colour colour = juce::Colour (0xffff2a1a))
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
void RE2LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float,
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
RE2AudioProcessorEditor::RE2AudioProcessorEditor (RE2AudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&laf);
    background = juce::ImageFileFormat::loadFrom (RE2Data::background_png, (size_t) RE2Data::background_pngSize);

    addKnob (saturation, "saturation");
    addKnob (wowFlutter, "wowFlutter");
    addKnob (mode,       "mode");
    addKnob (bass,       "bass");
    addKnob (treble,     "treble");
    addKnob (reverbVol,  "reverbVol");
    addKnob (repeatRate, "repeatRate");
    addKnob (intensity,  "intensity");
    addKnob (echoVol,    "echoVol");

    // the printed 1-12 dial: pointer on "1" at the lower left, sweeping clockwise round to "12"
    mode.slider.setRotaryParameters (juce::degreesToRadians (-150.0f), juce::degreesToRadians (180.0f), true);
    mode.slider.setRange (1.0, 12.0, 1.0);

    addSwitch (inputSwitch, inputAttachment, "inputLine");
    addSwitch (tapeSwitch,  tapeAttachment,  "tapeAged");
    addSwitch (powerSwitch, powerAttachment, "power");

    memoryZone.onDown  = [this] { memHeld = true; memWritten = false; memDownAt = juce::Time::getMillisecondCounter(); };
    memoryZone.onUp    = [this] { memHeld = false; };
    memoryZone.onClick = [this] { if (! memWritten) memory.stepNext(); repaint(); };      // short press
    addAndMakeVisible (memoryZone);

    setSize (kW, kH);
    timerCallback();      // initial LED state
    startTimerHz (20);
}

RE2AudioProcessorEditor::~RE2AudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void RE2AudioProcessorEditor::addKnob (Knob& k, const juce::String& paramId)
{
    k.slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    k.slider.setRotaryParameters (juce::degreesToRadians (-148.0f), juce::degreesToRadians (148.0f), true); // matches the printed min/max feet
    k.slider.setMouseDragSensitivity (180);
    k.slider.setScrollWheelEnabled (false);
    addAndMakeVisible (k.slider);
    k.attachment = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter (paramId), k.slider, nullptr);
}

void RE2AudioProcessorEditor::addSwitch (juce::ToggleButton& b, std::unique_ptr<juce::ButtonParameterAttachment>& att, const juce::String& paramId)
{
    addAndMakeVisible (b);
    att = std::make_unique<juce::ButtonParameterAttachment> (*proc.apvts.getParameter (paramId), b);
    b.onStateChange = [this] { repaint(); };
}

void RE2AudioProcessorEditor::resized()
{
    auto place = [] (Knob& k, juce::Point<float> imgCentre, float r = kKnobRadius)
    {
        const float d = 2.0f * (r * S + kPad);
        k.slider.setBounds (juce::Rectangle<float> (d, d).withCentre (img (imgCentre.x, imgCentre.y)).toNearestInt());
    };
    place (saturation, kSaturation);
    place (wowFlutter, kWow);
    place (mode,       kMode, kModeRadius);
    place (bass,       kBass);
    place (treble,     kTreble);
    place (reverbVol,  kReverb);
    place (repeatRate, kRate);
    place (intensity,  kIntensity);
    place (echoVol,    kEcho);

    const juce::Rectangle<float> buttonSize (124.0f * S, 70.0f * S);
    inputSwitch.setBounds (buttonSize.withCentre (img (kInputButton.x, kInputButton.y)).toNearestInt());
    tapeSwitch.setBounds  (buttonSize.withCentre (img (kTapeButton.x, kTapeButton.y)).toNearestInt());
    memoryZone.setBounds (buttonSize.withCentre (img (kMemoryButton.x, kMemoryButton.y)).toNearestInt());
    powerSwitch.setBounds (juce::Rectangle<float> (110.0f * S, 70.0f * S).withCentre (img (kPowerSwitch.x, kPowerSwitch.y)).toNearestInt());
}

void RE2AudioProcessorEditor::timerCallback()
{
    // memory: holding the button writes the slot; moving a knob after a recall goes back to MANUAL
    const auto now = juce::Time::getMillisecondCounter();
    if (memHeld && ! memWritten && now - memDownAt >= kHoldMs)
    {
        memory.write();
        memWritten = true;
        memFlashUntil = now + 500;
    }
    memory.revertIfEdited();
    const bool flashing = now < memFlashUntil;
    if (memory.current() != memShown || flashing || flashing != memFlashShown)
    {
        memShown = memory.current();
        memFlashShown = flashing;
        repaint (juce::Rectangle<float> (60.0f * S, 296.0f * S, 820.0f * S, 100.0f * S).toNearestInt());
    }

    // head LEDs follow the Mode Selector
    const int m = juce::jlimit (1, 12, (int) std::round (proc.apvts.getRawParameterValue ("mode")->load()));
    const int mask = re2::kModes[m - 1].headMask;
    if (mask != lastHeadMask)
    {
        lastHeadMask = mask;
        repaint (juce::Rectangle<float> (1020.0f * S, 380.0f * S, 440.0f * S, 90.0f * S).toNearestInt());
    }
}

void RE2AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (background, 0, 0, kW, kH, 0, 0, background.getWidth(), background.getHeight());
    g.addTransform (juce::AffineTransform::scale (S));      // from here on: background pixels

    // head LEDs (heads 1-3 from the Mode Selector; this engine has three heads)
    const int mask = juce::jmax (0, lastHeadMask);
    for (int i = 0; i < 4; ++i)
        drawLamp (g, { kHeadLedX[i], kHeadLedY }, i < 3 && (mask & (1 << i)) != 0);

    // memory LEDs: MANUAL, 1-4; all five flash green after a write
    const bool memFlashing = juce::Time::getMillisecondCounter() < memFlashUntil;
    const bool blinkOff = juce::Time::getMillisecondCounter() < memFlashUntil && ((juce::Time::getMillisecondCounter() / 100) % 2) == 1;
    for (int i = 0; i < 5; ++i)
        drawLamp (g, { kMemLedX[i], kMemoryLed0.y }, (memFlashing ? ! blinkOff : memory.current() == i), juce::Colour (0xff40d070));

    // input select + tape type
    const bool line = inputSwitch.getToggleState(), aged = tapeSwitch.getToggleState();
    drawLamp (g, kGuitarLed, ! line);
    drawLamp (g, kLineLed,    line);
    drawLamp (g, kNewLed,   ! aged);
    drawLamp (g, kAgedLed,    aged);

    // power slide switch: chrome ball left = off, right = on; the slot lights up green when on
    const bool on = powerSwitch.getToggleState();
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
