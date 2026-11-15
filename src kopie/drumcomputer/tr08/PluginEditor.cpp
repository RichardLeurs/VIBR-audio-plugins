// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "TR08Data.h"
#include <functional>
#include <map>

namespace
{
// Positions are in the 1200 x 700 coordinates of the editor (make_background.py renders the 2400 x 1400 background; every
// position there is twice these). The editor is resizable at a fixed aspect ratio.
constexpr float kImgW = 1200.0f, kImgH = 700.0f;
constexpr int kBaseW = 1200;

struct KnobSpec { const char* id; float x, y, r; float startDeg, endDeg; };

float colX (int i) { return 73.0f + 105.5f * (float) i; }       // voice column i (BD .. CH)
constexpr float kRowY[3] = { 432.5f, 482.5f, 532.5f };         // level / tone / decay rows
constexpr float kVoiceR = 14.0f, kVoiceDx = -20.0f;

#define V(i, row, id) { id, colX (i) + kVoiceDx, kRowY[row], kVoiceR, -135.0f, 135.0f }
const KnobSpec kKnobs[] = {
    { "accent",   357.5f, 225.0f, 22.0f, -135.0f, 135.0f },
    V (0, 0, "bdLevel"),  V (0, 1, "bdTone"),  V (0, 2, "bdDecay"),
    V (1, 0, "sdLevel"),  V (1, 1, "sdTone"),  V (1, 2, "sdSnappy"),
    V (2, 0, "ltLevel"),  V (2, 1, "ltTune"),
    V (3, 0, "mtLevel"),  V (3, 1, "mtTune"),
    V (4, 0, "htLevel"),  V (4, 1, "htTune"),
    V (5, 0, "rsLevel"),
    V (6, 0, "cpLevel"),
    V (7, 0, "cbLevel"),
    V (8, 0, "cyLevel"),  V (8, 1, "cyTone"),  V (8, 2, "cyDecay"),
    V (9, 0, "ohLevel"),  V (9, 1, "ohDecay"),
    V (10, 0, "chLevel"),
    { "tempo",    100.0f, 255.0f, 29.0f, -135.0f, 135.0f },
    { "fine",     255.0f, 175.0f, 18.0f, -135.0f, 135.0f },
    { "mode",     462.5f, 225.0f, 22.0f,  -90.0f,  61.0f },    // pattern clear, 1st part, 2nd part, manual play, play, compose
    { "inst",     705.0f, 225.0f, 23.0f,  180.0f, 510.0f },    // AC at the bottom, then clockwise BD ... CH
    { "measures", 585.0f, 225.0f, 22.0f,  -90.0f,  62.0f },    // auto fill in: Manual, 16, 12, 8, 4, 2 measures
};
#undef V

// pre-scale slide switch: 4 vertical positions (1 = 8th triplet, 2 = 16th triplet, 3 = 16th, 4 = 32nd)
const juce::Point<float> kPreScale { 840.0f, 222.5f };
const float kPreScaleY[4] = { 185.0f, 210.0f, 235.0f, 260.0f };
juce::Rectangle<float> preScaleHit() { return juce::Rectangle<float> (44.0f, 120.0f).withCentre ({ kPreScale.x + 8.0f, kPreScale.y }); }

// I/F-variation slide switch (A / B): which pattern the fill-in measure plays; basic-variation switch (A / AB / B)
const juce::Point<float> kIfSwitch { 980.0f, 277.5f }, kBasicVar { 980.0f, 200.0f };
constexpr float kCapR = 10.0f, kCapShift = 25.0f;

// sample selector under each voice name: [<] n [>]
float selX (int i) { return colX (i); }
constexpr float kSelY = 400.0f, kSelW = 65.0f, kSelH = 20.0f, kSelArrow = 16.0f;
const char* const kVarParam[tr08::kNumInst] = { "bdVar", "sdVar", "ltVar", "mtVar", "htVar", "rsVar", "cpVar", "cbVar", "cyVar", "ohVar", "chVar" };
juce::Rectangle<float> selectorRect (int i) { return juce::Rectangle<float> (kSelW, kSelH).withCentre ({ selX (i), kSelY }); }

float altX (int i) { return colX (i + 2) - 22.0f; }                 // LT, MT, HT, RS, CP columns
const char* const kAltIds[5] = { "ltConga", "mtConga", "htConga", "rsClaves", "cpMaracas" };
constexpr float kAltY = 532.5f;

const juce::Rectangle<float> kLogo { 280.0f, 28.0f, 330.0f, 35.0f };       // kit selector
const juce::Rectangle<float> kDisplay { 40.0f, 145.0f, 150.0f, 50.0f };
const float kDigitX[4] = { 62.5f, 100.0f, 137.5f, 175.0f };
constexpr float kDigitW = 24.0f, kDigitH = 36.0f, kDigitY = 170.0f;

const juce::Point<float> kStart { 240.0f, 227.5f }, kTap { 240.0f, 275.0f }, kBank { 1075.0f, 260.0f }, kClear { 1145.0f, 260.0f };
const juce::Point<float> kVarLedA { 960.0f, 175.0f }, kVarLedB { 1000.0f, 175.0f }, kPart1Led { 1135.0f, 180.0f }, kPart2Led { 1135.0f, 215.0f };

float stepX (int i) { return 56.25f + 72.5f * (float) i; }
constexpr float kStepY = 638.0f, kStepLedY = 606.0f;

// seven-segment digit; digit < 0 = blank; segment order a b c d e f g
void drawSegDigit (juce::Graphics& g, juce::Point<float> c, float w, float h, int digit, juce::Colour on, juce::Colour off, bool dot)
{
    static const int masks[10] = { 0b0111111, 0b0000110, 0b1011011, 0b1001111, 0b1100110, 0b1101101, 0b1111101, 0b0000111, 0b1111111, 0b1101111 };
    const int mask = digit >= 0 ? masks[digit] : 0;
    const float t = h * 0.12f, hl = w * 0.62f, vl = h * 0.40f;

    const auto seg = [&] (int bit, juce::Rectangle<float> r)
    {
        const bool lit = (mask >> bit) & 1;
        if (lit)
        {
            g.setColour (on.withAlpha (0.28f));
            g.fillRoundedRectangle (r.expanded (1.6f), t * 0.7f);
        }
        g.setColour (lit ? on : off);
        g.fillRoundedRectangle (r, t * 0.45f);
    };
    const float x = c.x, y = c.y;
    seg (0, juce::Rectangle<float> (hl, t).withCentre ({ x, y - h * 0.46f + t * 0.5f }));              // a
    seg (3, juce::Rectangle<float> (hl, t).withCentre ({ x, y + h * 0.46f - t * 0.5f }));              // d
    seg (6, juce::Rectangle<float> (hl, t).withCentre ({ x, y }));                                    // g
    seg (5, juce::Rectangle<float> (t, vl).withCentre ({ x - w * 0.38f, y - h * 0.22f }));            // f
    seg (1, juce::Rectangle<float> (t, vl).withCentre ({ x + w * 0.38f, y - h * 0.22f }));            // b
    seg (4, juce::Rectangle<float> (t, vl).withCentre ({ x - w * 0.38f, y + h * 0.22f }));            // e
    seg (2, juce::Rectangle<float> (t, vl).withCentre ({ x + w * 0.38f, y + h * 0.22f }));            // c
    if (dot)
    {
        g.setColour (on);
        g.fillEllipse (juce::Rectangle<float> (t * 1.4f, t * 1.4f).withCentre ({ x + w * 0.68f, y + h * 0.46f - t * 0.4f }));
    }
}

void drawLed (juce::Graphics& g, juce::Point<float> c, float r, juce::Colour colour, bool on)
{
    g.setColour (juce::Colour (0xff0c0e14));
    g.fillEllipse (juce::Rectangle<float> (r * 2.0f + 3.0f, r * 2.0f + 3.0f).withCentre (c));
    if (on)
    {
        g.setColour (colour.withAlpha (0.35f));
        g.fillEllipse (juce::Rectangle<float> (r * 3.6f, r * 3.6f).withCentre (c));
    }
    g.setGradientFill (on ? juce::ColourGradient (colour.brighter (0.7f), c.x - r * 0.4f, c.y - r * 0.5f, colour.darker (0.2f), c.x + r * 0.6f, c.y + r * 0.7f, true)
                          : juce::ColourGradient (colour.darker (1.8f), c.x - r * 0.4f, c.y - r * 0.5f, colour.darker (3.0f), c.x + r * 0.6f, c.y + r * 0.7f, true));
    g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
    g.setColour (juce::Colours::white.withAlpha (on ? 0.55f : 0.18f));
    g.fillEllipse (juce::Rectangle<float> (r * 0.8f, r * 0.55f).withCentre (c.translated (-r * 0.3f, -r * 0.45f)));
}

void drawSlideTrack (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (juce::Colour (0xff05070c));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (juce::Colours::white.withAlpha (0.22f));
    g.drawRoundedRectangle (r.expanded (0.8f), 3.5f, 0.8f);
}

// round silver cap of the basic / I/F variation switches
void drawSilverCap (juce::Graphics& g, juce::Point<float> c, float r)
{
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (juce::Rectangle<float> (2.0f * r, 2.0f * r).withCentre (c + juce::Point<float> (0.8f, 1.6f)));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff4f5f7), c.x - 0.4f * r, c.y - 0.5f * r,
                                             juce::Colour (0xff80848c), c.x + 0.7f * r, c.y + 0.8f * r, true));
    g.fillEllipse (juce::Rectangle<float> (2.0f * r, 2.0f * r).withCentre (c));
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawEllipse (juce::Rectangle<float> (2.0f * r, 2.0f * r).withCentre (c), 0.8f);
    g.setColour (juce::Colours::white.withAlpha (0.35f));                       // brushed highlight
    g.fillEllipse (juce::Rectangle<float> (r * 0.9f, r * 0.55f).withCentre (c - juce::Point<float> (0.25f * r, 0.35f * r)));
}

void drawLever (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (r.translated (1.0f, 1.5f), 2.5f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff868a92), r.getX(), r.getY(), juce::Colour (0xff23262c), r.getRight(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 2.5f);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    for (int i = 1; i <= 2; ++i)
    {
        const float yy = r.getY() + (float) i * r.getHeight() / 3.0f;
        g.drawLine (r.getX() + 2.0f, yy, r.getRight() - 2.0f, yy, 1.0f);
    }
}

} // namespace

//==============================================================================
void TR08LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float,
                                        float startAngle, float endAngle, juce::Slider& s)
{
    const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const auto c = b.getCentre();
    const float R = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f - 0.5f;
    const double lo = s.getMinimum(), hi = s.getMaximum();
    const float pos = (float) ((s.getValue() - lo) / (hi - lo));
    const float angle = startAngle + pos * (endAngle - startAngle);
    const auto circle = [&] (float r) { return juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c); };
    const auto polar = [&] (float r, float a) { return juce::Point<float> (c.x + r * std::sin (a), c.y - r * std::cos (a)); };

    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (circle (R).translated (R * 0.05f, R * 0.09f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4c51), c.x - R * 0.7f, c.y - R * 0.8f, juce::Colour (0xff050506), c.x + R * 0.7f, c.y + R * 0.9f, false));
    g.fillEllipse (circle (R));
    const int ridges = R > 20.0f ? 60 : 36;
    for (int i = 0; i < ridges; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / (float) ridges;
        g.setColour (juce::Colours::white.withAlpha (0.10f + 0.10f * std::max (0.0f, std::cos (a + 0.8f))));
        g.drawLine ({ polar (R * 0.90f, a), polar (R * 0.99f, a) }, 1.0f);
    }
    g.setColour (juce::Colours::black);
    g.drawEllipse (circle (R), 1.0f);

    const float capR = R * 0.80f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3b3f), c.x - capR * 0.6f, c.y - capR * 0.7f, juce::Colour (0xff0b0b0c), c.x + capR * 0.6f, c.y + capR * 0.8f, false));
    g.fillEllipse (circle (capR));
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.14f), c.x - capR * 0.5f, c.y - capR * 0.6f,
                                             juce::Colours::transparentWhite, c.x + capR * 0.1f, c.y + capR * 0.1f, true));
    g.fillEllipse (circle (capR));
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.35f), c.x - capR, c.y - capR, juce::Colour (0xff000000), c.x + capR, c.y + capR, false));
    g.drawEllipse (circle (capR), 1.0f);

    juce::Path p;
    p.addRoundedRectangle (-capR * 0.055f, -capR * 0.94f, capR * 0.11f, capR * 0.50f, capR * 0.05f);
    g.setColour (juce::Colour (0xfff2f2ee));
    g.fillPath (p, juce::AffineTransform::rotation (angle).translated (c));
}

//==============================================================================
TR08AudioProcessorEditor::TR08AudioProcessorEditor (TR08AudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&laf);

    background = juce::ImageFileFormat::loadFrom (TR08Data::background_png, (size_t) TR08Data::background_pngSize);

    for (int i = 0; i < kNumKnobs; ++i)
        addKnob (knobs[i], kKnobs[i].id, { kKnobs[i].x, kKnobs[i].y }, kKnobs[i].r, "black", kKnobs[i].startDeg, kKnobs[i].endDeg);
    knobs[24].slider.setMouseDragSensitivity (120);      // mode: 6 positions
    knobs[25].slider.setMouseDragSensitivity (200);      // instrument: 12 positions
    knobs[26].slider.setMouseDragSensitivity (120);      // measures: 6 positions

    for (int i = 0; i < 5; ++i)
        addZone (altZones[i], [this, i] { toggle (kAltIds[i]); });
    for (int i = 0; i < 16; ++i)
        addZone (stepZones[i], [this, i] { pressStep (i); });
    addZone (startZone,     [this] { toggle ("run"); });
    addZone (tapZone,       [this] { tap(); });
    addZone (bankZone,      [this] { toggle ("shuffle"); });
    addZone (clearZone,     [this] { trackClear(); });
    addZone (kitZone,       [this] { showKitMenu(); });

    constrainer.setFixedAspectRatio ((double) kImgW / (double) kImgH);
    constrainer.setSizeLimits (600, 350, 2400, 1400);
    setConstrainer (&constrainer);
    setResizable (true, true);
    setSize (kBaseW, (int) std::round ((float) kBaseW * kImgH / kImgW));
    startTimerHz (30);
}

TR08AudioProcessorEditor::~TR08AudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void TR08AudioProcessorEditor::addKnob (Knob& k, const char* paramId, juce::Point<float> centre, float radius, const char* style,
                                        float startDeg, float endDeg)
{
    k.centre = centre;
    k.radius = radius;
    k.slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    k.slider.setRotaryParameters (juce::degreesToRadians (startDeg), juce::degreesToRadians (endDeg), true);
    k.slider.setMouseDragSensitivity (180);
    k.slider.setScrollWheelEnabled (false);
    k.slider.getProperties().set ("style", style);
    addAndMakeVisible (k.slider);
    k.attachment = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter (paramId), k.slider, nullptr);
}

void TR08AudioProcessorEditor::addZone (Zone& z, std::function<void()> onPress)
{
    z.onPress = std::move (onPress);
    addAndMakeVisible (z);
}

void TR08AudioProcessorEditor::setValue (const char* id, float v)
{
    auto* prm = proc.apvts.getParameter (id);
    prm->beginChangeGesture();
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
    prm->endChangeGesture();
}

void TR08AudioProcessorEditor::toggle (const char* id)
{
    auto* prm = proc.apvts.getParameter (id);
    prm->beginChangeGesture();
    prm->setValueNotifyingHost (prm->getValue() > 0.5f ? 0.0f : 1.0f);
    prm->endChangeGesture();
}

// Which variation / part the step buttons show and edit
int TR08AudioProcessorEditor::editVar() const  { return (int) std::round (get ("variation")) == 2 ? 1 : 0; }
int TR08AudioProcessorEditor::editHalf() const { return (int) std::round (get ("mode")) == 2 ? 1 : 0; }

void TR08AudioProcessorEditor::pressStep (int step)
{
    const int mode = (int) std::round (get ("mode"));
    const int track = (int) std::round (get ("inst"));
    if ((mode == 1 || mode == 2) && clearZone.isDown())        // hold [CLEAR] + step button: STEP NUMBER of the shown part
        proc.setSteps (editHalf(), step + 1);
    else if (mode == 1 || mode == 2)
    {
        proc.toggleStep (editVar(), editHalf(), track, step);
        if (track >= 1 && track <= tr08::kNumInst && proc.getStep (editVar(), editHalf(), track, step))   // preview when switched on (track 0 is the accent row)
            proc.triggerManual (track - 1);
    }
    else if (mode == 3 && step < tr08::kNumInst)       // manual play: buttons 1-11 play BD ... CH
        proc.triggerManual (step);
}

// The red [CLEAR] button follows the Pattern Write knob (Roland TR-08 owner's manual):
//   PATTERN CLEAR : clears the variation chosen with BASIC-VARIATION (A or B; AB clears both), back to 16 + 0 steps
//   1st PART      : applies the PRE-SCALE switch (hold it and press a step button for the STEP NUMBER, see pressStep)
//   2nd PART      : hold it and press a step button for the 2nd part's STEP NUMBER; with [TAP] the 2nd part is set to 0 steps
//   COMPOSE       : TRACK CLEAR - there is no rhythm track here, so it clears the instrument chosen with the select knob
void TR08AudioProcessorEditor::trackClear()
{
    const int mode = (int) std::round (get ("mode"));
    const int variation = (int) std::round (get ("variation"));
    if (mode == 0)
    {
        if (variation != 2) proc.clearVariation (0);
        if (variation != 0) proc.clearVariation (1);
    }
    else if (mode == 1)
        proc.applyPreScale();
    else if (mode == 5)
    {
        const int track = (int) std::round (get ("inst"));
        if (variation != 2) proc.clearTrack (0, track);
        if (variation != 0) proc.clearTrack (1, track);
    }
}

void TR08AudioProcessorEditor::tap()
{
    if (clearZone.isDown())                                    // [CLEAR] + [TAP]: 2nd part back to 0 steps
    {
        proc.setSteps (1, 0);
        return;
    }
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (! taps.empty() && now - taps.back() > 2000.0)
        taps.clear();
    taps.push_back (now);
    if (taps.size() > 5)
        taps.erase (taps.begin());
    if (taps.size() >= 2)
    {
        const double avg = (taps.back() - taps.front()) / (double) (taps.size() - 1);
        setValue ("tempo", (float) juce::jlimit (40.0, 300.0, 60000.0 / avg));
        setValue ("fine", 5.0f);
    }
}

// Kit selector: built-in synth or any kit found in the samples folder (grouped by folder), plus folder / rescan commands.
void TR08AudioProcessorEditor::showKitMenu()
{
    struct Node { std::map<juce::String, Node> kids; int id = -1; };
    const auto names = proc.getKitNames();
    const auto current = proc.getKitName();

    Node root;
    for (int i = 0; i < names.size(); ++i)
    {
        Node* n = &root;
        for (auto& part : juce::StringArray::fromTokens (names[i], "/", ""))
            n = &n->kids[part];
        n->id = 100 + i;
    }

    std::function<void (juce::PopupMenu&, const Node&, const juce::String&)> fill;
    fill = [&] (juce::PopupMenu& menu, const Node& node, const juce::String& prefix)
    {
        for (auto& [label, kid] : node.kids)
        {
            const auto path = prefix.isEmpty() ? label : prefix + "/" + label;
            if (kid.kids.empty())
                menu.addItem (kid.id, label, true, path == current);
            else
            {
                juce::PopupMenu sub;
                fill (sub, kid, path);
                menu.addSubMenu (label, sub, true, nullptr, current.startsWith (path + "/"));
            }
        }
    };

    juce::PopupMenu menu;
    menu.addItem (1, "Built-in synth", true, current.isEmpty());
    menu.addSeparator();
    if (names.isEmpty())
        menu.addItem (-1, "No kits found in " + proc.getSamplesRoot().getFullPathName(), false);
    else
        fill (menu, root, {});
    static const char* const codes[tr08::kNumInst] = { "BD", "SD", "LT", "MT", "HT", "RS", "CP", "CB", "CY", "OH", "CH" };
    static const char* const full[tr08::kNumInst] = { "Bass drum", "Snare", "Low tom", "Mid tom", "Hi tom", "Rimshot", "Hand clap",
                                                       "Cowbell", "Cymbal", "Open hat", "Closed hat" };
    juce::PopupMenu variants;
    for (int i = 0; i < tr08::kNumInst; ++i)
    {
        const int count = std::min (proc.getVariantCount (i), 127);
        if (count < 2) continue;
        const int cur = (int) std::round (get ((juce::String (codes[i]).toLowerCase() + "Var").toRawUTF8()));
        juce::PopupMenu sub;
        sub.addItem (1000 + i * 200, "Cycle through all (" + juce::String (count) + ")", true, cur == 0);
        sub.addSeparator();
        for (int v = 1; v <= count; ++v)
            sub.addItem (1000 + i * 200 + v, juce::String (codes[i]) + "-" + juce::String (v), true, cur == v || (cur > count && v == count));
        variants.addSubMenu (juce::String (codes[i]) + "  " + full[i], sub);
    }
    if (variants.getNumItems() > 0)
    {
        menu.addSeparator();
        menu.addSubMenu ("Sample variants", variants);
    }
    menu.addSeparator();
    menu.addItem (2, "Choose samples folder...");
    menu.addItem (3, "Rescan samples folder");

    juce::Component::SafePointer<TR08AudioProcessorEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&kitZone), [safe, names] (int result)
    {
        if (safe == nullptr || result == 0) return;
        if (result == 1)       safe->proc.selectKit ({});
        else if (result == 3)  safe->proc.rescanKits();
        else if (result == 2)
        {
            safe->chooser = std::make_unique<juce::FileChooser> ("Select the folder that holds your drum kits", safe->proc.getSamplesRoot());
            safe->chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                                        [safe] (const juce::FileChooser& fc)
                                        {
                                            if (safe != nullptr && fc.getResult().isDirectory())
                                                safe->proc.setSamplesRoot (fc.getResult());
                                        });
        }
        else if (result >= 1000)
        {
            const int inst = (result - 1000) / 200, v = (result - 1000) % 200;
            safe->setValue ((juce::String (codes[inst]).toLowerCase() + "Var").toRawUTF8(), (float) v);
        }
        else if (result >= 100 && result < 1000 && result - 100 < names.size())
            safe->proc.selectKit (names[result - 100]);
    });
}

//==============================================================================
void TR08AudioProcessorEditor::resized()
{
    const float S = (float) getWidth() / kImgW;
    const auto box = [S] (juce::Rectangle<float> r) { return juce::Rectangle<float> (r.getX() * S, r.getY() * S, r.getWidth() * S, r.getHeight() * S).toNearestInt(); };
    const auto around = [&] (juce::Point<float> c, float w, float h) { return box (juce::Rectangle<float> (w, h).withCentre (c)); };

    for (auto& k : knobs)
        k.slider.setBounds (around (k.centre, 2.0f * k.radius, 2.0f * k.radius));
    for (int i = 0; i < 5; ++i)  altZones[i].setBounds (around ({ altX (i), kAltY }, 24.0f, 44.0f));
    for (int i = 0; i < 16; ++i) stepZones[i].setBounds (around ({ stepX (i), kStepY }, 66.0f, 36.0f));
    startZone.setBounds (around (kStart, 94.0f, 36.0f));
    tapZone.setBounds (around (kTap, 94.0f, 36.0f));
    bankZone.setBounds (around (kBank, 40.0f, 40.0f));
    clearZone.setBounds (around (kClear, 40.0f, 40.0f));
    kitZone.setBounds (box (kLogo));
}

// Step the sample variant of a voice by dir (+1 / -1), wrapping through 0 = "cycle all", and play it as a preview.
void TR08AudioProcessorEditor::stepVariant (int inst, int dir)
{
    const int n = std::min (proc.getVariantCount (inst), 127);
    if (n < 1) return;
    int v = std::min ((int) std::round (get (kVarParam[inst])), n);
    v = (v + dir + n + 1) % (n + 1);
    setValue (kVarParam[inst], (float) v);
    proc.triggerManual (inst);
}

void TR08AudioProcessorEditor::setSwitchFrom (int which, juce::Point<float> p)
{
    if (which == 1)
    {
        int best = 0;
        for (int i = 1; i < 4; ++i)
            if (std::abs (p.y - kPreScaleY[i]) < std::abs (p.y - kPreScaleY[best])) best = i;
        if (best != (int) std::round (get ("preScale"))) setValue ("preScale", (float) best);
    }
    else if (which == 2)                                   // A / AB / B by horizontal position
    {
        const int v = p.x < kBasicVar.x - 0.5f * kCapShift ? 0 : (p.x > kBasicVar.x + 0.5f * kCapShift ? 2 : 1);
        if (v != (int) std::round (get ("variation"))) setValue ("variation", (float) v);
    }
    else if (which == 3)                                   // A / B
    {
        const int v = p.x > kIfSwitch.x ? 1 : 0;
        if (v != (int) std::round (get ("fillPart"))) setValue ("fillPart", (float) v);
    }
}

void TR08AudioProcessorEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingSwitch != 0)
        setSwitchFrom (draggingSwitch, e.position / ((float) getWidth() / kImgW));
}

void TR08AudioProcessorEditor::mouseUp (const juce::MouseEvent&) { draggingSwitch = 0; }

void TR08AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / ((float) getWidth() / kImgW);
    const juce::Rectangle<float> basicHit (100.0f, 40.0f), ifHit (100.0f, 40.0f);
    if (preScaleHit().contains (p))                       draggingSwitch = 1;
    else if (basicHit.withCentre (kBasicVar).contains (p))  draggingSwitch = 2;
    else if (ifHit.withCentre (kIfSwitch).contains (p))     draggingSwitch = 3;
    if (draggingSwitch != 0)
    {
        setSwitchFrom (draggingSwitch, p);
        return;
    }
    for (int i = 0; i < tr08::kNumInst; ++i)
    {
        const auto r = selectorRect (i);
        if (! r.contains (p)) continue;
        if (p.x < r.getX() + kSelArrow)            stepVariant (i, -1);
        else if (p.x > r.getRight() - kSelArrow)   stepVariant (i, +1);
        else                                       proc.triggerManual (i);       // preview the current sample
        return;
    }
}

void TR08AudioProcessorEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const auto p = e.position / ((float) getWidth() / kImgW);
    for (int i = 0; i < tr08::kNumInst; ++i)
        if (selectorRect (i).contains (p) && w.deltaY != 0.0f) { stepVariant (i, w.deltaY > 0.0f ? -1 : 1); return; }
}

void TR08AudioProcessorEditor::timerCallback()
{
    juce::String state;
    state << proc.currentStep.load() << "," << juce::String (proc.displayBpm.load(), 1) << "," << proc.getKitName() << "," << (int) proc.isLoading() << ",";
    for (auto id : { "run", "mode", "inst", "variation", "shuffle", "ltConga", "mtConga", "htConga", "rsClaves", "cpMaracas", "fillPart", "preScale" })
        state << juce::String (get (id), 1) << ",";
    state << proc.getSteps (0) << ":" << proc.getSteps (1) << ":" << proc.getAppliedPreScale() << ",";
    for (int i = 0; i < tr08::kNumInst; ++i)
        state << proc.getVariantCount (i) << ":" << (int) std::round (get (kVarParam[i])) << ",";
    const int var = editVar(), half = editHalf(), track = (int) std::round (get ("inst"));
    for (int i = 0; i < 16; ++i)
        state << (proc.getStep (var, half, juce::jlimit (0, TR08AudioProcessor::kTracks - 1, track), i) ? '1' : '0');
    if (state != lastState)
    {
        lastState = state;
        repaint();
    }
}

//==============================================================================
void TR08AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (background, 0, 0, getWidth(), getHeight(), 0, 0, background.getWidth(), background.getHeight());
    g.addTransform (juce::AffineTransform::scale ((float) getWidth() / kImgW));      // from here on: editor units

    const int mode = (int) std::round (get ("mode"));
    const int variation = (int) std::round (get ("variation"));
    const int track = juce::jlimit (0, TR08AudioProcessor::kTracks - 1, (int) std::round (get ("inst")));
    const int var = editVar(), half = editHalf();
    const int steps1 = proc.getSteps (0), steps = proc.getSteps (half);
    const int cur = proc.currentStep.load();                       // index in 1st part followed by 2nd part
    const int playing = (cur >= 0 && (half == 0 ? cur < steps1 : cur >= steps1)) ? cur - (half == 0 ? 0 : steps1) : -1;
    const juce::Colour red (0xffe8281e);

    // ---- kit selector
    {
        const auto kit = proc.getKitName();
        const juce::String label = proc.isLoading() ? juce::String ("LOADING...")
                                                    : (kit.isEmpty() ? juce::String ("SYNTH") : kit.replace ("/", "  >  "));
        g.setColour (juce::Colour (0xffb6e04a));
        g.setFont (juce::Font (juce::FontOptions (16.0f, juce::Font::bold)));
        g.drawFittedText (label, kLogo.reduced (14.0f, 4.0f).withTrimmedRight (16.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.6f);
        juce::Path arrow;
        arrow.addTriangle (kLogo.getRight() - 24.0f, kLogo.getCentreY() - 4.0f, kLogo.getRight() - 10.0f, kLogo.getCentreY() - 4.0f,
                           kLogo.getRight() - 17.0f, kLogo.getCentreY() + 5.0f);
        g.fillPath (arrow);
    }

    // ---- tempo display: "120.0"
    const juce::Colour segOn (0xffff3a24), segOff (0xff2a0805);
    const int tenths = juce::jlimit (0, 9999, (int) std::round (proc.displayBpm.load() * 10.0f));
    const int digits[4] = { tenths / 1000, (tenths / 100) % 10, (tenths / 10) % 10, tenths % 10 };
    for (int i = 0; i < 4; ++i)
        drawSegDigit (g, { kDigitX[i], kDigitY }, kDigitW, kDigitH, (i == 0 && digits[0] == 0) ? -1 : digits[i], segOn, segOff, i == 2);

    // ---- LEDs: step buttons (selected track in the shown part), variation, part
    for (int i = 0; i < 16; ++i)
    {
        const bool on = proc.getStep (var, half, track, i);
        if (i == playing)
        {
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.fillRoundedRectangle (juce::Rectangle<float> (59.0f, 30.0f).withCentre ({ stepX (i), kStepY }), 7.0f);
        }
        drawLed (g, { stepX (i), kStepLedY }, 5.0f, red, on);
        if (i >= steps)                                                    // beyond this part's step number
        {
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.fillRoundedRectangle (juce::Rectangle<float> (60.0f, 31.0f).withCentre ({ stepX (i), kStepY }), 7.0f);
        }
    }
    drawLed (g, kVarLedA, 5.0f, red, variation != 2);
    drawLed (g, kVarLedB, 5.0f, red, variation != 0);
    drawLed (g, kPart1Led, 5.0f, red, half == 0 && mode != 0);
    drawLed (g, kPart2Led, 5.0f, red, half == 1 && mode != 0);

    // ---- tom/conga, rimshot/claves, clap/maracas switches
    for (int i = 0; i < 5; ++i)
    {
        const float x = altX (i);
        const bool alt = get (kAltIds[i]) > 0.5f;
        drawLever (g, juce::Rectangle<float> (11.0f, 14.0f).withCentre ({ x, kAltY + (alt ? -9.0f : 9.0f) }));
    }

    // ---- pre-scale switch
    drawLever (g, juce::Rectangle<float> (14.0f, 9.0f).withCentre ({ kPreScale.x, kPreScaleY[juce::jlimit (0, 3, (int) std::round (get ("preScale")))] }));

    // ---- I/F-variation and basic-variation switches
    drawSilverCap (g, { kIfSwitch.x + (get ("fillPart") > 0.5f ? 1.0f : -1.0f) * kCapShift, kIfSwitch.y }, kCapR);
    drawSilverCap (g, { kBasicVar.x + ((float) juce::jlimit (0, 2, (int) std::round (get ("variation"))) - 1.0f) * kCapShift, kBasicVar.y }, kCapR);

    // ---- sample selectors
    for (int i = 0; i < tr08::kNumInst; ++i)
    {
        const auto r = selectorRect (i);
        const int n = proc.getVariantCount (i);
        const int v = std::min ((int) std::round (get (kVarParam[i])), n);
        const auto cream = juce::Colour (0xffe9dfc4);
        g.setColour (juce::Colour (0xff0c0e12));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (juce::Colours::white.withAlpha (0.22f));
        g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
        g.setColour (n > 1 ? cream : cream.withAlpha (0.25f));
        for (int side = -1; side <= 1; side += 2)
        {
            const float ax = side < 0 ? r.getX() + 8.0f : r.getRight() - 8.0f;
            const float sd = (float) side;
            juce::Path tri;
            tri.addTriangle (ax + sd * 3.0f, r.getCentreY(), ax - sd * 2.5f, r.getCentreY() - 4.5f, ax - sd * 2.5f, r.getCentreY() + 4.5f);
            g.fillPath (tri);
        }
        g.setColour (n > 0 ? juce::Colour (0xffff9a2e) : cream.withAlpha (0.3f));
        g.setFont (juce::Font (juce::FontOptions (12.5f, juce::Font::bold)));
        const juce::String label = n == 0 ? "syn" : (v == 0 ? "all" : juce::String (v));
        g.drawText (label, r.reduced (kSelArrow, 0.0f), juce::Justification::centred, false);
    }

    // ---- button feedback
    if (get ("run") > 0.5f)
    {
        g.setColour (juce::Colour (0xffb8ffc8).withAlpha (0.35f));
        g.fillRoundedRectangle (juce::Rectangle<float> (95.0f, 33.0f).withCentre (kStart), 8.0f);
    }
    if (get ("shuffle") > 0.5f)
    {
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.drawEllipse (juce::Rectangle<float> (45.0f, 45.0f).withCentre (kBank), 2.5f);
        g.setColour (juce::Colour (0xff9cc4ff).withAlpha (0.35f));
        g.fillEllipse (juce::Rectangle<float> (38.0f, 38.0f).withCentre (kBank));
    }
    for (auto z : { std::pair<Zone*, juce::Point<float>> { &tapZone, kTap }, { &bankZone, kBank }, { &clearZone, kClear } })
        if (z.first->isDown())
        {
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            if (z.first == &tapZone) g.fillRoundedRectangle (juce::Rectangle<float> (95.0f, 33.0f).withCentre (z.second), 8.0f);
            else g.fillEllipse (juce::Rectangle<float> (38.0f, 38.0f).withCentre (z.second));
        }
    for (int i = 0; i < 16; ++i)
        if (stepZones[i].isDown())
        {
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillRoundedRectangle (juce::Rectangle<float> (59.0f, 30.0f).withCentre ({ stepX (i), kStepY }), 7.0f);
        }
}
