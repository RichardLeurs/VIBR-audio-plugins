// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "TR2Data.h"

namespace
{
// Positions are in the 1840 x 890 coordinates of the rendered background (make_background.py). The editor is resizable at a
// fixed aspect ratio; paint() scales image pixels to the window.
constexpr float kImgW = 1840.0f, kImgH = 968.0f;
constexpr int kBaseW = 920;

constexpr float kKnobRadius = 100.0f;                      // black knob; the printed scale sits outside it
constexpr float kKnobY = 532.0f;
struct KnobSpec { const char* id; float x; float startDeg, endDeg; };
const KnobSpec kRate { "rate", 326.0f, -154.0f, 154.0f }, kWave { "wave", 920.0f, -154.0f, 154.0f }, kDepth { "depth", 1514.0f, -154.0f, 154.0f };
const juce::Point<float> kLed { 1765.0f, 87.0f };          // power slide switch (top right, ball left = off) = bypass

// tempo sync: a SYNC button in the SPEED panel under the RATE knob, below a divider line (drawn in the background). While sync is on the
// RATE knob selects the note division (it is attached to "division") and the division is shown right of the button.
const juce::Rectangle<float> kSyncPill = juce::Rectangle<float> (116.0f, 46.0f).withCentre ({ 255.0f, 776.0f });
const juce::Rectangle<float> kDivisionText = juce::Rectangle<float> (130.0f, 46.0f).withCentre ({ 397.0f, 776.0f });
const juce::Colour kBody (0xff159c70);                          // body colour (BODY in make_background.py)
const juce::Colour kPanel (0xff1b1c1f);

} // namespace

//==============================================================================
//==============================================================================
TR2AudioProcessorEditor::TR2AudioProcessorEditor (TR2AudioProcessor& p)
    : PedalEditor (p, TR2Data::background_png, (size_t) TR2Data::background_pngSize, kImgW, kImgH, kBaseW, 460), proc (p),
      syncAtt (*p.apvts.getParameter ("sync"), [this] (float v) { syncChanged (v > 0.5f); }, nullptr),
      divisionAtt (*p.apvts.getParameter ("division"), [this] (float v) { division = juce::jlimit (0, vibr::sync::kNumDivisions - 1, (int) std::lround (v)); repaint(); }, nullptr)
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
        k.attachment = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter (spec.id), k.slider, nullptr);
    };
    add (rate, kRate);
    add (wave, kWave);
    add (depth, kDepth);
    midiUI.add (rate.slider, "rate");  midiUI.add (wave.slider, "wave");  midiUI.add (depth.slider, "depth");

    syncAtt.sendInitialUpdate();
    divisionAtt.sendInitialUpdate();
    resized();                                                       // place the knobs (the base class sized the window before they existed)
    startTimerHz (20);
}

TR2AudioProcessorEditor::~TR2AudioProcessorEditor() { setLookAndFeel (nullptr); }

void TR2AudioProcessorEditor::resized()
{
    const float S = (float) getWidth() / kImgW;
    const auto place = [S] (Knob& k, const KnobSpec& spec)
    {
        k.slider.setBounds (juce::Rectangle<float> (2.0f * kKnobRadius * S, 2.0f * kKnobRadius * S)
                                .withCentre ({ spec.x * S, kKnobY * S }).toNearestInt());
    };
    place (rate, kRate);
    place (wave, kWave);
    place (depth, kDepth);
}

void TR2AudioProcessorEditor::syncChanged (bool on)
{
    // the RATE knob is the rate, or the note division while sync is on (the attachment sets the slider range from the parameter)
    syncOn = on;
    rate.attachment.reset();
    divisionKnobAtt.reset();
    if (syncOn) divisionKnobAtt = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter ("division"), rate.slider, nullptr);
    else        rate.attachment = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter ("rate"), rate.slider, nullptr);
    repaint();
}

void TR2AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const float S = (float) getWidth() / kImgW;
    const auto pos = e.position / S;
    if (kSyncPill.expanded (4.0f).contains (pos)) { syncAtt.setValueAsCompleteGesture (syncOn ? 0.0f : 1.0f); return; }

    // the power slide switch is the bypass switch
    if (juce::Rectangle<float> (110.0f * S, 70.0f * S).withCentre (kLed * S).contains (e.position))
    {
        auto* prm = proc.apvts.getParameter ("bypass");
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->getValue() > 0.5f ? 0.0f : 1.0f);
        prm->endChangeGesture();
    }
}

void TR2AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    midiUI.paintMarks (g, kBody);
}

void TR2AudioProcessorEditor::timerCallback()
{
    if (midiUI.needsRepaint()) { midiUI.tick(); repaint(); }
    const bool on = proc.apvts.getRawParameterValue ("bypass")->load() < 0.5f;
    if (on != lastOn) { lastOn = on; repaint(); }
}

void TR2AudioProcessorEditor::paint (juce::Graphics& g)
{
    paintBackground (g);       // the image, then image pixels

    // SYNC: a flat rounded button, dark with a body-colour border when off, filled body colour when on
    const float rad = kSyncPill.getHeight() * 0.5f;
    g.setColour (syncOn ? kBody : kPanel.darker (0.4f));
    g.fillRoundedRectangle (kSyncPill, rad);
    g.setColour (kBody);
    g.drawRoundedRectangle (kSyncPill.reduced (1.0f), rad, 2.0f);
    g.setFont (juce::Font (juce::FontOptions ("Avenir Next", 24.0f, juce::Font::bold)));
    g.setColour (syncOn ? juce::Colours::white : kBody);
    g.drawText ("SYNC", kSyncPill, juce::Justification::centred, false);

    // the chosen note division (the RATE knob sets it) next to the button while sync is on
    if (syncOn)
    {
        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (juce::FontOptions ("Avenir Next", 34.0f, juce::Font::bold)));
        g.drawText (vibr::sync::divisionNames()[juce::jlimit (0, vibr::sync::kNumDivisions - 1, division)], kDivisionText, juce::Justification::centredLeft, false);
    }

    // power slide switch: chrome ball left = off, right = on; the slot lights up green when on
    const bool on = proc.apvts.getRawParameterValue ("bypass")->load() < 0.5f;
    vibr::ui::paintSlideSwitch (g, kLed, on);
}
