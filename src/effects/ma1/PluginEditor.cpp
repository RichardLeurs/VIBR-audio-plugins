// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "MA1Data.h"

namespace
{
// Positions are in the 2400 x 856 coordinates of the rendered background (make_background.py). The editor is resizable at a
// fixed aspect ratio; paint() scales image pixels to the window.
constexpr float kImgW = 2400.0f, kImgH = 856.0f;
constexpr int kBaseW = 1400;
constexpr float kPad = 3.0f;
// Two tabs, each with its own background: 0 = AMP (GATE / TIGHT BOOST / AMP), 1 = CABINET (CABINET / EQ / OUTPUT). The control
// positions below are the old single-window ones; each tab moves its row up/down so it sits under the tab bar (see make_background.py).
constexpr float kDyTab[2] = { 60.0f, -390.0f };
constexpr int kKnobsTab0 = 14, kButtonsTab0 = 4;            // the first 14 knobs and the first 4 buttons are on tab 0
inline int knobTab (int i) { return i < kKnobsTab0 ? 0 : 1; }
inline int buttonTab (int i) { return i < kButtonsTab0 ? 0 : 1; }
const juce::Point<float> kTabBtn[2] = { { 1095.0f, 250.0f }, { 1305.0f, 250.0f } };      // tab buttons under the rule (drawn in the background)
constexpr float kTabW = 190.0f, kTabH = 44.0f;

struct KnobSpec { const char* id; float x, y, r, startDeg, endDeg; };
#define K(id, x, y) { id, x, y, 36.0f, -154.0f, 154.0f }
const KnobSpec kKnobs[] = {
    K ("gateThresh", 115, 488), K ("gateRelease", 260, 488),
    K ("boostDrive", 460, 488), K ("boostLevel", 610, 488), K ("boostTight", 760, 488),
    { "ampType", 1040, 488, 46.0f, -67.5f, 67.5f },
    K ("gain", 1230, 488), K ("bass", 1372, 488), K ("middle", 1514, 488), K ("treble", 1656, 488),
    K ("presence", 1798, 488), K ("resonance", 1940, 488), K ("sag", 2082, 488), K ("master", 2224, 488),
    K ("irALevel", 194, 958), K ("irBLevel", 564, 958), K ("lowCut", 874, 958), K ("highCut", 1074, 958),
    K ("eqLow", 1366, 958), K ("eqMid", 1506, 958), K ("eqFreq", 1646, 958), K ("eqHigh", 1786, 958),
    K ("inGain", 2068, 958), K ("out", 2228, 958),
};
#undef K
const char* const kButtonIds[6] = { "gateOn", "boostOn", "bright", "deep", "cabOn", "limiter" };
const juce::Point<float> kButtonPos[6] = { { 150, 613 }, { 620, 613 }, { 1230, 613 }, { 1372, 613 }, { 1074, 808 }, { 2068, 808 } };
constexpr float kButtonW = 128.0f, kButtonH = 44.0f;
const juce::Rectangle<float> kIrBox[2] = { { 104, 783, 330, 50 }, { 474, 783, 330, 50 } };
const juce::Point<float> kGateLamp { 290.0f, 610.0f + kDyTab[0] }, kLimLamp { 2228.0f, 812.0f + kDyTab[1] };
const juce::Point<float> kLed { 2325.0f, 87.0f };          // power slide switch (top right, ball left = off) = bypass

juce::Point<float> buttonPos (int i) { return kButtonPos[i].translated (0.0f, kDyTab[buttonTab (i)]); }
juce::Rectangle<float> irBox (int s) { return kIrBox[s].translated (0.0f, kDyTab[1]); }

juce::Point<float> polar (juce::Point<float> c, float r, float a)
{
    return { c.x + r * std::sin (a), c.y - r * std::cos (a) };
}

void drawLamp (juce::Graphics& g, juce::Point<float> c, float level, juce::Colour colour, float r = 12.0f)
{
    const auto circle = [&] (float rr) { return juce::Rectangle<float> (rr * 2.0f, rr * 2.0f).withCentre (c); };
    g.setColour (juce::Colour (0xff0c0e14));
    g.fillEllipse (circle (r + 4.0f));
    if (level > 0.02f)
    {
        g.setColour (colour.withAlpha (0.30f * level));
        g.fillEllipse (circle (r * 2.2f));
    }
    g.setGradientFill (juce::ColourGradient (level > 0.5f ? colour.brighter (0.7f) : colour.darker (1.8f), c.x - r * 0.4f, c.y - r * 0.5f,
                                             level > 0.5f ? colour.darker (0.2f) : colour.darker (3.0f), c.x + r * 0.6f, c.y + r * 0.7f, true));
    g.fillEllipse (circle (r));
}
} // namespace

//==============================================================================
void MA1LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float,
                                         float startAngle, float endAngle, juce::Slider& s)
{
    const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const auto c = b.getCentre();
    const float R = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f - kPad;
    const double lo = s.getMinimum(), hi = s.getMaximum();
    const float pos = (float) ((s.getValue() - lo) / (hi - lo));
    const float angle = startAngle + pos * (endAngle - startAngle);
    const auto circle = [&] (float r) { return juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c); };

    // soft drop shadow
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (circle (R).translated (R * 0.05f, R * 0.09f));

    // matt black body with a knurled grip edge
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4c51), c.x - R * 0.7f, c.y - R * 0.8f,
                                             juce::Colour (0xff050506), c.x + R * 0.7f, c.y + R * 0.9f, false));
    g.fillEllipse (circle (R));
    constexpr int ridges = 60;
    for (int i = 0; i < ridges; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / (float) ridges;
        g.setColour (juce::Colours::white.withAlpha (0.10f + 0.10f * std::max (0.0f, std::cos (a + 0.8f))));
        g.drawLine (polar (c, R * 0.90f, a).x, polar (c, R * 0.90f, a).y, polar (c, R * 0.99f, a).x, polar (c, R * 0.99f, a).y, 1.0f);
    }
    g.setColour (juce::Colours::black);
    g.drawEllipse (circle (R), 1.2f);

    // raised cap
    const float capR = R * 0.80f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3b3f), c.x - capR * 0.6f, c.y - capR * 0.7f,
                                             juce::Colour (0xff0b0b0c), c.x + capR * 0.6f, c.y + capR * 0.8f, false));
    g.fillEllipse (circle (capR));
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.14f), c.x - capR * 0.5f, c.y - capR * 0.6f,
                                             juce::Colours::transparentWhite, c.x + capR * 0.1f, c.y + capR * 0.1f, true));
    g.fillEllipse (circle (capR));
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.35f), c.x - capR, c.y - capR,
                                             juce::Colour (0xff000000), c.x + capR, c.y + capR, false));
    g.drawEllipse (circle (capR), 1.2f);

    // pointer: white bar from the rim toward the centre
    juce::Path p;
    p.addRoundedRectangle (-capR * 0.055f, -capR * 0.94f, capR * 0.11f, capR * 0.50f, capR * 0.05f);
    g.setColour (juce::Colour (0xfff2f2ee));
    g.fillPath (p, juce::AffineTransform::rotation (angle).translated (c));
}

//==============================================================================
MA1AudioProcessorEditor::MA1AudioProcessorEditor (MA1AudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&laf);
    background[0] = juce::ImageFileFormat::loadFrom (MA1Data::background_png, (size_t) MA1Data::background_pngSize);      // tab AMP
    background[1] = juce::ImageFileFormat::loadFrom (MA1Data::background2_png, (size_t) MA1Data::background2_pngSize);    // tab CABINET
    tab = juce::jlimit (0, 1, (int) proc.apvts.state.getProperty ("tab", 0));

    for (int i = 0; i < kNumKnobs; ++i)
    {
        auto& k = knobs[i];
        const auto& spec = kKnobs[i];
        k.centre = { spec.x, spec.y + kDyTab[knobTab (i)] };
        k.radius = spec.r;
        k.slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        k.slider.setRotaryParameters (juce::degreesToRadians (spec.startDeg), juce::degreesToRadians (spec.endDeg), true);
        k.slider.setMouseDragSensitivity (juce::String (spec.id) == "ampType" ? 120 : 180);
        k.slider.setScrollWheelEnabled (false);
        addAndMakeVisible (k.slider);
        k.attachment = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter (spec.id), k.slider, nullptr);
    }
    for (int i = 0; i < kNumButtons; ++i)
    {
        buttons[i].onPress = [this, i] { toggle (kButtonIds[i]); };
        addAndMakeVisible (buttons[i]);
    }
    for (int s = 0; s < 2; ++s)
    {
        irZones[s].onPress = [this, s] { irMenu (s); };
        addAndMakeVisible (irZones[s]);
    }

    constrainer.setFixedAspectRatio ((double) kImgW / (double) kImgH);
    constrainer.setSizeLimits (700, 250, 2400, 856);
    setConstrainer (&constrainer);
    setResizable (true, true);
    setSize (kBaseW, (int) std::round ((float) kBaseW * kImgH / kImgW));
    updateTabVisibility();
    startTimerHz (20);
}

MA1AudioProcessorEditor::~MA1AudioProcessorEditor() { setLookAndFeel (nullptr); }

void MA1AudioProcessorEditor::updateTabVisibility()
{
    for (int i = 0; i < kNumKnobs; ++i)
        knobs[i].slider.setVisible (knobTab (i) == tab);
    for (int i = 0; i < kNumButtons; ++i)
        buttons[i].setVisible (buttonTab (i) == tab);
    for (auto& z : irZones)
        z.setVisible (tab == 1);
}

void MA1AudioProcessorEditor::setTab (int newTab)
{
    if (newTab == tab) return;
    tab = newTab;
    proc.apvts.state.setProperty ("tab", tab, nullptr);       // saved with the session
    updateTabVisibility();
    repaint();
}

void MA1AudioProcessorEditor::toggle (const char* id)
{
    auto* prm = proc.apvts.getParameter (id);
    prm->beginChangeGesture();
    prm->setValueNotifyingHost (prm->getValue() > 0.5f ? 0.0f : 1.0f);
    prm->endChangeGesture();
}

void MA1AudioProcessorEditor::irMenu (int slot)
{
    juce::PopupMenu menu;
    menu.addItem (1, "Load impulse response...");
    menu.addItem (2, "Clear (use the factory cabinet)", proc.getImpulseName (slot).isNotEmpty());
    juce::Component::SafePointer<MA1AudioProcessorEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&irZones[slot]), [safe, slot] (int result)
    {
        if (safe == nullptr || result == 0) return;
        if (result == 2) { safe->proc.clearImpulseResponse (slot); safe->repaint(); return; }
        safe->chooser = std::make_unique<juce::FileChooser> ("Choose an impulse response (WAV or AIFF)", juce::File(), "*.wav;*.aif;*.aiff");
        safe->chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                    [safe, slot] (const juce::FileChooser& fc)
                                    {
                                        if (safe != nullptr && fc.getResult().existsAsFile())
                                        {
                                            safe->proc.loadImpulseResponse (slot, fc.getResult());
                                            safe->repaint();
                                        }
                                    });
    });
}

void MA1AudioProcessorEditor::resized()
{
    const float S = (float) getWidth() / kImgW;
    const auto box = [S] (juce::Rectangle<float> r) { return juce::Rectangle<float> (r.getX() * S, r.getY() * S, r.getWidth() * S, r.getHeight() * S).toNearestInt(); };
    for (auto& k : knobs)
        k.slider.setBounds (box (juce::Rectangle<float> (2.0f * k.radius, 2.0f * k.radius).withCentre (k.centre)));
    for (int i = 0; i < kNumButtons; ++i)
        buttons[i].setBounds (box (juce::Rectangle<float> (kButtonW, kButtonH).withCentre (buttonPos (i))));
    for (int s = 0; s < 2; ++s)
        irZones[s].setBounds (box (irBox (s)));
}

void MA1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const float S = (float) getWidth() / kImgW;
    for (int i = 0; i < 2; ++i)                               // tab buttons
        if (juce::Rectangle<float> (kTabW * S, kTabH * S).withCentre (kTabBtn[i] * S).contains (e.position))
        {
            setTab (i);
            return;
        }
    // the power slide switch is the bypass switch
    if (juce::Rectangle<float> (110.0f * S, 70.0f * S).withCentre (kLed * S).contains (e.position))
        toggle ("bypass");
}

void MA1AudioProcessorEditor::timerCallback()
{
    const bool on = proc.apvts.getRawParameterValue ("bypass")->load() < 0.5f;
    juce::String state;
    state << (int) on << ",";
    for (auto id : kButtonIds) state << (int) (proc.apvts.getRawParameterValue (id)->load() > 0.5f);
    state << "," << (int) (proc.gateOpenLamp.load() > 0.5f) << (int) (proc.limiterLamp.load() > 0.03f) << proc.getImpulseName (0) << "|" << proc.getImpulseName (1);
    if (state != lastState) { lastState = state; lastOn = on; repaint(); }
}

void MA1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    const auto& bg = background[tab];
    g.drawImage (bg, 0, 0, getWidth(), getHeight(), 0, 0, bg.getWidth(), bg.getHeight());
    g.addTransform (juce::AffineTransform::scale ((float) getWidth() / kImgW));      // image pixels from here on

    // power slide switch: chrome ball left = off, right = on; the slot lights up green when on
    const bool on = proc.apvts.getRawParameterValue ("bypass")->load() < 0.5f;
    if (on)
    {
        const auto slot = juce::Rectangle<float> (66.0f, 34.0f).withCentre (kLed);
        g.setColour (juce::Colour (0xff40d070).withAlpha (0.05f));
        g.fillRoundedRectangle (slot.expanded (16.0f), 30.0f);
        g.setColour (juce::Colour (0xff40d070).withAlpha (0.08f));
        g.fillRoundedRectangle (slot.expanded (8.0f), 24.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2f8a4c), slot.getX(), slot.getY(),
                                                 juce::Colour (0xff17512b), slot.getX(), slot.getBottom(), false));
        g.fillRoundedRectangle (slot, 17.0f);
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.fillRoundedRectangle (slot.withHeight (slot.getHeight() * 0.40f).reduced (8.0f, 3.0f), 6.0f);
    }
    {
        const juce::Point<float> c (kLed.x + (on ? 17.0f : -17.0f), kLed.y);
        const auto circle = [&] (float r) { return juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c); };
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillEllipse (circle (15.0f).translated (2.0f, 4.0f));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffafbfd), c.x - 6.0f, c.y - 8.0f, juce::Colour (0xff6b6f7a), c.x + 9.0f, c.y + 11.0f, true));
        g.fillEllipse (circle (14.0f));
    }

    // selected tab: lit in the recessed button
    {
        const auto r = juce::Rectangle<float> (kTabW, kTabH).withCentre (kTabBtn[tab]);
        g.setColour (juce::Colour (0xffd94c48).withAlpha (0.45f));
        g.fillRoundedRectangle (r, 13.0f);
        g.setColour (juce::Colour (0xffffb0a8).withAlpha (0.85f));
        g.drawRoundedRectangle (r.reduced (1.0f), 13.0f, 2.0f);
    }

    // buttons: LED on the left, pressed shade
    for (int i = 0; i < kNumButtons; ++i)
    {
        if (buttonTab (i) != tab) continue;
        const bool lit = proc.apvts.getRawParameterValue (kButtonIds[i])->load() > 0.5f;
        drawLamp (g, buttonPos (i).translated (-kButtonW * 0.5f + 22.0f, 0.0f), lit ? 1.0f : 0.0f, juce::Colour (0xff40e070), 8.0f);
        if (buttons[i].isDown())
        {
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillRoundedRectangle (juce::Rectangle<float> (kButtonW, kButtonH).withCentre (buttonPos (i)), 12.0f);
        }
    }

    // lamps: gate open, limiter working
    if (tab == 0)
        drawLamp (g, kGateLamp, proc.gateOpenLamp.load() > 0.5f ? 1.0f : 0.0f, juce::Colour (0xff40e070), 9.0f);
    else
        drawLamp (g, kLimLamp, proc.limiterLamp.load() > 0.03f ? 1.0f : 0.0f, juce::Colour (0xffff7a1c), 9.0f);

    // impulse response names
    for (int s = 0; s < 2 && tab == 1; ++s)
    {
        const auto name = proc.getImpulseName (s);
        g.setColour (name.isNotEmpty() ? juce::Colour (0xffffb36a) : juce::Colour (0xff7a808c));
        g.setFont (juce::Font (juce::FontOptions (28.0f, juce::Font::bold)));
        g.drawFittedText (name.isNotEmpty() ? name : (s == 0 ? juce::String ("FACTORY CAB") : juce::String ("EMPTY")),
                          irBox (s).reduced (16.0f, 4.0f).withTrimmedRight (26.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
        juce::Path arrow;
        const auto r = irBox (s);
        arrow.addTriangle (r.getRight() - 34.0f, r.getCentreY() - 6.0f, r.getRight() - 14.0f, r.getCentreY() - 6.0f, r.getRight() - 24.0f, r.getCentreY() + 8.0f);
        g.fillPath (arrow);
    }
}
