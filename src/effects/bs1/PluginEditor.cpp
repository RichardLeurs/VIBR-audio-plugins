// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"

namespace ui = vibr::ui::vintage;

namespace
{
const ui::Palette kPal { juce::Colour (137, 89, 213) };

// Design space 1120 x 522 (margin 28, gap 18): header 0 to 100, one row of two panels y 114 to 492, footer centre 15 above the bottom.
constexpr float kDesignW = 1120.0f, kDesignH = 522.0f;
constexpr float kPanelY = 114.0f, kPanelH = 378.0f, kGap = 18.0f;
constexpr float kTypeW = 598.0f, kCtlW = kDesignW - 2.0f * ui::kMargin - kGap - kTypeW;      // 448
constexpr float kKnobDy = 86.0f, kRowPitch = 172.0f, kColPitch = 108.0f;

struct KnobSpec { const char* id; const char* name; };
const KnobSpec kKnobs[8] = { { "value", "VALUE" }, { "decay", "DECAY" }, { "predelay", "PRE-DELAY" }, { "mix", "MIX" },
                             { "tone", "TONE" }, { "param1", "PARAM 1" }, { "param2", "PARAM 2" }, { "mod", "MOD" } };
const char* kTypeNames[bigsky::kNumTypes] = { "ROOM", "HALL", "PLATE", "SPRING", "SWELL", "BLOOM", "CLOUD", "CHORALE", "SHIMMER", "MAGNETO", "NONLINEAR", "REFLECTIONS" };

juce::Rectangle<float> typePanel() { return { ui::kMargin, kPanelY, kTypeW, kPanelH }; }
juce::Rectangle<float> ctlPanel()  { return { ui::kMargin + kTypeW + kGap, kPanelY, kCtlW, kPanelH }; }
juce::Rectangle<float> displayRect() { const auto p = typePanel(); return { p.getX() + 20.0f, p.getY() + 30.0f, p.getWidth() - 40.0f, 220.0f }; }
juce::Rectangle<float> typeButton (int i)       // two rows of six, equal width, 6 apart
{
    const auto d = displayRect();
    const float w = (d.getWidth() - 5.0f * 6.0f) / 6.0f;
    return { d.getX() + (float) (i % 6) * (w + 6.0f), d.getBottom() + 24.0f + (float) (i / 6) * 40.0f, w, 34.0f };
}
juce::Point<float> knobCentre (int i)
{
    const auto p = ctlPanel();
    return { p.getCentreX() + ((float) (i % 4) - 1.5f) * kColPitch, p.getY() + kKnobDy + (float) (i / 4) * kRowPitch };
}
}

//==============================================================================
BigSkyAudioProcessorEditor::BigSkyAudioProcessorEditor (BigSkyAudioProcessor& p)
    : PedalEditor (p, kDesignW, kDesignH), proc (p),
      typeAtt (*p.apvts.getParameter ("type"), [this] (float v)
               {
                   const int t = juce::jlimit (0, bigsky::kNumTypes - 1, (int) std::lround (v));
                   if (t != lastType) { lastType = t; shown = {}; shownUntil = 0; }
                   repaint();
               }, nullptr)
{
    for (int i = 0; i < 8; ++i)
    {
        auto& s = knobs[i].slider;
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        s.setRotaryParameters (juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
        s.setMouseDragSensitivity (180);
        s.setScrollWheelEnabled (false);
        s.setPopupDisplayEnabled (true, true, this);
        s.onValueChange = [this] { repaint(); };
        addAndMakeVisible (s);
        knobs[i].attachment = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter (kKnobs[i].id), s, nullptr);
        lastVals[i] = proc.apvts.getRawParameterValue (kKnobs[i].id)->load();
    }
    typeAtt.sendInitialUpdate();
    resized();                                                       // place the knobs (the base class sized the window before they existed)
    startTimerHz (20);
}

BigSkyAudioProcessorEditor::~BigSkyAudioProcessorEditor() = default;

void BigSkyAudioProcessorEditor::resized()
{
    for (int i = 0; i < 8; ++i) placeKnob (knobs[i].slider, knobCentre (i));
}

void BigSkyAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto pt = e.position / scale();
    proc.undoManager.beginNewTransaction();
    if (ui::presetBarMouseDown (pt, *this, proc.presets, proc.undoManager, scale(), [this] { repaint(); })) { repaint(); return; }
    if (toggleBypassAt (e.position)) { repaint(); return; }           // the power switch is the bypass switch
    if (e.mods.isPopupMenu()) return;
    for (int i = 0; i < bigsky::kNumTypes; ++i)
        if (typeButton (i).contains (pt)) { typeAtt.setValueAsCompleteGesture ((float) i); return; }
}

juce::String BigSkyAudioProcessorEditor::formatValue (int i) const
{
    const float v = (float) knobs[i].slider.getValue();
    const juce::String id (kKnobs[i].id);
    if (id == "decay")
    {
        const float s = bigsky::decaySeconds (lastType, v * 0.1f);
        return s < 1.0f ? juce::String ((int) std::lround (s * 1000.0f)) + "mS" : juce::String (s, 1) + "S";
    }
    if (id == "predelay")
        return juce::String ((int) std::lround (bigsky::predelayMs (v * 0.1f))) + "mS";
    return juce::String (v, 1);
}

void BigSkyAudioProcessorEditor::timerCallback()
{
    const bool on = powerOn();
    bool dirty = on != lastOn;
    lastOn = on;
    for (int i = 0; i < 8; ++i)
    {
        const float v = proc.apvts.getRawParameterValue (kKnobs[i].id)->load();
        if (std::fabs (v - lastVals[i]) > 1.0e-4f)
        {
            lastVals[i] = v;
            shown = juce::String (kKnobs[i].name) + "  " + formatValue (i);
            shownUntil = juce::Time::getMillisecondCounter() + 1500;
            dirty = true;
        }
    }
    if (shownUntil != 0 && (juce::int32) (juce::Time::getMillisecondCounter() - shownUntil) > 0)
    {
        shown = {}; shownUntil = 0; dirty = true;
    }
    if (dirty) repaint();
}

void BigSkyAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    ui::paintBody (g, kPal, kDesignW, kDesignH);
    ui::paintHeader (g, kPal, "BS-1", "REVERB MACHINE", "ENDLESS SPACES");
    ui::paintPresetBar (g, kPal, proc.presets, proc.undoManager, powerOn());

    // TYPE: the display with the name of the current type, and the 12 types as buttons
    ui::paintPanel (g, kPal, typePanel(), "TYPE");
    const auto d = displayRect();
    ui::paintScreen (g, kPal, d);
    ui::drawText (g, "REVERB TYPE", d.getX() + 16.0f, d.getY() + 22.0f, 15.0f, kPal.phosphorDim, false, 0.08f, juce::Justification::centredLeft, ui::Face::Mono);
    ui::drawText (g, juce::String (lastType + 1) + " / " + juce::String (bigsky::kNumTypes), d.getRight() - 16.0f, d.getY() + 22.0f, 15.0f,
                  kPal.phosphorDim, false, 0.08f, juce::Justification::centredRight, ui::Face::Mono);
    const juce::String name (kTypeNames[lastType]);
    const float nameW = ui::textWidth (ui::font (64.0f, true, ui::Face::Mono).withExtraKerningFactor (0.05f), name);
    const float nameSize = juce::jmin (64.0f, 64.0f * (d.getWidth() - 40.0f) / nameW);
    ui::drawText (g, name, d.getCentreX(), d.getCentreY() - 8.0f, nameSize, kPal.phosphor, true, 0.05f, juce::Justification::centred, ui::Face::Mono);
    const float decay = bigsky::decaySeconds (lastType, lastVals[1] * 0.1f), pre = bigsky::predelayMs (lastVals[2] * 0.1f);
    const juce::String line = shown.isNotEmpty() ? shown.toUpperCase()
        : "DECAY " + (decay < 1.0f ? juce::String ((int) std::lround (decay * 1000.0f)) + "mS" : juce::String (decay, 1) + "S") + "   PRE " + juce::String ((int) std::lround (pre)) + "mS";
    ui::drawText (g, line, d.getCentreX(), d.getBottom() - 34.0f, 22.0f, shown.isNotEmpty() ? kPal.phosphor : kPal.phosphorDim, true, 0.05f, juce::Justification::centred, ui::Face::Mono);
    for (int i = 0; i < bigsky::kNumTypes; ++i)
        ui::paintFlatButton (g, kPal, typeButton (i), kTypeNames[i], i == lastType, 17.0f, 1.0f, 4.0f, 0.02f);

    // CONTROLS: eight knobs in two rows of four
    ui::paintPanel (g, kPal, ctlPanel(), "CONTROLS");
    for (int i = 0; i < 8; ++i)
    {
        auto& s = knobs[i].slider;
        ui::paintKnob (g, kPal, knobCentre (i), kKnobs[i].name, formatValue (i), "0", "10", (float) s.valueToProportionOfLength (s.getValue()));
    }

    ui::paintFooter (g, kPal, kDesignH - 15.0f);
}
