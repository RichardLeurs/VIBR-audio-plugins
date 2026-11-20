// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"

namespace ui = vibr::ui::vintage;

namespace
{
const ui::Palette kPal { juce::Colour (120, 200, 60) };

// Design space: 1120 wide (margin 28, gap 18), one row of three panels y 114 to 360, footer centre 15 above the bottom.
// SWEEP (two knobs and the SYNC button) takes the rest; RESONANCE and MODE are the same width.
constexpr float kDesignW = 1120.0f, kDesignH = 390.0f;
constexpr float kPanelY = 114.0f, kPanelH = 246.0f, kKnobDy = 86.0f, kGap = 18.0f;
constexpr float kOneW = 300.0f, kSweepW = kDesignW - 2.0f * ui::kMargin - 2.0f * kGap - 2.0f * kOneW;
constexpr float kKnobSpread = 80.0f;                        // the two SWEEP knobs sit this far each side of the panel centre

juce::Rectangle<float> sweepPanel() { return { ui::kMargin, kPanelY, kSweepW, kPanelH }; }
juce::Rectangle<float> resPanel()   { return { ui::kMargin + kSweepW + kGap, kPanelY, kOneW, kPanelH }; }
juce::Rectangle<float> modePanel()  { return { ui::kMargin + kSweepW + kOneW + 2.0f * kGap, kPanelY, kOneW, kPanelH }; }

juce::Point<float> rateCentre()  { return { sweepPanel().getCentreX() - kKnobSpread, kPanelY + kKnobDy }; }
juce::Point<float> depthCentre() { return { sweepPanel().getCentreX() + kKnobSpread, kPanelY + kKnobDy }; }
juce::Point<float> resCentre()   { return { resPanel().getCentreX(), kPanelY + kKnobDy }; }

juce::Rectangle<float> syncRect() { return juce::Rectangle<float> (120.0f, 30.0f).withCentre ({ rateCentre().x, kPanelY + 215.0f }); }
// I / II: two buttons that share their middle border, with a caption under each; the block is centred in the panel
juce::Rectangle<float> modeButton (int i)
{
    const auto p = modePanel();
    return { p.getCentreX() - 99.0f + 98.0f * (float) i, p.getY() + 92.0f, 100.0f, 30.0f };
}
juce::Rectangle<float> modeAreaRect() { return modeButton (0).getUnion (modeButton (1)); }
}

//==============================================================================
PH2AudioProcessorEditor::PH2AudioProcessorEditor (PH2AudioProcessor& p)
    : PedalEditor (p, kDesignW, kDesignH), proc (p),
      syncAtt (*p.apvts.getParameter ("sync"), [this] (float v) { syncChanged (v > 0.5f); }, nullptr),
      divisionAtt (*p.apvts.getParameter ("division"), [this] (float v) { division = juce::jlimit (0, vibr::sync::kNumDivisions - 1, (int) std::lround (v)); repaint(); }, nullptr),
      modeAtt (*p.apvts.getParameter ("mode"), [this] (float v) { modeIndex = v > 0.5f ? 1 : 0; repaint(); }, nullptr)
{
    const auto add = [this] (Knob& k, const char* id)
    {
        auto& s = k.slider;
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        s.setRotaryParameters (juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
        s.setMouseDragSensitivity (180);
        s.setScrollWheelEnabled (false);
        s.setPopupDisplayEnabled (true, true, this);
        s.onValueChange = [this] { repaint(); };
        addAndMakeVisible (s);
        midiUI.add (s, id);
        k.attachment = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter (id), s, nullptr);
    };
    add (res, "res");
    add (depth, "depth");
    add (rate, "rate");

    addAndMakeVisible (modeArea);
    modeArea.onDown = [this] (juce::Point<float> pos)
    {
        const auto pt = (pos + modeArea.getPosition().toFloat()) / scale();     // area-local pixels to design units
        for (int i = 0; i < 2; ++i)
            if (modeButton (i).expanded (3.0f).contains (pt)) { modeAtt.setValueAsCompleteGesture ((float) i); return; }
    };
    midiUI.add (modeArea, "mode");

    syncAtt.sendInitialUpdate();
    divisionAtt.sendInitialUpdate();
    modeAtt.sendInitialUpdate();
    resized();                                                       // place the knobs (the base class sized the window before they existed)
    startTimerHz (20);
}

PH2AudioProcessorEditor::~PH2AudioProcessorEditor() = default;

void PH2AudioProcessorEditor::resized()
{
    placeKnob (rate.slider, rateCentre());
    placeKnob (depth.slider, depthCentre());
    placeKnob (res.slider, resCentre());
    modeArea.setBounds (modeAreaRect().transformedBy (juce::AffineTransform::scale (scale())).toNearestInt());
}

void PH2AudioProcessorEditor::syncChanged (bool on)
{
    // the RATE knob is the rate, or the note division while sync is on (the attachment sets the slider range from the parameter)
    syncOn = on;
    rate.attachment.reset();
    divisionKnobAtt.reset();
    if (on) divisionKnobAtt = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter ("division"), rate.slider, nullptr);
    else    rate.attachment = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter ("rate"), rate.slider, nullptr);
    repaint();
}

void PH2AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    if (ui::presetBarMouseDown (e.position / scale(), *this, proc.presets, proc.undoManager, scale(), [this] { repaint(); })) return;
    if (toggleBypassAt (e.position)) { repaint(); return; }           // the power switch is the bypass switch
    if (syncRect().expanded (3.0f).contains (e.position / scale()))
        syncAtt.setValueAsCompleteGesture (syncOn ? 0.0f : 1.0f);
}

void PH2AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    midiUI.paintMarks (g, kPal.accent);
}

void PH2AudioProcessorEditor::timerCallback()
{
    if (midiUI.needsRepaint()) { midiUI.tick(); repaint(); }
    const bool on = powerOn();
    if (on != lastOn) { lastOn = on; repaint(); }
}

void PH2AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    ui::paintBody (g, kPal, kDesignW, kDesignH);
    ui::paintHeader (g, kPal, "PH-2", "SUPER PHASER", "PHASE MOVEMENT");
    ui::paintPresetBar (g, kPal, proc.presets, proc.undoManager, powerOn());

    // SWEEP: RATE (the note division while sync is on) and DEPTH, the SYNC button under RATE
    ui::paintPanel (g, kPal, sweepPanel(), "SWEEP");
    const float rateProp = (float) rate.slider.valueToProportionOfLength (rate.slider.getValue());
    if (syncOn)
        ui::paintKnob (g, kPal, rateCentre(), "RATE", vibr::sync::divisionNames()[division], "SLOW", "FAST", rateProp);
    else
        ui::paintKnob (g, kPal, rateCentre(), "RATE", juce::String (rate.slider.getValue(), 1), "0", "10", rateProp);
    ui::paintKnob (g, kPal, depthCentre(), "DEPTH", juce::String (depth.slider.getValue(), 1), "0", "10",
                   (float) depth.slider.valueToProportionOfLength (depth.slider.getValue()));
    ui::paintFlatButton (g, kPal, syncRect(), "SYNC", syncOn);

    ui::paintPanel (g, kPal, resPanel(), "RESONANCE");
    ui::paintKnob (g, kPal, resCentre(), "RES", juce::String (res.slider.getValue(), 1), "0", "10",
                   (float) res.slider.valueToProportionOfLength (res.slider.getValue()));

    // MODE: I = 4 stages, II = 8 stages (the lit button is painted last so its border stays on top)
    ui::paintPanel (g, kPal, modePanel(), "MODE");
    const char* labels[] = { "I", "II" };
    const char* captions[] = { "4 STAGES", "8 STAGES" };
    for (int k = 0; k < 2; ++k)
    {
        const int i = k == 0 ? 1 - modeIndex : modeIndex;
        ui::paintFlatButton (g, kPal, modeButton (i), labels[i], i == modeIndex, 20.0f);
    }
    for (int i = 0; i < 2; ++i)
        ui::drawText (g, captions[i], modeButton (i).getCentreX(), modeButton (i).getBottom() + 22.0f, 14.0f, kPal.inkSoft, false, 0.08f);

    ui::paintFooter (g, kPal, kDesignH - 15.0f);
}
