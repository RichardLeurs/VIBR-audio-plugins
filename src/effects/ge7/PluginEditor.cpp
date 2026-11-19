// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"

namespace ui = vibr::ui::vintage;

namespace
{
const ui::Palette kPal { juce::Colour (40, 169, 213) };

// Design space: 1120 wide (margin 28, gap 18), one row of two panels (EQUALIZER 846 wide, LEVEL 200), y 114 to 472, footer centre 15 above the bottom.
constexpr float kDesignW = 1120.0f, kDesignH = 502.0f;
constexpr float kPanelY = 114.0f, kPanelH = 358.0f;
constexpr float kLevelW = 200.0f, kEqW = kDesignW - 2.0f * ui::kMargin - 18.0f - kLevelW;
constexpr float kTrackTop = 48.0f, kTravel = 200.0f;          // fader travel (+15 dB top, -15 dB bottom) from the panel top: the cap centre
constexpr float kCapW = 46.0f, kCapH = 26.0f;
constexpr float kNameDy = 284.0f, kValueDy = 305.0f;          // name and value rows from the panel top (40 under the cap, 40 above ... equal to the panel's top gap)
constexpr float kEqPad = 24.0f;                               // panel edge to the scale labels (left) and to the last ticks (right)
const char* const kIds[7] = { "f100", "f200", "f400", "f800", "f1k6", "f3k2", "f6k4" };
const char* const kNames[7] = { "100 HZ", "200 HZ", "400 HZ", "800 HZ", "1.6 KHZ", "3.2 KHZ", "6.4 KHZ" };

juce::Rectangle<float> eqRect()    { return { ui::kMargin, kPanelY, kEqW, kPanelH }; }
juce::Rectangle<float> levelRect() { return { ui::kMargin + kEqW + 18.0f, kPanelY, kLevelW, kPanelH }; }
float bandX (int b) { const float x0 = eqRect().getX() + kEqPad + 60.0f, x1 = eqRect().getRight() - kEqPad - 38.0f; return x0 + (x1 - x0) / 6.0f * (float) b; }
float levelX() { return levelRect().getCentreX(); }
float trackTop() { return kPanelY + kTrackTop; }

juce::String dbText (double v) { return (v > 0.04 ? "+" : "") + juce::String (v, 1) + " dB"; }
} // namespace

//==============================================================================
void GE7FaderLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int, int w, int, float sliderPos, float, float,
                                            juce::Slider::SliderStyle, juce::Slider&)
{
    const float cx = (float) x + (float) w * 0.5f;
    const auto r = juce::Rectangle<float> (kCapW * scale, kCapH * scale).withCentre ({ cx, sliderPos });
    g.setColour (juce::Colour (0xff8F846F));                                         // the lower rim, as under the knobs
    g.fillRoundedRectangle (r.translated (0.0f, 3.0f * scale), 4.0f * scale);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4A443D), r.getX(), r.getY(), juce::Colour (0xff15120F), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 4.0f * scale);
    g.setColour (juce::Colour (0xffF3ECDC));                                         // the cream pointer line
    g.fillRect (juce::Rectangle<float> (r.getWidth() - 14.0f * scale, 3.0f * scale).withCentre (r.getCentre()));
}

//==============================================================================
GE7AudioProcessorEditor::GE7AudioProcessorEditor (GE7AudioProcessor& p)
    : PedalEditor (p, kDesignW, kDesignH), proc (p)
{
    const auto add = [this] (Fader& k, const char* id)
    {
        auto& s = k.slider;
        s.setLookAndFeel (&faderLaf);
        s.setSliderStyle (juce::Slider::LinearVertical);
        s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        s.setSliderSnapsToMousePosition (false);
        s.setDoubleClickReturnValue (true, 0.0);
        s.setScrollWheelEnabled (false);
        s.setPopupDisplayEnabled (true, true, this);
        s.onValueChange = [this] { repaint(); };
        addAndMakeVisible (s);
        k.attachment = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter (id), s, nullptr);
    };
    for (int b = 0; b < ge7::kBands; ++b) add (band[b], kIds[b]);
    add (level, "level");

    resized();
    startTimerHz (20);
}

GE7AudioProcessorEditor::~GE7AudioProcessorEditor()
{
    for (auto& b : band) b.slider.setLookAndFeel (nullptr);
    level.slider.setLookAndFeel (nullptr);
}

void GE7AudioProcessorEditor::resized()
{
    const float S = scale();
    faderLaf.scale = S;
    const auto place = [S] (Fader& k, float x)
    {
        k.slider.setBounds (juce::Rectangle<float> (60.0f * S, (kTravel + kCapH) * S)
                                .withCentre ({ x * S, (trackTop() + kTravel * 0.5f) * S }).toNearestInt());
    };
    for (int b = 0; b < ge7::kBands; ++b) place (band[b], bandX (b));
    place (level, levelX());
}

void GE7AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    if (toggleBypassAt (e.position)) repaint();                       // the power switch is the bypass switch
}

void GE7AudioProcessorEditor::timerCallback()
{
    const bool on = powerOn();
    if (on != lastOn) { lastOn = on; repaint(); }
}

void GE7AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    ui::paintBody (g, kPal, kDesignW, kDesignH);
    ui::paintHeaderNoPresets (g, kPal, "GE-7", "GRAPHIC EQUALIZER", "TONE SHAPING", powerOn());
    ui::paintPanel (g, kPal, eqRect(), "EQUALIZER");
    ui::paintPanel (g, kPal, levelRect(), "LEVEL");

    const auto fader = [&] (Fader& f, float x, const juce::String& name, bool scaleLabels)
    {
        const float top = trackTop(), mid = top + kTravel * 0.5f, bot = top + kTravel;
        const float prop = (float) f.slider.valueToProportionOfLength (f.slider.getValue());
        const float capY = bot - kTravel * prop;
        g.setColour (kPal.track);                                                     // the slot, and the value from 0 dB to the cap in the accent
        g.fillRoundedRectangle (x - 3.0f, top - 4.0f, 6.0f, kTravel + 8.0f, 3.0f);
        g.setColour (kPal.accent);
        g.fillRect (x - 3.0f, juce::jmin (capY, mid), 6.0f, std::abs (capY - mid));
        for (int k = 0; k < 7; ++k)                                                   // +15, +10, +5, 0, -5, -10, -15
        {
            const float y = top + kTravel * (float) k / 6.0f;
            const float w = (k == 3 || k == 0 || k == 6) ? 28.0f : 21.0f;
            g.setColour (k == 3 ? kPal.ink : kPal.inkSoft);
            g.fillRect (x - w - 10.0f, y - (k == 3 ? 1.0f : 0.75f), w, k == 3 ? 2.0f : 1.5f);
            g.fillRect (x + 10.0f, y - (k == 3 ? 1.0f : 0.75f), w, k == 3 ? 2.0f : 1.5f);
        }
        if (scaleLabels)
            for (const auto& [k, t] : { std::pair<int, const char*> { 0, "+15" }, { 3, "0" }, { 6, "-15" } })
                ui::drawText (g, t, x - 44.0f, top + kTravel * (float) k / 6.0f, 13.0f, kPal.inkSoft, false, 0.0f, juce::Justification::centredRight, ui::Face::Mono);
        ui::drawText (g, name, x, kPanelY + kNameDy, 21.0f, kPal.ink, true, 0.1f);
        ui::drawText (g, dbText (f.slider.getValue()), x, kPanelY + kValueDy, 14.0f, kPal.accent, true, 0.0f, juce::Justification::centred, ui::Face::Mono);
    };
    for (int b = 0; b < ge7::kBands; ++b) fader (band[b], bandX (b), kNames[b], b == 0);
    fader (level, levelX(), "LEVEL", true);
    ui::paintFooter (g, kPal, kDesignH - 15.0f);
}
