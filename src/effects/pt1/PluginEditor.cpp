// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "VibrLogo.h"

namespace
{
// layout units: the design is 1200 x 690 (style.md: 20 side margin, 10 gap, name centre 55, rule 104, panels 30 below the rule, 69 above the bottom)
constexpr float kW = 1200.0f, kH = 650.0f, kMargin = 20.0f, kRuleY = 104.0f, kPad = 5.0f;
constexpr float kVoiceY = 134.0f, kVoiceH = 236.0f, kBottomY = 394.0f, kBottomH = 186.0f;

const juce::Colour kBody (96, 190, 168), kDark (10, 36, 30), kPanel (26, 27, 30), kInk (238, 238, 232);

juce::Font font (float h, bool bold = false, bool italic = false)
{
    if (italic) return juce::Font (juce::FontOptions ("Avenir Next", "Heavy Italic", h));
    if (! bold) return juce::Font (juce::FontOptions ("Avenir Next", "Medium", h));
    return juce::Font (juce::FontOptions ("Avenir Next", h, bold ? juce::Font::bold : juce::Font::plain));
}

float textWidth (const juce::Font& f, const juce::String& s)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (f, s, 0.0f, 0.0f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}

void drawText (juce::Graphics& g, const juce::String& s, float cx, float cy, float size, juce::Colour c, bool bold = true,
               float kerning = 0.08f, juce::Justification j = juce::Justification::centred, bool italic = false)
{
    auto f = font (size, bold, italic).withExtraKerningFactor (kerning);
    g.setColour (c);
    g.setFont (f);
    const float w = textWidth (f, s) + 8.0f;
    juce::Rectangle<float> r (w, size * 1.6f);
    if (j == juce::Justification::centred) r = r.withCentre ({ cx, cy });
    else if (j == juce::Justification::centredRight) r = r.withCentre ({ cx - w * 0.5f, cy });
    else r = r.withCentre ({ cx + w * 0.5f, cy });
    g.drawText (s, r, j, false);
}

juce::Rectangle<float> voiceRect (int v) { return { kMargin + 292.5f * (float) v, kVoiceY, 282.5f, kVoiceH }; }
juce::Rectangle<float> keyRect() { return { 20.0f, kBottomY, 360.0f, kBottomH }; }
juce::Rectangle<float> chordsRect() { return { 390.0f, kBottomY, 440.0f, kBottomH }; }
juce::Rectangle<float> outputRect() { return { 840.0f, kBottomY, 340.0f, kBottomH }; }
juce::Rectangle<float> intelRect() { return { 236.0f, kBottomY + 52.0f, 130.0f, 44.0f }; }
juce::Rectangle<float> chordRect (int c) { return { 406.0f + 82.0f * (float) c, kBottomY + 58.0f, 76.0f, 46.0f }; }
// knob column k of n inside a panel: the knobs (ticks and legends reach 41 each side) keep 14 clear of the panel edge
float colX (const juce::Rectangle<float>& r, int k, int n) { return r.getX() + 55.0f + (r.getWidth() - 110.0f) * (float) k / (float) (n - 1); }
juce::Rectangle<float> powerRect() { return juce::Rectangle<float> (50.0f, 50.0f).withCentre ({ kW - kMargin - 16.5f, 43.5f }); }
} // namespace

//==============================================================================
void PT1LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider&)
{
    const auto c = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).getCentre();
    const float R = (float) juce::jmin (w, h) * 0.5f - 5.0f;
    const auto circle = [&c] (float r) { return juce::Rectangle<float> (2.0f * r, 2.0f * r).withCentre (c); };

    g.setColour (juce::Colours::black.withAlpha (0.5f));                                            // shadow
    g.fillEllipse (circle (R).translated (1.0f, R * 0.14f));

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff35373d), c.x - R * 0.6f, c.y - R * 0.8f, juce::Colour (0xff08090a), c.x + R * 0.6f, c.y + R * 0.9f, false));
    g.fillEllipse (circle (R));                                                                     // matt black body
    for (int k = 0; k < 60; ++k)                                                                    // knurled grip edge
    {
        const float a = (float) k * juce::MathConstants<float>::twoPi / 60.0f, s = std::sin (a), co = std::cos (a);
        g.setColour ((k & 1) ? juce::Colours::black.withAlpha (0.45f) : juce::Colours::white.withAlpha (0.13f));
        g.drawLine (c.x + s * R * 0.86f, c.y - co * R * 0.86f, c.x + s * R * 0.995f, c.y - co * R * 0.995f, 1.1f);
    }
    g.setColour (juce::Colours::white.withAlpha (0.16f));
    g.drawEllipse (circle (R - 0.5f), 1.0f);

    const float cr = R * 0.74f;                                                                     // raised cap
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3f4147), c.x - cr * 0.7f, c.y - cr * 0.8f, juce::Colour (0xff121315), c.x + cr * 0.7f, c.y + cr * 0.9f, false));
    g.fillEllipse (circle (cr));
    g.setColour (juce::Colours::white.withAlpha (0.22f));
    g.drawEllipse (circle (cr), 1.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.16f), c.x - cr * 0.4f, c.y - cr * 0.5f, juce::Colours::transparentWhite, c.x, c.y, true));
    g.fillEllipse (circle (cr * 0.95f));

    const float a = startAngle + pos * (endAngle - startAngle);                                     // white pointer bar
    juce::Path bar;
    bar.addRoundedRectangle (-R * 0.05f, -R * 0.84f, R * 0.10f, R * 0.5f, R * 0.05f);
    g.setColour (juce::Colour (0xfff2f2ee));
    g.fillPath (bar, juce::AffineTransform::rotation (a).translated (c));
}

//==============================================================================
PT1AudioProcessorEditor::PT1AudioProcessorEditor (PT1AudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      powerAtt (*p.apvts.getParameter ("power"), [this] (float) { repaint(); }, nullptr),
      intelAtt (*p.apvts.getParameter ("intel"), [this] (float) { repaint(); }, nullptr),
      rootAtt  (*p.apvts.getParameter ("root"),  [this] (float) { repaint(); }, nullptr),
      scaleAtt (*p.apvts.getParameter ("scale"), [this] (float) { repaint(); }, nullptr)
{
    setLookAndFeel (&laf);
    // kinds: 0 interval, 1 detune, 2 level, 3 root, 4 scale, 5 percent, 6 dB
    const auto add = [this] (const juce::String& id, int kind, float cx, float cy, float r, const char* name)
    {
        auto k = std::make_unique<Knob>();
        keepIds.add (id);
        k->id = keepIds.getReference (keepIds.size() - 1).toRawUTF8(); k->kind = kind; k->cx = cx; k->cy = cy; k->r = r; k->name = name;
        auto& s = k->slider;
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        s.setRotaryParameters (juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
        s.setMouseDragSensitivity (160);
        s.setScrollWheelEnabled (false);
        s.setPopupDisplayEnabled (true, true, this);
        s.setTextValueSuffix (kind == 6 ? " dB" : " %");
        auto* par = proc.apvts.getParameter (id);
        s.setDoubleClickReturnValue (true, par->convertFrom0to1 (par->getDefaultValue()));
        addAndMakeVisible (s);
        k->att = std::make_unique<juce::SliderParameterAttachment> (*dynamic_cast<juce::RangedAudioParameter*> (par), s, nullptr);
        s.onValueChange = [this] { repaint(); };
        midiUI.add (s, id);
        knobs.push_back (std::move (k));
    };
    keepIds.ensureStorageAllocated (64);
    for (int v = 0; v < pt1::kNumVoices; ++v)
    {
        const auto r = voiceRect (v);
        const juce::String n (v + 1);
        add ("int" + n, 0, colX (r, 0, 3), kVoiceY + 118.0f, 23.0f, "INTERVAL");
        add ("det" + n, 1, colX (r, 1, 3), kVoiceY + 118.0f, 23.0f, "DETUNE");
        add ("lvl" + n, 2, colX (r, 2, 3), kVoiceY + 118.0f, 23.0f, "LEVEL");
    }
    add ("root", 3, colX (voiceRect (0), 0, 3), kBottomY + 66.0f, 23.0f, "KEY");     add ("scale", 4, colX (voiceRect (0), 1, 3), kBottomY + 66.0f, 23.0f, "SCALE");
    add ("spread", 5, colX (outputRect(), 0, 3), kBottomY + 66.0f, 23.0f, "SPREAD");  add ("mix", 5, colX (outputRect(), 1, 3), kBottomY + 66.0f, 23.0f, "MIX");  add ("level", 6, colX (outputRect(), 2, 3), kBottomY + 66.0f, 23.0f, "LEVEL");
    powerAtt.sendInitialUpdate(); intelAtt.sendInitialUpdate(); rootAtt.sendInitialUpdate(); scaleAtt.sendInitialUpdate();

    juce::Random rng (11);                                                                          // satin grain tile
    grain = juce::Image (juce::Image::ARGB, 128, 128, true);
    for (int y = 0; y < 128; ++y)
        for (int x = 0; x < 128; ++x)
            grain.setPixelAt (x, y, (rng.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (rng.nextFloat() * 0.05f));

    constrainer.setSizeLimits (720, 369, 2400, 1228);
    constrainer.setFixedAspectRatio ((double) kW / (double) kH);
    setConstrainer (&constrainer);
    setResizable (true, true);
    setSize ((int) kW, (int) kH);
    layoutKnobs();
    startTimerHz (12);
}

PT1AudioProcessorEditor::~PT1AudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void PT1AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    midiUI.paintMarks (g, kBody);
}

void PT1AudioProcessorEditor::timerCallback() { repaint(); }          // the heard and played notes

void PT1AudioProcessorEditor::toggle (const char* id)
{
    auto* par = proc.apvts.getParameter (id);
    par->beginChangeGesture();
    par->setValueNotifyingHost (par->getValue() > 0.5f ? 0.0f : 1.0f);
    par->endChangeGesture();
}

void PT1AudioProcessorEditor::layoutKnobs()
{
    const float sc = scale();
    for (auto& k : knobs)
    {
        const float d = 2.0f * (k->r + kPad) * sc;
        k->slider.setBounds (juce::Rectangle<float> (d, d).withCentre (juce::Point<float> (k->cx, k->cy) * sc).toNearestInt());
    }
}

void PT1AudioProcessorEditor::resized() { layoutKnobs(); }

juce::String PT1AudioProcessorEditor::knobText (int kind, double v) const
{
    const float f = (float) v * 0.01f;
    switch (kind)
    {
        case 0:
        {
            static const char* names[13] = { "UNISON", "m2", "2nd", "m3", "3rd", "4th", "b5", "5th", "m6", "6th", "b7", "7th", "OCTAVE" };
            const int s = pt1::intervalSemis (f);
            return std::abs (s) <= 12 ? (s == 0 ? juce::String (names[0]) : (s > 0 ? "+" : "-") + juce::String (names[std::abs (s)])) : juce::String::formatted ("%+d st", s);
        }
        case 1: return juce::String::formatted ("%+d ct", (int) std::lround (pt1::detuneCents (f)));
        case 3: return pt1::noteName (choice ("root"));
        case 4: return pt1::scaleName (choice ("scale"));
        case 6: return juce::String::formatted ("%+d dB", (int) std::lround (v));
        default: return juce::String ((int) std::lround (v)) + " %";
    }
}

//==============================================================================
void PT1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));

    // satin body
    g.setGradientFill (juce::ColourGradient (kBody.brighter (0.22f), 0.0f, 0.0f, kBody.darker (0.18f), kW, kH, false));
    g.fillRect (0.0f, 0.0f, kW, kH);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.14f), kW * 0.3f, kH * 0.1f, juce::Colours::transparentWhite, kW * 0.3f, kH * 0.75f, true));
    g.fillRect (0.0f, 0.0f, kW, kH);
    g.setTiledImageFill (grain, 0, 0, 1.0f);
    g.fillRect (0.0f, 0.0f, kW, kH);
    g.setColour (juce::Colours::white.withAlpha (0.28f));
    g.drawLine (0.0f, 1.5f, kW, 1.5f, 3.0f);
    g.drawLine (1.5f, 0.0f, 1.5f, kH, 3.0f);
    g.setColour (juce::Colours::black.withAlpha (0.12f));
    g.drawLine (0.0f, kH - 1.5f, kW, kH - 1.5f, 3.0f);
    g.drawLine (kW - 1.5f, 0.0f, kW - 1.5f, kH, 3.0f);

    // header
    drawText (g, "PT-1", kMargin + 3.0f, 55.0f, 70.0f, kDark, true, 0.0f, juce::Justification::centredLeft, true);
    drawText (g, "HARMONIZER", kW - kMargin - 95.0f, 43.0f, 20.0f, kDark, false, 0.2f, juce::Justification::centredRight);
    drawText (g, "PHANTOM VOICES", kW - kMargin - 95.0f, 67.0f, 17.0f, kDark, false, 0.2f, juce::Justification::centredRight);
    g.setColour (kDark.withAlpha (0.85f));
    g.fillRect (kMargin, kRuleY, kW - 2.0f * kMargin, 1.6f);
    {
        const bool on = isOn ("power");
        const auto sr = juce::Rectangle<float> (33.0f, 17.0f).withCentre ({ kW - kMargin - 16.5f, 43.5f });
        g.setGradientFill (on ? juce::ColourGradient (juce::Colour (0xff0e3a1e), sr.getX(), sr.getY(), juce::Colour (0xff1a7a3e), sr.getX(), sr.getBottom(), false)
                              : juce::ColourGradient (juce::Colour (0xff111214), sr.getX(), sr.getY(), juce::Colour (0xff25272b), sr.getX(), sr.getBottom(), false));
        g.fillRoundedRectangle (sr, 8.5f);
        g.setColour (juce::Colours::white.withAlpha (0.22f));
        g.drawRoundedRectangle (sr, 8.5f, 1.0f);
        const auto ball = juce::Rectangle<float> (14.0f, 14.0f).withCentre ({ on ? sr.getRight() - 8.5f : sr.getX() + 8.5f, sr.getCentreY() });
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillEllipse (ball.translated (0.5f, 1.2f));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffafbfd), ball.getX() + 3.0f, ball.getY() + 2.0f, juce::Colour (0xff6e727e), ball.getRight(), ball.getBottom(), false));
        g.fillEllipse (ball);
        drawText (g, "OFF / ON", kW - kMargin, 68.5f, 13.0f, kDark, false, 0.12f, juce::Justification::centredRight);
    }

    const auto drawPanel = [&] (juce::Rectangle<float> r, const char* name)
    {
        g.setColour (kPanel);
        g.fillRoundedRectangle (r, 20.0f);
        g.setColour (kInk.withAlpha (0.55f));
        g.drawRoundedRectangle (r.reduced (0.5f), 20.0f, 1.4f);
        const float w = textWidth (font (18.0f, true).withExtraKerningFactor (0.08f), name) + 38.0f;
        const auto pill = juce::Rectangle<float> (w, 22.0f).withCentre ({ r.getCentreX(), r.getY() });
        g.setColour (kPanel);
        g.fillRoundedRectangle (pill, 11.0f);
        drawText (g, name, pill.getCentreX(), pill.getCentreY(), 18.0f, kBody, true, 0.08f);
    };
    for (int v = 0; v < pt1::kNumVoices; ++v) drawPanel (voiceRect (v), ("VOICE " + juce::String (v + 1)).toRawUTF8());
    drawPanel (keyRect(), "KEY");
    drawPanel (chordsRect(), "CHORDS");
    drawPanel (outputRect(), "OUTPUT");

    const auto& eng = proc.getEngine();
    const auto midiName = [] (float m) { return m < 0.0f ? juce::String ("--") : juce::String (pt1::noteName ((int) std::lround (m))) + juce::String ((int) std::lround (m) / 12 - 1); };
    const bool intel = isOn ("intel");
    for (int v = 0; v < pt1::kNumVoices; ++v)                                                     // the note each voice plays now
    {
        const auto r = voiceRect (v);
        const float n = eng.voiceNote[v].load (std::memory_order_relaxed);
        drawText (g, midiName (n), r.getCentreX(), kVoiceY + 52.0f, 26.0f, n < 0.0f ? kInk.withAlpha (0.3f) : kBody, true, 0.06f);
    }
    {
        const auto b = intelRect();
        if (intel) { g.setColour (kBody); g.fillRoundedRectangle (b, 10.0f); }
        g.setColour (intel ? kBody : kInk.withAlpha (0.5f));
        g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, 1.4f);
        drawText (g, "INTELLIGENT", b.getCentreX(), b.getCentreY(), 14.0f, intel ? kDark : kInk.withAlpha (0.9f), true, 0.1f);
        drawText (g, "HEARD: " + midiName (eng.detected.load (std::memory_order_relaxed)), b.getCentreX(), b.getBottom() + 26.0f, 14.0f, kInk.withAlpha (0.85f), true, 0.08f);
    }
    for (int c = 0; c < pt1::kNumChords; ++c)
    {
        const auto b = chordRect (c);
        // a chord button is "on" while the four interval knobs still match its chord
        bool on = true;
        for (int v = 0; v < pt1::kNumVoices && on; ++v)
            on = std::abs (proc.apvts.getParameter ("int" + juce::String (v + 1))->getValue() - pt1::intervalValue (pt1::chordSemis (c, v))) < 0.0075f;
        if (on) { g.setColour (kBody); g.fillRoundedRectangle (b, 10.0f); }
        g.setColour (on ? kBody : kInk.withAlpha (0.4f));
        g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, 1.2f);
        drawText (g, pt1::chordName (c), b.getCentreX(), b.getCentreY(), 12.0f, on ? kDark : kInk.withAlpha (0.9f), true, 0.06f);
    }
    drawText (g, "SETS THE FOUR INTERVALS", chordsRect().getCentreX(), kBottomY + 150.0f, 11.0f, kInk.withAlpha (0.5f), true, 0.12f);

    // knobs: recessed ring, 11 ticks, legends, name and value
    for (auto& kp : knobs)
    {
        const Knob& k = *kp;
        const juce::Point<float> c (k.cx, k.cy);
        const auto ring = juce::Rectangle<float> (2.0f * (k.r + 5.0f), 2.0f * (k.r + 5.0f)).withCentre (c);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillEllipse (ring);
        g.setColour (juce::Colours::white.withAlpha (0.1f));
        g.drawEllipse (ring, 1.0f);
        for (int t = 0; t <= 10; ++t)
        {
            const float a = juce::degreesToRadians (-135.0f + 27.0f * (float) t), s = std::sin (a), co = std::cos (a);
            const bool major = t == 0 || t == 5 || t == 10;
            const float r0 = k.r + 10.0f, r1 = r0 + (major ? 8.0f : 5.0f);
            g.setColour (kInk.withAlpha (0.85f));
            g.drawLine (c.x + s * r0, c.y - co * r0, c.x + s * r1, c.y - co * r1, major ? 2.0f : 1.2f);
        }
        const float lx = (k.r + 18.0f) * 0.7071f, ly = lx + 13.0f;
        const char* lo = k.kind == 0 ? "-24" : k.kind == 1 ? "-50" : k.kind == 6 ? "-24" : k.kind == 3 ? "C" : k.kind == 4 ? "" : "0";
        const char* hi = k.kind == 0 ? "+24" : k.kind == 1 ? "+50" : k.kind == 6 ? "+12" : k.kind == 3 ? "B" : k.kind == 4 ? "" : "10";
        drawText (g, lo, c.x - lx, c.y + ly, 12.0f, kInk.withAlpha (0.85f), false, 0.0f);
        drawText (g, hi, c.x + lx, c.y + ly, 12.0f, kInk.withAlpha (0.85f), false, 0.0f);
        drawText (g, k.name, c.x, c.y + 64.0f, 14.0f, kInk.withAlpha (0.95f), true, 0.08f);
        drawText (g, knobText (k.kind, k.slider.getValue()), c.x, c.y + 86.0f, 14.0f, kBody, false, 0.04f);
    }

    drawText (g, "Made by Richard Leurs | VIBR", kW - kMargin - 20.0f, kBottomY + kBottomH + 35.0f, 13.0f, kDark, false, 0.06f, juce::Justification::centredRight);
    drawVibrLogo (g, juce::Rectangle<float> (13.0f, 13.0f).withCentre ({ kW - kMargin - 6.5f, kBottomY + kBottomH + 35.0f }), kDark);
}

void PT1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / scale();
    if (powerRect().contains (p)) { toggle ("power"); return; }
    if (intelRect().expanded (3.0f).contains (p)) { toggle ("intel"); return; }
    for (int c = 0; c < pt1::kNumChords; ++c)
        if (chordRect (c).expanded (3.0f).contains (p))
        {
            for (int v = 0; v < pt1::kNumVoices; ++v)
            {
                auto* par = proc.apvts.getParameter ("int" + juce::String (v + 1));
                par->beginChangeGesture();
                par->setValueNotifyingHost (pt1::intervalValue (pt1::chordSemis (c, v)));
                par->endChangeGesture();
            }
            return;
        }
}
