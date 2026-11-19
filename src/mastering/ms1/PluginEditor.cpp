// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"
#include "VibrLogo.h"
#include "ui/VintageUI.h"

#include "EditorLayout.h"

using namespace ms1ui;

//==============================================================================
MS1AudioProcessorEditor::MS1AudioProcessorEditor (MS1AudioProcessor& p)
    : LabEditor (p, p.apvts, p.undoManager, kW, kDesignH), proc (p),
      knobs { { { "inGain", 0, 0, 1, "TRIM", 0, {}, {} },
                { "limGain", 1, 0, 6, "GAIN", 1, {}, {} },       { "limCeiling", 1, 1, 6, "CEILING", 2, {}, {} },  { "limRelease", 1, 2, 6, "RELEASE", 3, {}, {} },
                { "limLook", 1, 3, 6, "LOOKAHEAD", 4, {}, {} },  { "limLink", 1, 4, 6, "LINK", 5, {}, {} },        { "limTarget", 1, 5, 6, "TARGET", 6, {}, {} },
                { "outGain", 2, 0, 2, "GAIN", 0, {}, {} },       { "ditherBits", 2, 1, 2, "DITHER", 7, {}, {} },
                { "busThresh", 5, 0, 6, "THRESHOLD", 8, {}, {}, 3 }, { "busRatio", 5, 1, 6, "RATIO", 9, {}, {}, 3 },    { "busAttack", 5, 2, 6, "ATTACK", 10, {}, {}, 3 },
                { "busRelease", 5, 3, 6, "RELEASE", 11, {}, {}, 3 }, { "busMakeup", 5, 4, 6, "MAKE-UP", 12, {}, {}, 3 }, { "busMix", 5, 5, 6, "MIX", 5, {}, {}, 3 },
                { "Thresh", 5, 0, 6, "THRESHOLD", 13, {}, {}, 0, true }, { "Ratio", 5, 1, 6, "RATIO", 14, {}, {}, 0, true },    { "Attack", 5, 2, 6, "ATTACK", 15, {}, {}, 0, true },
                { "Release", 5, 3, 6, "RELEASE", 16, {}, {}, 0, true },   { "Make", 5, 4, 6, "MAKE-UP", 17, {}, {}, 0, true },   { "mbMix", 5, 5, 6, "MIX", 5, {}, {}, 0 },
                { "width1", 5, 0, 5, "WIDTH LOW", 18, {}, {}, 1 },        { "width2", 5, 1, 5, "WIDTH MID", 18, {}, {}, 1 },    { "width3", 5, 2, 5, "WIDTH HIGH", 18, {}, {}, 1 },
                { "monoBelow", 5, 3, 5, "MONO BELOW", 19, {}, {}, 1 },     { "imgBalance", 5, 4, 5, "BALANCE", 20, {}, {}, 1 },
                { "colourDrive", 5, 0, 4, "DRIVE", 5, {}, {}, 2 },        { "colourTilt", 5, 1, 4, "TONE", 20, {}, {}, 2 },     { "colourMix", 5, 2, 4, "MIX", 5, {}, {}, 2 },
                { "colourOut", 5, 3, 4, "OUTPUT", 17, {}, {}, 2 } } },
      autoAtt   (*p.apvts.getParameter ("limAuto"),     [this] (float) { repaint(); }, nullptr),
      deltaAtt  (*p.apvts.getParameter ("delta"),       [this] (float) { repaint(); }, nullptr),
      charAtt   (*p.apvts.getParameter ("limChar"),     [this] (float) { repaint(); }, nullptr),
      shapeAtt  (*p.apvts.getParameter ("ditherShape"), [this] (float) { repaint(); }, nullptr),
      targetAtt (*p.apvts.getParameter ("target"),      [this] (float) { repaint(); }, nullptr)
{
    for (auto& k : knobs)
    {
        auto& s = k.slider;
        vibr::ui::setupKnob (s, *this);
        s.onDragStart = [this] { proc.undoManager.beginNewTransaction(); };
        addAndMakeVisible (s);
        bindKnob (k);
        s.onValueChange = [this] { repaint(); };
    }
    autoAtt.sendInitialUpdate(); deltaAtt.sendInitialUpdate(); charAtt.sendInitialUpdate(); shapeAtt.sendInitialUpdate(); targetAtt.sendInitialUpdate();

    constrainer.setSizeLimits (720, 300, 2400, 2400);
    constrainer.setFixedAspectRatio ((double) kW / (double) designH());
    setWantsKeyboardFocus (true);
    holdPeak.fill (-120.0f);
    setTab (0);
    startTimerHz (30);
}

MS1AudioProcessorEditor::~MS1AudioProcessorEditor() = default;
void MS1AudioProcessorEditor::timerCallback()
{
    if (autoOpen) { autoRefresh(); repaint (autoRect().expanded (4.0f).transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer()); }
    if (autoIsLearning != proc.autoAn.learning.load()) { autoIsLearning = ! autoIsLearning; repaint (autoButtonRect().expanded (4.0f).transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer()); }
    meter.update (proc.outMeter);                                                    // the OUTPUT L / R dB meters
    if (proc.overLatch.exchange (false)) overLitUntil = juce::Time::getMillisecondCounter() + 1500;     // each overshoot lights the OVER lamp for 1.5 s
    repaint (juce::Rectangle<float> (0.0f, kRow1Y - 6.0f, kW, designH() - kRow1Y + 6.0f).transformedBy (juce::AffineTransform::scale (scale())).getSmallestIntegerContainer());     // the meters, history and displays move
    if (matchNoteTicks > 0) --matchNoteTicks;
    grHist[(size_t) grPos] = proc.busGrDb(); limHist[(size_t) grPos] = -std::abs (proc.grDb.load()); grPos = (grPos + 1) % (int) grHist.size();
}

void MS1AudioProcessorEditor::setFloat (const char* id, float v)
{
    auto* par = proc.apvts.getParameter (id);
    par->beginChangeGesture();
    par->setValueNotifyingHost (par->convertTo0to1 (v));
    par->endChangeGesture();
}

void MS1AudioProcessorEditor::applyTarget (int index)
{
    const auto& t = ms1::target (index);
    setChoice ("target", index);
    if (index != ms1::kNumTargets - 1)          // CUSTOM keeps the current values
    {
        setFloat ("limTarget", t.lufs);
        setFloat ("limCeiling", t.ceiling);
        setChoice ("limChar", t.character);
    }
}

void MS1AudioProcessorEditor::showTargetMenu()
{
    static vibr::ui::PresetMenuLook look;                                          // the same dark menu as the PRESET dropdown
    juce::PopupMenu menu;
    menu.setLookAndFeel (&look);
    for (int i = 0; i < ms1::kNumTargets; ++i)
    {
        const auto& t = ms1::target (i);
        menu.addItem (i + 1, juce::String (t.name) + (i < ms1::kNumTargets - 1 ? juce::String::formatted ("   %.0f LUFS   %.1f dBTP", t.lufs, t.ceiling) : juce::String()), true, i == choice ("target"));
    }
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (targetChipRect().transformedBy (juce::AffineTransform::scale (scale())).toNearestInt().translated (getScreenX(), getScreenY())),
                        [this] (int result) { if (result > 0) applyTarget (result - 1); });
}

void MS1AudioProcessorEditor::bindKnob (Knob& k)          // (re)connects a knob to its parameter (the multiband knobs follow the selected band)
{
    auto* par = proc.apvts.getParameter (knobParam (k));
    k.att.reset();
    k.slider.setDoubleClickReturnValue (true, par->convertFrom0to1 (par->getDefaultValue()));
    k.att = std::make_unique<juce::SliderParameterAttachment> (*dynamic_cast<juce::RangedAudioParameter*> (par), k.slider, nullptr);
}

void MS1AudioProcessorEditor::setStep (int s)
{
    step = juce::jlimit (0, kChainStages - 1, s);
    if (step >= 1 && step <= 4) tab = step - 1;
    layoutKnobs();
    repaint();
}

void MS1AudioProcessorEditor::setRefOpen (bool open)
{
    if (refOpen == open) return;
    refOpen = open;
    constrainer.setFixedAspectRatio ((double) kW / (double) designH());
    setSize (getWidth(), juce::roundToInt ((float) getWidth() * designH() / kW));
    repaint();
}

void MS1AudioProcessorEditor::setTab (int t)
{
    setStep (juce::jlimit (0, 3, t) + 1);
}

void MS1AudioProcessorEditor::selectMbBand (int b)
{
    selMb = juce::jlimit (0, 3, b);
    for (auto& k : knobs) if (k.perBand) bindKnob (k);
    repaint();
}

void MS1AudioProcessorEditor::layoutKnobs()
{
    for (auto& k : knobs)
    {
        placeKnob (k.slider, knobCentre (k.panel, k.index, k.count));
        k.slider.setVisible (knobVisible (k));
    }
}

void MS1AudioProcessorEditor::resized() { layoutKnobs(); }


//==============================================================================
void MS1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));

    ui::paintBody (g, kPal, kW, designH());
    ui::paintHeader (g, kPal, "MS-1", "MASTERING SUITE", "AUDIO PERFECTION");
    ui::paintPresetBar (g, kPal, proc.presets, proc.undoManager, isOn ("power"));
    {   // the target chip: a phosphor screen under the preset screen, it opens the target menu
        const auto chip = targetChipRect();
        ui::paintScreen (g, kPal, chip, 5.0f);
        const auto& t = ms1::target (choice ("target"));
        drawText (g, "TARGET", chip.getX() + 14.0f, chip.getCentreY(), 13.0f, kPal.phosphorDim, false, 0.1f, juce::Justification::centredLeft, ui::Face::Mono);
        drawText (g, t.name, chip.getX() + 82.0f, chip.getCentreY(), 13.0f, kPal.phosphor, true, 0.04f, juce::Justification::centredLeft, ui::Face::Mono);
        drawText (g, juce::String::formatted ("%.1f LUFS  %.1f dBTP", value ("limTarget"), value ("limCeiling")), chip.getRight() - 14.0f, chip.getCentreY(), 13.0f, kPal.phosphor, false, 0.0f,
                  juce::Justification::centredRight, ui::Face::Mono);
    }

    static const char* displayTitle[4] = { "BAND REDUCTION", "STEREO FIELD", "TRANSFER CURVE", "GAIN REDUCTION" };
    for (int i = 0; i < 8; ++i)
        if (panelShown (i))
            ui::paintPanel (g, kPal, panelRect (i), i == 5 ? juce::String (kTabName[tab]) : i == 6 ? juce::String (step == 5 ? "GAIN REDUCTION" : displayTitle[tab]) : juce::String (kPanelName[i]));
    if (step == 4 && ! autoOpen)
    {   // EXT lamp on the top edge: lit while the host's sidechain input is the key of the bus compressor
        const auto r6 = panelRect (6);
        const bool ext = proc.extKey.load (std::memory_order_relaxed);
        const auto patch = juce::Rectangle<float> (r6.getRight() - 16.0f - 62.0f, r6.getY() - 11.0f, 62.0f, 24.0f);
        g.setColour (kPal.paper);
        g.fillRect (patch.expanded (4.0f, 0.0f));
        paintLamp (g, { patch.getX() + 7.0f, r6.getY() + 1.0f }, ext, kPal.segGreen, kPal.segOff);
        drawText (g, "EXT", patch.getX() + 22.0f, r6.getY() + 1.0f, 16.0f, kPal.ink, true, 0.1f, juce::Justification::centredLeft);
    }
    drawChain (g);
    if (step == 0 && ! autoOpen) drawSpectrum (g);
    drawLoudness (g);
    drawOutputMeters (g);
    if (autoOpen) drawAuto (g); else if (step != 0) drawStageDisplay (g);
    if (refOpen) drawReference (g);
    {   // RESET: integrated loudness, range and the true-peak hold start again (under the screens of LOUDNESS)
        ui::paintFlatButton (g, kPal, resetRect(), "RESET", false, 16.0f, 1.0f, 4.0f, 0.08f);
    }

    // the buttons of the LIMITER panel and of OUTPUT
    const int ch = choice ("limChar"), shape = choice ("ditherShape");
    const bool autoOn = isOn ("limAuto");
    const bool btnOn[8] = { ch == 0, ch == 1, ch == 2, autoOn, isOn ("delta"), shape != 0, isOn ("limRelAuto"), isOn ("limFreeze") };
    const juce::String label[8] = { "TRANSPARENT", "PUNCHY", "LOUD", "AUTO TO TARGET", "DELTA", shape == 0 ? juce::String ("SHAPE: FLAT") : shape == 1 ? juce::String ("SHAPE: GENTLE") : juce::String ("SHAPE: AGGR."), "AUTO REL.", "FREEZE GAIN" };
    for (int i = 0; i < 8; ++i)
    {
        if ((i < 4 || i >= 6) && (step != 5 || autoOpen)) continue;                                // the limiter buttons belong to the LIMITER step
        ui::paintFlatButton (g, kPal, buttonRect (i), label[i], btnOn[i], 14.0f, 1.0f, 4.0f, 0.04f);
        if (i == 5) paintCycleDots (g, buttonRect (i), 3, shape, btnOn[i]);                    // the dither shape: flat, gentle, aggressive
    }

    {   // the INPUT panel buttons
        const float hpf = value ("trimHpf");
        const juce::String lab[3] = { "PHASE", hpf < 1.0f ? juce::String ("HPF") : "HPF " + juce::String ((int) std::lround (std::max (10.0f, hpf))), "MONO" };
        const bool on[3] = { isOn ("trimPolarity"), hpf >= 1.0f, isOn ("monoCheck") };
        for (int i = 0; i < 3; ++i)
        {
            ui::paintFlatButton (g, kPal, inputButtonRect (i), i == 0 && on[0] ? "INVERT" : lab[i], on[i], 14.0f, 1.0f, 4.0f, 0.02f);
            if (i == 0) paintCycleDots (g, inputButtonRect (i), 2, on[0] ? 1 : 0, on[0]);                                    // PHASE / INVERT
            else if (i == 1) paintCycleDots (g, inputButtonRect (i), 5, hpf < 1.0f ? 0 : hpf < 25.0f ? 1 : hpf < 35.0f ? 2 : hpf < 50.0f ? 3 : 4, on[1]);   // HPF: off, 20, 30, 40, 60 Hz
        }
    }
    if (! autoOpen && step >= 1 && step <= 4)
    {   // the buttons of the stage tab
        const auto sb = stageButtons();
        for (int i = 0; i < (int) sb.size(); ++i)
        {
            ui::paintFlatButton (g, kPal, stageButtonRect (i, (int) sb.size()), sb[(size_t) i].label, sb[(size_t) i].on, 14.0f, 1.0f, 4.0f, 0.04f);
            if (sb[(size_t) i].steps > 0) paintCycleDots (g, stageButtonRect (i, (int) sb.size()), sb[(size_t) i].steps, sb[(size_t) i].cur, sb[(size_t) i].on);
        }
    }

    // knobs: value ring, legends, name and value
    for (auto& k : knobs)
    {
        if (! knobVisible (k)) continue;
        const auto c = knobCentre (k.panel, k.index, k.count);
        static const char* lo[21] = { "-24", "0", "-3", "10", "0.5", "0", "-24", "off", "-30", "1.5", ".1", ".1", "0", "-60", "1", ".1", "10", "-12", "0", "off", "-6" },
                         * hi[21] = { "+24", "+24", "0", "1k", "5", "100", "-6", "24", "0", "10", "30", "AUTO", "+12", "0", "20", "200", "2k", "+12", "200", "250", "+6" };
        const bool outPanel = k.panel == 2;
        const char* loText = outPanel && k.kind == 0 ? "-12" : lo[k.kind], * hiText = outPanel && k.kind == 0 ? "+6" : hi[k.kind];

        const double v = k.slider.getValue();
        juce::String txt;
        switch (k.kind)
        {
            case 0: txt = juce::String::formatted ("%+.1f dB", v); break;
            case 1: txt = autoOn ? juce::String::formatted ("AUTO %+.1f dB", (double) proc.autoGainDb.load()) : juce::String::formatted ("+%.1f dB", v); break;
            case 2: txt = juce::String::formatted ("%.1f dBTP", v); break;
            case 3: txt = juce::String ((int) std::lround (v)) + " ms"; break;
            case 4: txt = juce::String (v, 1) + " ms"; break;
            case 5: txt = juce::String ((int) std::lround (v)) + " %"; break;
            case 6: txt = juce::String::formatted ("%.1f LUFS", v); break;
            case 7: { static const char* d[4] = { "OFF", "16 BIT", "20 BIT", "24 BIT" }; txt = d[juce::jlimit (0, 3, (int) std::lround (v))]; break; }
            case 8: txt = juce::String::formatted ("%.1f dB", v); break;
            case 9: txt = juce::String (ms1::busRatioValue ((int) std::lround (v)), 1) + " : 1"; break;
            case 10: { const float a = ms1::busAttackMs ((int) std::lround (v)); txt = juce::String (a, a < 1.0f ? 1 : 0) + " ms"; break; }
            case 11: { const float rl = ms1::busReleaseMs ((int) std::lround (v)); txt = rl <= 0.0f ? juce::String ("AUTO") : juce::String (rl * 0.001f, 1) + " s"; break; }
            case 12: txt = juce::String::formatted ("+%.1f dB", v); break;
            case 13: txt = juce::String::formatted ("%.1f dB", v); break;
            case 14: txt = juce::String (v, 1) + " : 1"; break;
            case 15: txt = v < 10.0 ? juce::String (v, 1) + " ms" : juce::String ((int) std::lround (v)) + " ms"; break;
            case 16: txt = isOn (("mb" + juce::String (selMb + 1) + "Auto").toRawUTF8()) ? juce::String ("AUTO") : v >= 1000.0 ? juce::String (v * 0.001, 2) + " s" : juce::String ((int) std::lround (v)) + " ms"; break;
            case 17: case 20: txt = juce::String::formatted ("%+.1f dB", v); break;
            case 18: txt = juce::String ((int) std::lround (v)) + " %"; break;
            default: txt = v < 1.0 ? juce::String ("OFF") : juce::String ((int) std::lround (std::max (20.0, v))) + " Hz"; break;      // 19: mono below
        }
        ui::paintKnob (g, kPal, c, k.name, txt, loText, hiText, (float) k.slider.valueToProportionOfLength (k.slider.getValue()));
    }

    ui::paintFooter (g, kPal, designH() - 15.0f);
}

void MS1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    const auto p = e.position / scale();
    proc.undoManager.beginNewTransaction();                                                 // every click or drag is one undo step
    if (ui::powerRect().contains (p)) { toggle ("power"); return; }
    if (targetChipRect().contains (p)) { showTargetMenu(); return; }
    if (autoButtonRect().contains (p)) { setAutoOpen (! autoOpen); return; }
    if (autoOpen && autoMouseDown (p)) return;
    if (ui::presetBarMouseDown (p, *this, proc.presets, proc.undoManager, scale(), [this] { repaint(); })) return;
    const bool eqShown = step == 0 && ! autoOpen;
    for (int i = 0; i < 5 && eqShown; ++i)                                                  // the spectrum options
        if (specButtonRect (i).expanded (2.0f).contains (p))
        {
            using P = MS1AudioProcessor;
            if (i == 0) proc.setSpecOption (P::SpecMode, 1 - proc.specOption (P::SpecMode));
            else if (i == 1) proc.setSpecOption (P::SpecSlope, (proc.specOption (P::SpecSlope) + 1) % 3);
            else if (i == 2) { specHold = ! specHold; holdPeak.fill (-120.0f); }
            else if (i == 3) specFreeze = ! specFreeze;
            else proc.setSpecOption (P::SpecSource, (proc.specOption (P::SpecSource) + 1) % 4);
            repaint();
            return;
        }
    if (resetRect().expanded (3.0f).contains (p)) { proc.requestReset(); overLitUntil = 0; repaint(); return; }
    if (overRect().expanded (3.0f).contains (p)) { proc.overLatch.store (false); overLitUntil = 0; repaint(); return; }

    // the dynamic EQ strip, the EQ strip buttons, then the band nodes
    const juce::String id = "eq" + juce::String (selBand + 1);
    if (eqShown) {
    if (eqPhaseRect().expanded (2.0f).contains (p)) { setChoice ("eqPhase", choice ("eqPhase") == 0 ? 1 : 0); repaint(); return; }
    if (eqDynButtonRect (0).expanded (2.0f).contains (p)) { toggle ((id + "Dyn").toRawUTF8()); repaint(); return; }
    if (eqDynButtonRect (1).expanded (2.0f).contains (p)) { toggle ((id + "DynBelow").toRawUTF8()); repaint(); return; }
    for (int i = 0; i < 4; ++i)
        if (eqDynFieldRect (i).contains (p))
        {
            static const char* suffix[4] = { "DynThr", "DynRatio", "DynAtk", "DynRel" };
            auto* par = proc.apvts.getParameter (id + suffix[i]);
            par->beginChangeGesture();
            dragField = i; dragStartY = p.y; dragStartNorm = par->getValue();
            return;
        }
    for (int i = 0; i < 7; ++i)
        if (eqButtonRect (i).expanded (2.0f).contains (p))
        {
            if (i == 0) toggle ("eqOn");
            else if (i == 1) toggle ((id + "On").toRawUTF8());
            else setChoice ((id + "Mode").toRawUTF8(), i == 2 ? ms1::EqStereo : i == 3 ? ms1::EqLeft : i == 4 ? ms1::EqRight : i == 5 ? ms1::EqMid : ms1::EqSide);
            repaint();
            return;
        }
    if (const int n = nodeAt (p, 16.0f); n >= 0)
    {
        selBand = n; dragBand = n;
        const juce::String nid = "eq" + juce::String (n + 1);
        proc.apvts.getParameter (nid + "Freq")->beginChangeGesture();
        proc.apvts.getParameter (nid + "Gain")->beginChangeGesture();
        repaint();
        return;
    }
    }
    // stage tabs and buttons (row 3), the display (selects a multiband band, drags the crossovers), the reference (row 4)
    {
        for (int i = 0; i < kChainStages; ++i)
            if (chainRect (i).expanded (3.0f).contains (p))
            {
                if (kChainOn[i] != nullptr && chainDotRect (i).contains (p)) toggle (kChainOn[i]);
                else { if (autoOpen) setAutoOpen (false); setStep (i); }
                repaint();
                return;
            }
        if (refPillRect().expanded (3.0f).contains (p)) { setRefOpen (! refOpen); return; }
        auto sb = stageButtons();
        for (int i = 0; i < (int) sb.size() && step >= 1 && step <= 4 && ! autoOpen; ++i)
            if (stageButtonRect (i, (int) sb.size()).expanded (3.0f).contains (p)) { sb[(size_t) i].act(); repaint(); return; }

        if ((tab == 0 || tab == 1) && step >= 1 && step <= 4 && ! autoOpen && dispRect().expanded (8.0f).contains (p))
        {
            const auto r = tab == 1 ? dispRect().withTrimmedRight (172.0f) : dispRect();
            const int nDiv = tab == 0 ? 3 : 2;
            for (int j = 0; j < nDiv; ++j)
            {
                const juce::String did = tab == 0 ? "xover" + juce::String (j + 1) : "imgX" + juce::String (j + 1);
                const float dx = r.getX() + r.getWidth() * std::log (value (did.toRawUTF8()) / 20.0f) / std::log (1000.0f);
                if (std::abs (p.x - dx) < 10.0f) { dragDiv = j; proc.apvts.getParameter (did)->beginChangeGesture(); return; }
            }
            if (tab == 0)
            {
                float edge[5] = { 20.0f, value ("xover1"), value ("xover2"), value ("xover3"), 20000.0f };
                for (int k = 0; k < 4; ++k)
                {
                    const float x0 = r.getX() + r.getWidth() * std::log (edge[k] / 20.0f) / std::log (1000.0f), x1 = r.getX() + r.getWidth() * std::log (edge[k + 1] / 20.0f) / std::log (1000.0f);
                    if (p.x >= x0 && p.x < x1) { selectMbBand (k); return; }
                }
            }
        }
        for (int i = 0; i < 8 && refOpen; ++i)
            if (refButtonRect (i).expanded (3.0f).contains (p))
            {
                if (i == 6) { proc.resetLearn(); matchNote = "Learning the input again"; matchNoteTicks = 90; repaint(); return; }
                if (i == 7)
                {
                    matchNote = proc.matchEq() ? "EQ fitted to the reference (undo with UNDO)" : proc.reference.hasData() ? "Play the input for 3 seconds first" : "Load a reference first";
                    matchNoteTicks = 120; repaint(); return;
                }
                if (i == 0) showReferenceChooser();
                else if (i == 1) toggle ("refPlay");
                else if (i == 2) setChoice ("refAB", 0);
                else if (i == 3) setChoice ("refAB", 1);
                else if (i == 4) toggle ("refMatch");
                else toggle ("refFollow");
                repaint();
                return;
            }
        if (refOpen && refBarRect().expanded (4.0f).contains (p)) { proc.reference.seek ((p.x - refBarRect().getX()) / refBarRect().getWidth()); repaint(); return; }
    }
    for (int i = 0; i < 3; ++i)                                                              // INPUT: polarity, high-pass, mono check
        if (inputButtonRect (i).expanded (2.0f).contains (p))
        {
            if (i == 0) toggle ("trimPolarity");
            else if (i == 2) toggle ("monoCheck");
            else { const float h = value ("trimHpf"); setFloat ("trimHpf", h < 1.0f ? 20.0f : h < 25.0f ? 30.0f : h < 35.0f ? 40.0f : h < 50.0f ? 60.0f : 0.0f); }
            repaint();
            return;
        }
    for (int i = 0; i < 8; ++i)
        if (((i >= 4 && i < 6) || (step == 5 && ! autoOpen)) && buttonRect (i).expanded (3.0f).contains (p))
        {
            if (i == 6) { toggle ("limRelAuto"); return; }
            if (i == 7) { toggle ("limFreeze"); return; }
            if (i < 3) setChoice ("limChar", i);
            else if (i == 3) toggle ("limAuto");
            else if (i == 4) toggle ("delta");
            else setChoice ("ditherShape", (choice ("ditherShape") + 1) % 3);
            return;
        }
}

void MS1AudioProcessorEditor::setEqFreqGain (int band, float freq, float gain)
{
    const juce::String id = "eq" + juce::String (band + 1);
    for (auto [suffix, v] : { std::pair<const char*, float> { "Freq", freq }, { "Gain", gain } })
        if (auto* par = proc.apvts.getParameter (id + suffix)) par->setValueNotifyingHost (par->convertTo0to1 (v));
}

void MS1AudioProcessorEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (dragField >= 0)                                                                     // a field of the dynamic EQ strip
    {
        static const char* suffix[4] = { "DynThr", "DynRatio", "DynAtk", "DynRel" };
        if (auto* par = proc.apvts.getParameter ("eq" + juce::String (selBand + 1) + suffix[dragField]))
            par->setValueNotifyingHost (std::clamp (dragStartNorm + (dragStartY - e.position.y / scale()) / 160.0f, 0.0f, 1.0f));
        repaint();
        return;
    }
    if (dragDiv >= 0)                                                                       // a crossover of the multiband or the imager
    {
        const auto r = tab == 1 ? dispRect().withTrimmedRight (172.0f) : dispRect();
        const float f = 20.0f * std::pow (1000.0f, std::clamp ((e.position.x / scale() - r.getX()) / r.getWidth(), 0.0f, 1.0f));
        const juce::String id = tab == 0 ? "xover" + juce::String (dragDiv + 1) : "imgX" + juce::String (dragDiv + 1);
        float lo = 0.0f, hi = 1.0e9f;
        if (tab == 0) { if (dragDiv > 0) lo = value (("xover" + juce::String (dragDiv)).toRawUTF8()) * 1.2f; if (dragDiv < 2) hi = value (("xover" + juce::String (dragDiv + 2)).toRawUTF8()) / 1.2f; }
        else if (dragDiv == 1) lo = value ("imgX1") * 1.2f; else hi = value ("imgX2") / 1.2f;
        if (auto* par = proc.apvts.getParameter (id)) par->setValueNotifyingHost (par->convertTo0to1 (std::clamp (f, lo, hi)));
        repaint();
        return;
    }
    if (dragBand < 0) return;
    const auto p = e.position / scale();
    const auto r = plotRect();
    const float f = 20.0f * std::pow (1000.0f, std::clamp ((p.x - r.getX()) / r.getWidth(), 0.0f, 1.0f));
    const float gain = std::clamp ((r.getCentreY() - p.y) / (r.getHeight() * 0.5f) * kEqRange, -kEqRange, kEqRange);
    setEqFreqGain (dragBand, f, std::round (gain * 10.0f) / 10.0f);
    repaint();
}

void MS1AudioProcessorEditor::endEqDrag()
{
    if (dragBand < 0) return;
    const juce::String id = "eq" + juce::String (dragBand + 1);
    proc.apvts.getParameter (id + "Freq")->endChangeGesture();
    proc.apvts.getParameter (id + "Gain")->endChangeGesture();
    dragBand = -1;
}

void MS1AudioProcessorEditor::endDivDrag()
{
    if (dragDiv < 0) return;
    const juce::String id = tab == 0 ? "xover" + juce::String (dragDiv + 1) : "imgX" + juce::String (dragDiv + 1);
    proc.apvts.getParameter (id)->endChangeGesture();
    dragDiv = -1;
}

void MS1AudioProcessorEditor::mouseUp (const juce::MouseEvent&)
{
    if (dragField >= 0)
    {
        static const char* suffix[4] = { "DynThr", "DynRatio", "DynAtk", "DynRel" };
        proc.apvts.getParameter ("eq" + juce::String (selBand + 1) + suffix[dragField])->endChangeGesture();
        dragField = -1;
    }
    endEqDrag(); endDivDrag();
}

void MS1AudioProcessorEditor::mouseDoubleClick (const juce::MouseEvent& e)          // a node: back to 0 dB
{
    const auto p = e.position / scale();
    for (int i = 0; i < kChainStages; ++i)                                                  // a chain pill: bypass the stage
        if (kChainOn[i] != nullptr && chainRect (i).contains (p) && ! chainDotRect (i).contains (p)) { toggle (kChainOn[i]); repaint(); return; }
    for (int i = 0; i < 4 && step == 0 && ! autoOpen; ++i)                                  // a dynamic EQ field: back to its default
        if (eqDynFieldRect (i).contains (p))
        {
            static const char* suffix[4] = { "DynThr", "DynRatio", "DynAtk", "DynRel" };
            if (auto* par = proc.apvts.getParameter ("eq" + juce::String (selBand + 1) + suffix[i])) { par->beginChangeGesture(); par->setValueNotifyingHost (par->getDefaultValue()); par->endChangeGesture(); }
            repaint();
            return;
        }
    if (const int n = step == 0 && ! autoOpen ? nodeAt (p, 16.0f) : -1; n >= 0)
    {
        endEqDrag();
        setFloat (("eq" + juce::String (n + 1) + "Gain").toRawUTF8(), 0.0f);
        repaint();
    }
}

void MS1AudioProcessorEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)     // over the plot: the Q of the node under the mouse (else the selected band)
{
    const auto p = e.position / scale();
    if (step != 0 || autoOpen || ! plotRect().expanded (10.0f).contains (p)) return;
    const int n = nodeAt (p, 24.0f) >= 0 ? nodeAt (p, 24.0f) : selBand;
    selBand = n;
    const float dy = w.deltaY * (w.isReversed ? -1.0f : 1.0f);
    if (std::abs (dy) < 1.0e-4f) return;
    const juce::String id = "eq" + juce::String (n + 1) + "Q";
    setFloat (id.toRawUTF8(), std::clamp (value (id.toRawUTF8()) * std::exp (dy * 1.2f), 0.2f, 10.0f));
    repaint();
}

bool MS1AudioProcessorEditor::keyPressed (const juce::KeyPress& k)               // 0 .. 5: the workflow step
{
    if (autoOpen && k == juce::KeyPress::escapeKey) { if (autoReason >= 0) autoReason = -1; else setAutoOpen (false); repaint(); return true; }
    const int c = k.getTextCharacter() - '1';
    if (k.getModifiers().isAnyModifierKeyDown()) return false;
    if (c >= 0 && c < 4) { setStep (c + 1); return true; }                                   // 1 .. 4: MULTIBAND, IMAGER, COLOUR, BUS COMP
    if (c == 4 || k.getTextCharacter() == '0') { setStep (c == 4 ? 5 : 0); return true; }    // 5: LIMITER, 0: EQ
    return false;
}
