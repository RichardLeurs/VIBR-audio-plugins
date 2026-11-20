// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"

namespace ui = vibr::ui::vintage;

namespace
{
const ui::Palette kPal { juce::Colour (216, 190, 53) };

// Design space: 1120 wide (margin 28, gap 18), two equal panels OUTPUT / COMPRESSION, y 114 to 344.
constexpr float kDesignW = 1120.0f, kDesignH = 374.0f;
constexpr float kPanelY = 114.0f, kPanelH = 230.0f, kKnobDy = 96.0f, kBigR = 44.0f, kLabelUp = 12.0f, kGap = 18.0f, kKnobSpread = 130.0f;
constexpr float kPanelW = (kDesignW - 2.0f * ui::kMargin - kGap) / 2.0f;

juce::Rectangle<float> panelRect (int i) { return { ui::kMargin + (float) i * (kPanelW + kGap), kPanelY, kPanelW, kPanelH }; }
juce::Point<float> knobCentre (int k)         // 0, 1 = OUTPUT; 2, 3 = COMPRESSION
{
    return { panelRect (k / 2).getCentreX() + (k % 2 == 0 ? -kKnobSpread : kKnobSpread), kPanelY + kKnobDy };
}
} // namespace

CS3AudioProcessorEditor::CS3AudioProcessorEditor (CS3AudioProcessor& p)
    : PedalEditor (p, kDesignW, kDesignH), proc (p)
{
    for (auto* k : { &level, &tone, &attack, &sustain })
    {
        auto& s = k->slider;
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        s.setRotaryParameters (juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
        s.setMouseDragSensitivity (180);
        s.setScrollWheelEnabled (false);
        s.setPopupDisplayEnabled (true, true, this);
        s.onValueChange = [this] { repaint(); };
        addAndMakeVisible (s);
        midiUI.add (s, k->id);
        k->attachment = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter (k->id), s, nullptr);
    }

    resized();
    startTimerHz (20);
}

CS3AudioProcessorEditor::~CS3AudioProcessorEditor() = default;

void CS3AudioProcessorEditor::resized()
{
    placeKnob (level.slider, knobCentre (0), kBigR);
    placeKnob (tone.slider, knobCentre (1), kBigR);
    placeKnob (attack.slider, knobCentre (2), kBigR);
    placeKnob (sustain.slider, knobCentre (3), kBigR);
}

void CS3AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    if (ui::presetBarMouseDown (e.position / scale(), *this, proc.presets, proc.undoManager, scale(), [this] { repaint(); })) return;
    if (toggleBypassAt (e.position)) repaint();       // the power switch is the bypass switch
}

void CS3AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    midiUI.paintMarks (g, kPal.accent);
}

void CS3AudioProcessorEditor::timerCallback()
{
    if (midiUI.needsRepaint()) { midiUI.tick(); repaint(); }
    const bool on = powerOn();
    if (on != lastOn) { lastOn = on; repaint(); }
}

void CS3AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    ui::paintBody (g, kPal, kDesignW, kDesignH);
    ui::paintHeader (g, kPal, "CS-3", "COMPRESSION SUSTAINER", "SMOOTH SUSTAIN");
    ui::paintPresetBar (g, kPal, proc.presets, proc.undoManager, powerOn());

    ui::paintPanel (g, kPal, panelRect (0), "OUTPUT");
    ui::paintPanel (g, kPal, panelRect (1), "COMPRESSION");

    Knob* knobs[] = { &level, &tone, &attack, &sustain };
    for (int k = 0; k < 4; ++k)
    {
        auto& s = knobs[k]->slider;
        ui::paintKnob (g, kPal, knobCentre (k), knobs[k]->name, juce::String (s.getValue(), 1), knobs[k]->lo, knobs[k]->hi,
                       (float) s.valueToProportionOfLength (s.getValue()), kBigR, kLabelUp);
    }
    ui::paintFooter (g, kPal, kDesignH - 15.0f);
}
