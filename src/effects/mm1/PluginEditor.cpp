// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"

namespace ui = vibr::ui::vintage;

namespace
{
const ui::Palette kPal { juce::Colour (75, 145, 209) };

// Design space: 1120 wide (margin 28, gap 18), one row of three equal panels y 114 to 360, footer centre 15 above the bottom.
constexpr float kDesignW = 1120.0f, kDesignH = 390.0f;
constexpr float kPanelY = 114.0f, kPanelH = 246.0f, kKnobDy = 86.0f, kGap = 18.0f;
constexpr float kPanelW = (kDesignW - 2.0f * ui::kMargin - 2.0f * kGap) / 3.0f;
constexpr float kKnobSpread = 70.0f;                        // the two knobs sit this far each side of the panel centre
constexpr int kDelayKnob = 3;                               // index of "delay": while sync is on it picks the division

struct KnobSpec { const char* id; const char* name; int panel; int side; };
const KnobSpec kKnobs[6] = { { "blend", "BLEND", 0, -1 },     { "level", "LEVEL", 0, 1 },
                             { "feedback", "FEEDBACK", 1, -1 }, { "delay", "DELAY", 1, 1 },
                             { "chrsVibr", "CHRS/VIBR", 2, -1 }, { "depth", "DEPTH", 2, 1 } };
const char* kTitles[3] = { "MIX", "ECHO", "MODULATION" };

juce::Rectangle<float> panelRect (int i) { return { ui::kMargin + (float) i * (kPanelW + kGap), kPanelY, kPanelW, kPanelH }; }
juce::Point<float> knobCentre (int i) { const auto p = panelRect (kKnobs[i].panel); return { p.getCentreX() + (float) kKnobs[i].side * kKnobSpread, kPanelY + kKnobDy }; }
juce::Rectangle<float> syncRect() { return juce::Rectangle<float> (120.0f, 30.0f).withCentre ({ knobCentre (kDelayKnob).x, kPanelY + 215.0f }); }
}

//==============================================================================
MemoryManAudioProcessorEditor::MemoryManAudioProcessorEditor (MemoryManAudioProcessor& p)
    : PedalEditor (p, kDesignW, kDesignH), proc (p),
      syncAtt (*p.apvts.getParameter ("sync"), [this] (float v) { syncChanged (v > 0.5f); }, nullptr),
      divisionAtt (*p.apvts.getParameter ("division"), [this] (float v) { division = juce::jlimit (0, vibr::sync::kNumDivisions - 1, (int) std::lround (v)); repaint(); }, nullptr)
{
    for (int i = 0; i < 6; ++i)
    {
        auto& k = knobs[i];
        auto& s = k.slider;
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        s.setRotaryParameters (juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
        s.setMouseDragSensitivity (180);
        s.setScrollWheelEnabled (false);
        s.setPopupDisplayEnabled (true, true, this);
        s.onValueChange = [this] { repaint(); };
        addAndMakeVisible (s);
        midiUI.add (s, kKnobs[i].id);
        k.attachment = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter (kKnobs[i].id), s, nullptr);
    }

    syncAtt.sendInitialUpdate();
    divisionAtt.sendInitialUpdate();
    resized();                                                       // place the knobs (the base class sized the window before they existed)
    startTimerHz (20);
}

MemoryManAudioProcessorEditor::~MemoryManAudioProcessorEditor() = default;

void MemoryManAudioProcessorEditor::resized()
{
    for (int i = 0; i < 6; ++i) placeKnob (knobs[i].slider, knobCentre (i));
}

void MemoryManAudioProcessorEditor::syncChanged (bool on)
{
    // the DELAY knob is the delay time, or the note division while sync is on (the attachment sets the slider range from the parameter)
    syncOn = on;
    auto& k = knobs[kDelayKnob];
    k.attachment.reset();
    divisionKnobAtt.reset();
    if (on) divisionKnobAtt = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter ("division"), k.slider, nullptr);
    else    k.attachment    = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter ("delay"), k.slider, nullptr);
    repaint();
}

void MemoryManAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    if (toggleBypassAt (e.position)) { repaint(); return; }           // the power switch is the bypass switch
    if (syncRect().expanded (3.0f).contains (e.position / scale()))
        syncAtt.setValueAsCompleteGesture (syncOn ? 0.0f : 1.0f);
}

void MemoryManAudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    midiUI.paintMarks (g, kPal.accent);
}

void MemoryManAudioProcessorEditor::timerCallback()
{
    if (midiUI.needsRepaint()) { midiUI.tick(); repaint(); }
    const bool on = powerOn();
    if (on != lastOn) { lastOn = on; repaint(); }
}

void MemoryManAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    ui::paintBody (g, kPal, kDesignW, kDesignH);
    ui::paintHeaderNoPresets (g, kPal, "MM-1", "ANALOG DELAY", "WARM REPEATS", powerOn());

    for (int i = 0; i < 3; ++i) ui::paintPanel (g, kPal, panelRect (i), kTitles[i]);
    for (int i = 0; i < 6; ++i)
    {
        auto& s = knobs[i].slider;
        const float prop = (float) s.valueToProportionOfLength (s.getValue());
        if (i == kDelayKnob && syncOn)
            ui::paintKnob (g, kPal, knobCentre (i), kKnobs[i].name, vibr::sync::divisionNames()[division], "SHORT", "LONG", prop);
        else
            ui::paintKnob (g, kPal, knobCentre (i), kKnobs[i].name, juce::String (s.getValue(), 1), "0", "10", prop);
    }
    ui::paintFlatButton (g, kPal, syncRect(), "SYNC", syncOn);

    ui::paintFooter (g, kPal, kDesignH - 15.0f);
}
