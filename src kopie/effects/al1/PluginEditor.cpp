// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "ui/OutputMeter.h"
#include "ui/Presets.h"

namespace
{
using vibr::ui::drawText;
namespace ui = vibr::ui;

constexpr float kW = 1200.0f, kH = 690.0f, kPanelY = 134.0f, kPanelH = 487.0f, kBarY = 39.0f, kKnobR = 24.0f, kPad = 5.0f;
const ui::Palette kPal { juce::Colour (240, 91, 96), juce::Colour (33, 13, 13) };              // coral #F05B60 (Amp & Cabinet, as the IR-2) and its print
const ui::Palette kAcc = kPal;                                                                  // tabs, lit buttons, values, curves and lamps use the body colour, as in the other Labs

const juce::Rectangle<float> kControls (20.0f, kPanelY, 760.0f, kPanelH), kDisplay (790.0f, kPanelY, 390.0f, kPanelH);
const std::vector<juce::String> kTabs { "GATE / BOOST", "AMP", "CABINET", "EQ / OUTPUT" }, kDisplayTabs { "CURVE", "CABINET", "METERS" };

// knob rows inside the CONTROLS panel: five slots 140 apart, two rows
float slotX (int s) { return 380.0f + 140.0f * ((float) s - 2.0f); }
constexpr float kRow1 = 190.0f, kRow2 = 346.0f;
constexpr float kGateY = 130.0f, kBoostY = 348.0f;                 // the GATE / BOOST tab spreads its two rows over the whole panel

juce::Rectangle<float> typeButton (int i) { const int row = i / 7, col = i % 7; return { kControls.getX() + 34.0f + (float) col * 100.0f, kControls.getY() + 46.0f + (float) row * 38.0f, 92.0f, 28.0f }; }
juce::Rectangle<float> brightRect() { return juce::Rectangle<float> (112.0f, 30.0f).withCentre ({ kControls.getX() + slotX (3), kControls.getY() + kRow2 }); }
juce::Rectangle<float> deepRect()   { return juce::Rectangle<float> (112.0f, 30.0f).withCentre ({ kControls.getX() + slotX (4), kControls.getY() + kRow2 }); }
juce::Rectangle<float> loadRect (int s)  { return { kControls.getX() + 34.0f, kControls.getY() + 100.0f + (float) s * 56.0f, 84.0f, 30.0f }; }
juce::Rectangle<float> nameRect (int s)  { return { kControls.getX() + 126.0f, kControls.getY() + 100.0f + (float) s * 56.0f, 500.0f, 30.0f }; }
juce::Rectangle<float> clearRect (int s) { return { kControls.getX() + 634.0f, kControls.getY() + 100.0f + (float) s * 56.0f, 92.0f, 30.0f }; }

// the display panel's inner areas
juce::Rectangle<float> curveRect()   { return { kDisplay.getX() + 80.0f, kDisplay.getY() + 38.0f, 230.0f, 230.0f }; }
juce::Rectangle<float> waveRect()    { return { kDisplay.getX() + 35.0f, kDisplay.getY() + 316.0f, 320.0f, 80.0f }; }
juce::Rectangle<float> graphRect()   { return { kDisplay.getX() + 48.0f, kDisplay.getY() + 44.0f, 324.0f, 300.0f }; }
constexpr float kGraphLo = -36.0f, kGraphHi = 18.0f;

void divider (juce::Graphics& g, juce::Point<float> origin, bool horizontal, float at, float len = 0.0f)     // a thin line between sections of the controls panel
{
    g.setColour (kAcc.ink.withAlpha (0.16f));
    if (horizontal) g.fillRect (origin.x + 34.0f, origin.y + at, len > 0.0f ? len : kControls.getWidth() - 68.0f, 1.0f);
    else            g.fillRect (origin.x + at, origin.y + 70.0f, 1.0f, kControls.getHeight() - 110.0f);
}

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
}

//==============================================================================
AL1AudioProcessorEditor::AL1AudioProcessorEditor (AL1AudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      knobs { { { "inGain", 0, slotX (4), 0.5f * (kGateY + kBoostY), "INPUT", "-18", "+18", {}, {} },        { "gateThresh", 0, slotX (1), kGateY, "THRESH", "-70", "-30", {}, {} },
                { "gateRelease", 0, slotX (2), kGateY, "RELEASE", "30", "400", {}, {} },  { "boostDrive", 0, slotX (1), kBoostY, "DRIVE", "0", "10", {}, {} },
                { "boostLevel", 0, slotX (2), kBoostY, "LEVEL", "-12", "+12", {}, {} },    { "boostTight", 0, slotX (3), kBoostY, "TIGHT", "40", "900", {}, {} },
                { "gain", 1, slotX (0), kRow1, "GAIN", "0", "10", {}, {} },              { "bass", 1, slotX (1), kRow1, "BASS", "0", "10", {}, {} },
                { "middle", 1, slotX (2), kRow1, "MIDDLE", "0", "10", {}, {} },          { "treble", 1, slotX (3), kRow1, "TREBLE", "0", "10", {}, {} },
                { "presence", 1, slotX (4), kRow1, "PRESENCE", "-6", "+6", {}, {} },     { "resonance", 1, slotX (0), kRow2, "RESONANCE", "-6", "+6", {}, {} },
                { "sag", 1, slotX (1), kRow2, "SAG", "0", "10", {}, {} },                { "master", 1, slotX (2), kRow2, "MASTER", "-24", "+6", {}, {} },
                { "irALevel", 2, 100.0f, 300.0f, "IR A LEVEL", "0", "10", {}, {} },      { "irBLevel", 2, 220.0f, 300.0f, "IR B LEVEL", "0", "10", {}, {} },
                { "lowCut", 2, 340.0f, 300.0f, "LOW CUT", "40", "400", {}, {} },         { "highCut", 2, 460.0f, 300.0f, "HIGH CUT", "3k", "16k", {}, {} },
                { "mic", 2, 580.0f, 300.0f, "MIC", "0", "10", {}, {} },                  { "ambience", 2, 700.0f, 300.0f, "AMBIENCE", "0", "10", {}, {} },
                { "eqLow", 3, slotX (1), kGateY, "LOW", "-12", "+12", {}, {} },           { "eqMid", 3, slotX (2), kGateY, "MID", "-12", "+12", {}, {} },
                { "eqFreq", 3, slotX (3), kGateY, "MID FREQ", "200", "3k", {}, {} },      { "eqHigh", 3, slotX (4), kGateY, "HIGH", "-12", "+12", {}, {} },
                { "out", 3, slotX (1), kBoostY, "OUTPUT", "-inf", "+6", {}, {} } } },
      switches { { { "gateOn", 0, juce::Rectangle<float> (104.0f, 30.0f).withCentre ({ slotX (0), kGateY + 22.0f }), "GATE", {} },
                   { "boostOn", 0, juce::Rectangle<float> (104.0f, 30.0f).withCentre ({ slotX (0), kBoostY }), "BOOST", {} },
                   { "cabOn", 2, { 34.0f, 36.0f, 130.0f, 30.0f }, "CABINET ON", {} },
                   { "eqOn", 3, juce::Rectangle<float> (104.0f, 30.0f).withCentre ({ slotX (0), kGateY }), "EQ", {} },
                   { "limiter", 3, juce::Rectangle<float> (104.0f, 30.0f).withCentre ({ slotX (0), kBoostY + 22.0f }), "LIMITER", {} } } },
      powerAtt (*p.apvts.getParameter ("power"), [this] (float) { repaint(); }, nullptr)
{
    setLookAndFeel (&laf);
    for (auto& k : knobs)
    {
        ui::setupKnob (k.slider, *this);
        auto* par = proc.apvts.getParameter (k.id);
        k.slider.setDoubleClickReturnValue (true, par->convertFrom0to1 (par->getDefaultValue()));
        k.slider.onDragStart = [this] { proc.undoManager.beginNewTransaction(); };
        addAndMakeVisible (k.slider);
        k.att = std::make_unique<juce::SliderParameterAttachment> (*dynamic_cast<juce::RangedAudioParameter*> (par), k.slider, nullptr);
        k.slider.onValueChange = [this] { repaint(); };
    }
    for (auto& s : switches)
    {
        s.target.onPress = [this, id = s.id] { proc.undoManager.beginNewTransaction(); toggle (id); };
        addAndMakeVisible (s.target);
    }
    powerAtt.sendInitialUpdate();
    grain = ui::makeGrain();
    constrainer.setSizeLimits (720, 414, 2400, 1380);
    constrainer.setFixedAspectRatio ((double) kW / (double) kH);
    setConstrainer (&constrainer);
    setResizable (true, true);
    setSize ((int) kW, (int) kH);
    layoutKnobs();
    updateDisplay();
    startTimerHz (30);
}

AL1AudioProcessorEditor::~AL1AudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void AL1AudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    if (midiUI != nullptr) midiUI->paintMarks (g, kAcc.body);
}

void AL1AudioProcessorEditor::toggle (const char* id)
{
    auto* par = proc.apvts.getParameter (id);
    par->beginChangeGesture(); par->setValueNotifyingHost (par->getValue() > 0.5f ? 0.0f : 1.0f); par->endChangeGesture();
}
void AL1AudioProcessorEditor::setChoice (const char* id, int index)
{
    auto* par = proc.apvts.getParameter (id);
    par->beginChangeGesture(); par->setValueNotifyingHost (par->convertTo0to1 ((float) index)); par->endChangeGesture();
}

void AL1AudioProcessorEditor::layoutKnobs()
{
    const float sc = scale(), d = 2.0f * (kKnobR + kPad) * sc;
    const int t = tab();
    for (auto& k : knobs)
    {
        k.slider.setBounds (juce::Rectangle<float> (d, d).withCentre ((kControls.getPosition() + juce::Point<float> (k.x, k.y)) * sc).toNearestInt());
        k.slider.setVisible (k.tab == t);
    }
    for (auto& s : switches)
    {
        s.target.setBounds (s.r.translated (kControls.getX(), kControls.getY()).expanded (3.0f).transformedBy (juce::AffineTransform::scale (sc)).toNearestInt());
        s.target.setVisible (s.tab == t);
    }
    if (midiUI == nullptr || midiTab != t)                            // MIDI learn marks only for the controls that are on screen
    {
        midiTab = t;
        midiUI = std::make_unique<MidiLearnUI> (proc.midiMap);
        for (auto& k : knobs) if (k.tab == t) midiUI->add (k.slider, k.id);
        for (auto& s : switches) if (s.tab == t) midiUI->add (s.target, s.id);
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
    const float sc = scale();
    repaint (kDisplay.expanded (4.0f).transformedBy (juce::AffineTransform::scale (sc)).getSmallestIntegerContainer());
    if (tab() == 0 || tab() == 3) repaint (kControls.expanded (4.0f).transformedBy (juce::AffineTransform::scale (sc)).getSmallestIntegerContainer());     // the gate and limiter lamps
    if (barState.copiedAt != 0) { repaint (juce::Rectangle<float> (0.0f, 30.0f, 900.0f, 50.0f).transformedBy (juce::AffineTransform::scale (sc)).getSmallestIntegerContainer()); if (! barState.copiedRecently()) barState.copiedAt = 0; }
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
    for (int c = 0; c < 2; ++c)                                                  // output meter: instant attack, 24 dB / s release, 1.5 s peak hold
    {
        const float pk = eng.outPeakCh[c].exchange (0.0f), db = pk > 1.0e-5f ? juce::Decibels::gainToDecibels (pk) : -90.0f;
        barDb[c] = db > barDb[c] ? db : juce::jmax (db, barDb[c] - 24.0f / 30.0f);
        if (db >= holdDb[c]) { holdDb[c] = db; holdAge[c] = 0.0f; }
        else if ((holdAge[c] += 1.0f / 30.0f) > 1.5f) holdDb[c] = juce::jmax (db, holdDb[c] - 24.0f / 30.0f);
    }

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
void AL1AudioProcessorEditor::drawCurve (juce::Graphics& g)
{
    const bool power = isOn ("power");
    const auto c = curveRect();
    g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (c.expanded (8.0f), 10.0f);
    g.setColour (kAcc.ink.withAlpha (0.14f));
    g.drawLine (c.getX(), c.getCentreY(), c.getRight(), c.getCentreY(), 1.0f); g.drawLine (c.getCentreX(), c.getY(), c.getCentreX(), c.getBottom(), 1.0f);
    g.setColour (kAcc.ink.withAlpha (0.28f)); g.drawLine (c.getX(), c.getBottom(), c.getRight(), c.getY(), 1.0f);              // the straight line: no distortion
    {
        juce::Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (c.expanded (4.0f).toNearestInt());
        g.setColour (kAcc.body); g.strokePath (curvePath, juce::PathStrokeType (2.2f));
        if (power) { g.setColour (kAcc.ink); g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ dotX, dotY })); }
    }
    drawText (g, "IN", c.getRight() - 12.0f, c.getCentreY() + 12.0f, 13.0f, kAcc.ink.withAlpha (0.55f), true, 0.1f);
    drawText (g, "OUT", c.getCentreX() + 20.0f, c.getY() + 10.0f, 13.0f, kAcc.ink.withAlpha (0.55f), true, 0.1f);
    drawText (g, "AMP STAGES, INPUT +-0.1", c.getCentreX(), c.getBottom() + 22.0f, 13.0f, kAcc.ink.withAlpha (0.55f), true, 0.1f);

    const auto w = waveRect();
    g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (w.expanded (8.0f), 10.0f);
    g.setColour (kAcc.ink.withAlpha (0.14f)); g.drawLine (w.getX(), w.getCentreY(), w.getRight(), w.getCentreY(), 1.0f);
    g.setColour (kAcc.ink.withAlpha (0.45f)); g.strokePath (sineInPath, juce::PathStrokeType (1.4f));
    g.setColour (kAcc.body); g.strokePath (sineOutPath, juce::PathStrokeType (2.2f));
    drawText (g, "SINE IN (GREY), OUT SCALED", w.getCentreX(), w.getBottom() + 22.0f, 13.0f, kAcc.ink.withAlpha (0.55f), true, 0.1f);
    drawText (g, juce::String (al1::kTypes[ampType()].name) + "   4X OVERSAMPLED", kDisplay.getCentreX(), kDisplay.getBottom() - 47.0f, 13.0f, kAcc.ink.withAlpha (0.6f), true, 0.1f);
    drawText (g, "LATENCY " + juce::String (latency) + " SAMPLES", kDisplay.getCentreX(), kDisplay.getBottom() - 26.0f, 13.0f, kAcc.ink.withAlpha (0.6f), true, 0.1f);
}

void AL1AudioProcessorEditor::drawCabinet (juce::Graphics& g)
{
    const auto r = graphRect();
    g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (r.expanded (8.0f), 10.0f);
    g.setColour (kAcc.ink.withAlpha (0.14f));
    for (float d = -24.0f; d <= 12.0f; d += 12.0f) g.drawLine (r.getX(), dbToY (r, d), r.getRight(), dbToY (r, d), d == 0.0f ? 1.4f : 1.0f);
    for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f }) g.drawLine (freqToX (r, f), r.getY(), freqToX (r, f), r.getBottom(), 1.0f);
    for (float d : { -24.0f, -12.0f, 0.0f, 12.0f }) drawText (g, juce::String ((int) d), r.getX() - 6.0f, dbToY (r, d), 13.0f, kAcc.ink.withAlpha (0.5f), true, 0.0f, juce::Justification::centredRight);
    for (auto [f, s] : { std::pair<float, const char*> { 100.0f, "100" }, { 1000.0f, "1k" }, { 10000.0f, "10k" } }) drawText (g, s, freqToX (r, f), r.getBottom() + 12.0f, 13.0f, kAcc.ink.withAlpha (0.5f), true, 0.0f);
    {
        juce::Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (r.expanded (2.0f).toNearestInt());
        if (! irPresent[0] && ! irPresent[1]) { g.setColour (kAcc.ink.withAlpha (0.3f)); g.strokePath (modelPath, juce::PathStrokeType (1.2f)); }
        g.setColour (juce::Colour (120, 190, 255).withAlpha (0.65f)); if (irPresent[0]) g.strokePath (irPath[0], juce::PathStrokeType (1.2f));
        g.setColour (juce::Colour (150, 230, 140).withAlpha (0.65f)); if (irPresent[1]) g.strokePath (irPath[1], juce::PathStrokeType (1.2f));
        g.setColour (kAcc.body); g.strokePath (totalPath, juce::PathStrokeType (2.2f));
    }
    const float y0 = r.getBottom() + 40.0f;
    g.setColour (kAcc.body); g.fillRect (juce::Rectangle<float> (14.0f, 3.0f).withCentre ({ kDisplay.getX() + 40.0f, y0 }));
    drawText (g, isOn ("cabOn") ? "CABINET, CUTS AND EQ" : "CABINET OFF", kDisplay.getX() + 54.0f, y0, 13.0f, kAcc.ink.withAlpha (0.8f), true, 0.1f, juce::Justification::centredLeft);
    g.setColour (juce::Colour (120, 190, 255)); g.fillRect (juce::Rectangle<float> (14.0f, 3.0f).withCentre ({ kDisplay.getX() + 40.0f, y0 + 24.0f }));
    drawText (g, "IR A  " + (irPresent[0] ? irNames[0].toUpperCase().substring (0, 26) : juce::String ("NONE")), kDisplay.getX() + 54.0f, y0 + 24.0f, 13.0f, kAcc.ink.withAlpha (0.8f), true, 0.1f, juce::Justification::centredLeft);
    g.setColour (juce::Colour (150, 230, 140)); g.fillRect (juce::Rectangle<float> (14.0f, 3.0f).withCentre ({ kDisplay.getX() + 40.0f, y0 + 48.0f }));
    drawText (g, "IR B  " + (irPresent[1] ? irNames[1].toUpperCase().substring (0, 26) : juce::String ("NONE")), kDisplay.getX() + 54.0f, y0 + 48.0f, 13.0f, kAcc.ink.withAlpha (0.8f), true, 0.1f, juce::Justification::centredLeft);
    drawText (g, irPresent[0] || irPresent[1] ? "IMPULSE RESPONSES" : "FILTER MODEL (NO IR LOADED)", kDisplay.getCentreX(), kDisplay.getBottom() - 26.0f, 13.0f, kAcc.ink.withAlpha (0.6f), true, 0.1f);
}

void AL1AudioProcessorEditor::drawMeters (juce::Graphics& g)
{
    constexpr float lo = -48.0f;
    const bool power = isOn ("power");
    const juce::Colour red (0xffe5432c);
    const float top = kDisplay.getY() + 62.0f, h = 355.0f;       // equal padding above the L / R labels and below the peak values (to the caption)
    const auto bar = [&] (int ch) { return juce::Rectangle<float> (kDisplay.getCentreX() - 32.0f + (float) ch * 40.0f, top, 24.0f, h); };
    const auto yOf = [&] (const juce::Rectangle<float>& b, float db) { return b.getBottom() - b.getHeight() * juce::jlimit (0.0f, 1.0f, (db - lo) / -lo); };
    for (int ch = 0; ch < 2; ++ch)
    {
        const auto b = bar (ch);
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (b, 2.0f);
        const float db = power ? barDb[ch] : -90.0f;
        if (db > lo)
        {
            g.setColour (db > -1.0f ? red : kAcc.body);
            g.fillRect (b.withTop (yOf (b, db)).reduced (2.0f, 0.0f).withBottom (b.getBottom() - 2.0f));
        }
        if (power && holdDb[ch] > lo) { g.setColour (holdDb[ch] > -1.0f ? red : kAcc.ink.withAlpha (0.9f)); g.fillRect (b.getX() + 1.0f, yOf (b, holdDb[ch]) - 1.0f, b.getWidth() - 2.0f, 2.0f); }
        drawText (g, ch == 0 ? "L" : "R", b.getCentreX(), b.getY() - 14.0f, 13.0f, kAcc.ink.withAlpha (0.85f), true, 0.1f);
        drawText (g, power && holdDb[ch] > -60.0f ? juce::String (holdDb[ch], 1) : juce::String ("-inf"), b.getCentreX(), b.getBottom() + 20.0f, 13.0f, power && holdDb[ch] > -1.0f ? red : kAcc.body, true, 0.02f);
    }
    const auto b1 = bar (1);
    for (const float t : { 0.0f, -6.0f, -12.0f, -24.0f, -48.0f })
        drawText (g, t == 0.0f ? "0" : juce::String ((int) t), b1.getRight() + 20.0f, yOf (b1, t), 13.0f, kAcc.ink.withAlpha (0.7f), false, 0.0f);
    drawText (g, "dB", b1.getRight() + 20.0f, b1.getY() - 19.0f, 13.0f, kAcc.ink.withAlpha (0.7f), false, 0.0f);

    const auto lamp = juce::Rectangle<float> (14.0f, 14.0f).withCentre ({ kDisplay.getX() + 56.0f, top + 12.0f });
    const bool gateOn = gateLamp >= 0.0f, open = gateLamp > 0.5f;
    g.setColour (open && power ? kAcc.body : kAcc.ink.withAlpha (0.18f)); g.fillEllipse (lamp);
    g.setColour (kAcc.ink.withAlpha (0.35f)); g.drawEllipse (lamp, 1.0f);
    drawText (g, "GATE", lamp.getCentreX(), lamp.getBottom() + 16.0f, 13.0f, kAcc.ink.withAlpha (0.8f), true, 0.1f);
    drawText (g, ! gateOn || ! power ? "OFF" : open ? "OPEN" : "CLOSED", lamp.getCentreX(), lamp.getBottom() + 34.0f, 13.0f, kAcc.body, true, 0.06f);
    drawText (g, "LIMITER", lamp.getCentreX(), top + 100.0f, 13.0f, kAcc.ink.withAlpha (0.8f), true, 0.1f);
    drawText (g, isOn ("limiter") ? (power && grDb > 0.05f ? juce::String::formatted ("-%.1f dB", (double) grDb) : juce::String ("0.0 dB")) : juce::String ("OFF"), lamp.getCentreX(), top + 120.0f, 13.0f, kAcc.body, true, 0.06f);
    drawText (g, "OUTPUT, PEAK, -48 TO 0 DB", kDisplay.getCentreX(), kDisplay.getBottom() - 26.0f, 13.0f, kAcc.ink.withAlpha (0.6f), true, 0.1f);
}

//==============================================================================
void AL1AudioProcessorEditor::paintSwitches (juce::Graphics& g)
{
    for (const auto& s : switches)
        if (s.tab == tab()) ui::paintFlatButton (g, kAcc, s.r.translated (kControls.getX(), kControls.getY()), s.label, isOn (s.id), 13.0f);
}

void AL1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    ui::paintBody (g, kPal, kW, kH, grain);
    ui::paintHeader (g, kPal, "AL-1", "AMP LAB", "RAW POWER", isOn ("power"));
    ui::paintPresetBar (g, kPal, proc.presets, proc.undoManager, kBarY, barState, false, 6.0f);

    const int t = tab(), dt = displayTab();
    ui::paintPanel (g, kAcc, kControls, {});
    ui::paintTabs (g, kAcc, kControls, kTabs, t, 13.0f);
    ui::paintPanel (g, kAcc, kDisplay, {});
    ui::paintTabs (g, kAcc, kDisplay, kDisplayTabs, dt, 13.0f);
    if (dt == 0) drawCurve (g); else if (dt == 1) drawCabinet (g); else drawMeters (g);

    const auto origin = kControls.getPosition();
    paintSwitches (g);
    if (t == 0)
    {
        const bool gateOn = gateLamp >= 0.0f, open = gateLamp > 0.5f && isOn ("power");
        const auto lamp = juce::Rectangle<float> (14.0f, 14.0f).withCentre (origin + juce::Point<float> (slotX (0), kGateY - 10.0f));
        g.setColour (open && gateOn ? kAcc.body : kAcc.ink.withAlpha (0.18f)); g.fillEllipse (lamp);
        g.setColour (kAcc.ink.withAlpha (0.35f)); g.drawEllipse (lamp, 1.0f);
        drawText (g, ! gateOn ? "GATE OFF" : open ? "GATE OPEN" : "GATE CLOSED", lamp.getCentreX(), origin.y + kGateY - 36.0f, 13.0f, kAcc.ink.withAlpha (0.85f), true, 0.1f);
        drawText (g, "NOISE GATE", origin.x + slotX (0), origin.y + kGateY - 72.0f, 13.0f, kAcc.ink.withAlpha (0.45f), true, 0.14f);
        divider (g, origin, true, 262.0f, 0.5f * (slotX (3) + slotX (4)) - 34.0f); divider (g, origin, false, 0.5f * (slotX (3) + slotX (4)));
        drawText (g, "INPUT TRIM", origin.x + slotX (4), origin.y + kGateY - 72.0f, 13.0f, kAcc.ink.withAlpha (0.45f), true, 0.14f);
        drawText (g, "TIGHT BOOST", origin.x + slotX (0), origin.y + kBoostY - 44.0f, 13.0f, kAcc.ink.withAlpha (0.45f), true, 0.14f);
    }
    else if (t == 1)
    {
        for (int i = 0; i < al1::kNumTypes; ++i) ui::paintFlatButton (g, kAcc, typeButton (i), al1::kTypes[i].name, i == ampType(), 13.0f);
        ui::paintFlatButton (g, kAcc, brightRect(), "BRIGHT", isOn ("bright"), 13.0f);
        ui::paintFlatButton (g, kAcc, deepRect(), "DEEP", isOn ("deep"), 13.0f);
        drawText (g, "PREAMP AND TONE STACK", origin.x + slotX (2), origin.y + 138.0f, 13.0f, kAcc.ink.withAlpha (0.45f), true, 0.14f);
        drawText (g, "POWER AMP", origin.x + slotX (1), origin.y + kRow2 - 44.0f, 13.0f, kAcc.ink.withAlpha (0.45f), true, 0.14f);
        drawText (g, "VOICING", origin.x + 0.5f * (slotX (3) + slotX (4)), origin.y + kRow2 - 44.0f, 13.0f, kAcc.ink.withAlpha (0.45f), true, 0.14f);
    }
    else if (t == 2)
    {
        for (int s = 0; s < 2; ++s)
        {
            const bool has = proc.hasImpulse (s);
            drawText (g, s == 0 ? "IR A" : "IR B", loadRect (s).getX(), loadRect (s).getY() - 12.0f, 13.0f, kAcc.ink.withAlpha (0.5f), true, 0.14f, juce::Justification::centredLeft);
            ui::paintFlatButton (g, kAcc, loadRect (s), "LOAD  v", false, 13.0f);
            const auto n = nameRect (s);
            g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (n, 8.0f);
            g.setColour (kAcc.ink.withAlpha (has ? 0.5f : 0.2f)); g.drawRoundedRectangle (n.reduced (0.5f), 8.0f, 1.0f);
            g.setColour (has ? kAcc.body : kAcc.ink.withAlpha (0.4f));
            g.setFont (ui::font (13.0f, true));
            g.drawText (has ? proc.getImpulseName (s) : juce::String ("NO IR LOADED"), n.reduced (12.0f, 0.0f), juce::Justification::centredLeft, true);
            ui::paintFlatButton (g, kAcc, clearRect (s), "CLEAR", false, 13.0f, has ? 1.0f : 0.4f);
        }
        drawText (g, "IR LEVELS", origin.x + 160.0f, origin.y + 240.0f, 13.0f, kAcc.ink.withAlpha (0.45f), true, 0.14f);
        drawText (g, "CABINET FILTERS", origin.x + 400.0f, origin.y + 240.0f, 13.0f, kAcc.ink.withAlpha (0.45f), true, 0.14f);
        drawText (g, "MIC AND ROOM", origin.x + 640.0f, origin.y + 240.0f, 13.0f, kAcc.ink.withAlpha (0.45f), true, 0.14f);
        drawText (g, "NO IR LOADED: A 4X12 FILTER MODEL IS USED, MIC MOVES ITS TOP END.", origin.x + 380.0f, origin.y + 424.0f, 13.0f, kAcc.ink.withAlpha (0.75f), true, 0.04f);
        drawText (g, "IR FILES: DOCUMENTS / VIBR / AL-1 / IRS, OR BROWSE.", origin.x + 380.0f, origin.y + 444.0f, 13.0f, kAcc.ink.withAlpha (0.75f), true, 0.04f);
        drawText (g, "THEY ARE SAVED IN THE SESSION, NOT IN THE PRESETS.", origin.x + 380.0f, origin.y + 464.0f, 13.0f, kAcc.ink.withAlpha (0.75f), true, 0.04f);
    }
    else
    {
        const bool lim = grDb > 0.2f && isOn ("limiter") && isOn ("power");
        drawText (g, "EQUALISER", origin.x + slotX (0), origin.y + kGateY - 72.0f, 13.0f, kAcc.ink.withAlpha (0.45f), true, 0.14f);
        divider (g, origin, true, 262.0f);
        drawText (g, "LIMITER AND OUTPUT", origin.x + slotX (0), origin.y + kBoostY - 44.0f, 13.0f, kAcc.ink.withAlpha (0.45f), true, 0.14f);
        const auto lamp = juce::Rectangle<float> (14.0f, 14.0f).withCentre (origin + juce::Point<float> (slotX (0), kBoostY - 10.0f));
        g.setColour (lim ? kAcc.body : kAcc.ink.withAlpha (0.18f)); g.fillEllipse (lamp);
        g.setColour (kAcc.ink.withAlpha (0.35f)); g.drawEllipse (lamp, 1.0f);
    }

    for (auto& k : knobs)
    {
        if (k.tab != t) continue;
        const auto c = origin + juce::Point<float> (k.x, k.y);
        ui::paintKnob (g, kAcc, c, k.name, knobText (k.id, (float) k.slider.getValue()), k.lo, k.hi, c.y + 62.0f, c.y + 90.0f);
        ui::paintValueArc (g, c, kKnobR, (float) k.slider.valueToProportionOfLength (k.slider.getValue()), kAcc.body);
    }
    ui::paintFooter (g, kPal, kPanelY + kPanelH + 35.0f);
}

void AL1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / scale();
    proc.undoManager.beginNewTransaction();
    if (ui::powerRect().contains (p)) { toggle ("power"); return; }
    if (ui::presetBarMouseDown (p, *this, proc.presets, proc.undoManager, kBarY, scale(), barState, [this] { repaint(); }, false)) { layoutKnobs(); return; }
    const auto tabs = ui::tabRects (kControls, kTabs, 13.0f);
    for (int i = 0; i < (int) tabs.size(); ++i)
        if (tabs[(size_t) i].expanded (3.0f).contains (p)) { proc.uiTab.store (i); layoutKnobs(); repaint(); return; }
    const auto dtabs = ui::tabRects (kDisplay, kDisplayTabs, 13.0f);
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
