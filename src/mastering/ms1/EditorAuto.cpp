// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
// MS-1 editor, the AUTO panel and the REFERENCE panel.
#include "PluginEditor.h"
#include "EditorLayout.h"
#include "VibrLogo.h"
#include "ui/VibrUI.h"

using namespace ms1ui;

//==============================================================================
// REFERENCE: load, play, A / B, level match, follow the host, position
void MS1AudioProcessorEditor::showReferenceChooser()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose a reference track", proc.reference.file().getParentDirectory(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.m4a;*.ogg");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (f.existsAsFile()) proc.loadReference (f);
    });
}

void MS1AudioProcessorEditor::drawReference (juce::Graphics& g)
{
    const auto& ref = proc.reference;
    const bool has = ref.hasData();
    const auto button = [&] (int i, const juce::String& label, bool on, float alpha = 1.0f)
    {
        vibr::ui::paintFlatButton (g, kPal, refButtonRect (i), label, on, 13.0f, alpha);
    };
    button (0, ref.isLoading() ? "LOADING..." : "LOAD", false);
    button (1, isOn ("refPlay") ? "PAUSE" : "PLAY", isOn ("refPlay"), has ? 1.0f : 0.4f);
    button (2, "A  CHAIN", ! isOn ("refAB"));
    button (3, "B  REFERENCE", isOn ("refAB"), has ? 1.0f : 0.5f);
    button (4, isOn ("refMatch") ? juce::String::formatted ("LEVEL MATCH %+.1f dB", (double) proc.refMatchDb.load()) : juce::String ("LEVEL MATCH"), isOn ("refMatch"));
    button (5, "FOLLOW HOST", isOn ("refFollow"));
    button (6, "RESET", false);
    button (7, "MATCH EQ", false, has ? 1.0f : 0.4f);

    const auto r = panelRect (7);
    juce::String info = ref.isLoading() ? "Loading and analysing..." : ref.hasFailed() ? "This file could not be read" : has ? ref.name() : "No reference loaded: load a mastered track to compare";
    drawText (g, info, r.getX() + 140.0f, r.getY() + 43.0f, 15.0f, has ? kInk.withAlpha (0.95f) : kInk.withAlpha (0.5f), true, 0.04f, juce::Justification::centredLeft);
    if (has)
    {
        const int secs = (int) ref.seconds();
        drawText (g, juce::String::formatted ("%.1f LUFS   |   %d:%02d", (double) ref.integratedLufs(), secs / 60, secs % 60), r.getX() + 710.0f, r.getY() + 43.0f, 13.0f, kBody, true, 0.04f, juce::Justification::centredRight);
    }
    drawText (g, matchNoteTicks > 0 ? matchNote : "INPUT LEARNED: " + juce::String (proc.learnSeconds(), 0) + " S", r.getX() + 556.0f, r.getY() + 119.0f, 13.0f, matchNoteTicks > 0 ? kBody : kInk.withAlpha (0.5f), true, 0.1f, juce::Justification::centredLeft);
    const auto bar = refBarRect();
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (bar, 7.0f);
    if (has)
    {
        g.setColour (kBody.withAlpha (isOn ("refPlay") ? 0.9f : 0.5f));
        g.fillRoundedRectangle (bar.withWidth (std::max (8.0f, bar.getWidth() * ref.positionFraction())), 7.0f);
    }
}

//==============================================================================
// AUTO (stages 1 to 3): the measurements of the chain input and the proposal for every row (see AutoMaster.h)
void MS1AudioProcessorEditor::setAutoOpen (bool open)
{
    autoOpen = open; autoReason = -1;
    if (open) autoRefresh();
    layoutKnobs();                                                      // the knobs under the panel are hidden
    repaint();
}

void MS1AudioProcessorEditor::autoRefresh()
{
    proc.autoAn.read (autoSnap);                                        // false (the audio thread was writing for too long): keep the last one
    if (proc.autoAn.clearRequest.load()) autoSnap = {};                  // CLEAR is done at the next audio block: show it at once
    autoSnap.learning = proc.autoAn.learning.load() ? 1u : 0u;
    autoProp = ms1::autom::propose (autoSnap, proc.autoContext());
    if (! autoProp.valid) autoReason = -1;
}

void MS1AudioProcessorEditor::autoApplyNow (bool toB)
{
    const bool in = autoRowChecked (0), eq = autoRowChecked (1), mb = autoRowChecked (2), img = autoRowChecked (3), bus = autoRowChecked (4), lim = autoRowChecked (5), out = autoRowChecked (6);
    if (! (in || eq || mb || img || bus || lim || out)) return;
    const int n = toB ? proc.autoApplyToB (autoProp, in, eq, mb, img, bus, lim, out) : proc.autoApply (autoProp, in, eq, mb, img, bus, lim, out);
    matchNote = toB ? juce::String ("AUTO: slot A = before, slot B = with the proposal (") + juce::String (n) + " parameters)" : "AUTO applied: " + juce::String (n) + " parameters (UNDO reverts all of it)";
    matchNoteTicks = 150;
    setAutoOpen (false);
}

bool MS1AudioProcessorEditor::autoMouseDown (juce::Point<float> p)
{
    if (! autoRect().contains (p)) return false;
    if (autoReason >= 0) { autoReason = -1; repaint(); return true; }       // a click closes the '?' popup
    if (autoCloseRect().expanded (4.0f).contains (p)) { setAutoOpen (false); return true; }
    if (autoRunRect().contains (p))
    {
        if (proc.autoAn.learning.load()) proc.autoStop();
        else if (autoSnap.programmeSeconds < ms1::autom::kMaxProgrammeSeconds) proc.autoStart();
    }
    else if (autoClearRect().contains (p)) { proc.autoClear(); autoSnap = {}; }
    else for (int i = 0; i < 4; ++i) if (autoStyleRect (i).contains (p)) proc.autoStyle.store (i);
    for (int i = 0; i < 7; ++i)
    {
        if (autoHelpRect (i).expanded (2.0f).contains (p) && autoProp.valid && rowOf (autoProp, i).available && ! rowOf (autoProp, i).reason.empty()) autoReason = i;
        else if (autoRowRect (i).withWidth (150.0f).contains (p) && autoProp.valid && rowOf (autoProp, i).available && rowOf (autoProp, i).needed) autoOff[(size_t) i] = ! autoOff[(size_t) i];
    }
    for (int i = 0; i < 3; ++i)
        if (autoApplyRect (i).contains (p))
        {
            if (i == 2) setAutoOpen (false);
            else if (autoProp.valid) autoApplyNow (i == 1);
        }
    autoRefresh();
    repaint();
    return true;
}

void MS1AudioProcessorEditor::drawAuto (juce::Graphics& g)
{
    const auto r = autoRect();
    vibr::ui::paintPanel (g, kPal, r, "AUTO");
    const auto& s = autoSnap;
    const bool learning = autoIsLearning || s.learning != 0;
    const int secs = (int) std::floor (s.programmeSeconds);
    const bool full = s.programmeSeconds >= ms1::autom::kMaxProgrammeSeconds - 0.5f;

    // the cross
    {
        const auto c = autoCloseRect();
        g.setColour (kInk.withAlpha (0.6f));
        g.drawLine (c.getX() + 6.0f, c.getY() + 6.0f, c.getRight() - 6.0f, c.getBottom() - 6.0f, 1.6f);
        g.drawLine (c.getRight() - 6.0f, c.getY() + 6.0f, c.getX() + 6.0f, c.getBottom() - 6.0f, 1.6f);
    }
    // the state line
    {
        juce::String st;
        if (learning) st = "LEARNING  " + juce::String (secs) + " s of " + juce::String ((int) ms1::autom::kMinProgrammeSeconds) + " s minimum";
        else if (s.programmeSeconds <= 0.0f) st = "IDLE";
        else if (s.programmeSeconds < ms1::autom::kMinProgrammeSeconds) st = "STOPPED  " + juce::String (secs) + " s of " + juce::String ((int) ms1::autom::kMinProgrammeSeconds) + " s minimum";
        else st = "READY  " + juce::String (secs) + " s heard" + (full ? " (full)" : "");
        drawText (g, st, r.getX() + 16.0f, r.getY() + 38.0f, 17.0f, learning ? kBody : s.programmeSeconds >= ms1::autom::kMinProgrammeSeconds ? kBody : kInk.withAlpha (0.8f), true, 0.08f, juce::Justification::centredLeft);
        if (learning || s.programmeSeconds > 0.0f)                                                     // the progress towards the 10 s minimum
        {
            const auto bar = juce::Rectangle<float> (r.getX() + 296.0f, r.getY() + 35.0f, 84.0f, 6.0f);
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.fillRoundedRectangle (bar, 3.0f);
            g.setColour (kBody);
            g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * std::min (1.0f, s.programmeSeconds / ms1::autom::kMinProgrammeSeconds)), 3.0f);
        }
    }
    vibr::ui::paintFlatButton (g, kPal, autoRunRect(), learning ? "STOP" : "ANALYZE", learning, 14.0f, learning || ! full ? 1.0f : 0.4f);
    vibr::ui::paintFlatButton (g, kPal, autoClearRect(), "CLEAR", false, 14.0f, s.programmeSeconds > 0.0f ? 1.0f : 0.45f);
    for (int i = 0; i < 4; ++i) vibr::ui::paintFlatButton (g, kPal, autoStyleRect (i), kAutoStyleName[i], proc.autoStyle.load() == i, 13.0f, 1.0f, 9.0f, 0.08f);
    drawText (g, "STYLE (the EQ target; a loaded reference replaces it)", r.getX() + 16.0f, r.getY() + 132.0f, 13.0f, kInk.withAlpha (0.55f), true, 0.06f, juce::Justification::centredLeft);

    // the live picture: the long-term spectrum of the input and the short-term loudness distribution, as they build up
    {
        const auto plot = autoPlotRect();
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (plot.expanded (4.0f), 8.0f);
        const auto sp = plot.withWidth (236.0f).reduced (2.0f, 4.0f), hi = plot.withTrimmedLeft (252.0f).reduced (2.0f, 4.0f);
        drawText (g, "SPECTRUM", sp.getX() + 4.0f, sp.getY() + 6.0f, 13.0f, kInk.withAlpha (0.4f), true, 0.12f, juce::Justification::centredLeft);
        drawText (g, "SHORT-TERM LUFS", hi.getX() + 4.0f, hi.getY() + 6.0f, 13.0f, kInk.withAlpha (0.4f), true, 0.12f, juce::Justification::centredLeft);
        if (s.spectrumFrames > 2)
        {
            float peak = -200.0f;
            std::array<float, ms1::autom::kSpecBands> v {};
            for (int b = 0; b < ms1::autom::kSpecBands; ++b)
            {
                const float f = 20.0f * std::pow (1000.0f, ((float) b + 0.5f) / (float) ms1::autom::kSpecBands);
                v[(size_t) b] = s.spectrumDb[b] + 4.5f * std::log2 (f / 1000.0f);                    // the display's +4.5 dB per octave slope
                peak = std::max (peak, v[(size_t) b]);
            }
            juce::Path path;
            path.startNewSubPath (sp.getX(), sp.getBottom());
            for (int b = 0; b < ms1::autom::kSpecBands; ++b)
                path.lineTo (sp.getX() + sp.getWidth() * ((float) b + 0.5f) / (float) ms1::autom::kSpecBands, sp.getBottom() - sp.getHeight() * std::clamp ((v[(size_t) b] - (peak - 54.0f)) / 54.0f, 0.0f, 1.0f));
            path.lineTo (sp.getRight(), sp.getBottom());
            path.closeSubPath();
            g.setColour (kBody.withAlpha (0.35f));
            g.fillPath (path);
            g.setColour (kBody.withAlpha (0.9f));
            g.strokePath (path, juce::PathStrokeType (1.2f));
        }
        else drawText (g, learning ? "listening ..." : "no audio learned yet", sp.getCentreX(), sp.getCentreY() + 4.0f, 13.0f, kInk.withAlpha (0.35f), true, 0.1f);
        {
            uint32_t mx = 1;
            for (int b = 40; b < ms1::autom::kHistBins; ++b) mx = std::max (mx, s.shortTermHist[b]);        // -60 .. 0 LUFS
            const float bw = hi.getWidth() / 60.0f;
            g.setColour (kBody.withAlpha (0.8f));
            for (int b = 40; b < ms1::autom::kHistBins; ++b)
            {
                const float h = hi.getHeight() * (float) s.shortTermHist[b] / (float) mx;
                if (h > 0.4f) g.fillRect (hi.getX() + bw * (float) (b - 40), hi.getBottom() - h, std::max (1.0f, bw - 0.4f), h);
            }
        }
        const juce::String live = (s.momentaryNow > -100.0f ? "M " + juce::String (s.momentaryNow, 1) : juce::String ("M --")) + "   I " + (s.integratedLufs > -100.0f ? juce::String (s.integratedLufs, 1) : juce::String ("--"))
                                  + " LUFS   TP " + (s.truePeakDb > -100.0f ? juce::String (s.truePeakDb, 1) : juce::String ("--")) + "   CORR " + juce::String::formatted ("%+.2f", (double) s.correlationNow);
        drawText (g, live, plot.getX(), r.getY() + 271.0f, 13.0f, kInk.withAlpha (0.85f), true, 0.02f, juce::Justification::centredLeft);
    }

    // the proposal
    drawText (g, "PROPOSAL", r.getX() + 410.0f, r.getY() + 36.0f, 14.0f, kInk.withAlpha (0.65f), true, 0.16f, juce::Justification::centredLeft);
    if (! autoProp.valid) drawText (g, juce::String (autoProp.message), r.getX() + 530.0f, r.getY() + 36.0f, 13.0f, kInk.withAlpha (0.8f), true, 0.04f, juce::Justification::centredLeft);
    for (int i = 0; i < 7; ++i)
    {
        const auto& row = rowOf (autoProp, i);
        const auto rr = autoRowRect (i);
        const bool have = autoProp.valid && row.available;
        g.setColour (juce::Colours::black.withAlpha (0.3f));
        g.fillRoundedRectangle (rr, 8.0f);
        const auto cb = autoCheckRect (i);
        const bool checked = autoRowChecked (i), canCheck = have && row.needed;
        g.setColour (checked ? kBody : kInk.withAlpha (canCheck ? 0.5f : 0.2f));
        if (checked) g.fillRoundedRectangle (cb, 4.0f); else g.drawRoundedRectangle (cb.reduced (0.5f), 4.0f, 1.2f);
        if (checked)
        {
            juce::Path tick;
            tick.startNewSubPath (cb.getX() + 3.5f, cb.getCentreY()); tick.lineTo (cb.getCentreX() - 1.0f, cb.getBottom() - 4.5f); tick.lineTo (cb.getRight() - 3.0f, cb.getY() + 4.0f);
            g.setColour (kDark);
            g.strokePath (tick, juce::PathStrokeType (1.8f));
        }
        const bool warn = have && (i == 0 ? autoProp.clipWarning : i == 5 ? autoProp.heavyWarning : false);
        drawText (g, kAutoRowName[i], rr.getX() + 30.0f, rr.getCentreY(), 13.0f, have ? (warn ? kRed : kBody) : kInk.withAlpha (0.35f), true, 0.08f, juce::Justification::centredLeft);
        juce::String text = ! autoProp.valid ? juce::String() : juce::String::fromUTF8 (row.summary.c_str());
        float size = 13.5f;
        const float maxW = rr.getWidth() - 140.0f - 34.0f;
        while (size > 13.0f && textWidth (font (size, false).withExtraKerningFactor (0.02f), text) > maxW) size -= 0.5f;
        while (text.length() > 4 && textWidth (font (size, false).withExtraKerningFactor (0.02f), text) > maxW) text = text.dropLastCharacters (4) + "...";
        drawText (g, text, rr.getX() + 140.0f, rr.getCentreY(), size, have ? (row.needed ? kInk.withAlpha (0.95f) : kInk.withAlpha (0.55f)) : kInk.withAlpha (0.35f), false, 0.02f, juce::Justification::centredLeft);
        if (have && ! row.reason.empty())
        {
            const auto hq = autoHelpRect (i).withSizeKeepingCentre (18.0f, 18.0f);
            g.setColour (kInk.withAlpha (0.55f));
            g.drawEllipse (hq.reduced (0.5f), 1.2f);
            drawText (g, "?", hq.getCentreX(), hq.getCentreY(), 13.0f, kInk.withAlpha (0.8f), true, 0.0f);
        }
    }
    const bool can = autoProp.valid && (autoRowChecked (0) || autoRowChecked (1) || autoRowChecked (2) || autoRowChecked (3) || autoRowChecked (4) || autoRowChecked (5) || autoRowChecked (6));
    vibr::ui::paintFlatButton (g, kPal, autoApplyRect (0), "APPLY", can, 13.0f, can ? 1.0f : 0.4f);
    vibr::ui::paintFlatButton (g, kPal, autoApplyRect (1), "APPLY TO B", false, 13.0f, can ? 1.0f : 0.4f);
    vibr::ui::paintFlatButton (g, kPal, autoApplyRect (2), "CANCEL", false, 13.0f);
    drawText (g, "Nothing changes until you press APPLY.", r.getX() + 830.0f, r.getY() + 253.0f, 14.0f, kInk.withAlpha (0.8f), false, 0.02f, juce::Justification::centredLeft);
    drawText (g, "A measured starting point, not the best setting.", r.getX() + 830.0f, r.getY() + 274.0f, 14.0f, kInk.withAlpha (0.8f), false, 0.02f, juce::Justification::centredLeft);

    if (autoReason >= 0 && autoProp.valid)                                                              // the '?' popup: the reason and the numbers
    {
        const auto box = juce::Rectangle<float> (autoRowRect (0).getX() + 40.0f, autoRowRect (0).getY() - 2.0f, 650.0f, 222.0f);
        g.setColour (juce::Colour (14, 15, 17));
        g.fillRoundedRectangle (box, 12.0f);
        g.setColour (kBody.withAlpha (0.7f));
        g.drawRoundedRectangle (box.reduced (0.5f), 12.0f, 1.4f);
        drawText (g, kAutoRowName[autoReason], box.getX() + 16.0f, box.getY() + 18.0f, 14.0f, kBody, true, 0.14f, juce::Justification::centredLeft);
        g.setColour (kInk.withAlpha (0.9f));
        g.setFont (font (14.0f, false));
        g.drawFittedText (juce::String::fromUTF8 (rowOf (autoProp, autoReason).reason.c_str()), box.reduced (16.0f, 0.0f).withTrimmedTop (36.0f).withTrimmedBottom (10.0f).toNearestInt(), juce::Justification::topLeft, 11, 1.0f);
    }
}
