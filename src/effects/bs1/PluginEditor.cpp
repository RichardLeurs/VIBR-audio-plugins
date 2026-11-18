// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "BS1Data.h"

namespace
{
// Positions are in the 1786 x 1048 coordinates of the rendered background (make_background.py). The editor is resizable at a
// fixed aspect ratio; paint() scales image pixels to the window.
constexpr float kImgW = 1786.0f, kImgH = 1048.0f;
constexpr int kBaseW = 893;

constexpr float kKnobRadius = 64.0f, kTypeRadius = 80.0f;
struct KnobSpec { const char* id; float x, y; };
const float kCols[4] = { 851.0f, 1101.0f, 1351.0f, 1601.0f };
const float kRows[2] = { 400.0f, 715.0f };
const KnobSpec kKnobs[8] = {
    { "value", kCols[0], kRows[0] }, { "decay", kCols[1], kRows[0] }, { "predelay", kCols[2], kRows[0] }, { "mix", kCols[3], kRows[0] },
    { "tone", kCols[0], kRows[1] }, { "param1", kCols[1], kRows[1] }, { "param2", kCols[2], kRows[1] }, { "mod", kCols[3], kRows[1] } };
constexpr float kStartDeg = -135.0f, kEndDeg = 135.0f;
const juce::Point<float> kTypeKnob { 363.0f, 624.0f };            // 12 positions, ROOM at the bottom, clockwise
const juce::Rectangle<float> kDisplay { 203.0f, 308.0f, 320.0f, 64.0f };
const juce::Point<float> kLed { 1711.0f, 87.0f };                 // power slide switch (top right, ball left = off) = bypass

} // namespace

//==============================================================================
//==============================================================================
BigSkyAudioProcessorEditor::BigSkyAudioProcessorEditor (BigSkyAudioProcessor& p)
    : PedalEditor (p, BS1Data::background_png, (size_t) BS1Data::background_pngSize, kImgW, kImgH, kBaseW, 447), proc (p)
{
    setLookAndFeel (&laf);
    
    const auto setup = [this] (Knob& k, const char* id, float startDeg, float endDeg)
    {
        k.slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        k.slider.setRotaryParameters (juce::degreesToRadians (startDeg), juce::degreesToRadians (endDeg), true);
        k.slider.setMouseDragSensitivity (180);
        k.slider.setScrollWheelEnabled (false);
        addAndMakeVisible (k.slider);
        k.attachment = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter (id), k.slider, nullptr);
    };
    for (int i = 0; i < 8; ++i)
    {
        setup (knobs[i], kKnobs[i].id, kStartDeg, kEndDeg);
        lastVals[i] = proc.apvts.getRawParameterValue (kKnobs[i].id)->load();
    }
    setup (typeKnob, "type", 180.0f, 510.0f);                    // ROOM at the bottom, 30 degrees per type, clockwise
    typeKnob.slider.wrapSteps = bigsky::kNumTypes;               // turns endlessly
    lastType = (int) std::lround (proc.apvts.getRawParameterValue ("type")->load());

    resized();                                                       // place the knobs (the base class sized the window before they existed)
    startTimerHz (20);
}

BigSkyAudioProcessorEditor::~BigSkyAudioProcessorEditor() { setLookAndFeel (nullptr); }

void BigSkyAudioProcessorEditor::resized()
{
    const float S = (float) getWidth() / kImgW;
    for (int i = 0; i < 8; ++i)
        knobs[i].slider.setBounds (juce::Rectangle<float> (2.0f * kKnobRadius * S, 2.0f * kKnobRadius * S)
                                       .withCentre ({ kKnobs[i].x * S, kKnobs[i].y * S }).toNearestInt());
    typeKnob.slider.setBounds (juce::Rectangle<float> (2.0f * kTypeRadius * S, 2.0f * kTypeRadius * S)
                                   .withCentre ({ kTypeKnob.x * S, kTypeKnob.y * S }).toNearestInt());
}

void BigSkyAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    // the power slide switch is the bypass switch
    toggleBypassAt (e.position, kLed);
}

juce::String BigSkyAudioProcessorEditor::formatValue (int i) const
{
    const float v = proc.apvts.getRawParameterValue (kKnobs[i].id)->load();
    const juce::String id (kKnobs[i].id);
    if (id == "decay")
    {
        const float s = bigsky::decaySeconds (lastType, v * 0.1f);
        return s < 1.0f ? juce::String ((int) std::lround (s * 1000.0f)) + "mS" : juce::String (s, 1) + "S";
    }
    if (id == "predelay")
        return juce::String ((int) std::lround (bigsky::predelayMs (v * 0.1f))) + "mS";
    static const char* lab[8] = { "VL", "", "", "MX", "TN", "P1", "P2", "MD" };
    return juce::String (lab[i]) + " " + juce::String (v, 1);
}

void BigSkyAudioProcessorEditor::timerCallback()
{
    const int t = (int) std::lround (proc.apvts.getRawParameterValue ("type")->load());
    const bool on = proc.apvts.getRawParameterValue ("bypass")->load() < 0.5f;
    bool dirty = on != lastOn;
    lastOn = on;
    if (t != lastType) { lastType = t; shown = {}; shownUntil = 0; dirty = true; }
    for (int i = 0; i < 8; ++i)
    {
        const float v = proc.apvts.getRawParameterValue (kKnobs[i].id)->load();
        if (std::fabs (v - lastVals[i]) > 1.0e-4f)
        {
            lastVals[i] = v;
            shown = formatValue (i);
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
    paintBackground (g);       // the image, then image pixels

    // ---- display: six cells, green characters; the type name, or the value of the knob being turned ----
    {
        const auto disp = kDisplay.reduced (6.0f);
        const juce::String text = shown.isNotEmpty() ? shown : juce::String (bigsky::typeName (lastType));
        const float cellW = disp.getWidth() / 6.0f;
        juce::Font font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 44.0f, juce::Font::bold));
        g.setFont (font);
        const juce::String padded = text.paddedRight (' ', 6).substring (0, 6);
        for (int i = 0; i < 6; ++i)
        {
            const auto cell = juce::Rectangle<float> (disp.getX() + cellW * (float) i, disp.getY(), cellW, disp.getHeight());
            g.setColour (juce::Colour (0xff7cff2a).withAlpha (0.05f));
            g.drawText ("8", cell, juce::Justification::centred, false);
            const auto ch = padded.substring (i, i + 1);
            if (ch.trim().isEmpty()) continue;
            g.setColour (juce::Colour (0xff7cff2a).withAlpha (0.22f));
            for (auto o : { juce::Point<float> (-1.5f, 0), juce::Point<float> (1.5f, 0), juce::Point<float> (0, -1.5f), juce::Point<float> (0, 1.5f) })
                g.drawText (ch, cell.translated (o.x, o.y), juce::Justification::centred, false);
            g.setColour (juce::Colour (0xffb8ff6a));
            g.drawText (ch, cell, juce::Justification::centred, false);
        }
    }

    // ---- power slide switch: chrome ball left = off, right = on; the slot lights up green when on ----
    const bool on = proc.apvts.getRawParameterValue ("bypass")->load() < 0.5f;
    vibr::ui::paintSlideSwitch (g, kLed, on);
}
