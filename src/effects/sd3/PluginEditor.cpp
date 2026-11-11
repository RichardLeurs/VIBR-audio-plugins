// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "SD3Data.h"

namespace
{
// Positions are in the 2740 x 900 coordinates of the rendered background (make_background.py). The editor is resizable at a
// fixed aspect ratio; paint() scales image pixels to the window.
constexpr float kImgW = 2740.0f, kImgH = 900.0f;
constexpr int kBaseW = 1370;

constexpr float kKnobRadius = 60.0f, kPad = 3.0f, kKnobY = 500.0f;
const juce::Point<float> kLevel { 360.0f, kKnobY }, kFeedback { 1370.0f, kKnobY }, kWaveform { 1845.0f, kKnobY },
                         kIntensity { 2105.0f, kKnobY }, kBalance { 2400.0f, kKnobY };

const juce::Point<float> kAttIn { 360.0f, 692.0f }, kAttOut { 2400.0f, 692.0f };       // 3-position slide switches
constexpr float kFilterX = 1596.0f; const float kFilterY[4] = { 390.0f, 462.0f, 534.0f, 606.0f };

const juce::Rectangle<float> kDisplay { 560.0f, 330.0f, 580.0f, 110.0f };
const juce::Point<float> kProgramDigit { 610.0f, 385.0f };
const float kMsDigitX[4] = { 800.0f, 900.0f, 1000.0f, 1100.0f };
constexpr float kDigitW = 54.0f, kDigitH = 84.0f;

constexpr float kKeyY = 590.0f;
const juce::Point<float> kWrite { 620.0f, kKeyY }, kProg { 770.0f, kKeyY }, kDown { 920.0f, kKeyY }, kUp { 1070.0f, kKeyY };
const juce::Point<float> kInvLamp { 610.0f, 700.0f }, kInv { 700.0f, 700.0f }, kHoldLamp { 900.0f, 700.0f }, kHold { 990.0f, 700.0f };
const juce::Point<float> kBypassLamp { 2610.0f, 378.0f }, kBypass { 2610.0f, 440.0f }, kInvOutLamp { 2610.0f, 568.0f }, kInvOut { 2610.0f, 630.0f };
const juce::Point<float> kPower { 2665.0f, 87.0f };                                   // power slide switch (top right)

const juce::Rectangle<float> kMeter { 94.0f, 340.0f, 36.0f, 336.0f };      // headroom meter frame, six segments
const float kMeterThreshold[6] = { 6.0f, 0.0f, -12.0f, -20.0f, -30.0f, -40.0f };

juce::Point<float> polar (juce::Point<float> c, float r, float a)
{
    return { c.x + r * std::sin (a), c.y - r * std::cos (a) };
}

// seven-segment digit; digit < 0 = blank; segment order a b c d e f g
void drawSegDigit (juce::Graphics& g, juce::Point<float> c, float w, float h, int digit, juce::Colour on, juce::Colour off, bool dot)
{
    static const int masks[10] = { 0b0111111, 0b0000110, 0b1011011, 0b1001111, 0b1100110, 0b1101101, 0b1111101, 0b0000111, 0b1111111, 0b1101111 };
    const int mask = digit >= 0 ? masks[digit] : 0;
    const float t = h * 0.12f, hl = w * 0.62f, vl = h * 0.40f;

    const auto seg = [&] (int bit, juce::Rectangle<float> r)
    {
        const bool lit = (mask >> bit) & 1;
        if (lit)
        {
            g.setColour (on.withAlpha (0.28f));
            g.fillRoundedRectangle (r.expanded (1.6f), t * 0.7f);
        }
        g.setColour (lit ? on : off);
        g.fillRoundedRectangle (r, t * 0.45f);
    };
    const float x = c.x, y = c.y;
    seg (0, juce::Rectangle<float> (hl, t).withCentre ({ x, y - h * 0.46f + t * 0.5f }));              // a
    seg (3, juce::Rectangle<float> (hl, t).withCentre ({ x, y + h * 0.46f - t * 0.5f }));              // d
    seg (6, juce::Rectangle<float> (hl, t).withCentre ({ x, y }));                                    // g
    seg (5, juce::Rectangle<float> (t, vl).withCentre ({ x - w * 0.38f, y - h * 0.22f }));            // f
    seg (1, juce::Rectangle<float> (t, vl).withCentre ({ x + w * 0.38f, y - h * 0.22f }));            // b
    seg (4, juce::Rectangle<float> (t, vl).withCentre ({ x - w * 0.38f, y + h * 0.22f }));            // e
    seg (2, juce::Rectangle<float> (t, vl).withCentre ({ x + w * 0.38f, y + h * 0.22f }));            // c
    if (dot)
    {
        g.setColour (on);
        g.fillEllipse (juce::Rectangle<float> (t * 1.3f, t * 1.3f).withCentre ({ x + w * 0.62f, y + h * 0.46f - t * 0.4f }));
    }
}

void drawLed (juce::Graphics& g, juce::Point<float> c, float r, juce::Colour colour, bool on)
{
    g.setColour (juce::Colour (0xff0c0e14));
    g.fillEllipse (juce::Rectangle<float> (r * 2.0f + 3.0f, r * 2.0f + 3.0f).withCentre (c));
    if (on)
    {
        g.setColour (colour.withAlpha (0.35f));
        g.fillEllipse (juce::Rectangle<float> (r * 3.6f, r * 3.6f).withCentre (c));
    }
    g.setGradientFill (on ? juce::ColourGradient (colour.brighter (0.7f), c.x - r * 0.4f, c.y - r * 0.5f, colour.darker (0.2f), c.x + r * 0.6f, c.y + r * 0.7f, true)
                          : juce::ColourGradient (colour.darker (1.8f), c.x - r * 0.4f, c.y - r * 0.5f, colour.darker (3.0f), c.x + r * 0.6f, c.y + r * 0.7f, true));
    g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
    g.setColour (juce::Colours::white.withAlpha (on ? 0.55f : 0.18f));
    g.fillEllipse (juce::Rectangle<float> (r * 0.8f, r * 0.55f).withCentre (c.translated (-r * 0.3f, -r * 0.45f)));
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

void drawBall (juce::Graphics& g, juce::Point<float> c)
{
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (juce::Rectangle<float> (30.0f, 30.0f).withCentre (c + juce::Point<float> (2.0f, 4.0f)));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffafbfd), c.x - 6.0f, c.y - 8.0f,
                                             juce::Colour (0xff6b6f7a), c.x + 9.0f, c.y + 11.0f, true));
    g.fillEllipse (juce::Rectangle<float> (28.0f, 28.0f).withCentre (c));
}
} // namespace

//==============================================================================
void SDD3000LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float,
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
SDD3000AudioProcessorEditor::SDD3000AudioProcessorEditor (SDD3000AudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&laf);
    background = juce::ImageFileFormat::loadFrom (SD3Data::background_png, (size_t) SD3Data::background_pngSize);

    addKnob (level,     "level",     kLevel,     60.0f);
    addKnob (feedback,  "feedback",  kFeedback,  60.0f);
    addKnob (waveform,  "waveform",  kWaveform,  60.0f);
    addKnob (intensity, "intensity", kIntensity, 60.0f);
    addKnob (balance,   "balance",   kBalance,   60.0f);
    waveform.slider.setRotaryParameters (juce::degreesToRadians (-70.0f), juce::degreesToRadians (70.0f), true);     // 5 positions, 35 degrees apart

    addZone (attInZone,  [this] (juce::Point<float> pt) { setInt ("attIn",  juce::jlimit (0, 2, (int) (pt.x / ((float) attInZone.getWidth() / 3.0f)))); });
    addZone (attOutZone, [this] (juce::Point<float> pt) { setInt ("attOut", juce::jlimit (0, 2, (int) (pt.x / ((float) attOutZone.getWidth() / 3.0f)))); });
    addZone (filterZone, [this] (juce::Point<float> pt) { setInt ("filter", juce::jlimit (0, 3, (int) (pt.y / ((float) filterZone.getHeight() / 4.0f)))); });
    addZone (writeZone,  [this] (juce::Point<float>) { proc.writeProgram(); writeFlash = 12; repaint(); });
    addZone (progZone,   [this] (juce::Point<float>) { proc.selectNextProgram(); });
    addZone (downZone,   [this] (juce::Point<float>) { heldFrames = 0; stepDelay (-1); });
    addZone (upZone,     [this] (juce::Point<float>) { heldFrames = 0; stepDelay (+1); });
    addZone (invZone,    [this] (juce::Point<float>) { toggle ("invFeedback"); });
    addZone (holdZone,   [this] (juce::Point<float>) { toggle ("hold"); });
    addZone (bypassZone, [this] (juce::Point<float>) { toggle ("bypass"); });
    addZone (invOutZone, [this] (juce::Point<float>) { toggle ("invOut"); });
    addZone (powerZone,  [this] (juce::Point<float>) { toggle ("power"); });

    constrainer.setFixedAspectRatio ((double) kImgW / (double) kImgH);
    constrainer.setSizeLimits (685, 225, 2740, 900);
    setConstrainer (&constrainer);
    setResizable (true, true);
    setSize (kBaseW, (int) std::round ((float) kBaseW * kImgH / kImgW));
    startTimerHz (30);
}

SDD3000AudioProcessorEditor::~SDD3000AudioProcessorEditor() { setLookAndFeel (nullptr); }

void SDD3000AudioProcessorEditor::addKnob (Knob& k, const juce::String& paramId, juce::Point<float> centre, float radius, bool)
{
    k.centre = centre;
    k.radius = radius;
    k.slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    k.slider.setRotaryParameters (juce::degreesToRadians (-154.0f), juce::degreesToRadians (154.0f), true);
    k.slider.setMouseDragSensitivity (180);
    k.slider.setScrollWheelEnabled (false);
    addAndMakeVisible (k.slider);
    k.attachment = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter (paramId), k.slider, nullptr);
}

void SDD3000AudioProcessorEditor::addZone (Zone& z, std::function<void (juce::Point<float>)> onPress)
{
    z.onPress = std::move (onPress);
    addAndMakeVisible (z);
}

void SDD3000AudioProcessorEditor::setInt (const char* id, int v)
{
    auto* prm = proc.apvts.getParameter (id);
    prm->beginChangeGesture();
    prm->setValueNotifyingHost (prm->convertTo0to1 ((float) v));
    prm->endChangeGesture();
}

void SDD3000AudioProcessorEditor::toggle (const char* id)
{
    auto* prm = proc.apvts.getParameter (id);
    prm->beginChangeGesture();
    prm->setValueNotifyingHost (prm->getValue() > 0.5f ? 0.0f : 1.0f);
    prm->endChangeGesture();
}

void SDD3000AudioProcessorEditor::stepDelay (int delta)
{
    setInt ("delayMs", juce::jlimit (1, 1999, (int) std::round (get ("delayMs")) + delta));
}

//==============================================================================
void SDD3000AudioProcessorEditor::resized()
{
    const float S = (float) getWidth() / kImgW;
    const auto box = [S] (juce::Rectangle<float> r) { return juce::Rectangle<float> (r.getX() * S, r.getY() * S, r.getWidth() * S, r.getHeight() * S).toNearestInt(); };
    const auto around = [&] (juce::Point<float> c, float w, float h) { return box (juce::Rectangle<float> (w, h).withCentre (c)); };

    for (auto* k : { &level, &feedback, &waveform, &intensity, &balance })
        k->slider.setBounds (around (k->centre, 2.0f * k->radius, 2.0f * k->radius));

    attInZone.setBounds (around (kAttIn, 130.0f, 44.0f));
    attOutZone.setBounds (around (kAttOut, 130.0f, 44.0f));
    filterZone.setBounds (box (juce::Rectangle<float> (60.0f, kFilterY[3] - kFilterY[0] + 72.0f).withCentre ({ kFilterX, (kFilterY[0] + kFilterY[3]) * 0.5f })));
    for (auto z : { std::pair<Zone*, juce::Point<float>> { &writeZone, kWrite }, { &progZone, kProg }, { &downZone, kDown }, { &upZone, kUp },
                    { &invZone, kInv }, { &holdZone, kHold }, { &bypassZone, kBypass }, { &invOutZone, kInvOut } })
        z.first->setBounds (around (z.second, 110.0f, 64.0f));
    powerZone.setBounds (around (kPower, 110.0f, 70.0f));
}

void SDD3000AudioProcessorEditor::timerCallback()
{
    // UP / DOWN auto-repeat while held: slow at first, then faster
    if (upZone.isDown() || downZone.isDown())
    {
        ++heldFrames;
        if (heldFrames > 12)
            stepDelay ((upZone.isDown() ? 1 : -1) * (heldFrames > 45 ? 8 : 1));
    }

    if (writeFlash > 0) { --writeFlash; repaint(); }                  // display blink after WRITE

    const float db = juce::Decibels::gainToDecibels (proc.meterLevel.load(), -60.0f);
    meterDb += (db - meterDb) * 0.5f;

    // repaint the whole panel's live parts when any control changed; the meter always
    juce::String state;
    for (auto id : { "attIn", "attOut", "delayMs", "program", "filter", "invFeedback", "hold", "bypass", "invOut", "power" })
        state << juce::String (get (id), 1) << ",";
    if (state != lastState)
    {
        lastState = state;
        repaint();
    }
    else
    {
        const float S = (float) getWidth() / kImgW;
        repaint (juce::Rectangle<float> ((kMeter.getX() - 4.0f) * S, (kMeter.getY() - 4.0f) * S, (kMeter.getWidth() + 8.0f) * S, (kMeter.getHeight() + 8.0f) * S).toNearestInt());
    }
}

//==============================================================================
void SDD3000AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (background, 0, 0, getWidth(), getHeight(), 0, 0, background.getWidth(), background.getHeight());
    g.addTransform (juce::AffineTransform::scale ((float) getWidth() / kImgW));      // from here on: image pixels

    // ---- headroom meter: six LED segments in the frame, top (+6 dB) to bottom (-40 dB), like the SD-1
    {
        const float segH = kMeter.getHeight() / 6.0f;
        for (int i = 0; i < 6; ++i)
        {
            const bool lit = meterDb >= kMeterThreshold[i];
            const juce::Colour base = i == 0 ? juce::Colour (0xffe03a2a) : (i <= 2 ? juce::Colour (0xffe8b820) : juce::Colour (0xff35c46a));
            const juce::Rectangle<float> seg (kMeter.getX() + 6.0f, kMeter.getY() + (float) i * segH + 5.0f, kMeter.getWidth() - 12.0f, segH - 10.0f);
            g.setColour (lit ? base : base.darker (2.6f).withAlpha (0.55f));
            g.fillRoundedRectangle (seg, 3.0f);
            if (lit)
            {
                g.setColour (base.withAlpha (0.28f));
                g.fillRoundedRectangle (seg.expanded (3.0f), 5.0f);
            }
        }
    }

    // ---- programmer display: program number (red) and delay time in ms (green)
    const juce::Colour redOn (0xffff2a3a), redOff (0xff2c0a10), grnOn (0xff38e83a), grnOff (0xff0b2a0d);
    drawSegDigit (g, kProgramDigit, 34.0f, kDigitH, (int) std::round (get ("program")), redOn, redOff, true);
    const int ms = juce::jlimit (0, 1999, (int) std::round (get ("delayMs")));
    const int digits[4] = { ms / 1000, (ms / 100) % 10, (ms / 10) % 10, ms % 10 };
    for (int i = 0; i < 4; ++i)
    {
        const bool blinkOff = writeFlash > 0 && ((writeFlash / 3) % 2 == 1);       // two short blinks of the stored time
        drawSegDigit (g, { kMsDigitX[i], kProgramDigit.y }, kDigitW, kDigitH, blinkOff ? 8 : digits[i], blinkOff ? grnOff : grnOn, grnOff, false);
    }

    // pressed look for the programmer keys
    for (auto z : { std::pair<Zone*, juce::Point<float>> { &writeZone, kWrite }, { &progZone, kProg }, { &downZone, kDown }, { &upZone, kUp },
                    { &invZone, kInv }, { &holdZone, kHold }, { &bypassZone, kBypass }, { &invOutZone, kInvOut } })
        if (z.first->isDown())
        {
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillRoundedRectangle (juce::Rectangle<float> (96.0f, 54.0f).withCentre (z.second), 11.0f);
        }

    // ---- switches: balls in their slots
    const int attIn = juce::jlimit (0, 2, (int) std::round (get ("attIn"))), attOut = juce::jlimit (0, 2, (int) std::round (get ("attOut")));
    drawBall (g, { kAttIn.x + (float) (attIn - 1) * 44.0f, kAttIn.y });
    drawBall (g, { kAttOut.x + (float) (attOut - 1) * 44.0f, kAttOut.y });
    drawBall (g, { kFilterX, kFilterY[juce::jlimit (0, 3, (int) std::round (get ("filter")))] });

    // ---- lamps
    drawLamp (g, kInvLamp,    get ("invFeedback") > 0.5f, juce::Colour (0xffffa020));
    drawLamp (g, kHoldLamp,   get ("hold") > 0.5f,        juce::Colour (0xffff2a1a));
    drawLamp (g, kBypassLamp, get ("bypass") > 0.5f,      juce::Colour (0xffff2a1a));
    drawLamp (g, kInvOutLamp, get ("invOut") > 0.5f,      juce::Colour (0xffffa020));

    // ---- power slide switch: chrome ball left = off, right = on; the slot lights up green when on
    const bool on = get ("power") > 0.5f;
    if (on)
    {
        const auto slot = juce::Rectangle<float> (66.0f, 34.0f).withCentre (kPower);
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
    const juce::Point<float> c (kPower.x + (on ? 17.0f : -17.0f), kPower.y);
    const auto circle = [&] (float r) { return juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c); };
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (circle (15.0f).translated (2.0f, 4.0f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffafbfd), c.x - 6.0f, c.y - 8.0f,
                                             juce::Colour (0xff6b6f7a), c.x + 9.0f, c.y + 11.0f, true));
    g.fillEllipse (circle (14.0f));
}
