// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "DM1Data.h"

namespace
{
// Positions below are in the 2576 x 1018 coordinates of the rendered background (make_background.py);
// the editor shows it at 1288 x 548.
constexpr int kW = 1288, kH = 548;
constexpr float S = (float) kW / 2576.0f;

juce::Point<float> at (float x, float y) { return { x * S, y * S }; }

const juce::Point<float> kModRate { 207.0f, 728.0f }, kModDepth { 525.0f, 728.0f }, kVariation { 843.0f, 728.0f },
                         kDelayTime { 1737.0f, 728.0f }, kIntensity { 2052.0f, 728.0f }, kDelayVol { 2370.0f, 728.0f },
                         kMode { 1288.0f, 728.0f };
constexpr float kKnobRadius = 100.0f;                      // black knob; the printed scale sits outside it
constexpr float kPad = 3.0f;

const float kMemLedX[5] = { 186.0f, 300.0f, 416.0f, 531.0f, 646.0f };
const juce::Point<float> kMemoryButton { 840.0f, 398.0f };
constexpr juce::uint32 kHoldMs = 800;

const float kDivisionLedX[5] = { 1714.0f, 1829.0f, 1944.0f, 2059.0f, 2175.0f };
constexpr float kStripLedY = 398.0f;
const juce::Point<float> kDivisionButton { 2370.0f, 398.0f };

// the real pedal's footswitches are not part of the artwork, so ON/OFF and TAP are small buttons in the MODE panel
const juce::Point<float> kLed { 2501.0f, 87.0f }, kTapPad { 1288.0f, 398.0f };     // power slide switch (top right, ball left = off), TAP pad

juce::Point<float> polar (juce::Point<float> c, float r, float a)
{
    return { c.x + r * std::sin (a), c.y - r * std::cos (a) };
}

// strip LED (flat, see style.md "Flat style"): a plain disc inside the dark grey ring painted in the background; red when on (green for the
// memory strip), a dark shade of the same colour when off; no glow, no highlight
void drawStripLed (juce::Graphics& g, juce::Point<float> d, bool on, bool green = false)
{
    const auto c = at (d.x, d.y);
    const juce::Colour lit = green ? juce::Colour (0xff40d070) : juce::Colour (0xffe5432c);
    g.setColour (on ? lit : lit.interpolatedWith (juce::Colour (0xff0c0d0f), 0.78f));
    g.fillEllipse (juce::Rectangle<float> (22.0f * S, 22.0f * S).withCentre (c));
}

// power slide switch: chrome ball left = off, right = on; it is also the bypass switch
void drawPowerLed (juce::Graphics& g, bool on)
{
    if (on)      // the slot lights up green (subtle)
    {
        const auto slot = juce::Rectangle<float> (66.0f * S, 34.0f * S).withCentre (at (kLed.x, kLed.y));
        g.setColour (juce::Colour (0xff40d070).withAlpha (0.05f));
        g.fillRoundedRectangle (slot.expanded (16.0f * S), 30.0f * S);
        g.setColour (juce::Colour (0xff40d070).withAlpha (0.08f));
        g.fillRoundedRectangle (slot.expanded (8.0f * S), 24.0f * S);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2f8a4c), slot.getX(), slot.getY(),
                                                 juce::Colour (0xff17512b), slot.getX(), slot.getBottom(), false));
        g.fillRoundedRectangle (slot, 17.0f * S);
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.fillRoundedRectangle (slot.withHeight (slot.getHeight() * 0.40f).reduced (8.0f * S, 3.0f * S), 6.0f * S);
    }
    const auto c = at (kLed.x + (on ? 17.0f : -17.0f), kLed.y);
    const auto circle = [&] (float r) { return juce::Rectangle<float> (r * 2.0f * S, r * 2.0f * S).withCentre (c); };
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (circle (15.0f).translated (2.0f * S, 4.0f * S));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffafbfd), c.x - 6.0f * S, c.y - 8.0f * S,
                                             juce::Colour (0xff6b6f7a), c.x + 9.0f * S, c.y + 11.0f * S, true));
    g.fillEllipse (circle (14.0f));
}

// the TAP pad is its own light (flat, see style.md "Flat style"): a flat pad inside the ring of the background, bright red on the beat and
// when tapped, dark red otherwise
void drawTapPad (juce::Graphics& g, bool lit)
{
    const auto pad = juce::Rectangle<float> (104.0f * S, 50.0f * S).withCentre (at (kTapPad.x, kTapPad.y));
    g.setColour (lit ? juce::Colour (0xffe5432c) : juce::Colour (0xff3a1411));
    g.fillRoundedRectangle (pad, 8.0f * S);
}
} // namespace

//==============================================================================
void DM1LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float,
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
DM1AudioProcessorEditor::DM1AudioProcessorEditor (DM1AudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&laf);
    background = juce::ImageFileFormat::loadFrom (DM1Data::background_png, (size_t) DM1Data::background_pngSize);

    addKnob (modRate,     "modRate");
    addKnob (modDepth,    "modDepth");
    addKnob (variation,   "variation");
    addKnob (delayTime,   "delayTime");
    addKnob (intensity,   "intensity");
    addKnob (delayVolume, "delayVolume");
    addKnob (mode,        "mode");

    // 12 printed positions at 30 degree steps, Classic at 12 o'clock, clockwise
    mode.slider.setRotaryParameters (0.0f, juce::degreesToRadians (330.0f), true);
    mode.slider.setRange (1.0, 12.0, 1.0);
    // the printed min/max marks sit at about +-154 degrees on the other knobs
    for (auto* k : { &modRate, &modDepth, &variation, &delayTime, &intensity, &delayVolume })
        k->slider.setRotaryParameters (juce::degreesToRadians (-154.0f), juce::degreesToRadians (154.0f), true);

    powerZone.onClick    = [this] { togglePower(); };
    tapZone.onDown       = [this] { proc.tapTempo(); };
    divisionZone.onClick = [this] { cycleDivision(); };
    memoryZone.onDown    = [this] { memHeld = true; memWritten = false; memDownAt = juce::Time::getMillisecondCounter(); };
    memoryZone.onUp      = [this] { memHeld = false; };
    memoryZone.onClick   = [this] { if (! memWritten) memory.stepNext(); repaint(); };      // short press
    for (int i = 0; i < 5; ++i)      // clicking a lamp selects it: a memory slot (MANUAL, 1-4) or a tap division
    {
        memLedZone[i].onClick = [this, i] { memory.select (i); repaint(); };
        divLedZone[i].onClick = [this, i]
        {
            auto* prm = proc.apvts.getParameter ("tapDivision");
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 ((float) (i + 1)));
            prm->endChangeGesture();
        };
        addAndMakeVisible (memLedZone[i]);
        addAndMakeVisible (divLedZone[i]);
    }
    for (auto* z : { &powerZone, &tapZone, &divisionZone, &memoryZone })
        addAndMakeVisible (z);

    setSize (kW, kH);
    timerCallback();
    startTimerHz (30);
}

DM1AudioProcessorEditor::~DM1AudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void DM1AudioProcessorEditor::addKnob (Knob& k, const juce::String& paramId)
{
    k.slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    k.slider.setMouseDragSensitivity (180);
    k.slider.setScrollWheelEnabled (false);
    addAndMakeVisible (k.slider);
    k.attachment = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter (paramId), k.slider, nullptr);
}

void DM1AudioProcessorEditor::togglePower()
{
    auto* prm = proc.apvts.getParameter ("power");
    prm->beginChangeGesture();
    prm->setValueNotifyingHost (prm->getValue() > 0.5f ? 0.0f : 1.0f);
    prm->endChangeGesture();
}

void DM1AudioProcessorEditor::cycleDivision()
{
    auto* prm = proc.apvts.getParameter ("tapDivision");
    const int next = division >= 5 ? 1 : division + 1;
    prm->beginChangeGesture();
    prm->setValueNotifyingHost (prm->convertTo0to1 ((float) next));
    prm->endChangeGesture();
}

void DM1AudioProcessorEditor::resized()
{
    auto place = [] (Knob& k, juce::Point<float> d)
    {
        const float full = 2.0f * (kKnobRadius * S + kPad);
        k.slider.setBounds (juce::Rectangle<float> (full, full).withCentre (at (d.x, d.y)).toNearestInt());
    };
    place (modRate, kModRate);     place (modDepth, kModDepth);   place (variation, kVariation);
    place (delayTime, kDelayTime); place (intensity, kIntensity); place (delayVolume, kDelayVol);
    place (mode, kMode);

    const auto sq = [] (juce::Point<float> d, float w, float h) { return juce::Rectangle<float> (w * S, h * S).withCentre (at (d.x, d.y)).toNearestInt(); };
    powerZone.setBounds (sq (kLed, 110.0f, 70.0f));
    tapZone.setBounds (sq (kTapPad, 112.0f, 72.0f));
    divisionZone.setBounds (sq (kDivisionButton, 140.0f, 80.0f));
    memoryZone.setBounds (sq (kMemoryButton, 140.0f, 80.0f));
    for (int i = 0; i < 5; ++i)
    {
        memLedZone[i].setBounds (sq ({ kMemLedX[i], kStripLedY }, 50.0f, 50.0f));
        divLedZone[i].setBounds (sq ({ kDivisionLedX[i], kStripLedY }, 50.0f, 50.0f));
    }
}

void DM1AudioProcessorEditor::timerCallback()
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
        repaint();
    }

    const bool p = proc.apvts.getRawParameterValue ("power")->load() > 0.5f;
    const bool t = proc.tapLedOn.load();
    const int d = juce::jlimit (1, 5, (int) std::round (proc.apvts.getRawParameterValue ("tapDivision")->load()));
    if (p != powerOn || t != tapOn || d != division)
    {
        powerOn = p;
        tapOn = t;
        division = d;
        repaint();
    }
}

void DM1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (background, 0, 0, kW, kH, 0, 0, background.getWidth(), background.getHeight());

    // tap-division LEDs follow the Tap Division parameter
    for (int i = 0; i < 5; ++i)
        drawStripLed (g, { kDivisionLedX[i], kStripLedY }, division == i + 1);
    if (divisionZone.isDown())
    {
        g.setColour (juce::Colours::black.withAlpha (0.3f));
        g.fillRoundedRectangle (juce::Rectangle<float> (118.0f * S, 62.0f * S).withCentre (at (kDivisionButton.x, kDivisionButton.y)), 4.0f);
    }

    // memory LEDs: MANUAL, 1-4; all five flash green after a write
    const bool memFlashing = juce::Time::getMillisecondCounter() < memFlashUntil;
    const bool blinkOff = juce::Time::getMillisecondCounter() < memFlashUntil && ((juce::Time::getMillisecondCounter() / 100) % 2) == 1;
    for (int i = 0; i < 5; ++i)
        drawStripLed (g, { kMemLedX[i], kStripLedY }, (memFlashing ? ! blinkOff : memory.current() == i), true);
    if (memoryZone.isDown())
    {
        g.setColour (juce::Colours::black.withAlpha (0.3f));
        g.fillRoundedRectangle (juce::Rectangle<float> (118.0f * S, 62.0f * S).withCentre (at (kMemoryButton.x, kMemoryButton.y)), 4.0f);
    }

    drawPowerLed (g, powerOn);
    drawTapPad (g, tapOn || tapZone.isDown());
}
