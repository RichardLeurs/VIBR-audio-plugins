// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "DD1Data.h"

namespace
{
// Layout in background pixels (see make_background.py)
constexpr float kImgW = 1560.0f, kImgH = 891.0f;
constexpr float kDy = 79.0f;      // everything below the title rule moves down: room for the tab buttons under the rule (see make_background.py GAP_TOP)
constexpr int kBaseW = 1100;
constexpr float kBig = 44.0f, kSmall = 34.0f, kDelSmall = 30.0f;     // kDelSmall: the small knobs of tab DELAY
constexpr float kRow2Delay = 585.0f;   // tab DELAY: REPEATS / LEVEL / MOD / RATE row (the MEMORY section sits under it)
constexpr float kEffDy = -15.0f;  // tab EFFECTS: content sits 11 px higher so it is centred in the panels (see make_background.py)

struct KnobSpec { const char* id; float x, y, r; int tab; };      // tab 0 = DELAY, 1 = EFFECTS
std::vector<KnobSpec> makeSpecs()
{
    std::vector<KnobSpec> v;
    const float cx[2] = { 615.0f, 1310.0f };
    const char* pre[2] = { "a", "b" };
    static std::vector<std::string> names;     // keeps the id strings alive
    names.clear();
    names.reserve (32);
    for (int i = 0; i < 2; ++i)
    {
        const auto add = [&] (const char* suffix, float x, float y, float r)
        {
            names.push_back (std::string (pre[i]) + suffix);
            v.push_back ({ names.back().c_str(), x, y + kDy, r, 0 });
        };
        add ("Div", cx[i] - 110.0f, 365.0f, kBig);
        if (i == 0)                                              // each delay has its own repeat filter
        {
            add ("LowCut", cx[i] + 50.0f, 365.0f, kDelSmall);
            add ("HiCut",  cx[i] + 150.0f, 365.0f, kDelSmall);
        }
        else                                                     // delay B keeps the original ids lowCut / hiCut
        {
            v.push_back ({ "lowCut", cx[i] + 50.0f, 365.0f + kDy, kDelSmall, 0 });
            v.push_back ({ "hiCut",  cx[i] + 150.0f, 365.0f + kDy, kDelSmall, 0 });
        }
        add ("Rep",   cx[i] - 150.0f, kRow2Delay, kDelSmall);
        add ("Level", cx[i] - 50.0f,  kRow2Delay, kDelSmall);
        add ("Mod",   cx[i] + 50.0f,  kRow2Delay, kDelSmall);
        add ("Rate",  cx[i] + 150.0f, kRow2Delay, kDelSmall);
    }
    v.push_back ({ "aTime",  290.0f, 462.0f + kDy, kDelSmall, 0 });      // the time of delay A lives in the TEMPO panel
    v.push_back ({ "spread", 962.5f, 365.0f + kDy, kBig, 0 });
    // tab EFFECTS: SHIMMER panel (centre 1235), AMBIENCE panel (centre 665), BOOST panel (centre 210)
    v.push_back ({ "rInt",   1235.0f - 150.0f, 365.0f + kDy + kEffDy, kBig, 1 });
    v.push_back ({ "rDecay", 1235.0f + 150.0f, 365.0f + kDy + kEffDy, kBig, 1 });
    v.push_back ({ "rShim",  1235.0f - 186.0f, 585.0f + kDy + kEffDy, kSmall, 1 });
    v.push_back ({ "rTone",  1235.0f - 62.0f,  585.0f + kDy + kEffDy, kSmall, 1 });
    v.push_back ({ "rLevel", 1235.0f + 62.0f,  585.0f + kDy + kEffDy, kSmall, 1 });
    v.push_back ({ "chRate",  665.0f - 150.0f, 365.0f + kDy + kEffDy, kSmall, 1 });
    v.push_back ({ "chDepth", 665.0f,          365.0f + kDy + kEffDy, kSmall, 1 });
    v.push_back ({ "chLevel", 665.0f + 150.0f, 365.0f + kDy + kEffDy, kSmall, 1 });
    v.push_back ({ "rvDecay", 665.0f - 150.0f, 585.0f + kDy + kEffDy, kSmall, 1 });
    v.push_back ({ "rvTone",  665.0f,          585.0f + kDy + kEffDy, kSmall, 1 });
    v.push_back ({ "rvLevel", 665.0f + 150.0f, 585.0f + kDy + kEffDy, kSmall, 1 });
    v.push_back ({ "drive",      210.0f,          450.0f + kDy + kEffDy, kSmall, 1 });
    v.push_back ({ "boostTone",  210.0f - 70.0f,  590.0f + kDy + kEffDy, kSmall, 1 });
    v.push_back ({ "boostLevel", 210.0f + 70.0f,  590.0f + kDy + kEffDy, kSmall, 1 });
    return v;
}

const juce::Point<float> kLed { 1503.0f, 67.0f }, kRoute { 1421.0f, 585.0f + kDy + kEffDy };
const juce::Point<float> kPing[2] = { { 912.5f, 585.0f + kDy }, { 1012.5f, 585.0f + kDy } };
const juce::Point<float> kTapSw { 205.0f, 585.0f + kDy }, kBpm { 205.0f, 376.0f + kDy }, kSync { 115.0f, 468.0f + kDy }, kBoostSw { 210.0f, 335.0f + kDy + kEffDy };
const char* const kPingId[2] = { "pingA", "pingB" };
const float kDelayCx[2] = { 615.0f, 1310.0f };                     // delay A / B time displays
// tab buttons (below the title rule, centred on the window; drawn in the background, the selected one is lit here)
const juce::Point<float> kTabBtn[2] = { { 695.0f, 202.0f }, { 865.0f, 202.0f } };
constexpr float kTabW = 150.0f, kTabH = 44.0f;
#if 0   // presets panel (replaced by the memory panel): previous / next buttons and the name display
const juce::Point<float> kPresetLcd { 1355.0f, 82.0f }, kPrev { 1095.0f, 82.0f }, kNext { 1615.0f, 82.0f };
constexpr float kPresetLcdW = 400.0f, kPresetLcdH = 58.0f;
#endif
// memory: lamps MANUAL / 1-4 and the MEMORY button at the top of the TEMPO panel, a line under them (see make_background.py)
const float kMemLedX[5] = { 76.0f, 120.0f, 164.0f, 208.0f, 252.0f };
constexpr float kMemLampScale = 1.0f;
constexpr float kMemY = 270.0f + kDy;
const juce::Point<float> kMemoryButton { 326.0f, 270.0f + kDy };
constexpr juce::uint32 kHoldMs = 800;
constexpr float kDispY = 246.0f + kDy, kDispW = 176.0f, kDispH = 44.0f;
// memory lamp sitting in a chrome bezel painted in the background (image pixels)
void drawLamp (juce::Graphics& g, juce::Point<float> c, bool on, float k = 1.0f)
{
    // flat, the same as the DM-1 and RE-2 lamps (style.md "Flat style"): a plain 22 px disc inside the ring painted in the background,
    // green when selected, a dark shade of green otherwise; no glow, no highlight
    const juce::Colour colour (0xff40d070);
    g.setColour (on ? colour : colour.interpolatedWith (juce::Colour (0xff0c0d0f), 0.78f));
    g.fillEllipse (juce::Rectangle<float> (22.0f * k, 22.0f * k).withCentre (c));
}

juce::String msText (float ms)
{
    return (ms < 100.0f ? juce::String (ms, 1) : juce::String ((int) std::lround (ms))) + " ms";
}

} // namespace

//==============================================================================
void DD1LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                       float startAngle, float endAngle, juce::Slider&)
{
    // black knob (the look of the DM-1): matt body with a knurled grip edge, raised cap, white pointer bar
    const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const auto c = b.getCentre();
    const float R = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f - 2.0f;
    const float angle = startAngle + pos * (endAngle - startAngle);
    const auto circle = [&] (float r) { return juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c); };

    g.setColour (juce::Colours::black.withAlpha (0.55f));                                           // soft drop shadow
    g.fillEllipse (circle (R).translated (R * 0.02f, R * 0.05f));

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4c51), c.x - R * 0.7f, c.y - R * 0.8f,
                                             juce::Colour (0xff050506), c.x + R * 0.7f, c.y + R * 0.9f, false));
    g.fillEllipse (circle (R));                                                                     // matt black body
    constexpr int ridges = 60;
    for (int i = 0; i < ridges; ++i)                                                                // knurled grip edge
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / (float) ridges;
        const auto p0 = c + juce::Point<float> (std::sin (a), -std::cos (a)) * (R * 0.90f), p1 = c + juce::Point<float> (std::sin (a), -std::cos (a)) * (R * 0.99f);
        g.setColour (juce::Colours::white.withAlpha (0.10f + 0.10f * std::max (0.0f, std::cos (a + 0.8f))));
        g.drawLine (p0.x, p0.y, p1.x, p1.y, 1.0f);
    }
    g.setColour (juce::Colours::black);
    g.drawEllipse (circle (R), 1.2f);

    const float capR = R * 0.80f;                                                                   // raised cap
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3b3f), c.x - capR * 0.6f, c.y - capR * 0.7f,
                                             juce::Colour (0xff0b0b0c), c.x + capR * 0.6f, c.y + capR * 0.8f, false));
    g.fillEllipse (circle (capR));
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.14f), c.x - capR * 0.5f, c.y - capR * 0.6f,
                                             juce::Colours::transparentWhite, c.x + capR * 0.1f, c.y + capR * 0.1f, true));
    g.fillEllipse (circle (capR));
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.35f), c.x - capR, c.y - capR,
                                             juce::Colour (0xff000000), c.x + capR, c.y + capR, false));
    g.drawEllipse (circle (capR), 1.2f);

    juce::Path bar;                                                                                 // white pointer bar from the rim toward the centre
    bar.addRoundedRectangle (-capR * 0.055f, -capR * 0.94f, capR * 0.11f, capR * 0.50f, capR * 0.05f);
    g.setColour (juce::Colour (0xfff2f2ee));
    g.fillPath (bar, juce::AffineTransform::rotation (angle).translated (c));
}

//==============================================================================
DD1AudioProcessorEditor::DD1AudioProcessorEditor (DD1AudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&laf);
    background[0] = juce::ImageFileFormat::loadFrom (DD1Data::background_png, (size_t) DD1Data::background_pngSize);      // tab DELAY
    background[1] = juce::ImageFileFormat::loadFrom (DD1Data::background2_png, (size_t) DD1Data::background2_pngSize);    // tab EFFECTS
    tab = juce::jlimit (0, 1, (int) proc.apvts.state.getProperty ("tab", 0));

    for (const auto& s : makeSpecs())
    {
        auto k = std::make_unique<Knob>();
        k->centre = { s.x, s.y };
        k->radius = s.r;
        k->tab = s.tab;
        k->slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        k->slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        k->slider.setRotaryParameters (juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
        k->slider.setMouseDragSensitivity (200);
        k->slider.setScrollWheelEnabled (true);
        addAndMakeVisible (k->slider);
        k->attachment = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter (s.id), k->slider, nullptr);
        midiUI.add (k->slider, s.id);
        knobs.push_back (std::move (k));
    }

    constrainer.setFixedAspectRatio ((double) kImgW / (double) kImgH);
    constrainer.setSizeLimits (780, 456, 2340, 1367);
    setConstrainer (&constrainer);
    setResizable (true, true);
    setSize (kBaseW, (int) std::round ((float) kBaseW * kImgH / kImgW));
    updateTabVisibility();
    startTimerHz (20);
}

DD1AudioProcessorEditor::~DD1AudioProcessorEditor() { setLookAndFeel (nullptr); }

void DD1AudioProcessorEditor::updateTabVisibility()
{
    for (auto& k : knobs)
        k->slider.setVisible (k->tab == tab);
}

void DD1AudioProcessorEditor::setTab (int newTab)
{
    if (newTab == tab) return;
    tab = newTab;
    proc.apvts.state.setProperty ("tab", tab, nullptr);       // saved with the session
    updateTabVisibility();
    repaint();
}

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
    proc.midiTap.tap();                                     // the same tap logic as the MIDI tap (in the processor)
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
    for (int i = 0; i < 2; ++i)
        if (juce::Rectangle<float> (kTabW, kTabH).withCentre (kTabBtn[i]).contains (p))
        {
            setTab (i);
            return;
        }
    if (tab == 0)       // clicking a memory lamp (MANUAL, 1-4) selects that slot and recalls it
        for (int i = 0; i < 5; ++i)
            if (juce::Rectangle<float> (40.0f, 40.0f).withCentre ({ kMemLedX[i], kMemY }).contains (p))
            {
                memory.select (i);
                repaint();
                return;
            }
    if (tab == 0 && juce::Rectangle<float> (92.0f, 64.0f).withCentre (kMemoryButton).contains (p))   // MEMORY: short press steps, hold writes
    {
        memHeld = true; memWritten = false; memDownAt = juce::Time::getMillisecondCounter();
    }
    else if (juce::Rectangle<float> (100.0f, 56.0f).withCentre (kLed).contains (p))     // the power slide switch is the bypass switch
        toggleParam ("bypass");
    else if (tab == 0 && juce::Rectangle<float> (92.0f, 60.0f).withCentre (kTapSw).contains (p))
        tap();
    else if (tab == 0 && juce::Rectangle<float> (100.0f, 56.0f).withCentre (kSync).contains (p))
        toggleParam ("hostSync");
    else if (tab == 1 && juce::Rectangle<float> (100.0f, 56.0f).withCentre (kBoostSw).contains (p))
        toggleParam ("boostOn");
    else if (tab == 1 && juce::Rectangle<float> (60.0f, 90.0f).withCentre (kRoute).contains (p))
        toggleParam ("series");
    else if (tab == 0)
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
    if (midiUI.needsRepaint()) { midiUI.tick(); repaint(); }
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
    const bool blink = std::fmod (nowMs, beat) < 90.0 || nowMs - proc.midiTap.lastMs() < 90.0;      // tempo LED
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

void DD1AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    midiUI.paintMarks (g, juce::Colour (255, 190, 60));
}

void DD1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    const auto& bg = background[tab];
    g.drawImage (bg, 0, 0, getWidth(), getHeight(), 0, 0, bg.getWidth(), bg.getHeight());
    g.addTransform (juce::AffineTransform::scale ((float) getWidth() / kImgW));      // background pixels from here on

    const bool on = proc.apvts.getRawParameterValue ("bypass")->load() < 0.5f;
    const bool series = proc.apvts.getRawParameterValue ("series")->load() > 0.5f;

    // memory lamps: MANUAL, 1-4; all five flash green after a write (the MEMORY section is in the TEMPO panel, on the DELAY tab)
    if (tab == 0)
    {
        const auto t = juce::Time::getMillisecondCounter();
        const bool memFlashing = t < memFlashUntil;
    const bool blinkOff = t < memFlashUntil && ((t / 100) % 2) == 1;
        for (int i = 0; i < 5; ++i)
            drawLamp (g, { kMemLedX[i], kMemY }, (memFlashing ? ! blinkOff : memory.current() == i), kMemLampScale);
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

    // selected tab: lit in the recessed button
    for (int i = 0; i < 2; ++i)
        if (i == tab)
        {
            const auto r = juce::Rectangle<float> (kTabW, kTabH).withCentre (kTabBtn[i]);
            g.setColour (juce::Colour (0xff5276e5).withAlpha (0.38f));
            g.fillRoundedRectangle (r, 14.0f);
            g.setColour (juce::Colour (0xffa9bdf5).withAlpha (0.85f));
            g.drawRoundedRectangle (r.reduced (1.0f), 14.0f, 2.0f);
        }

    // time displays: amber digits in the recessed windows
    if (tab == 0)
    {
        const juce::Font f (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 34.0f, juce::Font::bold));
        g.setFont (f);
        for (int i = 0; i < 2; ++i)
        {
            const auto box = juce::Rectangle<float> (kDispW, kDispH).withCentre ({ kDelayCx[i], kDispY });
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
    if (tab == 0)
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

        // the TAP pad is its own light: a flat pad inside the ring of the background, bright red on the beat and when tapped, dark red otherwise
        const double nowMs = (double) juce::Time::getMillisecondCounterHiRes();
        const bool lit = std::fmod (nowMs, 60000.0 / juce::jmax (30.0f, proc.bpmNow.load())) < 90.0 || nowMs - proc.midiTap.lastMs() < 90.0;
        const auto pad = juce::Rectangle<float> (76.0f, 44.0f).withCentre (kTapSw);
        g.setColour (lit ? juce::Colour (0xffe5432c) : juce::Colour (0xff3a1411));
        g.fillRoundedRectangle (pad, 7.0f);
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
    if (tab == 1)
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
        if (tab == 0)
            drawBallH (kSync, proc.apvts.getRawParameterValue ("hostSync")->load() > 0.5f);
        else
            drawBallH (kBoostSw, proc.apvts.getRawParameterValue ("boostOn")->load() < 0.5f);
    }
    if (tab == 0)
        for (int i = 0; i < 2; ++i)
            drawBall (kPing[i], proc.apvts.getRawParameterValue (kPingId[i])->load() > 0.5f);
}
