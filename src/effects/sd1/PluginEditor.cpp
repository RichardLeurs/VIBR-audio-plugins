// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"

namespace ui = vibr::ui::vintage;

namespace
{
const ui::Palette kPal { juce::Colour (57, 121, 239) };

// Design space 1120 x 636 (margin 28, gap 18): row 1 (y 114, 246 high) INPUT and DELAY TIME, row 2 (y 378, 228 high) three equal two-knob panels.
constexpr float kDesignW = 1120.0f, kDesignH = 636.0f, kGap = 18.0f;
constexpr float kRow1Y = 114.0f, kRow1H = 246.0f, kRow2Y = 378.0f, kRow2H = 228.0f, kKnobDy = 86.0f;
constexpr float kAreaW = kDesignW - 2.0f * ui::kMargin;
constexpr float kInputW = 360.0f, kTimeW = kAreaW - kInputW - kGap, kThirdW = (kAreaW - 2.0f * kGap) / 3.0f;

juce::Rectangle<float> inputPanel()  { return { ui::kMargin, kRow1Y, kInputW, kRow1H }; }
juce::Rectangle<float> timePanel()   { return { ui::kMargin + kInputW + kGap, kRow1Y, kTimeW, kRow1H }; }
juce::Rectangle<float> thirdPanel (int i) { return { ui::kMargin + (float) i * (kThirdW + kGap), kRow2Y, kThirdW, kRow2H }; }   // REGENERATION, MODULATION, OUTPUT

constexpr float kTimeSpread = 130.0f, kPairSpread = 85.0f;

// index in knobs[]: 0 level, 1 delayTime, 2 factor, 3 feedback, 4 hiCut, 5 modIntensity, 6 modFrequency, 7 direct, 8 effect
juce::Point<float> knobCentre (int i)
{
    switch (i)
    {
        case 0: return { inputPanel().getRight() - 76.0f, kRow1Y + kKnobDy };
        case 1: return { timePanel().getCentreX() - kTimeSpread, kRow1Y + kKnobDy };
        case 2: return { timePanel().getCentreX() + kTimeSpread, kRow1Y + kKnobDy };
        default:
        {
            const int panel = (i - 3) / 2, side = (i - 3) % 2;
            return { thirdPanel (panel).getCentreX() + (side == 0 ? -kPairSpread : kPairSpread), kRow2Y + kKnobDy };
        }
    }
}

juce::Rectangle<float> syncRect() { return juce::Rectangle<float> (120.0f, 30.0f).withCentre ({ knobCentre (1).x, kRow1Y + 215.0f }); }
juce::Rectangle<float> meterScreen() { return { inputPanel().getX() + 22.0f, kRow1Y + 36.0f, 130.0f, 190.0f }; }

struct KnobSpec { const char* id; const char* name; const char* lo; const char* hi; float startDeg, endDeg; };
const KnobSpec kKnobs[9] = {
    { "level",        "LEVEL",     "MIN",   "MAX",  -135.0f, 135.0f }, { "delayTime", "TIME", "SHORT", "LONG", -130.0f, 50.0f },     // TIME only turns from SHORT to LONG
    { "factor",       "FACTOR",    "x0.25", "x1",   -135.0f, 135.0f }, { "feedback",  "FEEDBACK", "-10", "+10", -135.0f, 135.0f },
    { "hiCut",        "HI-CUT",    "MIN",   "MAX",  -135.0f, 135.0f }, { "modIntensity", "INTENSITY", "MIN", "MAX", -135.0f, 135.0f },
    { "modFrequency", "FREQUENCY", "SLOW",  "FAST", -135.0f, 135.0f }, { "direct",    "DIRECT", "MIN", "MAX", -135.0f, 135.0f },
    { "effect",       "EFFECT",    "MIN",   "MAX",  -135.0f, 135.0f } };
constexpr int kTimeKnob = 1, kFactorKnob = 2, kFeedbackKnob = 3;

// a knob with its own sweep (the TIME knob): the ring, the legends at its ends, the name and the value, as ui::paintKnob
void paintRangeKnob (juce::Graphics& g, juce::Point<float> c, const char* name, const juce::String& value, const char* lo, const char* hi, float prop, float startDeg, float endDeg)
{
    const float R = ui::kKnobR, r = ui::ringRadius (R), w = R * 0.24f;
    const float a0 = juce::degreesToRadians (startDeg), a1 = juce::degreesToRadians (endDeg);
    juce::Path track, lit;
    track.addCentredArc (c.x, c.y, r, r, 0.0f, a0, a1, true);
    g.setColour (kPal.track);
    g.strokePath (track, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (prop > 0.002f)
    {
        lit.addCentredArc (c.x, c.y, r, r, 0.0f, a0, a0 + prop * (a1 - a0), true);
        g.setColour (kPal.accent);
        g.strokePath (lit, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    const auto endPoint = [&] (float a) { return juce::Point<float> (c.x + std::sin (a) * r, c.y - std::cos (a) * r); };
    const auto pe = endPoint (a0), pf = endPoint (a1);
    ui::drawText (g, lo, pe.x - 12.0f, pe.y + 24.0f, 13.0f, kPal.inkSoft, false, 0.0f, juce::Justification::centredLeft, ui::Face::Mono);       // under the lower left end
    ui::drawText (g, hi, pf.x + 12.0f, pf.y - 4.0f, 13.0f, kPal.inkSoft, false, 0.0f, juce::Justification::centredLeft, ui::Face::Mono);        // right of the upper right end
    ui::drawText (g, name, c.x, c.y + R * 1.71f + 22.0f, 21.0f, kPal.ink, true, 0.1f);
    ui::drawText (g, value, c.x, c.y + R * 1.71f + 43.0f, 14.0f, kPal.accent, true, 0.0f, juce::Justification::centred, ui::Face::Mono);
}
} // namespace

//==============================================================================
SDD1000AudioProcessorEditor::SDD1000AudioProcessorEditor (SDD1000AudioProcessor& p)
    : PedalEditor (p, kDesignW, kDesignH), proc (p),
      syncAtt (*p.apvts.getParameter ("sync"), [this] (float v) { syncOn = v > 0.5f; syncChanged(); }, nullptr),
      divisionAtt (*p.apvts.getParameter ("division"), [this] (float v) { division = (int) std::lround (v); repaint(); }, nullptr)
{
    for (int i = 0; i < 9; ++i)
    {
        auto& k = knobs[i];
        k.slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        k.slider.setRotaryParameters (juce::degreesToRadians (kKnobs[i].startDeg), juce::degreesToRadians (kKnobs[i].endDeg), true);
        k.slider.setMouseDragSensitivity (180);
        k.slider.setScrollWheelEnabled (false);
        k.slider.setPopupDisplayEnabled (true, true, this);
        k.slider.onValueChange = [this] { repaint(); };
        addAndMakeVisible (k.slider);
        k.attachment = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter (kKnobs[i].id), k.slider, nullptr);
        midiUI.add (k.slider, kKnobs[i].id);
    }

    syncAtt.sendInitialUpdate();
    divisionAtt.sendInitialUpdate();

    resized();                                                       // place the knobs (the base class sized the window before they existed)
    startTimerHz (30);
}

SDD1000AudioProcessorEditor::~SDD1000AudioProcessorEditor() = default;

void SDD1000AudioProcessorEditor::resized()
{
    for (int i = 0; i < 9; ++i) placeKnob (knobs[i].slider, knobCentre (i));
}

void SDD1000AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    const auto pt = e.position / scale();
    if (ui::powerRect().expanded (3.0f).contains (pt))                // the power switch = the `power` parameter
    {
        auto* prm = proc.apvts.getParameter ("power");
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->getValue() > 0.5f ? 0.0f : 1.0f);
        prm->endChangeGesture();
        repaint();
        return;
    }
    if (syncRect().expanded (3.0f).contains (pt)) syncAtt.setValueAsCompleteGesture (syncOn ? 0.0f : 1.0f);      // the TIME knob then selects the note division
}

void SDD1000AudioProcessorEditor::syncChanged()
{
    // the TIME knob is the delay time, or the note division while sync is on; FACTOR has no effect with sync on, so it is dimmed
    auto& k = knobs[kTimeKnob];
    k.attachment.reset();
    divisionKnobAtt.reset();
    if (syncOn) divisionKnobAtt = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter ("division"), k.slider, nullptr);
    else        k.attachment    = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter ("delayTime"), k.slider, nullptr);
    knobs[kFactorKnob].slider.setAlpha (syncOn ? 0.35f : 1.0f);
    repaint();
}

void SDD1000AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    midiUI.paintMarks (g, kPal.accent);
}

void SDD1000AudioProcessorEditor::timerCallback()
{
    if (midiUI.needsRepaint()) { midiUI.tick(); repaint(); }
    const float db = juce::Decibels::gainToDecibels (proc.meterLevel.load(), -60.0f);
    meterDb += (db - meterDb) * 0.5f;
    const bool pwr = proc.apvts.getRawParameterValue ("power")->load() > 0.5f && proc.apvts.getRawParameterValue ("bypass")->load() < 0.5f;
    const int segs = (meterDb >= -20.0f) + (meterDb >= -10.0f) + (meterDb >= 0.0f) + (meterDb >= 6.0f);
    if (segs != lastSegs || pwr != lastPower)
    {
        lastSegs = segs; lastPower = pwr;
        repaint();
    }
}

void SDD1000AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    ui::paintBody (g, kPal, kDesignW, kDesignH);
    ui::paintHeaderNoPresets (g, kPal, "SD-1", "DIGITAL DELAY", "DIGITAL ECHOES", lastPower);

    const auto prop = [this] (int i) { return (float) knobs[i].slider.valueToProportionOfLength (knobs[i].slider.getValue()); };
    const auto one = [this] (int i) { return juce::String (knobs[i].slider.getValue(), 1); };
    const auto plain = [&] (int i)
    {
        ui::paintKnob (g, kPal, knobCentre (i), kKnobs[i].name, one (i), kKnobs[i].lo, kKnobs[i].hi, prop (i));
    };

    // INPUT: the headroom screen (4 segments, bottom to top) and LEVEL
    ui::paintPanel (g, kPal, inputPanel(), "INPUT");
    {
        const auto screen = meterScreen();
        ui::paintScope (g, kPal, screen, "HEADROOM", "");
        const auto plot = ui::scopePlotRect (screen);
        constexpr int segs = 4;
        const float thresholds[segs] = { -20.0f, -10.0f, 0.0f, 6.0f };
        const char* labels[segs] = { "-20dB", "-10dB", "0dB", "+6dB" };
        const float gap = 6.0f, segH = (plot.getHeight() - gap * (float) (segs - 1)) / (float) segs;
        for (int i = 0; i < segs; ++i)
        {
            const bool lit = lastPower && meterDb >= thresholds[i];
            const auto lampCol = i == 3 ? kPal.segRed : (i == 2 ? kPal.segYellow : kPal.segGreen);
            const juce::Rectangle<float> seg (screen.getX() + 14.0f, plot.getBottom() - (float) (i + 1) * segH - (float) i * gap, 46.0f, segH);
            g.setColour (lit ? lampCol : kPal.segOff);
            g.fillRoundedRectangle (seg, 2.0f);
            ui::drawText (g, labels[i], seg.getRight() + 10.0f, seg.getCentreY(), 13.0f, kPal.phosphorDim, false, 0.0f, juce::Justification::centredLeft, ui::Face::Mono);
        }
    }
    plain (0);

    // DELAY TIME: TIME (the note division while sync is on), FACTOR (dimmed with sync on) and the SYNC button under TIME
    ui::paintPanel (g, kPal, timePanel(), "DELAY TIME");
    paintRangeKnob (g, knobCentre (kTimeKnob), kKnobs[kTimeKnob].name,
                    syncOn ? juce::String (vibr::sync::divisionNames()[juce::jlimit (0, vibr::sync::kNumDivisions - 1, division)]) : one (kTimeKnob),
                    syncOn ? "SLOW" : "SHORT", syncOn ? "FAST" : "LONG", prop (kTimeKnob), kKnobs[kTimeKnob].startDeg, kKnobs[kTimeKnob].endDeg);
    if (syncOn) g.beginTransparencyLayer (0.35f);
    ui::paintKnob (g, kPal, knobCentre (kFactorKnob), "FACTOR",
                   "x" + juce::String (0.25f * std::pow (4.0f, (float) knobs[kFactorKnob].slider.getValue() / 10.0f), 2), "x0.25", "x1", prop (kFactorKnob));
    if (syncOn) g.endTransparencyLayer();
    ui::paintFlatButton (g, kPal, syncRect(), "SYNC", syncOn);

    // REGENERATION (FEEDBACK is bipolar: 5 = none), MODULATION, OUTPUT
    ui::paintPanel (g, kPal, thirdPanel (0), "REGENERATION");
    {
        const double v = knobs[kFeedbackKnob].slider.getValue();
        const float amount = (float) (v - 5.0) * 2.0f;
        ui::paintBipolarArc (g, knobCentre (kFeedbackKnob), ui::kKnobR, amount / 10.0f, kPal.accent, kPal.track);
        const auto c = knobCentre (kFeedbackKnob);
        const float rr = ui::kKnobR * 1.71f, ly = c.y + ui::ringRadius (ui::kKnobR) * 0.707f + 15.0f;
        ui::drawText (g, "-10", c.x - ui::kKnobR * 1.4f, ly, 13.0f, kPal.inkSoft, false, 0.0f, juce::Justification::centredLeft, ui::Face::Mono);
        ui::drawText (g, "+10", c.x + ui::kKnobR * 1.4f, ly, 13.0f, kPal.inkSoft, false, 0.0f, juce::Justification::centredRight, ui::Face::Mono);
        ui::drawText (g, "FEEDBACK", c.x, c.y + rr + 22.0f, 21.0f, kPal.ink, true, 0.1f);
        ui::drawText (g, (amount > 0.0f ? "+" : "") + juce::String (amount, 1), c.x, c.y + rr + 43.0f, 14.0f, kPal.accent, true, 0.0f, juce::Justification::centred, ui::Face::Mono);
    }
    plain (4);
    ui::paintPanel (g, kPal, thirdPanel (1), "MODULATION");
    plain (5); plain (6);
    ui::paintPanel (g, kPal, thirdPanel (2), "OUTPUT");
    plain (7); plain (8);

    ui::paintFooter (g, kPal, kDesignH - 15.0f);
}
