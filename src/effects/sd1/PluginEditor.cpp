// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "SD1Data.h"

namespace
{
// Positions are in the 2520 x 918 coordinates of the rendered background (make_background.py). The editor is resizable at a
// fixed aspect ratio; paint() scales image pixels to the window.
constexpr float kImgW = 2520.0f, kImgH = 918.0f;
constexpr int kBaseW = 1260;

constexpr float kKnobRadius = 60.0f;                       // black knob; the printed scale sits outside it
constexpr float kKnobY = 500.0f;
struct KnobSpec { const char* id; float x; float startDeg, endDeg; };
const KnobSpec kKnobs[9] = {
    { "level",        360.0f, -154.0f, 154.0f }, { "delayTime",    635.0f, -130.0f, 50.0f },     // TIME only turns from SHORT to LONG
    { "factor",       845.0f, -154.0f, 154.0f }, { "feedback",    1135.0f, -154.0f, 154.0f },
    { "hiCut",       1345.0f, -154.0f, 154.0f }, { "modIntensity", 1635.0f, -154.0f, 154.0f },
    { "modFrequency", 1845.0f, -154.0f, 154.0f }, { "direct",     2135.0f, -154.0f, 154.0f },
    { "effect",      2345.0f, -154.0f, 154.0f } };

// tempo sync: SYNC button in the DELAY TIME panel (500 .. 980 x 268 .. 780) under the TIME knob, below a divider line (drawn in the background at y 660)
const juce::Rectangle<float> kSyncPill = juce::Rectangle<float> (116.0f, 46.0f).withCentre ({ 635.0f, 723.0f });
const juce::Rectangle<float> kDivisionText = juce::Rectangle<float> (130.0f, 46.0f).withCentre ({ 777.0f, 723.0f });
constexpr int kTimeKnob = 1;                               // index of "delayTime" in kKnobs; while sync is on it picks the division
const juce::Colour kBody { 0xff3979ef };

const juce::Point<float> kLed { 2445.0f, 87.0f };          // power slide switch (top right, ball left = off) = the `power` parameter
const juce::Rectangle<float> kMeter { 94.0f, 356.0f, 36.0f, 262.0f };      // headroom meter frame

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
//==============================================================================
SDD1000AudioProcessorEditor::SDD1000AudioProcessorEditor (SDD1000AudioProcessor& p)
    : PedalEditor (p, SD1Data::background_png, (size_t) SD1Data::background_pngSize, kImgW, kImgH, kBaseW, 630), proc (p),
      syncAtt (*p.apvts.getParameter ("sync"), [this] (float v) { syncOn = v > 0.5f; syncChanged(); }),
      divisionAtt (*p.apvts.getParameter ("division"), [this] (float v) { division = (int) std::lround (v); repaint(); })
{
    setLookAndFeel (&laf);
    
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
        midiUI.add (k.slider, kKnobs[i].id);
    }

    syncAtt.sendInitialUpdate();
    divisionAtt.sendInitialUpdate();

    resized();                                                       // place the knobs (the base class sized the window before they existed)
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
    // power slide switch (top right) = power
    const float S = (float) getWidth() / kImgW;
    const auto toggle = [this] (const char* id)
    {
        auto* prm = proc.apvts.getParameter (id);
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->getValue() > 0.5f ? 0.0f : 1.0f);
        prm->endChangeGesture();
    };
    if (juce::Rectangle<float> (110.0f * S, 70.0f * S).withCentre (kLed * S).contains (e.position)) toggle ("power");

    // tempo sync: the SYNC button toggles (the TIME knob then selects the note division)
    if ((kSyncPill.expanded (4.0f) * S).contains (e.position)) syncAtt.setValueAsCompleteGesture (syncOn ? 0.0f : 1.0f);
}

void SDD1000AudioProcessorEditor::syncChanged()
{
    // the TIME knob is the delay time, or the note division while sync is on; FACTOR has no effect with sync on, so it is dimmed
    auto& k = knobs[kTimeKnob];
    k.attachment.reset();
    divisionKnobAtt.reset();
    if (syncOn) divisionKnobAtt = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter ("division"), k.slider, nullptr);
    else        k.attachment    = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter ("delayTime"), k.slider, nullptr);
    knobs[2].slider.setAlpha (syncOn ? 0.35f : 1.0f);
    repaint();
}

void SDD1000AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    midiUI.paintMarks (g, kBody);
}

void SDD1000AudioProcessorEditor::timerCallback()
{
    if (midiUI.needsRepaint()) { midiUI.tick(); repaint(); }
    const float db = juce::Decibels::gainToDecibels (proc.meterLevel.load(), -60.0f);
    meterDb += (db - meterDb) * 0.5f;
    const bool pwr = proc.apvts.getRawParameterValue ("power")->load() > 0.5f;
    const int segs = (meterDb >= -20.0f) + (meterDb >= -10.0f) + (meterDb >= 0.0f) + (meterDb >= 6.0f);
    if (segs != lastSegs || pwr != lastPower)
    {
        lastSegs = segs; lastPower = pwr;
        repaint();
    }
}

void SDD1000AudioProcessorEditor::paint (juce::Graphics& g)
{
    paintBackground (g);       // the image, then image pixels

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

    // ---- power slide switch: chrome ball left = off, right = on; the slot lights up green when on
    const bool on = proc.apvts.getRawParameterValue ("power")->load() > 0.5f;
    vibr::ui::paintSlideSwitch (g, kLed, on);

    // ---- SYNC: a flat rounded button, dark with a body-colour border when off, filled body colour when on
    const float rad = kSyncPill.getHeight() * 0.5f;
    g.setColour (syncOn ? kBody : juce::Colour (0xff18191c));
    g.fillRoundedRectangle (kSyncPill, rad);
    g.setColour (kBody);
    g.drawRoundedRectangle (kSyncPill.reduced (1.0f), rad, 2.0f);
    g.setFont (juce::Font (juce::FontOptions ("Avenir Next", 24.0f, juce::Font::bold)));
    g.setColour (syncOn ? juce::Colours::white : kBody);
    g.drawText ("SYNC", kSyncPill, juce::Justification::centred, false);

    // the chosen note division (the TIME knob sets it) next to the button while sync is on
    if (syncOn)
    {
        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (juce::FontOptions ("Avenir Next", 34.0f, juce::Font::bold)));
        g.drawText (vibr::sync::divisionNames()[juce::jlimit (0, vibr::sync::kNumDivisions - 1, division)], kDivisionText, juce::Justification::centredLeft, false);
    }
}
