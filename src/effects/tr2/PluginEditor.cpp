// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"

namespace ui = vibr::ui::vintage;

namespace
{
const ui::Palette kPal { juce::Colour (21, 156, 112) };

// Design space: 1120 wide (margin 28, gap 18), one row of three equal panels, y 114 to 360, footer centre 15 above the bottom.
constexpr float kDesignW = 1120.0f, kDesignH = 390.0f;
constexpr float kPanelY = 114.0f, kPanelH = 246.0f, kKnobDy = 86.0f, kButtonDy = 200.0f;
constexpr float kPanelW = (kDesignW - 2.0f * ui::kMargin - 2.0f * 18.0f) / 3.0f;

juce::Rectangle<float> panelRect (int i) { return { ui::kMargin + (float) i * (kPanelW + 18.0f), kPanelY, kPanelW, kPanelH }; }
juce::Point<float> knobCentre (int i) { return { panelRect (i).getCentreX(), kPanelY + kKnobDy }; }
juce::Rectangle<float> syncRect() { return juce::Rectangle<float> (110.0f, 30.0f).withCentre ({ panelRect (0).getCentreX(), kPanelY + kButtonDy + 15.0f }); }
}

//==============================================================================
TR2AudioProcessorEditor::TR2AudioProcessorEditor (TR2AudioProcessor& p)
    : PedalEditor (p, kDesignW, kDesignH), proc (p),
      syncAtt (*p.apvts.getParameter ("sync"), [this] (float v) { syncChanged (v > 0.5f); }, nullptr),
      divisionAtt (*p.apvts.getParameter ("division"), [this] (float v) { division = juce::jlimit (0, vibr::sync::kNumDivisions - 1, (int) std::lround (v)); repaint(); }, nullptr)
{
    const auto add = [this] (Knob& k, const char* id)
    {
        k.slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        k.slider.setRotaryParameters (juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
        k.slider.setMouseDragSensitivity (180);
        k.slider.setScrollWheelEnabled (false);
        k.slider.setPopupDisplayEnabled (true, true, this);
        k.slider.onValueChange = [this] { repaint(); };
        addAndMakeVisible (k.slider);
        k.attachment = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter (id), k.slider, nullptr);
    };
    add (rate, "rate");
    add (wave, "wave");
    add (depth, "depth");
    midiUI.add (rate.slider, "rate");  midiUI.add (wave.slider, "wave");  midiUI.add (depth.slider, "depth");

    syncAtt.sendInitialUpdate();
    divisionAtt.sendInitialUpdate();
    resized();
    startTimerHz (20);
}

TR2AudioProcessorEditor::~TR2AudioProcessorEditor() = default;

void TR2AudioProcessorEditor::resized()
{
    placeKnob (rate.slider, knobCentre (0));
    placeKnob (wave.slider, knobCentre (1));
    placeKnob (depth.slider, knobCentre (2));
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
    const auto p = e.position / scale();
    proc.undoManager.beginNewTransaction();
    if (ui::presetBarMouseDown (p, *this, proc.presets, proc.undoManager, scale(), [this] { repaint(); })) return;
    if (syncRect().expanded (3.0f).contains (e.position / scale())) { syncAtt.setValueAsCompleteGesture (syncOn ? 0.0f : 1.0f); return; }
    if (toggleBypassAt (e.position)) repaint();                       // the power switch is the bypass switch
}

void TR2AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    midiUI.paintMarks (g, kPal.accent);
}

void TR2AudioProcessorEditor::timerCallback()
{
    if (midiUI.needsRepaint()) { midiUI.tick(); repaint(); }
    const bool on = powerOn();
    if (on != lastOn) { lastOn = on; repaint(); }
}

void TR2AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    ui::paintBody (g, kPal, kDesignW, kDesignH);
    ui::paintHeader (g, kPal, "TR-2", "TREMOLO", "PULSING MOTION");
    ui::paintPresetBar (g, kPal, proc.presets, proc.undoManager, powerOn());

    const char* titles[] = { "SPEED", "SHAPE", "AMOUNT" };
    const char* names[]  = { "RATE", "WAVE", "DEPTH" };
    Knob* knobs[] = { &rate, &wave, &depth };
    for (int i = 0; i < 3; ++i)
    {
        ui::paintPanel (g, kPal, panelRect (i), titles[i]);
        auto& s = knobs[i]->slider;
        const bool divisionShown = (i == 0 && syncOn);
        const juce::String value = divisionShown ? vibr::sync::divisionNames()[juce::jlimit (0, vibr::sync::kNumDivisions - 1, division)]
                                                 : juce::String (s.getValue(), 1);
        const char* lo = i == 1 ? "SINE" : (divisionShown ? "" : "0");
        const char* hi = i == 1 ? "SQR"  : (divisionShown ? "" : "10");
        ui::paintKnob (g, kPal, knobCentre (i), names[i], value, lo, hi, (float) s.valueToProportionOfLength (s.getValue()));
    }

    ui::paintFlatButton (g, kPal, syncRect(), "SYNC", syncOn);
    ui::paintFooter (g, kPal, kDesignH - 15.0f);
}
