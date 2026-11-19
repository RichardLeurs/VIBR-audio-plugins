// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "ui/Presets.h"

namespace
{
using vibr::ui::vintage::drawText;
namespace ui = vibr::ui::vintage;

constexpr float kW = ui::kW, kH = ui::kH;
const ui::Palette kPal { juce::Colour (240, 91, 96) };                 // coral #F05B60 (Amp & Cabinet): value rings, knob values and the plugin code

// panels: row 1 CONTROLS (four tabs), row 2 DISPLAY (two tabs) and OUTPUT
const juce::Rectangle<float> kControls (28.0f, 114.0f, 1064.0f, 244.0f), kDisplay (28.0f, 376.0f, 646.0f, 254.0f), kOutput (692.0f, 376.0f, 400.0f, 254.0f);
const std::vector<juce::String> kTabs { "GATE / BOOST", "AMP", "CABINET", "EQ" }, kDisplayTabs { "CURVE", "CABINET" };
constexpr float kTabsTop = -15.0f, kTabsFont = 16.0f, kPitch = 110.0f, kKnobDy = 86.0f, kButtonDy = 200.0f, kColumnW = 140.0f;
const float kTabsLeft = ui::titleEnd ("CONTROLS"), kDisplayTabsLeft = ui::titleEnd ("DISPLAY");

// the knob row of a CONTROLS tab: `n` knobs 110 apart, plus a column of buttons on the AMP and CABINET tabs, the whole group centred in the panel
constexpr int kKnobCount[4] = { 6, 8, 6, 4 };
bool hasColumn (int tab) { return tab == 1 || tab == 2; }
float groupStart (int tab) { return 0.5f * (kControls.getWidth() - ((float) kKnobCount[tab] * kPitch + (hasColumn (tab) ? kColumnW : 0.0f))); }
float knobX (int tab, int i) { return kControls.getX() + groupStart (tab) + 0.5f * kPitch + kPitch * (float) i; }
float columnX (int tab) { return kControls.getX() + groupStart (tab) + (float) kKnobCount[tab] * kPitch + 0.5f * kColumnW; }
juce::Point<float> outKnobCentre() { return { kOutput.getX() + 80.0f, kOutput.getY() + kKnobDy }; }

// buttons of the CONTROLS panel (the column buttons stack round the knob row)
juce::Rectangle<float> typeButton (int i)
{
    constexpr float gap = 5.0f, w = (1024.0f - gap * 13.0f) / 14.0f;
    return { kControls.getX() + 20.0f + (float) i * (w + gap), kControls.getY() + kButtonDy, w, 30.0f };
}
juce::Rectangle<float> columnButton (int tab, int row, int rows)        // `row` of `rows` stacked buttons, centred on the knob row
{
    const float cy = kControls.getY() + kKnobDy + (rows == 1 ? 0.0f : (row == 0 ? -24.0f : 24.0f));
    return juce::Rectangle<float> (kColumnW - 6.0f, 30.0f).withCentre ({ columnX (tab), cy });
}
juce::Rectangle<float> brightRect() { return columnButton (1, 0, 2); }
juce::Rectangle<float> deepRect()   { return columnButton (1, 1, 2); }
juce::Rectangle<float> switchRect (int i)                                // gate, boost, cabinet, EQ, limiter
{
    const float y = kControls.getY() + kButtonDy;
    switch (i)
    {
        case 0:  return juce::Rectangle<float> (130.0f, 30.0f).withCentre ({ 0.5f * (knobX (0, 1) + knobX (0, 2)), y + 15.0f });
        case 1:  return juce::Rectangle<float> (130.0f, 30.0f).withCentre ({ knobX (0, 4), y + 15.0f });
        case 2:  return columnButton (2, 0, 1);
        case 3:  return juce::Rectangle<float> (130.0f, 30.0f).withCentre ({ kControls.getCentreX(), y + 15.0f });
        default: return juce::Rectangle<float> (110.0f, 30.0f).withCentre ({ outKnobCentre().x, kOutput.getY() + kButtonDy + 15.0f });
    }
}
juce::Rectangle<float> loadRect (int s)  { return { kControls.getX() + 20.0f + (float) s * 524.0f, kControls.getY() + kButtonDy, 120.0f, 30.0f }; }
juce::Rectangle<float> nameRect (int s)  { return loadRect (s).withX (loadRect (s).getX() + 128.0f).withWidth (280.0f); }
juce::Rectangle<float> clearRect (int s) { return loadRect (s).withX (loadRect (s).getX() + 416.0f).withWidth (84.0f); }

// the display panel's screens: the transfer curve and the sine (CURVE), the response graph (CABINET)
juce::Rectangle<float> curveScreen() { return { kDisplay.getX() + 20.0f, kDisplay.getY() + 34.0f, 200.0f, 168.0f }; }
juce::Rectangle<float> waveScreen()  { const auto c = curveScreen(); return { c.getRight() + 10.0f, c.getY(), kDisplay.getRight() - 20.0f - c.getRight() - 10.0f, c.getHeight() }; }
juce::Rectangle<float> graphScreen() { return { kDisplay.getX() + 20.0f, kDisplay.getY() + 34.0f, 380.0f, 168.0f }; }
juce::Rectangle<float> curveRect()   { return juce::Rectangle<float> (124.0f, 124.0f).withCentre (ui::scopePlotRect (curveScreen()).getCentre()); }
juce::Rectangle<float> waveRect()    { return ui::scopePlotRect (waveScreen()); }
juce::Rectangle<float> graphRect()   { return ui::scopePlotRect (graphScreen()).withTrimmedLeft (34.0f).withTrimmedBottom (18.0f).withTrimmedRight (8.0f); }
constexpr float kGraphLo = -36.0f, kGraphHi = 18.0f;
const juce::Colour kIrA (70, 150, 230), kIrB (60, 170, 90);

float freqToX (const juce::Rectangle<float>& r, float f) { return r.getX() + r.getWidth() * std::log10 (f / 20.0f) / 3.0f; }
float dbToY (const juce::Rectangle<float>& r, float d) { return r.getBottom() - r.getHeight() * juce::jlimit (0.0f, 1.0f, (d - kGraphLo) / (kGraphHi - kGraphLo)); }

juce::String hzString (float hz) { return hz >= 1000.0f ? juce::String (hz / 1000.0f, 1) + " kHz" : juce::String ((int) std::lround (hz)) + " Hz"; }
juce::String knobText (const juce::String& id, float v)
{
    if (id == "inGain") return juce::String::formatted ("%+.1f dB", (double) v);
    if (id == "gateThresh") return juce::String::formatted ("%.0f dB", (double) al1::gateThresholdDb (v));
    if (id == "gateRelease") return juce::String ((int) std::lround (al1::gateReleaseMs (v))) + " ms";
    if (id == "boostTight") return hzString (al1::tightHz (v));
    if (id == "boostLevel" || id == "eqLow" || id == "eqMid" || id == "eqHigh") return juce::String::formatted ("%+.1f dB", (double) al1::bipolarDb (v));
    if (id == "presence" || id == "resonance") return juce::String::formatted ("%+.1f dB", (double) al1::presenceDb (v));
    if (id == "master") return juce::String::formatted ("%+.1f dB", (double) al1::masterDb (v));
    if (id == "irALevel" || id == "irBLevel") return v <= 0.01f ? juce::String ("OFF") : juce::String::formatted ("%+.1f dB", (double) ((v - 7.0f) * 3.0f));
    if (id == "lowCut") return hzString (al1::lowCutHz (v));
    if (id == "highCut") return hzString (al1::highCutHz (v));
    if (id == "eqFreq") return hzString (al1::eqFreqHz (v));
    if (id == "out") return v <= 0.01f ? juce::String ("-inf") : juce::String::formatted ("%+.1f dB", (double) al1::outputDb (v));
    return juce::String (v, 1);
}
float toDb (float x) { return 20.0f * std::log10 (x + 1.0e-6f); }

// a clipped single line of Plex Mono inside `r` (names of impulse responses)
void monoClipped (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r, float size, juce::Colour c)
{
    g.setColour (c);
    g.setFont (ui::font (size, false, ui::Face::Mono));
    g.drawText (s, r, juce::Justification::centredLeft, true);
}
}

//==============================================================================
AL1AudioProcessorEditor::AL1AudioProcessorEditor (AL1AudioProcessor& p)
    : LabEditor (p, kW, kH), proc (p),
      knobs { { { "inGain", 0, 0, "INPUT", "-18", "+18", {}, {} },        { "gateThresh", 0, 1, "THRESH", "-70", "-30", {}, {} },
                { "gateRelease", 0, 2, "RELEASE", "30", "400", {}, {} },  { "boostDrive", 0, 3, "DRIVE", "0", "10", {}, {} },
                { "boostLevel", 0, 4, "LEVEL", "-12", "+12", {}, {} },    { "boostTight", 0, 5, "TIGHT", "40", "900", {}, {} },
                { "gain", 1, 0, "GAIN", "0", "10", {}, {} },              { "bass", 1, 1, "BASS", "0", "10", {}, {} },
                { "middle", 1, 2, "MIDDLE", "0", "10", {}, {} },          { "treble", 1, 3, "TREBLE", "0", "10", {}, {} },
                { "presence", 1, 4, "PRESENCE", "-6", "+6", {}, {} },     { "resonance", 1, 5, "RESONANCE", "-6", "+6", {}, {} },
                { "sag", 1, 6, "SAG", "0", "10", {}, {} },                { "master", 1, 7, "MASTER", "-24", "+6", {}, {} },
                { "irALevel", 2, 0, "IR A LEVEL", "0", "10", {}, {} },    { "irBLevel", 2, 1, "IR B LEVEL", "0", "10", {}, {} },
                { "lowCut", 2, 2, "LOW CUT", "40", "400", {}, {} },       { "highCut", 2, 3, "HIGH CUT", "3k", "16k", {}, {} },
                { "mic", 2, 4, "MIC", "0", "10", {}, {} },                { "ambience", 2, 5, "AMBIENCE", "0", "10", {}, {} },
                { "eqLow", 3, 0, "LOW", "-12", "+12", {}, {} },           { "eqMid", 3, 1, "MID", "-12", "+12", {}, {} },
                { "eqFreq", 3, 2, "MID FREQ", "200", "3k", {}, {} },      { "eqHigh", 3, 3, "HIGH", "-12", "+12", {}, {} },
                { "out", -1, 0, "OUTPUT", "-inf", "+6", {}, {} } } },
      switches { { { "gateOn", 0, "GATE", {} }, { "boostOn", 0, "BOOST", {} }, { "cabOn", 2, "CABINET ON", {} }, { "eqOn", 3, "EQ", {} }, { "limiter", -1, "LIMITER", {} } } }
{
    for (auto& k : knobs) attachKnob (k.slider, k.att, k.id);
    for (auto& s : switches)
    {
        s.target.onPress = [this, id = s.id] { proc.undoManager.beginNewTransaction(); toggle (id); };
        addAndMakeVisible (s.target);
    }
    layoutKnobs();
    updateDisplay();
    startTimerHz (30);
}

AL1AudioProcessorEditor::~AL1AudioProcessorEditor() = default;

void AL1AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    if (midiUI != nullptr) midiUI->paintMarks (g, kPal.accent);
}

void AL1AudioProcessorEditor::layoutKnobs()
{
    const float sc = scale();
    const int t = tab();
    for (auto& k : knobs)
    {
        placeKnob (k.slider, k.tab < 0 ? outKnobCentre() : juce::Point<float> (knobX (k.tab, k.index), kControls.getY() + kKnobDy));
        k.slider.setVisible (k.tab < 0 || k.tab == t);
    }
    for (size_t i = 0; i < switches.size(); ++i)
    {
        auto& s = switches[i];
        s.target.setBounds (switchRect ((int) i).expanded (3.0f).transformedBy (juce::AffineTransform::scale (sc)).toNearestInt());
        s.target.setVisible (s.tab < 0 || s.tab == t);
    }
    if (midiUI == nullptr || midiTab != t)                            // MIDI learn marks only for the controls that are on screen
    {
        midiTab = t;
        midiUI = std::make_unique<MidiLearnUI> (proc.midiMap);
        for (auto& k : knobs) if (k.tab < 0 || k.tab == t) midiUI->add (k.slider, k.id);
        for (auto& s : switches) if (s.tab < 0 || s.tab == t) midiUI->add (s.target, s.id);
    }
}
void AL1AudioProcessorEditor::resized() { layoutKnobs(); }

//==============================================================================
// impulse responses
void AL1AudioProcessorEditor::loadIr (int slot, const juce::File& f)
{
    juce::String error;
    if (! proc.loadImpulseResponse (slot, f, &error))
        juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "AL-1", error.isEmpty() ? juce::String ("The impulse response could not be loaded.") : error);
    updateDisplay();
    repaint();
}

void AL1AudioProcessorEditor::showIrMenu (int slot)
{
    juce::PopupMenu menu;
    juce::Array<juce::File> files;
    const auto folder = AL1AudioProcessor::irFolder();
    if (folder.isDirectory())
        for (const auto& f : folder.findChildFiles (juce::File::findFiles, true, "*.wav;*.aif;*.aiff;*.WAV;*.AIF;*.AIFF", juce::File::FollowSymlinks::no))
            if (files.size() < 200) files.add (f);
    files.sort();
    menu.addSectionHeader ("Documents/VIBR/AL-1/IRs");
    if (files.isEmpty()) menu.addItem (-1, "(no impulse responses in the folder)", false);
    for (int i = 0; i < files.size(); ++i) menu.addItem (100 + i, files[i].getRelativePathFrom (folder), true, files[i].getFileNameWithoutExtension() == proc.getImpulseName (slot) && proc.hasImpulse (slot));
    menu.addSeparator();
    menu.addItem (1, "Browse...");
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (loadRect (slot).transformedBy (juce::AffineTransform::scale (scale())).toNearestInt())),
                        [this, slot, files, folder] (int r)
    {
        if (r >= 100 && r - 100 < files.size()) loadIr (slot, files[r - 100]);
        else if (r == 1)
        {
            chooser = std::make_unique<juce::FileChooser> ("Load an impulse response (WAV or AIFF, up to 3 s)", folder.isDirectory() ? folder : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.wav;*.aif;*.aiff");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this, slot] (const juce::FileChooser& c)
            {
                if (c.getResult().existsAsFile()) loadIr (slot, c.getResult());
            });
        }
    });
}
//==============================================================================
void AL1AudioProcessorEditor::timerCallback()
{
    if (midiUI != nullptr && midiUI->needsRepaint()) { midiUI->tick(); repaint(); }
    updateDisplay();
    repaintDesign (kDisplay.expanded (4.0f));
    repaintDesign (kOutput.expanded (4.0f));
}

// the data the display draws: curves and meters, from the parameters and the engine's atomics (the message thread, never the audio thread)
void AL1AudioProcessorEditor::updateDisplay()
{
    auto& eng = proc.getEngine();
    latency = eng.latency();
    const auto p = proc.readParams();

    // meters (peaks since the last call; fast attack, 45 dB / s fall)
    const float inPk = eng.inPeak.exchange (0.0f), outPk = eng.outPeak.exchange (0.0f), lim = eng.limiterMin.exchange (1.0f);
    inDb = juce::jmax (toDb (inPk), inDb - 1.5f);
    outDb = juce::jmax (toDb (outPk), outDb - 1.5f);
    grDb = juce::jmax (0.0f, juce::jmax (-toDb (lim), grDb - 0.6f));
    gateLamp = eng.gateLamp.load();
    meter.update (eng.outPeakCh[0].exchange (0.0f), eng.outPeakCh[1].exchange (0.0f));       // output meter: instant attack, 24 dB / s release, 1.5 s peak hold

    for (int s = 0; s < 2; ++s)
    {
        irPresent[s] = proc.hasImpulse (s);
        irNames[s] = irPresent[s] ? proc.getImpulseName (s) : juce::String();
    }
    const int dt = displayTab();
    if (dt == 0)
    {
        const auto c = curveRect();
        const auto toXY = [&] (float x, float y) { return juce::Point<float> (c.getCentreX() + x * 5.0f * c.getWidth(), c.getCentreY() - juce::jlimit (-1.1f, 1.1f, y) * 0.5f * c.getHeight()); };
        curvePath.clear();
        for (int i = 0; i <= 200; ++i)
        {
            const float x = -0.1f + 0.2f * (float) i / 200.0f, y = al1::Engine::staticCurve (p, x);
            const auto pt = toXY (x, y); if (i == 0) curvePath.startNewSubPath (pt); else curvePath.lineTo (pt);
        }
        const float x = juce::jlimit (-0.1f, 0.1f, eng.dotIn.load (std::memory_order_relaxed)), y = al1::Engine::staticCurve (p, x);
        const auto d = toXY (x, y); dotX = d.x; dotY = d.y;

        const auto w = waveRect();
        sineInPath.clear(); sineOutPath.clear();
        constexpr int N = 240;
        float peak = 0.05f; float ys[N];
        for (int i = 0; i < N; ++i) { ys[i] = al1::Engine::staticCurve (p, 0.05f * std::sin (6.2831853f * 2.0f * (float) i / (float) N)); peak = juce::jmax (peak, std::abs (ys[i])); }
        for (int i = 0; i < N; ++i)
        {
            const float xx = w.getX() + w.getWidth() * (float) i / (float) (N - 1), s = std::sin (6.2831853f * 2.0f * (float) i / (float) N);
            const juce::Point<float> a (xx, w.getCentreY() - s * 0.42f * w.getHeight()), b (xx, w.getCentreY() - ys[i] / peak * 0.42f * w.getHeight());
            if (i == 0) { sineInPath.startNewSubPath (a); sineOutPath.startNewSubPath (b); } else { sineInPath.lineTo (a); sineOutPath.lineTo (b); }
        }
    }
    else if (dt == 1)
    {
        const double sr = juce::jmax (44100.0, proc.getSampleRate());
        dispCab.setModel (sr, al1::kTypes[p.type], p.mic * 0.1f);
        dispCab.setCuts (sr, p.lowCut, p.highCut);
        dispCab.setEq (sr, p.eqLow, p.eqMid, p.eqFreq, p.eqHigh);
        for (int s = 0; s < 2; ++s) if (irPresent[s]) proc.copyIrCurve (s, irCurve[s].data());
        const auto g = graphRect();
        const float la = al1::irLevelGain (p.irALevel), lb = al1::irLevelGain (p.irBLevel);
        totalPath.clear(); modelPath.clear(); irPath[0].clear(); irPath[1].clear();
        constexpr int N = 160;
        bool first = true;
        for (int i = 0; i < N; ++i)
        {
            const double f = 20.0 * std::pow (1000.0, (double) i / (double) (N - 1));
            const float x = freqToX (g, (float) f);
            const double model = dispCab.modelMag (sr, f), cuts = p.cabOn ? dispCab.cutsMag (sr, f) : 1.0, eq = p.eqOn ? dispCab.eqMag (sr, f) : 1.0;
            double cab = model;
            if (irPresent[0] || irPresent[1])
            {
                cab = 0.0;
                if (irPresent[0]) cab += std::pow (10.0, irCurve[0][(size_t) i] / 20.0) * la;
                if (irPresent[1]) cab += std::pow (10.0, irCurve[1][(size_t) i] / 20.0) * lb;
                cab = juce::jmax (cab, 1.0e-4);
            }
            if (! p.cabOn) cab = 1.0;
            const auto pt = [&] (double mag) { return juce::Point<float> (x, dbToY (g, (float) (20.0 * std::log10 (mag + 1.0e-9)))); };
            const auto t = pt (cab * cuts * eq);
            if (first) { totalPath.startNewSubPath (t); modelPath.startNewSubPath (pt (model)); first = false; }
            else { totalPath.lineTo (t); modelPath.lineTo (pt (model)); }
            for (int s = 0; s < 2; ++s)
            {
                const auto q = juce::Point<float> (x, dbToY (g, irCurve[s][(size_t) i] + toDb (s == 0 ? la : lb)));
                if (i == 0) irPath[s].startNewSubPath (q); else irPath[s].lineTo (q);
            }
        }
    }
}
//==============================================================================
// CURVE: the transfer curve of the amp stages (input +-0.1 against output, the live input as a dot) and a sine before (dim) and after (bright), on two phosphor screens
void AL1AudioProcessorEditor::drawCurve (juce::Graphics& g)
{
    const bool power = isOn ("power");
    const auto cs = curveScreen();
    ui::paintScope (g, kPal, cs, "TRANSFER", {});
    const auto c = curveRect();
    g.setColour (kPal.phosphor.withAlpha (0.22f));
    g.drawLine (c.getX(), c.getCentreY(), c.getRight(), c.getCentreY(), 1.0f); g.drawLine (c.getCentreX(), c.getY(), c.getCentreX(), c.getBottom(), 1.0f);
    g.setColour (kPal.phosphorDim.withAlpha (0.5f)); g.drawLine (c.getX(), c.getBottom(), c.getRight(), c.getY(), 1.0f);              // the straight line: no distortion
    {
        juce::Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (c.getSmallestIntegerContainer().getIntersection (cs.reduced (3.0f).toNearestInt()));
        g.setColour (kPal.phosphor.withAlpha (0.18f)); g.strokePath (curvePath, juce::PathStrokeType (5.0f));
        g.setColour (kPal.phosphor); g.strokePath (curvePath, juce::PathStrokeType (2.0f));
        if (power) { g.setColour (kPal.cream); g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ dotX, dotY })); }
    }
    drawText (g, "IN", c.getRight() - 12.0f, c.getCentreY() + 12.0f, 13.0f, kPal.phosphorDim, false, 0.08f, juce::Justification::centred, ui::Face::Mono);
    drawText (g, "OUT", c.getCentreX() - 20.0f, c.getY() + 10.0f, 13.0f, kPal.phosphorDim, false, 0.08f, juce::Justification::centred, ui::Face::Mono);

    const auto ws = waveScreen();
    ui::paintScope (g, kPal, ws, "SINE IN / OUT", "SCALED");
    const auto w = waveRect();
    g.setColour (kPal.phosphor.withAlpha (0.22f)); g.drawLine (w.getX(), w.getCentreY(), w.getRight(), w.getCentreY(), 1.0f);
    {
        juce::Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (ws.reduced (3.0f).toNearestInt());
        g.setColour (kPal.phosphorDim.withAlpha (0.8f)); g.strokePath (sineInPath, juce::PathStrokeType (1.3f));
        g.setColour (kPal.phosphor.withAlpha (0.18f)); g.strokePath (sineOutPath, juce::PathStrokeType (5.0f));
        g.setColour (kPal.phosphor); g.strokePath (sineOutPath, juce::PathStrokeType (2.0f));
    }
    drawText (g, juce::String (al1::kTypes[ampType()].name) + "  |  4X OVERSAMPLED  |  LATENCY " + juce::String (latency) + " SAMPLES", kDisplay.getX() + 20.0f, kDisplay.getY() + 226.0f,
              14.0f, kPal.inkSoft, false, 0.05f, juce::Justification::centredLeft);
}

// CABINET: the response of the cabinet (filter model or impulse responses, with the cuts and the EQ) on a screen, and the legend
void AL1AudioProcessorEditor::drawCabinet (juce::Graphics& g)
{
    const auto gs = graphScreen();
    ui::paintScope (g, kPal, gs, "CABINET RESPONSE", "20 HZ - 20 KHZ");
    const auto r = graphRect();
    g.setColour (kPal.phosphor.withAlpha (0.14f));
    for (float d = -24.0f; d <= 12.0f; d += 12.0f) g.drawLine (r.getX(), dbToY (r, d), r.getRight(), dbToY (r, d), d == 0.0f ? 1.6f : 1.0f);
    for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f }) g.drawLine (freqToX (r, f), r.getY(), freqToX (r, f), r.getBottom(), 1.0f);
    for (float d : { -24.0f, -12.0f, 0.0f, 12.0f }) drawText (g, juce::String ((int) d), r.getX() - 4.0f, dbToY (r, d), 13.0f, kPal.phosphorDim, false, 0.0f, juce::Justification::centredRight, ui::Face::Mono);
    for (auto [f, s] : { std::pair<float, const char*> { 100.0f, "100" }, { 1000.0f, "1k" }, { 10000.0f, "10k" } }) drawText (g, s, freqToX (r, f), r.getBottom() + 9.0f, 13.0f, kPal.phosphorDim, false, 0.0f, juce::Justification::centred, ui::Face::Mono);
    {
        juce::Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (r.expanded (2.0f).toNearestInt());
        if (! irPresent[0] && ! irPresent[1]) { g.setColour (kPal.phosphorDim.withAlpha (0.6f)); g.strokePath (modelPath, juce::PathStrokeType (1.2f)); }
        if (irPresent[0]) { g.setColour (kIrA.brighter (0.3f).withAlpha (0.8f)); g.strokePath (irPath[0], juce::PathStrokeType (1.3f)); }
        if (irPresent[1]) { g.setColour (kIrB.brighter (0.5f).withAlpha (0.8f)); g.strokePath (irPath[1], juce::PathStrokeType (1.3f)); }
        g.setColour (kPal.phosphor.withAlpha (0.18f)); g.strokePath (totalPath, juce::PathStrokeType (5.0f));
        g.setColour (kPal.phosphor); g.strokePath (totalPath, juce::PathStrokeType (2.0f));
    }

    // legend: three entries, a swatch with its name and a second line
    const float x = gs.getRight() + 20.0f, w = kDisplay.getRight() - 20.0f - x - 18.0f;
    const bool anyIr = irPresent[0] || irPresent[1];
    const juce::String lines1[3] = { isOn ("cabOn") ? "CABINET, CUTS, EQ" : "CABINET OFF", "IR A", "IR B" };
    const juce::String lines2[3] = { anyIr ? "IMPULSE RESPONSES" : "FILTER MODEL (NO IR)", irPresent[0] ? irNames[0].toUpperCase() : juce::String ("NONE"), irPresent[1] ? irNames[1].toUpperCase() : juce::String ("NONE") };
    const juce::Colour sw[3] = { kPal.ink, kIrA, kIrB };
    for (int i = 0; i < 3; ++i)
    {
        const float y = kDisplay.getY() + 48.0f + 56.0f * (float) i;
        g.setColour (sw[i]); g.fillRoundedRectangle (juce::Rectangle<float> (14.0f, 4.0f).withCentre ({ x + 7.0f, y }), 1.5f);
        drawText (g, lines1[i], x + 24.0f, y, 17.0f, kPal.ink, true, 0.08f, juce::Justification::centredLeft);
        monoClipped (g, lines2[i], { x + 24.0f, y + 12.0f, w - 24.0f, 20.0f }, 14.0f, kPal.inkSoft);
    }
    drawText (g, "IR FILES: DOCUMENTS / VIBR / AL-1 / IRS, SAVED IN THE SESSION, NOT IN THE PRESETS", kDisplay.getX() + 20.0f, kDisplay.getY() + 226.0f, 14.0f, kPal.inkSoft, false, 0.05f, juce::Justification::centredLeft);
}

// OUTPUT: the gate and limiter readouts between the knob and the meter
void AL1AudioProcessorEditor::drawOutput (juce::Graphics& g)
{
    const bool power = isOn ("power");
    const float cx = kOutput.getX() + 210.0f, top = kOutput.getY();
    const bool gateOn = gateLamp >= 0.0f, open = gateLamp > 0.5f && power, limOn = isOn ("limiter");
    const bool lim = limOn && power && grDb > 0.2f;

    const auto lamp = [&] (float y, bool lit)
    {
        const auto d = juce::Rectangle<float> (14.0f, 14.0f).withCentre ({ cx, y });
        if (lit) { g.setColour (kPal.ledOn.withAlpha (0.25f)); g.fillEllipse (d.expanded (4.0f)); }
        g.setColour (lit ? kPal.ledOn : kPal.ledOff); g.fillEllipse (d);
        g.setColour (kPal.ink); g.drawEllipse (d.reduced (1.0f), 2.0f);
    };
    lamp (top + 56.0f, open && gateOn);
    drawText (g, "GATE", cx, top + 82.0f, 21.0f, kPal.ink, true, 0.1f);
    drawText (g, ! gateOn || ! power ? "OFF" : open ? "OPEN" : "CLOSED", cx, top + 104.0f, 14.0f, kPal.accent, true, 0.0f, juce::Justification::centred, ui::Face::Mono);

    lamp (top + 150.0f, lim);
    drawText (g, "LIMITER GR", cx, top + 176.0f, 21.0f, kPal.ink, true, 0.1f);
    drawText (g, ! limOn ? juce::String ("OFF") : power && grDb > 0.05f ? juce::String::formatted ("-%.1f dB", (double) grDb) : juce::String ("0.0 dB"), cx, top + 198.0f, 14.0f, kPal.accent, true, 0.0f,
              juce::Justification::centred, ui::Face::Mono);
}

//==============================================================================
void AL1AudioProcessorEditor::paintSwitches (juce::Graphics& g)
{
    for (size_t i = 0; i < switches.size(); ++i)
        if (switches[i].tab < 0 || switches[i].tab == tab()) ui::paintFlatButton (g, kPal, switchRect ((int) i), switches[i].label, isOn (switches[i].id));
}

void AL1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    ui::paintBody (g, kPal, kW, kH);
    ui::paintHeader (g, kPal, "AL-1", "AMP LAB", "RAW POWER");
    ui::paintPresetBar (g, kPal, proc.presets, proc.undoManager, isOn ("power"));
    ui::paintFooter (g, kPal, kH - 15.0f);

    const int t = tab(), dt = displayTab();
    ui::paintPanel (g, kPal, kControls, "CONTROLS");
    ui::paintTabs (g, kPal, kControls, kTabs, t, kTabsTop, kTabsFont, kTabsLeft);
    ui::paintPanel (g, kPal, kDisplay, "DISPLAY");
    ui::paintTabs (g, kPal, kDisplay, kDisplayTabs, dt, kTabsTop, kTabsFont, kDisplayTabsLeft);
    ui::paintPanel (g, kPal, kOutput, "OUTPUT");
    if (dt == 0) drawCurve (g); else drawCabinet (g);
    drawOutput (g);

    paintSwitches (g);
    if (t == 1)
    {
        for (int i = 0; i < al1::kNumTypes; ++i) ui::paintFlatButton (g, kPal, typeButton (i), al1::kTypes[i].name, i == ampType());
        ui::paintFlatButton (g, kPal, brightRect(), "BRIGHT", isOn ("bright"));
        ui::paintFlatButton (g, kPal, deepRect(), "DEEP", isOn ("deep"));
    }
    else if (t == 2)
    {
        for (int s = 0; s < 2; ++s)
        {
            const bool has = proc.hasImpulse (s);
            ui::paintFlatButton (g, kPal, loadRect (s), s == 0 ? "IR A  LOAD  v" : "IR B  LOAD  v", false);
            const auto n = nameRect (s);
            ui::paintScreen (g, kPal, n, 4.0f);
            monoClipped (g, has ? proc.getImpulseName (s).toUpperCase() : juce::String ("NO IR LOADED"), n.reduced (12.0f, 0.0f), 15.0f, has ? kPal.phosphor : kPal.phosphorDim);
            ui::paintFlatButton (g, kPal, clearRect (s), "CLEAR", false, 17.0f, has ? 1.0f : 0.4f);
        }
    }

    for (auto& k : knobs)
    {
        if (k.tab >= 0 && k.tab != t) continue;
        const auto c = k.tab < 0 ? outKnobCentre() : juce::Point<float> (knobX (k.tab, k.index), kControls.getY() + kKnobDy);
        ui::paintKnob (g, kPal, c, k.name, knobText (k.id, (float) k.slider.getValue()), k.lo, k.hi, (float) k.slider.valueToProportionOfLength (k.slider.getValue()));
    }
    ui::paintMeter (g, kPal, meter, kOutput.getRight() - 20.0f - ui::kMeterW, kOutput.getY() + std::floor ((kOutput.getHeight() - ui::kMeterH) * 0.5f), isOn ("power"));
}

void AL1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / scale();
    proc.undoManager.beginNewTransaction();
    if (ui::powerRect().contains (p)) { toggle ("power"); return; }
    if (ui::presetBarMouseDown (p, *this, proc.presets, proc.undoManager, scale(), [this] { repaint(); })) { layoutKnobs(); return; }
    const auto tabs = ui::tabRects (kControls, kTabs, kTabsTop, kTabsFont, kTabsLeft);
    for (int i = 0; i < (int) tabs.size(); ++i)
        if (tabs[(size_t) i].expanded (3.0f).contains (p)) { proc.uiTab.store (i); layoutKnobs(); repaint(); return; }
    const auto dtabs = ui::tabRects (kDisplay, kDisplayTabs, kTabsTop, kTabsFont, kDisplayTabsLeft);
    for (int i = 0; i < (int) dtabs.size(); ++i)
        if (dtabs[(size_t) i].expanded (3.0f).contains (p)) { proc.uiDisplayTab.store (i); updateDisplay(); repaint(); return; }
    if (tab() == 1)
    {
        for (int i = 0; i < al1::kNumTypes; ++i)
            if (typeButton (i).expanded (3.0f).contains (p)) { setChoice ("ampType", i); repaint(); return; }
        if (brightRect().expanded (3.0f).contains (p)) { toggle ("bright"); repaint(); return; }
        if (deepRect().expanded (3.0f).contains (p)) { toggle ("deep"); repaint(); return; }
    }
    if (tab() == 2)
        for (int s = 0; s < 2; ++s)
        {
            if (loadRect (s).expanded (3.0f).contains (p) || nameRect (s).contains (p)) { showIrMenu (s); return; }
            if (clearRect (s).expanded (3.0f).contains (p)) { proc.clearImpulseResponse (s); updateDisplay(); repaint(); return; }
        }
}
