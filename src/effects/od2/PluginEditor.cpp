// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "OD2Data.h"

namespace
{
// Positions are in the 2460 x 978 coordinates of the rendered background (make_background.py). The editor is resizable at a
// fixed aspect ratio; paint() scales image pixels to the window.
constexpr float kImgW = 2460.0f, kImgH = 978.0f;
constexpr int kBaseW = 1230;

constexpr float kKnobRadius = 80.0f;                       // black knob; the printed scale sits outside it
constexpr float kRowY = 490.0f;
struct KnobSpec { const char* id; float x, y; float startDeg, endDeg; };
const KnobSpec kType { "type", 390.0f, 584.0f, 0.0f, 330.0f };            // 12 positions, 30 degrees apart, OVERDRIVE at the top
const KnobSpec kParam { "param", 910.0f, kRowY, -154.0f, 154.0f }, kLevel { "level", 1170.0f, kRowY, -154.0f, 154.0f },
               kDrive { "drive", 1430.0f, kRowY, -154.0f, 154.0f }, kLow { "low", 1750.0f, kRowY, -154.0f, 154.0f },
               kMiddle { "middle", 2010.0f, kRowY, -154.0f, 154.0f }, kHigh { "high", 2270.0f, kRowY, -154.0f, 154.0f };

const juce::Point<float> kLed { 2385.0f, 87.0f };          // power slide switch (top right, ball left = off) = bypass
const juce::Point<float> kBoostSw { 1455.0f, 750.0f };     // BOOST slide switch: ball left = off
const juce::Point<float> kDisplay { 390.0f, 340.0f };      // centre of the red display (top of the TYPE panel)
const float kParamLedX[5] = { 990.0f, 1070.0f, 1150.0f, 1240.0f, 1330.0f };
const float kMemLedX[5] = { 1690.0f, 1780.0f, 1870.0f, 1960.0f, 2050.0f };
constexpr float kLedY = 750.0f;
const juce::Point<float> kParamButton { 860.0f, 760.0f }, kMemoryButton { 2250.0f, 760.0f };
constexpr juce::uint32 kHoldMs = 800;
const char* const kSlotId[5] = { "param", "boostPost", "boostPre", "gateDecay", "gateThresh" };      // what the PARAM knob edits

// lamp (flat, see style_guide.md "Flat style"): a plain 22 px disc inside the dark grey ring painted in the background, the colour when on, a dark
// shade of it when off; no glow, no highlight
void drawLamp (juce::Graphics& g, juce::Point<float> c, bool on, juce::Colour colour)
{
    g.setColour (on ? colour : colour.interpolatedWith (juce::Colour (0xff0c0d0f), 0.78f));
    g.fillEllipse (juce::Rectangle<float> (22.0f, 22.0f).withCentre (c));
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
//==============================================================================
OD2AudioProcessorEditor::OD2AudioProcessorEditor (OD2AudioProcessor& p)
    : PedalEditor (p, OD2Data::background_png, (size_t) OD2Data::background_pngSize, kImgW, kImgH, kBaseW, 615), proc (p)
{
    setLookAndFeel (&laf);
    
    const auto add = [this] (Knob& k, const KnobSpec& spec)
    {
        k.slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        k.slider.setRotaryParameters (juce::degreesToRadians (spec.startDeg), juce::degreesToRadians (spec.endDeg), true);
        k.slider.setMouseDragSensitivity (180);
        k.slider.setScrollWheelEnabled (false);
        addAndMakeVisible (k.slider);
        midiUI.add (k.slider, spec.id);
        k.attachment = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter (spec.id), k.slider, nullptr);
    };
    add (type, kType);
    type.slider.wrapSteps = od2::kNumTypes;                // the TYPE selector turns endlessly
    add (param, kParam);
    add (level, kLevel);
    add (drive, kDrive);
    add (low, kLow);
    add (middle, kMiddle);
    add (high, kHigh);
    param.slider.onValueChange = [this] { shownUntil = juce::Time::getMillisecondCounter() + 1200; repaint(); };   // show the value for a moment

    paramZone.onClick = [this] { selectSlot ((slot + 1) % 5); };
    memoryZone.onDown = [this] { memHeld = true; memWritten = false; memDownAt = juce::Time::getMillisecondCounter(); };
    memoryZone.onUp = [this] { memHeld = false; };
    memoryZone.onClick = [this] { if (! memWritten) memory.stepNext(); repaint(); };      // short press
    for (int i = 0; i < 5; ++i)      // clicking a lamp selects it: the parameter the PARAM knob edits, or a memory slot (MAN, 1-4)
    {
        paramLedZone[i].onClick = [this, i] { selectSlot (i); };
        memLedZone[i].onClick   = [this, i] { memory.select (i); repaint(); };
        addAndMakeVisible (paramLedZone[i]);
        addAndMakeVisible (memLedZone[i]);
    }
    boostTarget.onPress = [this] { auto* prm = proc.apvts.getParameter ("boostOn"); prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->getValue() > 0.5f ? 0.0f : 1.0f); prm->endChangeGesture(); };
    addAndMakeVisible (boostTarget);
    midiUI.add (boostTarget, "boostOn");
    addAndMakeVisible (paramZone);
    addAndMakeVisible (memoryZone);

    resized();                                                       // place the knobs (the base class sized the window before they existed)
    startTimerHz (30);
}

OD2AudioProcessorEditor::~OD2AudioProcessorEditor() { setLookAndFeel (nullptr); }

void OD2AudioProcessorEditor::selectSlot (int s)
{
    slot = s;
    param.attachment.reset();                       // the PARAM knob now edits another parameter
    param.attachment = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter (kSlotId[slot]), param.slider, nullptr);
    repaint();
}

void OD2AudioProcessorEditor::resized()
{
    const float S = (float) getWidth() / kImgW;
    const auto place = [S] (Knob& k, const KnobSpec& spec)
    {
        k.slider.setBounds (juce::Rectangle<float> (2.0f * kKnobRadius * S, 2.0f * kKnobRadius * S)
                                .withCentre ({ spec.x * S, spec.y * S }).toNearestInt());
    };
    place (type, kType); place (param, kParam); place (level, kLevel); place (drive, kDrive);
    place (low, kLow); place (middle, kMiddle); place (high, kHigh);
    boostTarget.setBounds (juce::Rectangle<float> (110.0f * S, 60.0f * S).withCentre (kBoostSw * S).toNearestInt());
    paramZone.setBounds (juce::Rectangle<float> (140.0f * S, 80.0f * S).withCentre (kParamButton * S).toNearestInt());
    memoryZone.setBounds (juce::Rectangle<float> (140.0f * S, 80.0f * S).withCentre (kMemoryButton * S).toNearestInt());
    for (int i = 0; i < 5; ++i)
    {
        paramLedZone[i].setBounds (juce::Rectangle<float> (50.0f * S, 50.0f * S).withCentre (juce::Point<float> (kParamLedX[i], kLedY) * S).toNearestInt());
        memLedZone[i].setBounds   (juce::Rectangle<float> (50.0f * S, 50.0f * S).withCentre (juce::Point<float> (kMemLedX[i], kLedY) * S).toNearestInt());
    }
}

void OD2AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    // the power slide switch is the bypass switch; the small slide switch in the DRIVE panel turns the boost on
    const float S = (float) getWidth() / kImgW;
    const auto toggle = [this] (const char* id)
    {
        auto* prm = proc.apvts.getParameter (id);
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->getValue() > 0.5f ? 0.0f : 1.0f);
        prm->endChangeGesture();
    };
    if (juce::Rectangle<float> (110.0f * S, 70.0f * S).withCentre (kLed * S).contains (e.position)) toggle ("bypass");
}

void OD2AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    midiUI.paintMarks (g, juce::Colour(240, 120, 53));
}

void OD2AudioProcessorEditor::timerCallback()
{
    if (midiUI.needsRepaint()) { midiUI.tick(); repaint(); }
    // memory: holding the button writes the slot; moving a knob after a recall goes back to MANUAL
    const auto now = juce::Time::getMillisecondCounter();
    if (memHeld && ! memWritten && now - memDownAt >= kHoldMs)
    {
        memory.write();
        memWritten = true;
        memFlashUntil = now + 500;
    }
    memory.revertIfEdited();

    const bool on = proc.apvts.getRawParameterValue ("bypass")->load() < 0.5f;
    const bool boost = proc.apvts.getRawParameterValue ("boostOn")->load() > 0.5f;
    const int t = (int) std::lround (proc.apvts.getRawParameterValue ("type")->load());
    const bool flashing = now < memFlashUntil, showing = now < shownUntil;
    if (on != lastOn || boost != lastBoost || t != lastType || memory.current() != memShown || flashing || flashing != memFlashShown
        || showing || showing != valueShown)
    {
        lastOn = on; lastBoost = boost; lastType = t; memShown = memory.current(); memFlashShown = flashing; valueShown = showing;
        repaint();
    }
}

void OD2AudioProcessorEditor::paint (juce::Graphics& g)
{
    paintBackground (g);       // the image, then image pixels

    // red LED display: the drive type, or the PARAM value while the PARAM knob is being turned
    {
        const int t = juce::jlimit (0, od2::kNumTypes - 1, (int) std::lround (proc.apvts.getRawParameterValue ("type")->load()));
        const bool showValue = juce::Time::getMillisecondCounter() < shownUntil;
        const juce::String text = showValue ? juce::String (param.slider.getValue(), 1) : juce::String (od2::typeDef (t).shortName).trim();
        const juce::Font f (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 44.0f, juce::Font::bold));
        g.setFont (f);
        const auto box = juce::Rectangle<float> (160.0f, 54.0f).withCentre (kDisplay);
        g.setColour (juce::Colour (0xffff3a2a).withAlpha (0.07f));
        g.drawText ("888", box, juce::Justification::centred, false);                    // unlit segments
        g.setColour (juce::Colour (0xffff3a2a).withAlpha (0.25f));
        for (auto o : { juce::Point<float> (-1.5f, 0), juce::Point<float> (1.5f, 0), juce::Point<float> (0, -1.5f), juce::Point<float> (0, 1.5f) })
            g.drawText (text, box.translated (o.x, o.y), juce::Justification::centred, false);
        g.setColour (juce::Colour (0xffff6a58));
        g.drawText (text, box, juce::Justification::centred, false);
    }

    // PARAM lamps (which parameter the PARAM knob edits) and the memory lamps
    for (int i = 0; i < 5; ++i)
        drawLamp (g, { kParamLedX[i], kLedY }, slot == i, juce::Colour (0xffe5432c));
    const auto t = juce::Time::getMillisecondCounter();
    const bool memFlashing = t < memFlashUntil;
    const bool blinkOff = t < memFlashUntil && ((t / 100) % 2) == 1;
    for (int i = 0; i < 5; ++i)
        drawLamp (g, { kMemLedX[i], kLedY }, (memFlashing ? ! blinkOff : memory.current() == i), juce::Colour (0xff40d070));

    // boost slide switch: chrome ball in the slot, left = off, right = on
    drawBall (g, { kBoostSw.x + (proc.apvts.getRawParameterValue ("boostOn")->load() > 0.5f ? 17.0f : -17.0f), kBoostSw.y });

    // power slide switch: chrome ball left = off, right = on; the slot lights up green when on
    const bool on = proc.apvts.getRawParameterValue ("bypass")->load() < 0.5f;
    vibr::ui::paintSlideSwitch (g, kLed, on);
}
