// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"

namespace ui = vibr::ui::vintage;

namespace
{
const ui::Palette kPal { juce::Colour (54, 183, 121) };           // the Modulation green #36B779: value rings, knob values and the plugin code

// Design space 1120 x 388: header (0 to 100), one row of three panels (y 114, 244 high), footer. Margin 28, gap 18.
constexpr float kDesignW = ui::kW, kDesignH = 388.0f;
constexpr float kPanelY = 114.0f, kPanelH = 244.0f, kKnobDY = 86.0f;
const juce::Rectangle<float> kPanels[3] { { 28.0f, kPanelY, 340.0f, kPanelH }, { 386.0f, kPanelY, 340.0f, kPanelH }, { 744.0f, kPanelY, 348.0f, kPanelH } };

constexpr float kKnobDX = 70.0f;                                   // the two knobs of a panel are 140 apart, symmetric about the panel centre
juce::Point<float> knobCentre (int panel, int side)
{
    const auto& r = kPanels[panel];
    return { r.getCentreX() + (side == 0 ? -kKnobDX : kKnobDX), r.getY() + kKnobDY };
}
juce::Rectangle<float> syncRect()                                  // under the RATE knob, 200 below the panel top, 30 high
{
    return juce::Rectangle<float> (110.0f, 30.0f).withCentre ({ knobCentre (1, 1).x, kPanelY + 215.0f });
}
juce::Rectangle<float> modeRect (int i)                            // three stacked buttons in the MODE panel
{
    const auto& r = kPanels[2];
    return { r.getX() + 24.0f, r.getY() + 46.0f + 60.0f * (float) i, r.getWidth() - 48.0f, 46.0f };
}
const char* const kModeNames[3] = { "GATE / PAN", "STANDARD", "ULTRA" };
} // namespace

//==============================================================================
BF3AudioProcessorEditor::BF3AudioProcessorEditor (BF3AudioProcessor& p)
    : PedalEditor (p, kDesignW, kDesignH, 560), proc (p),
      syncAtt (*p.apvts.getParameter ("sync"), [this] (float v) { syncOn = v > 0.5f; syncChanged(); }, nullptr),
      divisionAtt (*p.apvts.getParameter ("division"), [this] (float v) { division = (int) std::lround (v); repaint(); }, nullptr),
      modeAtt (*p.apvts.getParameter ("mode"), [this] (float v) { mode = juce::jlimit (0, 2, (int) std::lround (v)); repaint(); }, nullptr)
{
    const auto add = [this] (Knob& k, const char* id)
    {
        k.slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        k.slider.setRotaryParameters (juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
        k.slider.setMouseDragSensitivity (180);
        k.slider.setScrollWheelEnabled (false);
        k.slider.setPopupDisplayEnabled (true, true, this);
        addAndMakeVisible (k.slider);
        k.attachment = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter (id), k.slider, nullptr);
    };
    add (manual, "manual");
    add (res, "res");
    add (depth, "depth");
    add (rate, "rate");
    midiUI.add (manual.slider, "manual");  midiUI.add (res.slider, "res");
    midiUI.add (depth.slider, "depth");    midiUI.add (rate.slider, "rate");
    addAndMakeVisible (modeArea);
    modeArea.onDown = [this] (juce::Point<float> pos)
    {
        const auto pt = (pos + modeArea.getPosition().toFloat()) / scale();     // area-local pixels to design units
        for (int i = 0; i < 3; ++i)
            if (modeRect (i).expanded (3.0f).contains (pt)) { modeAtt.setValueAsCompleteGesture ((float) i / 2.0f); return; }   // normalised 0, 0.5, 1
    };
    midiUI.add (modeArea, "mode");
    syncAtt.sendInitialUpdate();
    divisionAtt.sendInitialUpdate();
    modeAtt.sendInitialUpdate();

    resized();                                                       // place the knobs (the base class sized the window before they existed)
    startTimerHz (20);
}

BF3AudioProcessorEditor::~BF3AudioProcessorEditor() { stopTimer(); }

void BF3AudioProcessorEditor::resized()
{
    placeKnob (manual.slider, knobCentre (0, 0));
    placeKnob (res.slider, knobCentre (0, 1));
    placeKnob (depth.slider, knobCentre (1, 0));
    placeKnob (rate.slider, knobCentre (1, 1));
    modeArea.setBounds (modeRect (0).getUnion (modeRect (2)).transformedBy (juce::AffineTransform::scale (scale())).toNearestInt());
}

void BF3AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    proc.undoManager.beginNewTransaction();
    const auto p = e.position / scale();
    if (ui::presetBarMouseDown (p, *this, proc.presets, proc.undoManager, scale(), [this] { repaint(); })) { repaint(); return; }
    if (toggleBypassAt (e.position)) return;
    if (syncRect().expanded (3.0f).contains (p))
    {
        syncAtt.setValueAsCompleteGesture (syncOn ? 0.0f : 1.0f);
        return;
    }
}

void BF3AudioProcessorEditor::syncChanged()
{
    // the RATE knob is the LFO rate, or the note division while sync is on (the attachment sets the slider range from the parameter)
    rate.attachment.reset();
    divisionKnobAtt.reset();
    if (syncOn) divisionKnobAtt = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter ("division"), rate.slider, nullptr);
    else        rate.attachment = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter ("rate"), rate.slider, nullptr);
    repaint();
}

void BF3AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    midiUI.paintMarks (g, kPal.accent);
}

int BF3AudioProcessorEditor::stateKey() const
{
    int k = powerOn() ? 1 : 0;
    for (const auto* s : { &manual.slider, &res.slider, &depth.slider, &rate.slider }) k = k * 31 + (int) std::lround (s->getValue() * 100.0);
    return k * 31 + (syncOn ? 1 : 0) * 7 + division * 3 + mode;
}

void BF3AudioProcessorEditor::timerCallback()
{
    const bool learn = midiUI.needsRepaint();
    if (learn) midiUI.tick();
    if (const int key = stateKey(); key != lastKey) { lastKey = key; repaint(); }
    else if (learn) repaint();
}

void BF3AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    ui::paintBody (g, kPal, kDesignW, kDesignH);
    ui::paintHeader (g, kPal, "BF-3", "STEREO FLANGER", "LIQUID MOTION");
    ui::paintPresetBar (g, kPal, proc.presets, proc.undoManager, powerOn());
    ui::paintFooter (g, kPal, kDesignH - 15.0f);

    ui::paintPanel (g, kPal, kPanels[0], "FLANGE");
    ui::paintPanel (g, kPal, kPanels[1], "SWEEP");
    ui::paintPanel (g, kPal, kPanels[2], "MODE");

    const auto knob = [&] (Knob& k, int panel, int side, const char* name, const juce::String& value, const juce::String& lo, const juce::String& hi)
    {
        ui::paintKnob (g, kPal, knobCentre (panel, side), name, value, lo, hi, (float) k.slider.valueToProportionOfLength (k.slider.getValue()));
    };
    const auto tenths = [] (Knob& k) { return juce::String (k.slider.getValue(), 1); };
    knob (manual, 0, 0, "MANUAL", tenths (manual), "0", "10");
    knob (res, 0, 1, "RES", tenths (res), "0", "10");
    knob (depth, 1, 0, "DEPTH", tenths (depth), "0", "10");

    const auto& names = vibr::sync::divisionNames();
    if (syncOn)
        knob (rate, 1, 1, "RATE", names[juce::jlimit (0, vibr::sync::kNumDivisions - 1, division)], "LONG", "SHORT");
    else
    {
        const float hz = bf3::rateHz ((float) rate.slider.getValue() * 0.1f);       // the knob is 0..10
        knob (rate, 1, 1, "RATE", juce::String (hz, hz < 1.0f ? 2 : 1) + " HZ", "0.05", "8 Hz");
    }
    ui::paintFlatButton (g, kPal, syncRect(), "SYNC", syncOn, 17.0f, 1.0f, 4.0f, 0.1f);

    for (int i = 0; i < 3; ++i)
        ui::paintFlatButton (g, kPal, modeRect (i), kModeNames[i], mode == i, 20.0f, 1.0f, 4.0f, 0.1f);
}
