// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "MM1Data.h"

namespace
{
// Positions are in the 1980 x 968 coordinates of the rendered background (make_background.py). The editor is resizable at a
// fixed aspect ratio; paint() scales image pixels to the window.
constexpr float kImgW = 1980.0f, kImgH = 968.0f;
constexpr int kBaseW = 990;

constexpr float kKnobRadius = 78.0f;                       // black knob; the printed scale sits outside it
constexpr float kPad = 3.0f;
constexpr float kKnobY = 532.0f;
struct KnobSpec { const char* id; float x; };
const KnobSpec kKnobs[6] = { { "blend", 207.0f }, { "level", 493.0f }, { "feedback", 847.0f }, { "delay", 1133.0f },
                             { "chrsVibr", 1487.0f }, { "depth", 1773.0f } };
constexpr float kStartDeg = -154.0f, kEndDeg = 154.0f;     // 11 ticks over 308 degrees

const juce::Point<float> kLed { 1905.0f, 87.0f };          // power slide switch (top right) = the effect on / off switch (ball right = on)

// Tempo sync: a SYNC button in the ECHO panel, under the DELAY knob, below a divider line (drawn in the background). SYNC toggles the
// "sync" parameter. While sync is on the DELAY knob selects the note division instead of the time (the same knob: it is attached to the
// "division" parameter) and the chosen division is shown next to the button.
const juce::Rectangle<float> kSyncPill = juce::Rectangle<float> (116.0f, 46.0f).withCentre ({ 1062.0f, 776.0f });
const juce::Rectangle<float> kDivisionText = juce::Rectangle<float> (130.0f, 46.0f).withCentre ({ 1204.0f, 776.0f });
const juce::Colour kBody (0xff4b91d1);      // the plugin's body colour (make_background.py BODY)
constexpr int kDelayKnob = 3;                               // index of "delay" in kKnobs; while sync is on it picks the division
const juce::Colour kInk (0xffeeeee8), kPanel (0xff1b1c1f), kLedGreen (0xff40d070);

juce::Point<float> polar (juce::Point<float> c, float r, float a)
{
    return { c.x + r * std::sin (a), c.y - r * std::cos (a) };
}
} // namespace

//==============================================================================
void MemoryManLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float,
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

    // pointer: white bar from the rim toward the centre (grey and the knob shaded when "dimmed", i.e. DELAY while sync is on)
    const bool dimmed = (bool) s.getProperties().getWithDefault ("dimmed", false);
    juce::Path p;
    p.addRoundedRectangle (-capR * 0.055f, -capR * 0.94f, capR * 0.11f, capR * 0.50f, capR * 0.05f);
    g.setColour (dimmed ? juce::Colour (0xff5c5d5a) : juce::Colour (0xfff2f2ee));
    g.fillPath (p, juce::AffineTransform::rotation (angle).translated (c));
    if (dimmed)
    {
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillEllipse (circle (R));
    }
}

//==============================================================================
MemoryManAudioProcessorEditor::MemoryManAudioProcessorEditor (MemoryManAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      syncAtt (*p.apvts.getParameter ("sync"), [this] (float v) { syncOn = v > 0.5f; syncChanged(); }, nullptr),
      divisionAtt (*p.apvts.getParameter ("division"), [this] (float v) { division = (int) std::lround (v); repaint(); }, nullptr)
{
    setLookAndFeel (&laf);
    background = juce::ImageFileFormat::loadFrom (MM1Data::background_png, (size_t) MM1Data::background_pngSize);

    const auto add = [this] (Knob& k, const KnobSpec& spec)
    {
        k.slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        k.slider.setRotaryParameters (juce::degreesToRadians (kStartDeg), juce::degreesToRadians (kEndDeg), true);
        k.slider.setMouseDragSensitivity (180);
        k.slider.setScrollWheelEnabled (false);
        addAndMakeVisible (k.slider);
        k.attachment = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter (spec.id), k.slider, nullptr);
    };
    for (int i = 0; i < 6; ++i) { add (knobs[i], kKnobs[i]); midiUI.add (knobs[i].slider, kKnobs[i].id); }

    constrainer.setFixedAspectRatio ((double) kImgW / (double) kImgH);
    constrainer.setSizeLimits (495, 242, 1980, 968);
    setConstrainer (&constrainer);
    setResizable (true, true);
    setSize (kBaseW, (int) std::round ((float) kBaseW * kImgH / kImgW));
    syncAtt.sendInitialUpdate();
    divisionAtt.sendInitialUpdate();
    startTimerHz (20);
}

MemoryManAudioProcessorEditor::~MemoryManAudioProcessorEditor() { setLookAndFeel (nullptr); }

void MemoryManAudioProcessorEditor::resized()
{
    const float S = (float) getWidth() / kImgW;
    const auto place = [S] (Knob& k, const KnobSpec& spec)
    {
        k.slider.setBounds (juce::Rectangle<float> (2.0f * kKnobRadius * S, 2.0f * kKnobRadius * S)
                                .withCentre ({ spec.x * S, kKnobY * S }).toNearestInt());
    };
    for (int i = 0; i < 6; ++i) place (knobs[i], kKnobs[i]);
}

void MemoryManAudioProcessorEditor::syncChanged()
{
    // the DELAY knob is the delay time, or the note division while sync is on (the attachment sets the slider range from the parameter)
    auto& k = knobs[kDelayKnob];
    k.attachment.reset();
    divisionKnobAtt.reset();
    if (syncOn) divisionKnobAtt = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter ("division"), k.slider, nullptr);
    else        k.attachment    = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter ("delay"), k.slider, nullptr);
    repaint();
}

void MemoryManAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const float S = (float) getWidth() / kImgW;
    const auto scaled = [S] (juce::Rectangle<float> r) { return r.expanded (4.0f).transformedBy (juce::AffineTransform::scale (S)); };
    if (scaled (kSyncPill).contains (e.position))     { syncAtt.setValueAsCompleteGesture (syncOn ? 0.0f : 1.0f); return; }

    // the power slide switch is the bypass switch
    if (juce::Rectangle<float> (110.0f * S, 70.0f * S).withCentre (kLed * S).contains (e.position))
    {
        auto* prm = proc.apvts.getParameter ("effect");
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->getValue() > 0.5f ? 0.0f : 1.0f);
        prm->endChangeGesture();
    }
}

void MemoryManAudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    midiUI.paintMarks (g, kBody);
}

void MemoryManAudioProcessorEditor::timerCallback()
{
    if (midiUI.needsRepaint()) { midiUI.tick(); repaint(); }
    const bool on = proc.apvts.getRawParameterValue ("effect")->load() > 0.5f;
    if (on != lastOn) { lastOn = on; repaint(); }
}

void MemoryManAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (background, 0, 0, getWidth(), getHeight(), 0, 0, background.getWidth(), background.getHeight());
    g.addTransform (juce::AffineTransform::scale ((float) getWidth() / kImgW));      // image pixels from here on

    // power slide switch: chrome ball left = off, right = on; the slot lights up green when on
    const bool on = proc.apvts.getRawParameterValue ("effect")->load() > 0.5f;
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

    // SYNC: a flat rounded button, dark with a blue border when off, filled blue when on
    const float rad = kSyncPill.getHeight() * 0.5f;
    const juce::Colour blue (0xff4b91d1);
    g.setColour (syncOn ? blue : kPanel.darker (0.4f));
    g.fillRoundedRectangle (kSyncPill, rad);
    g.setColour (blue);
    g.drawRoundedRectangle (kSyncPill.reduced (1.0f), rad, 2.0f);
    g.setFont (juce::Font (juce::FontOptions ("Avenir Next", 24.0f, juce::Font::bold)));
    g.setColour (syncOn ? juce::Colours::white : blue);
    g.drawText ("SYNC", kSyncPill, juce::Justification::centred, false);

    // the chosen note division (the DELAY knob sets it) next to the button while sync is on
    if (syncOn)
    {
        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (juce::FontOptions ("Avenir Next", 34.0f, juce::Font::bold)));
        g.drawText (vibr::sync::divisionNames()[juce::jlimit (0, vibr::sync::kNumDivisions - 1, division)], kDivisionText, juce::Justification::centredLeft, false);
    }
}
