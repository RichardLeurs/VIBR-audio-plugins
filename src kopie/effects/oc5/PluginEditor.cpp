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
constexpr float kPad = 3.0f;
constexpr float kKnobY = 532.0f;
struct KnobSpec { const char* id; float x; };
const KnobSpec kDirect { "direct", 230.0f }, kUp1 { "up1", 520.0f }, kDown1 { "down1", 810.0f }, kDown2 { "down2", 1210.0f };
constexpr float kStartDeg = -154.0f, kEndDeg = 154.0f;     // 11 ticks over 308 degrees

const juce::Point<float> kLed { 1765.0f, 87.0f };          // power slide switch (top right, ball left = off) = bypass
const juce::Point<float> kSwitch { 1610.0f, 532.0f };      // VINTAGE / POLY slide switch (vertical): ball up = VINTAGE, down = POLY

juce::Point<float> polar (juce::Point<float> c, float r, float a)
{
    return { c.x + r * std::sin (a), c.y - r * std::cos (a) };
}
} // namespace

//==============================================================================
void OC5LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float,
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
OC5AudioProcessorEditor::OC5AudioProcessorEditor (OC5AudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&laf);
    background = juce::ImageFileFormat::loadFrom (OC5Data::background_png, (size_t) OC5Data::background_pngSize);

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

    constrainer.setFixedAspectRatio ((double) kImgW / (double) kImgH);
    constrainer.setSizeLimits (460, 222, 1840, 890);
    setConstrainer (&constrainer);
    setResizable (true, true);
    setSize (kBaseW, (int) std::round ((float) kBaseW * kImgH / kImgW));
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
    const juce::Point<float> c (kLed.x + (on ? 17.0f : -17.0f), kLed.y);
    const auto circle = [&] (float r) { return juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c); };
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (circle (15.0f).translated (2.0f, 4.0f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffafbfd), c.x - 6.0f, c.y - 8.0f,
                                             juce::Colour (0xff6b6f7a), c.x + 9.0f, c.y + 11.0f, true));
    g.fillEllipse (circle (14.0f));

    // VINTAGE / POLY: chrome ball in the slot, up = VINTAGE, down = POLY
    const bool poly = proc.apvts.getRawParameterValue ("poly")->load() > 0.5f;
    const juce::Point<float> ball (kSwitch.x, kSwitch.y + (poly ? 40.0f : -40.0f));
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (juce::Rectangle<float> (30.0f, 30.0f).withCentre (ball + juce::Point<float> (2.0f, 4.0f)));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffafbfd), ball.x - 6.0f, ball.y - 8.0f,
                                             juce::Colour (0xff6b6f7a), ball.x + 9.0f, ball.y + 11.0f, true));
    g.fillEllipse (juce::Rectangle<float> (28.0f, 28.0f).withCentre (ball));
}
