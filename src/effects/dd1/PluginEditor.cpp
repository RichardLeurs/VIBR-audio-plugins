// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "DD1Data.h"

namespace
{
// Layout in background pixels (see make_background.py)
constexpr float kImgW = 2690.0f, kImgH = 800.0f;
constexpr float kDy = 14.0f;      // the artwork has 14 px more room under the title rule: everything below it moves down (see style.md)
constexpr int kBaseW = 1750;
constexpr float kBig = 44.0f, kSmall = 34.0f;

struct KnobSpec { const char* id; float x, y, r; };
std::vector<KnobSpec> makeSpecs()
{
    std::vector<KnobSpec> v;
    const float cx[2] = { 800.0f, 1560.0f };
    const char* pre[2] = { "a", "b" };
    static std::vector<std::string> names;     // keeps the id strings alive
    names.clear();
    names.reserve (32);
    for (int i = 0; i < 2; ++i)
    {
        const auto add = [&] (const char* suffix, float x, float y, float r)
        {
            names.push_back (std::string (pre[i]) + suffix);
            v.push_back ({ names.back().c_str(), x, y + kDy, r });
        };
        add ("Div", cx[i] - 110.0f, 365.0f, kBig);
        if (i == 0)                                              // each delay has its own repeat filter
        {
            add ("LowCut", cx[i] + 70.0f, 365.0f, kSmall);
            add ("HiCut",  cx[i] + 170.0f, 365.0f, kSmall);
        }
        else                                                     // delay B keeps the original ids lowCut / hiCut
        {
            v.push_back ({ "lowCut", cx[i] + 70.0f, 365.0f + kDy, kSmall });
            v.push_back ({ "hiCut",  cx[i] + 170.0f, 365.0f + kDy, kSmall });
        }
        add ("Rep",   cx[i] - 162.0f, 585.0f, kSmall);
        add ("Level", cx[i] - 54.0f,  585.0f, kSmall);
        add ("Mod",   cx[i] + 54.0f,  585.0f, kSmall);
        add ("Rate",  cx[i] + 162.0f, 585.0f, kSmall);
    }
    v.push_back ({ "rInt",   2430.0f - 110.0f, 365.0f + kDy, kBig });
    v.push_back ({ "rDecay", 2430.0f + 110.0f, 365.0f + kDy, kBig });
    v.push_back ({ "rShim",  2430.0f - 162.0f, 585.0f + kDy, kSmall });
    v.push_back ({ "rTone",  2430.0f - 54.0f,  585.0f + kDy, kSmall });
    v.push_back ({ "rLevel", 2430.0f + 54.0f,  585.0f + kDy, kSmall });
    v.push_back ({ "chRate",  1887.0f, 365.0f + kDy, kSmall });      // ambience panel; rows line up with the neighbouring panels
    v.push_back ({ "chDepth", 1995.0f, 365.0f + kDy, kSmall });
    v.push_back ({ "chLevel", 2103.0f, 365.0f + kDy, kSmall });
    v.push_back ({ "rvDecay", 1887.0f, 585.0f + kDy, kSmall });
    v.push_back ({ "rvTone",  1995.0f, 585.0f + kDy, kSmall });
    v.push_back ({ "rvLevel", 2103.0f, 585.0f + kDy, kSmall });
    v.push_back ({ "aTime",      150.0f, 450.0f + kDy, kSmall });      // the time of delay A lives in the TEMPO panel
    v.push_back ({ "drive",      420.0f, 450.0f + kDy, kSmall });
    v.push_back ({ "boostTone",  366.0f, 590.0f + kDy, kSmall });
    v.push_back ({ "boostLevel", 474.0f, 590.0f + kDy, kSmall });
    v.push_back ({ "spread", 1180.0f, 365.0f + kDy, kSmall });
    return v;
}

const juce::Point<float> kLed { 2633.0f, 67.0f }, kRoute { 2592.0f, 585.0f + kDy };
const juce::Point<float> kPing[2] = { { 1125.0f, 585.0f + kDy }, { 1235.0f, 585.0f + kDy } };
const juce::Point<float> kTapSw { 150.0f, 595.0f + kDy }, kBpm { 150.0f, 246.0f + kDy }, kSync { 150.0f, 335.0f + kDy }, kBoostSw { 420.0f, 335.0f + kDy };
const char* const kPingId[2] = { "pingA", "pingB" };
#if 0   // presets panel (replaced by the memory panel): previous / next buttons and the name display
const juce::Point<float> kPresetLcd { 1355.0f, 82.0f }, kPrev { 1095.0f, 82.0f }, kNext { 1615.0f, 82.0f };
constexpr float kPresetLcdW = 400.0f, kPresetLcdH = 58.0f;
#endif
// memory panel (top, same place as the old presets panel): lamps MANUAL / 1-4 and the MEMORY button (see make_background.py)
const float kMemLedX[5] = { 1150.0f, 1255.0f, 1360.0f, 1465.0f, 1570.0f };
constexpr float kMemY = 70.0f;
const juce::Point<float> kMemoryButton { 1705.0f, 70.0f };
constexpr juce::uint32 kHoldMs = 800;
constexpr float kDispY = 246.0f + kDy, kDispW = 176.0f, kDispH = 44.0f;
// memory lamp sitting in a chrome bezel painted in the background (image pixels)
void drawLamp (juce::Graphics& g, juce::Point<float> c, bool on)
{
    const juce::Colour colour (0xff40d070);
    const auto circle = [&] (float r) { return juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c); };
    if (on)
    {
        g.setColour (colour.withAlpha (0.20f));
        g.fillEllipse (circle (32.0f));
        g.setColour (colour.withAlpha (0.25f));
        g.fillEllipse (circle (21.0f));
    }
    g.setGradientFill (juce::ColourGradient (on ? colour.brighter (0.8f) : colour.darker (1.8f), c.x - 5.0f, c.y - 7.0f,
                                             on ? colour.darker (0.2f) : colour.darker (3.0f), c.x + 8.0f, c.y + 10.0f, true));
    g.fillEllipse (circle (11.5f));
    g.setColour (juce::Colours::white.withAlpha (on ? 0.55f : 0.18f));
    g.fillEllipse (juce::Rectangle<float> (6.0f, 4.0f).withCentre (c.translated (-4.0f, -6.0f)));
}

juce::String msText (float ms)
{
    return (ms < 100.0f ? juce::String (ms, 1) : juce::String ((int) std::lround (ms))) + " ms";
}

juce::Colour grey (float g) { return juce::Colour::fromFloatRGBA (juce::jlimit (0.0f, 1.0f, g * 0.97f), juce::jlimit (0.0f, 1.0f, g), juce::jlimit (0.0f, 1.0f, g * 1.04f), 1.0f); }

void conicRing (juce::Graphics& g, juce::Point<float> c, float r0, float r1, int steps, const std::function<float (float)>& brightness)
{
    for (int i = 0; i < steps; ++i)
    {
        const float a0 = juce::MathConstants<float>::twoPi * (float) i / (float) steps;
        const float a1 = juce::MathConstants<float>::twoPi * (float) (i + 1) / (float) steps;
        juce::Path p;
        p.startNewSubPath (c.x + r0 * std::sin (a0), c.y - r0 * std::cos (a0));
        p.lineTo (c.x + r1 * std::sin (a0), c.y - r1 * std::cos (a0));
        p.lineTo (c.x + r1 * std::sin (a1), c.y - r1 * std::cos (a1));
        p.lineTo (c.x + r0 * std::sin (a1), c.y - r0 * std::cos (a1));
        p.closeSubPath();
        g.setColour (grey (brightness (0.5f * (a0 + a1))));
        g.fillPath (p);
    }
}
} // namespace

//==============================================================================
void DD1LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                       float startAngle, float endAngle, juce::Slider&)
{
    const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const auto c = b.getCentre();
    const float r = b.getWidth() * 0.5f - 1.0f;
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float pi = juce::MathConstants<float>::pi;

    // knurled skirt: conic highlight with fine flutes
    conicRing (g, c, r * 0.80f, r, 180, [] (float a)
    {
        const float sheen = 0.50f + 0.34f * std::cos (2.0f * (a - 0.9f)) + 0.10f * std::cos (a - 0.9f);
        return sheen * (0.80f + 0.20f * std::cos (45.0f * a));
    });
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawEllipse (juce::Rectangle<float> (2.0f * r, 2.0f * r).withCentre (c), 1.2f);

    // groove, then the brushed top plate
    g.setColour (juce::Colour (0xff15161a));
    g.fillEllipse (juce::Rectangle<float> (1.64f * r, 1.64f * r).withCentre (c));
    conicRing (g, c, 0.0f, r * 0.76f, 120, [] (float a)
    {
        return 0.60f + 0.34f * std::cos (2.0f * (a - 0.9f));
    });
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.28f), c.x - 0.35f * r, c.y - 0.45f * r,
                                             juce::Colours::black.withAlpha (0.18f), c.x + 0.5f * r, c.y + 0.6f * r, true));
    g.fillEllipse (juce::Rectangle<float> (1.52f * r, 1.52f * r).withCentre (c));

    // pointer
    const auto dir = juce::Point<float> (std::sin (angle), -std::cos (angle));
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.drawLine (juce::Line<float> (c + dir * (0.18f * r) + juce::Point<float> (0.5f, 1.2f), c + dir * (0.72f * r) + juce::Point<float> (0.5f, 1.2f)), 0.13f * r);
    g.setColour (juce::Colour (0xff101114));
    g.drawLine (juce::Line<float> (c + dir * (0.18f * r), c + dir * (0.72f * r)), 0.11f * r);
    juce::ignoreUnused (pi);
}

//==============================================================================
DD1AudioProcessorEditor::DD1AudioProcessorEditor (DD1AudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&laf);
    background = juce::ImageFileFormat::loadFrom (DD1Data::background_png, (size_t) DD1Data::background_pngSize);

    for (const auto& s : makeSpecs())
    {
        auto k = std::make_unique<Knob>();
        k->centre = { s.x, s.y };
        k->radius = s.r;
        k->slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        k->slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        k->slider.setRotaryParameters (juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
        k->slider.setMouseDragSensitivity (200);
        k->slider.setScrollWheelEnabled (true);
        addAndMakeVisible (k->slider);
        k->attachment = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter (s.id), k->slider, nullptr);
        knobs.push_back (std::move (k));
    }

    constrainer.setFixedAspectRatio ((double) kImgW / (double) kImgH);
    constrainer.setSizeLimits (1000, 275, 3000, 825);
    setConstrainer (&constrainer);
    setResizable (true, true);
    setSize (kBaseW, (int) std::round ((float) kBaseW * kImgH / kImgW));
    startTimerHz (20);
}

DD1AudioProcessorEditor::~DD1AudioProcessorEditor() { setLookAndFeel (nullptr); }

void DD1AudioProcessorEditor::resized()
{
    const float S = (float) getWidth() / kImgW;
    for (auto& k : knobs)
        k->slider.setBounds (juce::Rectangle<float> (2.0f * k->radius * S, 2.0f * k->radius * S).withCentre (k->centre * S).toNearestInt());
}

#if 0   // presets panel (replaced by the memory panel)
void DD1AudioProcessorEditor::stepPreset (int dir)
{
    const int n = DD1AudioProcessor::getNumPresets();
    proc.applyPreset ((proc.getPresetIndex() + dir + n) % n);
    repaint();
}

void DD1AudioProcessorEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    for (int i = 0; i < DD1AudioProcessor::getNumPresets(); ++i)
        menu.addItem (i + 1, juce::String (i + 1).paddedLeft ('0', 2) + "  " + DD1AudioProcessor::getPresetName (i), true, i == proc.getPresetIndex());
    const float S = (float) getWidth() / kImgW;
    const auto lcd = juce::Rectangle<float> (kPresetLcdW, kPresetLcdH).withCentre (kPresetLcd) * S;
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (lcd.toNearestInt().translated (getScreenX(), getScreenY())),
                        [this] (int result) { if (result > 0) { proc.applyPreset (result - 1); repaint(); } });
}
#endif

void DD1AudioProcessorEditor::tap()
{
    const double now = (double) juce::Time::getMillisecondCounterHiRes();
    if (! taps.empty() && now - taps.back() > 2000.0) taps.clear();
    taps.push_back (now);
    if (taps.size() > 5) taps.erase (taps.begin());
    lastTap = now;
    if (taps.size() >= 2)
    {
        const double avg = (taps.back() - taps.front()) / (double) (taps.size() - 1);
        const float bpm = juce::jlimit (30.0f, 300.0f, (float) (60000.0 / avg));
        for (const char* id : { "hostSync" })                                       // tapping selects the tapped tempo
            if (proc.apvts.getRawParameterValue (id)->load() > 0.5f) toggleParam (id);
        auto* prm = proc.apvts.getParameter ("tempo");
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (bpm));
        prm->endChangeGesture();
    }
    repaint();
}

void DD1AudioProcessorEditor::toggleParam (const char* id)
{
    auto* prm = proc.apvts.getParameter (id);
    prm->beginChangeGesture();
    prm->setValueNotifyingHost (prm->getValue() > 0.5f ? 0.0f : 1.0f);
    prm->endChangeGesture();
}

void DD1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const float S = (float) getWidth() / kImgW;
    const auto p = e.position / S;
    // if (juce::Rectangle<float> (80.0f, 62.0f).withCentre (kPrev).contains (p)) stepPreset (-1);
    // else if (juce::Rectangle<float> (80.0f, 62.0f).withCentre (kNext).contains (p)) stepPreset (1);
    // else if (juce::Rectangle<float> (kPresetLcdW, kPresetLcdH).withCentre (kPresetLcd).contains (p)) showPresetMenu();
    // else
    if (juce::Rectangle<float> (112.0f, 64.0f).withCentre (kMemoryButton).contains (p))   // MEMORY: short press steps, hold writes
    {
        memHeld = true; memWritten = false; memDownAt = juce::Time::getMillisecondCounter();
    }
    else if (juce::Rectangle<float> (100.0f, 56.0f).withCentre (kLed).contains (p))     // the power slide switch is the bypass switch
        toggleParam ("bypass");
    else if (juce::Rectangle<float> (112.0f, 72.0f).withCentre (kTapSw).contains (p))
        tap();
    else if (juce::Rectangle<float> (100.0f, 56.0f).withCentre (kSync).contains (p))
        toggleParam ("hostSync");
    else if (juce::Rectangle<float> (100.0f, 56.0f).withCentre (kBoostSw).contains (p))
        toggleParam ("boostOn");
    else if (juce::Rectangle<float> (60.0f, 90.0f).withCentre (kRoute).contains (p))
        toggleParam ("series");
    else
        for (int i = 0; i < 2; ++i)
            if (juce::Rectangle<float> (60.0f, 90.0f).withCentre (kPing[i]).contains (p))
                toggleParam (kPingId[i]);
}

void DD1AudioProcessorEditor::mouseUp (const juce::MouseEvent&)
{
    if (memHeld && ! memWritten)
        memory.stepNext();            // short press: MANUAL -> 1..4 and recall
    memHeld = false;
    repaint();
}

void DD1AudioProcessorEditor::timerCallback()
{
    // memory: holding the button writes the slot; moving a knob after a recall goes back to MANUAL
    const auto nowMem = juce::Time::getMillisecondCounter();
    if (memHeld && ! memWritten && nowMem - memDownAt >= kHoldMs)
    {
        memory.write();
        memWritten = true;
        memFlashUntil = nowMem + 500;
    }
    memory.revertIfEdited();
    const bool flashing = nowMem < memFlashUntil;
    const bool memDirty = memory.current() != memShown || flashing || flashing != memFlashShown;
    memShown = memory.current();
    memFlashShown = flashing;

    const bool on = proc.apvts.getRawParameterValue ("bypass")->load() < 0.5f;
    const bool series = proc.apvts.getRawParameterValue ("series")->load() > 0.5f;
    const int ping = (proc.apvts.getRawParameterValue ("boostOn")->load() > 0.5f ? 4 : 0) | (proc.apvts.getRawParameterValue ("pingA")->load() > 0.5f ? 1 : 0) | (proc.apvts.getRawParameterValue ("pingB")->load() > 0.5f ? 2 : 0);
    const bool host = proc.apvts.getRawParameterValue ("hostSync")->load() > 0.5f;
    const float bpm = proc.bpmNow.load();
    const double nowMs = (double) juce::Time::getMillisecondCounterHiRes();
    const double beat = 60000.0 / juce::jmax (30.0f, bpm);
    const bool blink = std::fmod (nowMs, beat) < 90.0 || nowMs - lastTap < 90.0;      // tempo LED
    bool dirty = memDirty || on != lastOn || series != lastSeries || ping != lastPing || host != lastHost
                 || blink != lastBlink || std::fabs (bpm - lastBpm) > 0.04f;
    lastHost = host; lastBlink = blink; lastBpm = bpm;
    lastOn = on; lastSeries = series; lastPing = ping;
    for (int i = 0; i < 2; ++i)
    {
        const float ms = proc.timeMs[i].load();
        if (std::fabs (ms - lastMs[i]) > 0.04f) { lastMs[i] = ms; dirty = true; }
    }
    if (dirty) repaint();
}

void DD1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (background, 0, 0, getWidth(), getHeight(), 0, 0, background.getWidth(), background.getHeight());
    g.addTransform (juce::AffineTransform::scale ((float) getWidth() / kImgW));      // background pixels from here on

    const bool on = proc.apvts.getRawParameterValue ("bypass")->load() < 0.5f;
    const bool series = proc.apvts.getRawParameterValue ("series")->load() > 0.5f;

    // memory lamps: MANUAL, 1-4; all five flash green after a write
    {
        const auto t = juce::Time::getMillisecondCounter();
        const bool memFlashing = t < memFlashUntil;
    const bool blinkOff = t < memFlashUntil && ((t / 100) % 2) == 1;
        for (int i = 0; i < 5; ++i)
            drawLamp (g, { kMemLedX[i], kMemY }, (memFlashing ? ! blinkOff : memory.current() == i));
    }

#if 0   // preset name display (replaced by the memory panel)
    {
        const juce::Font f (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 32.0f, juce::Font::bold));
        g.setFont (f);
        const auto box = juce::Rectangle<float> (kPresetLcdW, kPresetLcdH).withCentre (kPresetLcd);
        const int idx = proc.getPresetIndex();
        const auto t = juce::String (idx + 1).paddedLeft ('0', 2) + "  " + DD1AudioProcessor::getPresetName (idx);
        g.setColour (juce::Colour (0xffff9a1a).withAlpha (0.25f));
        for (auto o : { juce::Point<float> (-1.5f, 0), juce::Point<float> (1.5f, 0), juce::Point<float> (0, -1.5f), juce::Point<float> (0, 1.5f) })
            g.drawText (t, box.translated (o.x, o.y), juce::Justification::centred, false);
        g.setColour (juce::Colour (0xffffc860));
        g.drawText (t, box, juce::Justification::centred, false);
    }
#endif

    // time displays: amber digits in the recessed windows
    {
        const juce::Font f (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 34.0f, juce::Font::bold));
        g.setFont (f);
        for (int i = 0; i < 2; ++i)
        {
            const auto box = juce::Rectangle<float> (kDispW, kDispH).withCentre ({ i == 0 ? 800.0f : 1560.0f, kDispY });
            g.setColour (juce::Colour (0xffff9a1a).withAlpha (0.07f));
            g.drawText ("8888.8 ms", box, juce::Justification::centred, false);          // unlit segments
            const auto t = msText (proc.timeMs[i].load());
            g.setColour (juce::Colour (0xffff9a1a).withAlpha (0.25f));
            for (auto o : { juce::Point<float> (-1.5f, 0), juce::Point<float> (1.5f, 0), juce::Point<float> (0, -1.5f), juce::Point<float> (0, 1.5f) })
                g.drawText (t, box.translated (o.x, o.y), juce::Justification::centred, false);
            g.setColour (juce::Colour (0xffffc860));
            g.drawText (t, box, juce::Justification::centred, false);
        }
    }

    // tempo display, tap LED (blinks on the beat), source lever
    {
        const juce::Font f (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 28.0f, juce::Font::bold));
        g.setFont (f);
        const auto box = juce::Rectangle<float> (168.0f, 44.0f).withCentre (kBpm);
        const auto t = juce::String (proc.bpmNow.load(), 1) + " bpm";
        g.setColour (juce::Colour (0xffff9a1a).withAlpha (0.07f));
        g.drawText ("888.8 bpm", box, juce::Justification::centred, false);
        g.setColour (juce::Colour (0xffff9a1a).withAlpha (0.25f));
        for (auto o : { juce::Point<float> (-1.5f, 0), juce::Point<float> (1.5f, 0), juce::Point<float> (0, -1.5f), juce::Point<float> (0, 1.5f) })
            g.drawText (t, box.translated (o.x, o.y), juce::Justification::centred, false);
        g.setColour (juce::Colour (0xffffc860));
        g.drawText (t, box, juce::Justification::centred, false);

        // the TAP pad is its own light: a small rectangular pad with a faint red glow on the beat and when tapped
        const double nowMs = (double) juce::Time::getMillisecondCounterHiRes();
        const bool lit = std::fmod (nowMs, 60000.0 / juce::jmax (30.0f, proc.bpmNow.load())) < 90.0 || nowMs - lastTap < 90.0;
        const auto pad = juce::Rectangle<float> (88.0f, 52.0f).withCentre (kTapSw);
        if (lit)
        {
            g.setColour (juce::Colour (0xffff3a24).withAlpha (0.07f));
            g.fillRoundedRectangle (pad.expanded (16.0f), 22.0f);
            g.setColour (juce::Colour (0xffff3a24).withAlpha (0.09f));
            g.fillRoundedRectangle (pad.expanded (8.0f), 15.0f);
        }
        g.setGradientFill (juce::ColourGradient (lit ? juce::Colour (0xffd8503a) : juce::Colour (0xff3a1411), pad.getX(), pad.getY(),
                                                 lit ? juce::Colour (0xff8a1c12) : juce::Colour (0xff150605), pad.getX(), pad.getBottom(), false));
        g.fillRoundedRectangle (pad, 9.0f);
        g.setColour (juce::Colours::white.withAlpha (lit ? 0.22f : 0.07f));
        g.fillRoundedRectangle (pad.withHeight (pad.getHeight() * 0.38f).reduced (5.0f, 3.0f), 6.0f);
    }

    // power slide switch: chrome ball left = off, right = on (same size on screen as on the DM-1 and CE-1)
    {
        if (on)      // the slot lights up green (subtle)
        {
            const auto slot = juce::Rectangle<float> (50.8f, 26.2f).withCentre (kLed);
            g.setColour (juce::Colour (0xff40d070).withAlpha (0.05f));
            g.fillRoundedRectangle (slot.expanded (12.3f), 23.0f);
            g.setColour (juce::Colour (0xff40d070).withAlpha (0.08f));
            g.fillRoundedRectangle (slot.expanded (6.2f), 18.5f);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2f8a4c), slot.getX(), slot.getY(),
                                                     juce::Colour (0xff17512b), slot.getX(), slot.getBottom(), false));
            g.fillRoundedRectangle (slot, 13.1f);
            g.setColour (juce::Colours::white.withAlpha (0.10f));
            g.fillRoundedRectangle (slot.withHeight (slot.getHeight() * 0.40f).reduced (6.2f, 2.3f), 4.6f);
        }
        const juce::Point<float> ball (kLed.x + (on ? 13.1f : -13.1f), kLed.y);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillEllipse (juce::Rectangle<float> (23.1f, 23.1f).withCentre (ball + juce::Point<float> (1.5f, 3.1f)));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffafbfd), ball.x - 4.6f, ball.y - 6.2f,
                                                 juce::Colour (0xff6b6f7a), ball.x + 6.9f, ball.y + 8.5f, true));
        g.fillEllipse (juce::Rectangle<float> (21.5f, 21.5f).withCentre (ball));
    }

    // lever switches: chrome ball in the slot (routing: top = SERIES; ping-pong: top = ON)
    const auto drawBall = [&g] (juce::Point<float> centre, bool top)
    {
        const juce::Point<float> ball (centre.x, centre.y + (top ? -17.0f : 17.0f));
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillEllipse (juce::Rectangle<float> (30.0f, 30.0f).withCentre (ball + juce::Point<float> (2.0f, 4.0f)));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffafbfd), ball.x - 6.0f, ball.y - 8.0f,
                                                 juce::Colour (0xff6b6f7a), ball.x + 9.0f, ball.y + 11.0f, true));
        g.fillEllipse (juce::Rectangle<float> (28.0f, 28.0f).withCentre (ball));
    };
    drawBall (kRoute, series);
    {   // horizontal switches: ball left / right.  Source: left = HOST, right = TAP.  Boost: left = off, right = on.
        const auto drawBallH = [&g] (juce::Point<float> centre, bool left)
        {
            const juce::Point<float> ball (centre.x + (left ? -17.0f : 17.0f), centre.y);
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.fillEllipse (juce::Rectangle<float> (30.0f, 30.0f).withCentre (ball + juce::Point<float> (2.0f, 4.0f)));
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffafbfd), ball.x - 6.0f, ball.y - 8.0f,
                                                     juce::Colour (0xff6b6f7a), ball.x + 9.0f, ball.y + 11.0f, true));
            g.fillEllipse (juce::Rectangle<float> (28.0f, 28.0f).withCentre (ball));
        };
        drawBallH (kSync, proc.apvts.getRawParameterValue ("hostSync")->load() > 0.5f);
        drawBallH (kBoostSw, proc.apvts.getRawParameterValue ("boostOn")->load() < 0.5f);
    }
    for (int i = 0; i < 2; ++i)
        drawBall (kPing[i], proc.apvts.getRawParameterValue (kPingId[i])->load() > 0.5f);
}
