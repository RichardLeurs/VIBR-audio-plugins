// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"

namespace ui = vibr::ui::vintage;

namespace
{
const ui::Palette kPal { juce::Colour (32, 182, 165) };           // the Modulation turquoise #20B6A5: value rings, knob values and the plugin code

// Design space 1120 x 388: header (0 to 100), one row of four panels (y 114, 244 high), footer. Margin 28, gap 18.
constexpr float kDesignW = ui::kW, kDesignH = 388.0f;
constexpr float kPanelY = 114.0f, kPanelH = 244.0f, kKnobDY = 86.0f;
const juce::Rectangle<float> kPanels[4] { { 28.0f, kPanelY, 300.0f, kPanelH },  { 346.0f, kPanelY, 190.0f, kPanelH },
                                          { 554.0f, kPanelY, 330.0f, kPanelH }, { 902.0f, kPanelY, 190.0f, kPanelH } };   // INPUT, CHORUS, VIBRATO, EFFECT

constexpr float kKnobDX = 70.0f;                                   // the two knobs of the VIBRATO panel are 140 apart, symmetric about the panel centre
enum { kLevel, kChorus, kDepth, kRate };
juce::Point<float> knobCentre (int which)
{
    switch (which)
    {
        case kLevel:  return { kPanels[0].getX() + 225.0f, kPanels[0].getY() + kKnobDY };
        case kChorus: return { kPanels[1].getCentreX(), kPanels[1].getY() + kKnobDY };
        case kDepth:  return { kPanels[2].getCentreX() - kKnobDX, kPanels[2].getY() + kKnobDY };
        default:      return { kPanels[2].getCentreX() + kKnobDX, kPanels[2].getY() + kKnobDY };
    }
}
juce::Rectangle<float> syncRect()                                  // under the RATE knob, 200 below the panel top, 30 high
{
    return juce::Rectangle<float> (110.0f, 30.0f).withCentre ({ knobCentre (kRate).x, kPanelY + 215.0f });
}
// two stacked buttons that share their middle border: i = 0 on top (HIGH, CHORUS), 1 below (LOW, VIBRATO)
juce::Rectangle<float> pairRect (int panel, int i)
{
    const auto& r = kPanels[panel];
    const float w = panel == 0 ? 110.0f : r.getWidth() - 48.0f;
    return { r.getX() + (panel == 0 ? 25.0f : 24.0f), r.getY() + 73.0f + 48.0f * (float) i, w, 50.0f };
}
juce::Point<float> peakLamp() { return { kPanels[0].getX() + 53.0f, kPanelY + 215.0f }; }

void paintPair (juce::Graphics& g, int panel, const char* top, const char* bottom, bool topOn)
{
    for (const int i : { topOn ? 1 : 0, topOn ? 0 : 1 })            // the lit one last: it keeps its whole border
        ui::paintFlatButton (g, kPal, pairRect (panel, i), i == 0 ? top : bottom, (i == 0) == topOn, 20.0f, 1.0f, 4.0f, 0.1f);
}
} // namespace

//==============================================================================
CE1AudioProcessorEditor::CE1AudioProcessorEditor (CE1AudioProcessor& p)
    : PedalEditor (p, kDesignW, kDesignH, 560), proc (p),
      inputAtt (*p.apvts.getParameter ("inputHigh"), [this] (float v) { inputHigh = v > 0.5f; repaint(); }, nullptr),
      vibratoAtt (*p.apvts.getParameter ("vibrato"), [this] (float v) { vibrato = v > 0.5f; repaint(); }, nullptr),
      syncAtt (*p.apvts.getParameter ("sync"), [this] (float v) { syncOn = v > 0.5f; syncChanged(); }, nullptr),
      divisionAtt (*p.apvts.getParameter ("division"), [this] (float v) { division = (int) std::lround (v); repaint(); }, nullptr)
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
    add (level, "level");
    add (chorus, "chorusIntensity");
    add (depth, "vibratoDepth");
    add (rate, "vibratoRate");
    midiUI.add (level.slider, "level");  midiUI.add (chorus.slider, "chorusIntensity");
    midiUI.add (depth.slider, "vibratoDepth");  midiUI.add (rate.slider, "vibratoRate");
    inputAtt.sendInitialUpdate();
    vibratoAtt.sendInitialUpdate();
    syncAtt.sendInitialUpdate();
    divisionAtt.sendInitialUpdate();

    resized();                                                       // place the knobs (the base class sized the window before they existed)
    startTimerHz (20);
}

CE1AudioProcessorEditor::~CE1AudioProcessorEditor() { stopTimer(); }

void CE1AudioProcessorEditor::resized()
{
    placeKnob (level.slider, knobCentre (kLevel));
    placeKnob (chorus.slider, knobCentre (kChorus));
    placeKnob (depth.slider, knobCentre (kDepth));
    placeKnob (rate.slider, knobCentre (kRate));
}

void CE1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    if (toggleBypassAt (e.position)) return;
    const auto p = e.position / scale();
    const auto hit = [&] (juce::Rectangle<float> r) { return r.expanded (3.0f).contains (p); };
    if (hit (syncRect()))
    {
        syncAtt.setValueAsCompleteGesture (syncOn ? 0.0f : 1.0f);
        return;
    }
    const auto choose = [&] (juce::ParameterAttachment& att, int panel, bool currentTop)
    {
        for (int i = 0; i < 2; ++i)
            if (hit (pairRect (panel, i)))
            {
                if ((i == 0) != currentTop)
                {
                    att.setValueAsCompleteGesture (panel == 0 ? (i == 0 ? 1.0f : 0.0f) : (i == 0 ? 0.0f : 1.0f));   // HIGH = 1, LOW = 0; CHORUS = 0, VIBRATO = 1
                }
                return true;
            }
        return false;
    };
    if (choose (inputAtt, 0, inputHigh)) return;
    choose (vibratoAtt, 3, ! vibrato);
}

void CE1AudioProcessorEditor::syncChanged()
{
    // the RATE knob is the vibrato rate, or the note division while sync is on (the attachment sets the slider range from the parameter)
    rate.attachment.reset();
    divisionKnobAtt.reset();
    if (syncOn) divisionKnobAtt = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter ("division"), rate.slider, nullptr);
    else        rate.attachment = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter ("vibratoRate"), rate.slider, nullptr);
    repaint();
}

void CE1AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    midiUI.paintMarks (g, kPal.accent);
}

int CE1AudioProcessorEditor::stateKey() const
{
    int k = powerOn() ? 1 : 0;
    for (const auto* s : { &level.slider, &chorus.slider, &depth.slider, &rate.slider }) k = k * 31 + (int) std::lround (s->getValue() * 100.0);
    return k * 31 + (syncOn ? 1 : 0) * 7 + division * 3 + (inputHigh ? 1 : 0) * 2 + (vibrato ? 1 : 0) + (peakLit ? 1000003 : 0);
}

void CE1AudioProcessorEditor::timerCallback()
{
    const bool learn = midiUI.needsRepaint();
    if (learn) midiUI.tick();
    if (proc.inputPeak.exchange (0.0f) > 0.9f) peakHold = 3;
    const bool lit = peakHold > 0;
    if (peakHold > 0) --peakHold;
    peakLit = lit;
    if (const int key = stateKey(); key != lastKey) { lastKey = key; repaint(); }
    else if (learn) repaint();
}

//==============================================================================
void CE1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    ui::paintBody (g, kPal, kDesignW, kDesignH);
    ui::paintHeaderNoPresets (g, kPal, "CE-1", "CHORUS / VIBRATO", "LUSH DIMENSION", powerOn());
    ui::paintFooter (g, kPal, kDesignH - 15.0f);

    ui::paintPanel (g, kPal, kPanels[0], "INPUT");
    ui::paintPanel (g, kPal, kPanels[1], "CHORUS");
    ui::paintPanel (g, kPal, kPanels[2], "VIBRATO");
    ui::paintPanel (g, kPal, kPanels[3], "EFFECT");

    const auto knob = [&] (Knob& k, int which, const char* name, const juce::String& value, const juce::String& lo, const juce::String& hi)
    {
        ui::paintKnob (g, kPal, knobCentre (which), name, value, lo, hi, (float) k.slider.valueToProportionOfLength (k.slider.getValue()));
    };
    const auto tenths = [] (Knob& k) { return juce::String (k.slider.getValue(), 1); };
    knob (level, kLevel, "LEVEL", tenths (level), "0", "10");
    knob (chorus, kChorus, "INTENSITY", tenths (chorus), "0", "10");
    knob (depth, kDepth, "DEPTH", tenths (depth), "0", "10");

    const auto& names = vibr::sync::divisionNames();
    if (syncOn)
        knob (rate, kRate, "RATE", names[juce::jlimit (0, vibr::sync::kNumDivisions - 1, division)], "LONG", "SHORT");
    else
    {
        const float hz = ce1::vibratoRateHz ((float) rate.slider.getValue() * 0.1f);       // the knob is 0..10
        knob (rate, kRate, "RATE", juce::String (hz, hz < 1.0f ? 2 : 1) + " HZ", "0.4", "10 Hz");
    }
    ui::paintFlatButton (g, kPal, syncRect(), "SYNC", syncOn, 17.0f, 1.0f, 4.0f, 0.1f);

    paintPair (g, 0, "HIGH", "LOW", inputHigh);
    paintPair (g, 3, "CHORUS", "VIBRATO", ! vibrato);

    // peak lamp (input clipping) with its name
    {
        const auto c = peakLamp();
        const auto disc = juce::Rectangle<float> (14.0f, 14.0f).withCentre (c);
        if (peakLit) { g.setColour (kPal.ledOn.withAlpha (0.25f)); g.fillEllipse (disc.expanded (7.0f)); }
        g.setColour (peakLit ? kPal.ledOn : kPal.ledOff);
        g.fillEllipse (disc);
        g.setColour (kPal.ink);
        g.drawEllipse (disc.expanded (1.0f), 2.0f);
        ui::drawText (g, "PEAK", c.x + 17.0f, c.y, 17.0f, kPal.ink, true, 0.1f, juce::Justification::centredLeft);
    }
}
