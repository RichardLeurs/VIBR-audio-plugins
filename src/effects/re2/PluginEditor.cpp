// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "RE2Data.h"

namespace
{
// Positions are in the coordinates of the rendered background (2610 x 1260, see make_background.py);
// the editor shows it at 1305 x 630 and paint() scales them with S.
constexpr int kW = 1305, kH = 630;
constexpr float S = (float) kW / 2610.0f;

juce::Point<float> img (float x, float y) { return { x * S, y * S }; }

const juce::Point<float> kSaturation { 250.0f, 790.0f }, kWow { 690.0f, 790.0f }, kMode { 1240.0f, 700.0f },
                         kBass { 1775.0f, 458.0f }, kTreble { 2075.0f, 458.0f }, kReverb { 2375.0f, 458.0f },
                         kRate { 1775.0f, 790.0f }, kIntensity { 2075.0f, 790.0f }, kEcho { 2375.0f, 790.0f };
constexpr float kPad = 3.0f;
constexpr float kKnobRadius = 72.0f, kModeRadius = 100.0f;

const float kHeadLedX[4] = { 1075.0f, 1185.0f, 1295.0f, 1405.0f };
constexpr float kHeadLedY = 422.0f;
const juce::Point<float> kGuitarLed { 150.0f, 574.0f }, kLineLed { 260.0f, 574.0f },
                         kNewLed { 550.0f, 574.0f }, kAgedLed { 660.0f, 574.0f };
const juce::Point<float> kInputButton { 390.0f, 574.0f }, kTapeButton { 780.0f, 574.0f };
const float kMemLedX[5] = { 150.0f, 260.0f, 370.0f, 480.0f, 590.0f };
const juce::Point<float> kMemoryLed0 { 0.0f, 386.0f }, kMemoryButton { 780.0f, 386.0f };
constexpr juce::uint32 kHoldMs = 800;
const juce::Point<float> kPowerSwitch { 2535.0f, 87.0f };      // power slide switch (top right)
// SYNC button inside the ECHO panel, under REPEAT RATE, below the divider line drawn in the background (y 1012); the division readout sits to its right
const juce::Rectangle<float> kSyncPill = juce::Rectangle<float> (116.0f, 46.0f).withCentre ({ 1680.0f, 1070.0f });
const juce::Rectangle<float> kDivisionText = juce::Rectangle<float> (200.0f, 46.0f).withCentre ({ 1860.0f, 1070.0f });
const juce::Colour kBody (0xff3155b5);      // the plugin's body colour (make_background.py BODY)

// lamp (flat, see style_guide.md "Flat style"): a plain disc inside the dark grey ring painted in the background; the colour when on (red by default,
// green for the memory strip), a dark shade of it when off; no glow, no highlight
void drawLamp (juce::Graphics& g, juce::Point<float> c, bool on, juce::Colour colour = juce::Colour (0xffe5432c))
{
    g.setColour (on ? colour : colour.interpolatedWith (juce::Colour (0xff0c0d0f), 0.78f));
    g.fillEllipse (juce::Rectangle<float> (22.0f, 22.0f).withCentre (c));
}

} // namespace

//==============================================================================
//==============================================================================
RE2AudioProcessorEditor::RE2AudioProcessorEditor (RE2AudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      syncAtt (*p.apvts.getParameter ("sync"), [this] (float v) { syncOn = v > 0.5f; syncChanged(); }, nullptr),
      divisionAtt (*p.apvts.getParameter ("division"), [this] (float v) { division = (int) std::lround (v); repaint(); }, nullptr)
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
    midiUI.add (saturation.slider, "saturation");  midiUI.add (wowFlutter.slider, "wowFlutter");  midiUI.add (mode.slider, "mode");
    midiUI.add (bass.slider, "bass");              midiUI.add (treble.slider, "treble");          midiUI.add (reverbVol.slider, "reverbVol");
    midiUI.add (repeatRate.slider, "repeatRate");  midiUI.add (intensity.slider, "intensity");    midiUI.add (echoVol.slider, "echoVol");

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
    for (int i = 0; i < 5; ++i)      // clicking a lamp selects it: a memory slot (MANUAL, 1-4), GUITAR / LINE, NEW / AGED
    {
        memLedZone[i].onClick = [this, i] { memory.select (i); repaint(); };
        addAndMakeVisible (memLedZone[i]);
    }
    for (int i = 0; i < 2; ++i)
    {
        inputLedZone[i].onClick = [this, i] { inputSwitch.setToggleState (i == 1, juce::sendNotificationSync); repaint(); };      // GUITAR = off, LINE = on
        tapeLedZone[i].onClick  = [this, i] { tapeSwitch.setToggleState  (i == 1, juce::sendNotificationSync); repaint(); };      // NEW = off, AGED = on
        addAndMakeVisible (inputLedZone[i]);
        addAndMakeVisible (tapeLedZone[i]);
    }

    setSize (kW, kH);
    syncAtt.sendInitialUpdate();
    divisionAtt.sendInitialUpdate();
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
    const auto lampZone = [] (juce::Point<float> d) { return juce::Rectangle<float> (50.0f * S, 50.0f * S).withCentre (img (d.x, d.y)).toNearestInt(); };
    for (int i = 0; i < 5; ++i) memLedZone[i].setBounds (lampZone ({ kMemLedX[i], kMemoryLed0.y }));
    inputLedZone[0].setBounds (lampZone (kGuitarLed)); inputLedZone[1].setBounds (lampZone (kLineLed));
    tapeLedZone[0].setBounds  (lampZone (kNewLed));    tapeLedZone[1].setBounds  (lampZone (kAgedLed));
    powerSwitch.setBounds (juce::Rectangle<float> (110.0f * S, 70.0f * S).withCentre (img (kPowerSwitch.x, kPowerSwitch.y)).toNearestInt());
}

void RE2AudioProcessorEditor::syncChanged()
{
    // REPEAT RATE is the echo rate, or the note division while sync is on (the attachment sets the slider range from the parameter)
    auto& k = repeatRate;
    k.attachment.reset();
    divisionKnobAtt.reset();
    if (syncOn) divisionKnobAtt = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter ("division"), k.slider, nullptr);
    else        k.attachment    = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter ("repeatRate"), k.slider, nullptr);
    repaint();
}

void RE2AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    if (kSyncPill.expanded (4.0f).transformedBy (juce::AffineTransform::scale (S)).contains (e.position))
        syncAtt.setValueAsCompleteGesture (syncOn ? 0.0f : 1.0f);
}

void RE2AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    midiUI.paintMarks (g, kBody);
}

void RE2AudioProcessorEditor::timerCallback()
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

    // SYNC: a flat rounded button, dark with a body-colour border when off, filled body colour when on
    const float rad = kSyncPill.getHeight() * 0.5f;
    g.setColour (syncOn ? kBody : juce::Colour (0xff131417));
    g.fillRoundedRectangle (kSyncPill, rad);
    g.setColour (kBody);
    g.drawRoundedRectangle (kSyncPill.reduced (1.0f), rad, 2.0f);
    g.setFont (juce::Font (juce::FontOptions ("Avenir Next", 24.0f, juce::Font::bold)));
    g.setColour (syncOn ? juce::Colours::white : kBody);
    g.drawText ("SYNC", kSyncPill, juce::Justification::centred, false);

    // the chosen note division (REPEAT RATE sets it) next to the button while sync is on
    if (syncOn)
    {
        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (juce::FontOptions ("Avenir Next", 34.0f, juce::Font::bold)));
        g.drawText (vibr::sync::divisionNames()[juce::jlimit (0, vibr::sync::kNumDivisions - 1, division)], kDivisionText, juce::Justification::centredLeft, false);
    }

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
