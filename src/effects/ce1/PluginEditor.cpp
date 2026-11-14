// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "CE1Data.h"

namespace
{
// Positions are in the coordinates of the rendered background (2200 x 890, see make_background.py);
// the editor shows it at 1100 x 383 and paint() scales them with S.
constexpr int kW = 1100, kH = 445;
constexpr float S = (float) kW / 2200.0f;

juce::Point<float> at (float x, float y) { return { x * S, y * S }; }

const juce::Point<float> kLevel { 400.0f, 438.0f }, kChorus { 835.0f, 438.0f }, kDepth { 1280.0f, 438.0f }, kRate { 1620.0f, 438.0f };
constexpr float kKnobRadius = 72.0f;
constexpr float kPad = 3.0f;

const juce::Point<float> kLever { 160.0f, 438.0f };                                   // input HIGH / LOW
const juce::Point<float> kPeakLed { 160.0f, 548.0f };
const juce::Point<float> kEffectButton { 1980.0f, 458.0f };                           // chorus / vibrato
const juce::Point<float> kPowerSwitch { 2125.0f, 87.0f };                             // power slide switch (ball left = off)

// Tempo sync (vibrato rate only): a SYNC button in the VIBRATO panel under the RATE knob, below a divider line drawn in the background.
// While sync is on the RATE knob selects the note division (it is attached to "division") and the division is shown next to the button.
const auto kSyncPill     = juce::Rectangle<float> (116.0f, 46.0f).withCentre ({ 1548.0f, 698.0f });
const auto kDivisionText = juce::Rectangle<float> (130.0f, 46.0f).withCentre ({ 1677.0f, 698.0f });
const juce::Colour kBody (0xff20b6a5), kPanel (0xff18191c);

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
// Black knurled knob with a raised cap and a white pointer (same as the DM-1).
void CE1LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float,
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
CE1AudioProcessorEditor::CE1AudioProcessorEditor (CE1AudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      syncAtt (*p.apvts.getParameter ("sync"), [this] (float v) { syncOn = v > 0.5f; syncChanged(); }, nullptr),
      divisionAtt (*p.apvts.getParameter ("division"), [this] (float v) { division = (int) std::lround (v); repaint(); }, nullptr)
{
    setLookAndFeel (&laf);

    background = juce::ImageFileFormat::loadFrom (CE1Data::background_png, (size_t) CE1Data::background_pngSize);

    addKnob (level,  "level");
    addKnob (chorus, "chorusIntensity");
    addKnob (depth,  "vibratoDepth");
    addKnob (rate,   "vibratoRate");
    midiUI.add (level.slider, "level");  midiUI.add (chorus.slider, "chorusIntensity");
    midiUI.add (depth.slider, "vibratoDepth");  midiUI.add (rate.slider, "vibratoRate");
    addSwitch (inputSwitch,  "inputHigh");
    addSwitch (effectSwitch, "vibrato");
    addSwitch (powerSwitch,  "power");
    syncAtt.sendInitialUpdate();
    divisionAtt.sendInitialUpdate();

    setSize (kW, kH);
    startTimerHz (30);
}

CE1AudioProcessorEditor::~CE1AudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void CE1AudioProcessorEditor::addKnob (Knob& k, const juce::String& paramId)
{
    k.slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    k.slider.setRotaryParameters (juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
    k.slider.setMouseDragSensitivity (180);
    k.slider.setScrollWheelEnabled (false);
    addAndMakeVisible (k.slider);
    k.attachment = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter (paramId), k.slider, nullptr);
}

void CE1AudioProcessorEditor::addSwitch (Switch& s, const juce::String& paramId)
{
    addAndMakeVisible (s.button);
    s.attachment = std::make_unique<juce::ButtonParameterAttachment> (*proc.apvts.getParameter (paramId), s.button);
    s.button.onStateChange = [this] { repaint(); };
}

void CE1AudioProcessorEditor::resized()
{
    auto place = [] (Knob& k, juce::Point<float> c)
    {
        const float d = 2.0f * (kKnobRadius * S + kPad);
        k.slider.setBounds (juce::Rectangle<float> (d, d).withCentre (at (c.x, c.y)).toNearestInt());
    };
    place (level, kLevel);  place (chorus, kChorus);  place (depth, kDepth);  place (rate, kRate);

    const auto sq = [] (juce::Point<float> c, float w, float h) { return juce::Rectangle<float> (w * S, h * S).withCentre (at (c.x, c.y)).toNearestInt(); };
    inputSwitch.button.setBounds (sq (kLever, 90.0f, 110.0f));
    powerSwitch.button.setBounds (sq (kPowerSwitch, 110.0f, 70.0f));
    effectSwitch.button.setBounds (sq (kEffectButton, 110.0f, 150.0f));
}

void CE1AudioProcessorEditor::syncChanged()
{
    // the RATE knob is the vibrato rate, or the note division while sync is on (the attachment sets the slider range from the parameter)
    rate.attachment.reset();
    divisionKnobAtt.reset();
    if (syncOn) divisionKnobAtt = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter ("division"), rate.slider, nullptr);
    else        rate.attachment = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter ("vibratoRate"), rate.slider, nullptr);
    repaint();
}

void CE1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    if (kSyncPill.expanded (4.0f).transformedBy (juce::AffineTransform::scale (S)).contains (e.position))
        syncAtt.setValueAsCompleteGesture (syncOn ? 0.0f : 1.0f);
}

void CE1AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    midiUI.paintMarks (g, kBody);
}

void CE1AudioProcessorEditor::timerCallback()
{
    if (midiUI.needsRepaint()) { midiUI.tick(); repaint(); }
    if (proc.inputPeak.exchange (0.0f) > 0.9f) peakHold = 8;
    const bool lit = peakHold > 0;
    if (peakHold > 0) --peakHold;
    if (lit != peakLit)
    {
        peakLit = lit;
        repaint (juce::Rectangle<float> (80.0f * S, 80.0f * S).withCentre (at (kPeakLed.x, kPeakLed.y)).toNearestInt());
    }
}

//==============================================================================
void CE1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (background, 0, 0, kW, kH, 0, 0, background.getWidth(), background.getHeight());
    g.addTransform (juce::AffineTransform::scale (S));      // from here on: background pixels

    // input HIGH / LOW lever: chrome ball in the slot (top = HIGH)
    {
        const bool high = inputSwitch.button.getToggleState();
        const juce::Point<float> ball (kLever.x, kLever.y + (high ? -17.0f : 17.0f));
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillEllipse (juce::Rectangle<float> (30.0f, 30.0f).withCentre (ball + juce::Point<float> (2.0f, 4.0f)));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffafbfd), ball.x - 6.0f, ball.y - 8.0f,
                                                 juce::Colour (0xff6b6f7a), ball.x + 9.0f, ball.y + 11.0f, true));
        g.fillEllipse (juce::Rectangle<float> (28.0f, 28.0f).withCentre (ball));
    }

    drawLamp (g, kPeakLed, peakLit, juce::Colour (0xffff2a1a));

    // chorus / vibrato lever: chrome ball in the slot (top = chorus, bottom = vibrato)
    {
        const bool vib = effectSwitch.button.getToggleState();
        const juce::Point<float> ball (kEffectButton.x, kEffectButton.y + (vib ? 17.0f : -17.0f));
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillEllipse (juce::Rectangle<float> (30.0f, 30.0f).withCentre (ball + juce::Point<float> (2.0f, 4.0f)));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffafbfd), ball.x - 6.0f, ball.y - 8.0f,
                                                 juce::Colour (0xff6b6f7a), ball.x + 9.0f, ball.y + 11.0f, true));
        g.fillEllipse (juce::Rectangle<float> (28.0f, 28.0f).withCentre (ball));
    }

    // SYNC: a flat rounded button, dark with a body-colour border when off, filled body colour when on
    {
        const float rad = kSyncPill.getHeight() * 0.5f;
        g.setColour (syncOn ? kBody : kPanel.darker (0.4f));
        g.fillRoundedRectangle (kSyncPill, rad);
        g.setColour (kBody);
        g.drawRoundedRectangle (kSyncPill.reduced (1.0f), rad, 2.0f);
        g.setFont (juce::Font (juce::FontOptions ("Avenir Next", 24.0f, juce::Font::bold)));
        g.setColour (syncOn ? juce::Colours::white : kBody);
        g.drawText ("SYNC", kSyncPill, juce::Justification::centred, false);

        if (syncOn)
        {
            g.setColour (juce::Colours::white);
            g.setFont (juce::Font (juce::FontOptions ("Avenir Next", 34.0f, juce::Font::bold)));
            g.drawText (vibr::sync::divisionNames()[juce::jlimit (0, vibr::sync::kNumDivisions - 1, division)], kDivisionText, juce::Justification::centredLeft, false);
        }
    }

    // power slide switch: chrome ball left = off, right = on
    {
        const bool on = powerSwitch.button.getToggleState();
        if (on)      // the slot lights up green (subtle)
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
