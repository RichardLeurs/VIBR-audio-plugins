// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "BS1Data.h"

namespace
{
// Positions are in the 1786 x 1048 coordinates of the rendered background (make_background.py). The editor is resizable at a
// fixed aspect ratio; paint() scales image pixels to the window.
constexpr float kImgW = 1786.0f, kImgH = 1048.0f;
constexpr int kBaseW = 893;

constexpr float kKnobRadius = 64.0f, kTypeRadius = 80.0f;
constexpr float kPad = 3.0f;
struct KnobSpec { const char* id; float x, y; };
const float kCols[4] = { 851.0f, 1101.0f, 1351.0f, 1601.0f };
const float kRows[2] = { 400.0f, 715.0f };
const KnobSpec kKnobs[8] = {
    { "value", kCols[0], kRows[0] }, { "decay", kCols[1], kRows[0] }, { "predelay", kCols[2], kRows[0] }, { "mix", kCols[3], kRows[0] },
    { "tone", kCols[0], kRows[1] }, { "param1", kCols[1], kRows[1] }, { "param2", kCols[2], kRows[1] }, { "mod", kCols[3], kRows[1] } };
constexpr float kStartDeg = -135.0f, kEndDeg = 135.0f;
const juce::Point<float> kTypeKnob { 363.0f, 624.0f };            // 12 positions, ROOM at the bottom, clockwise
const juce::Rectangle<float> kDisplay { 203.0f, 308.0f, 320.0f, 64.0f };
const juce::Point<float> kLed { 1711.0f, 87.0f };                 // power slide switch (top right, ball left = off) = bypass

juce::Point<float> polar (juce::Point<float> c, float r, float a)
{
    return { c.x + r * std::sin (a), c.y - r * std::cos (a) };
}
} // namespace

//==============================================================================
void BigSkyLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float,
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
BigSkyAudioProcessorEditor::BigSkyAudioProcessorEditor (BigSkyAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&laf);
    background = juce::ImageFileFormat::loadFrom (BS1Data::background_png, (size_t) BS1Data::background_pngSize);

    const auto setup = [this] (Knob& k, const char* id, float startDeg, float endDeg)
    {
        k.slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        k.slider.setRotaryParameters (juce::degreesToRadians (startDeg), juce::degreesToRadians (endDeg), true);
        k.slider.setMouseDragSensitivity (180);
        k.slider.setScrollWheelEnabled (false);
        addAndMakeVisible (k.slider);
        k.attachment = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter (id), k.slider, nullptr);
    };
    for (int i = 0; i < 8; ++i)
    {
        setup (knobs[i], kKnobs[i].id, kStartDeg, kEndDeg);
        lastVals[i] = proc.apvts.getRawParameterValue (kKnobs[i].id)->load();
    }
    setup (typeKnob, "type", 180.0f, 510.0f);                    // ROOM at the bottom, 30 degrees per type, clockwise
    typeKnob.slider.wrapSteps = bigsky::kNumTypes;               // turns endlessly
    lastType = (int) std::lround (proc.apvts.getRawParameterValue ("type")->load());

    constrainer.setFixedAspectRatio ((double) kImgW / (double) kImgH);
    constrainer.setSizeLimits (447, 262, 1786, 1048);
    setConstrainer (&constrainer);
    setResizable (true, true);
    setSize (kBaseW, (int) std::round ((float) kBaseW * kImgH / kImgW));
    startTimerHz (20);
}

BigSkyAudioProcessorEditor::~BigSkyAudioProcessorEditor() { setLookAndFeel (nullptr); }

void BigSkyAudioProcessorEditor::resized()
{
    const float S = (float) getWidth() / kImgW;
    for (int i = 0; i < 8; ++i)
        knobs[i].slider.setBounds (juce::Rectangle<float> (2.0f * kKnobRadius * S, 2.0f * kKnobRadius * S)
                                       .withCentre ({ kKnobs[i].x * S, kKnobs[i].y * S }).toNearestInt());
    typeKnob.slider.setBounds (juce::Rectangle<float> (2.0f * kTypeRadius * S, 2.0f * kTypeRadius * S)
                                   .withCentre ({ kTypeKnob.x * S, kTypeKnob.y * S }).toNearestInt());
}

void BigSkyAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
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

juce::String BigSkyAudioProcessorEditor::formatValue (int i) const
{
    const float v = proc.apvts.getRawParameterValue (kKnobs[i].id)->load();
    const juce::String id (kKnobs[i].id);
    if (id == "decay")
    {
        const float s = bigsky::decaySeconds (lastType, v * 0.1f);
        return s < 1.0f ? juce::String ((int) std::lround (s * 1000.0f)) + "mS" : juce::String (s, 1) + "S";
    }
    if (id == "predelay")
        return juce::String ((int) std::lround (bigsky::predelayMs (v * 0.1f))) + "mS";
    static const char* lab[8] = { "VL", "", "", "MX", "TN", "P1", "P2", "MD" };
    return juce::String (lab[i]) + " " + juce::String (v, 1);
}

void BigSkyAudioProcessorEditor::timerCallback()
{
    const int t = (int) std::lround (proc.apvts.getRawParameterValue ("type")->load());
    const bool on = proc.apvts.getRawParameterValue ("bypass")->load() < 0.5f;
    bool dirty = on != lastOn;
    lastOn = on;
    if (t != lastType) { lastType = t; shown = {}; shownUntil = 0; dirty = true; }
    for (int i = 0; i < 8; ++i)
    {
        const float v = proc.apvts.getRawParameterValue (kKnobs[i].id)->load();
        if (std::fabs (v - lastVals[i]) > 1.0e-4f)
        {
            lastVals[i] = v;
            shown = formatValue (i);
            shownUntil = juce::Time::getMillisecondCounter() + 1500;
            dirty = true;
        }
    }
    if (shownUntil != 0 && (juce::int32) (juce::Time::getMillisecondCounter() - shownUntil) > 0)
    {
        shown = {}; shownUntil = 0; dirty = true;
    }
    if (dirty) repaint();
}

void BigSkyAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (background, 0, 0, getWidth(), getHeight(), 0, 0, background.getWidth(), background.getHeight());
    g.addTransform (juce::AffineTransform::scale ((float) getWidth() / kImgW));      // image pixels from here on

    // ---- display: six cells, green characters; the type name, or the value of the knob being turned ----
    {
        const auto disp = kDisplay.reduced (6.0f);
        const juce::String text = shown.isNotEmpty() ? shown : juce::String (bigsky::typeName (lastType));
        const float cellW = disp.getWidth() / 6.0f;
        juce::Font font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 44.0f, juce::Font::bold));
        g.setFont (font);
        const juce::String padded = text.paddedRight (' ', 6).substring (0, 6);
        for (int i = 0; i < 6; ++i)
        {
            const auto cell = juce::Rectangle<float> (disp.getX() + cellW * (float) i, disp.getY(), cellW, disp.getHeight());
            g.setColour (juce::Colour (0xff7cff2a).withAlpha (0.05f));
            g.drawText ("8", cell, juce::Justification::centred, false);
            const auto ch = padded.substring (i, i + 1);
            if (ch.trim().isEmpty()) continue;
            g.setColour (juce::Colour (0xff7cff2a).withAlpha (0.22f));
            for (auto o : { juce::Point<float> (-1.5f, 0), juce::Point<float> (1.5f, 0), juce::Point<float> (0, -1.5f), juce::Point<float> (0, 1.5f) })
                g.drawText (ch, cell.translated (o.x, o.y), juce::Justification::centred, false);
            g.setColour (juce::Colour (0xffb8ff6a));
            g.drawText (ch, cell, juce::Justification::centred, false);
        }
    }

    // ---- power slide switch: chrome ball left = off, right = on; the slot lights up green when on ----
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
}
