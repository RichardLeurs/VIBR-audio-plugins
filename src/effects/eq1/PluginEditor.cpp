// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"

namespace
{
namespace ui = vibr::ui::vintage;          // the default look (src/common/ui/VintageUI.h)

constexpr float kW = ui::kW, kH = ui::kH, kEqRange = 18.0f;
constexpr float kRow1Y = 114.0f, kRow1H = 252.0f, kRow2Y = 398.0f, kRow2H = 232.0f;       // the content area is y 114 to 630; 32 between the rows (18 and the 14 the tabs of BAND stick out above their panel)
const ui::Palette kPal { juce::Colour (57, 197, 232) };                                   // the EQ & Filtering blue #39C5E8

constexpr float kColW = (kW - 2.0f * ui::kMargin - 2.0f * 18.0f) / 3.0f;
juce::Rectangle<float> panelRect (int i)         // 0 EQUALIZER, 1 BAND, 2 OUTPUT
{
    if (i == 0) return { ui::kMargin, kRow1Y, kW - 2.0f * ui::kMargin, kRow1H };
    if (i == 1) return { ui::kMargin, kRow2Y, 2.0f * kColW + 18.0f, kRow2H };
    return { ui::kMargin + 2.0f * (kColW + 18.0f), kRow2Y, kColW, kRow2H };
}
const std::vector<juce::String> kTabs { "EQ", "DYNAMIC" };
const float kTabsLeft = ui::titleEnd ("BAND");
std::vector<juce::Rectangle<float>> tabRects() { return ui::tabRects (panelRect (1), kTabs, -15.0f, 16.0f, kTabsLeft); }

// the display: a phosphor screen, the plot inside it (20 Hz .. 20 kHz on a log axis, +-18 dB), room for the dB scale on the right and the frequencies at the bottom
juce::Rectangle<float> screenRect() { const auto r = panelRect (0); return { r.getX() + 20.0f, r.getY() + 40.0f, r.getWidth() - 40.0f, r.getHeight() - 60.0f }; }
juce::Rectangle<float> plotRect() { return screenRect().withTrimmedLeft (14.0f).withTrimmedRight (48.0f).withTrimmedTop (20.0f).withTrimmedBottom (28.0f); }
juce::Rectangle<float> infoRect() { const auto r = panelRect (0); return { r.getX() + 20.0f, r.getY() + 23.0f, 700.0f, 14.0f }; }
float xOfFreq (float f) { const auto r = plotRect(); return r.getX() + r.getWidth() * std::log (f / 20.0f) / std::log (1000.0f); }
float yOfDb (float db) { const auto r = plotRect(); return r.getCentreY() - r.getHeight() * 0.5f * (std::clamp (db, -kEqRange, kEqRange) / kEqRange); }

// BAND: knobs on the left, the buttons in a column on the right (x +420)
juce::Point<float> knobCentre (int i)            // 0 FREQ, 1 GAIN, 2 Q (tab EQ); 3 .. 6 THRESHOLD, RATIO, ATTACK, RELEASE (tab DYNAMIC)
{
    const auto r = panelRect (1);
    return i < 3 ? juce::Point<float> { r.getX() + 80.0f + 110.0f * (float) i, r.getY() + 86.0f } : juce::Point<float> { r.getX() + 70.0f + 95.0f * (float) (i - 3), r.getY() + 86.0f };
}
juce::Rectangle<float> buttonRow (int row) { const auto r = panelRect (1); return { r.getX() + 420.0f, r.getY() + 38.0f + 56.0f * (float) row, r.getWidth() - 420.0f - 20.0f, 30.0f }; }
juce::Rectangle<float> modeRect (int i)          // STEREO, L, R, M, S on row 1
{
    const auto row = buttonRow (1);
    const float small = 36.0f, gap = 6.0f, first = row.getWidth() - 4.0f * (small + gap) + gap * 0.0f - 0.0f;
    const float w = i == 0 ? first - gap : small;
    const float x = i == 0 ? row.getX() : row.getX() + (first) + (float) (i - 1) * (small + gap);
    return { x, row.getY(), w, row.getHeight() };
}
juce::Rectangle<float> bandSelRect (int b)       // the band selector 1 .. 8 on row 2
{
    const auto row = buttonRow (2);
    const float gap = 4.0f, w = (row.getWidth() - 7.0f * gap) / 8.0f;
    return { row.getX() + (float) b * (w + gap), row.getY(), w, row.getHeight() };
}
juce::Point<float> outKnobCentre() { const auto r = panelRect (2); return { r.getX() + 16.0f + (r.getWidth() - 20.0f - ui::kMeterW - 14.0f - 16.0f) * 0.5f + 8.0f, r.getY() + 86.0f }; }      // the knob line of the BAND knobs
juce::Point<float> meterOrigin() { const auto r = panelRect (2); return { r.getRight() - 20.0f - ui::kMeterW, r.getY() + (r.getHeight() - ui::kMeterH) * 0.5f }; }

constexpr const char* kSuffix[7] = { "Freq", "Gain", "Q", "DynThr", "DynRatio", "DynAtk", "DynRel" };
constexpr const char* kKnobName[7] = { "FREQ", "GAIN", "Q", "THRESHOLD", "RATIO", "ATTACK", "RELEASE" };
constexpr const char* kLo[7] = { "20", "-18", "0.2", "-60", "1", "1", "10" };
constexpr const char* kHi[7] = { "20k", "+18", "10", "0", "10", "100", "1000" };
} // namespace

//==============================================================================
EQ1AudioProcessorEditor::EQ1AudioProcessorEditor (EQ1AudioProcessor& p)
    : LabEditor (p, kW, kH), proc (p)
{
    attachKnob (outKnob.slider, outKnob.att, "outGain");
    midiUI.add (outKnob.slider, "outGain");
    selectBand (selBand);
    startTimerHz (30);
}

EQ1AudioProcessorEditor::~EQ1AudioProcessorEditor() { stopTimer(); }

void EQ1AudioProcessorEditor::selectBand (int band)
{
    selBand = juce::jlimit (0, ms1::kEqBands - 1, band);
    for (int i = 0; i < 7; ++i)
    {
        bandKnobs[i].att.reset();
        attachKnob (bandKnobs[i].slider, bandKnobs[i].att, bandId() + kSuffix[i]);
    }
    layoutKnobs();
    repaint();
}

void EQ1AudioProcessorEditor::layoutKnobs()
{
    for (int i = 0; i < 7; ++i)
    {
        bandKnobs[i].slider.setVisible ((i < 3) == (tab == 0));
        placeKnob (bandKnobs[i].slider, knobCentre (i));
    }
    placeKnob (outKnob.slider, outKnobCentre());
}

void EQ1AudioProcessorEditor::resized() { layoutKnobs(); }
void EQ1AudioProcessorEditor::paintOverChildren (juce::Graphics& g) { midiUI.paintMarks (g, kPal.accent); }

//==============================================================================
// the spectrum of the output: a Hann-windowed FFT of the last 2048 samples, the peak of the bins in each of 240 log-spaced columns, smoothed in time
void EQ1AudioProcessorEditor::updateSpectrum()
{
    const double fs = proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 48000.0;
    const int pos = proc.specPos.load (std::memory_order_acquire);
    for (int i = 0; i < kFftSize; ++i)
        fftData[(size_t) i] = proc.spec[(pos - kFftSize + i + 2 * EQ1AudioProcessor::kSpec) % EQ1AudioProcessor::kSpec].load (std::memory_order_relaxed);
    std::fill (fftData.begin() + kFftSize, fftData.end(), 0.0f);
    window.multiplyWithWindowingTable (fftData.data(), (size_t) kFftSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    const float binHz = (float) fs / (float) kFftSize;
    const bool on = isOn ("power");
    for (int c = 0; c < kCols; ++c)
    {
        const float f0 = 20.0f * std::pow (1000.0f, (float) c / (float) kCols), f1 = 20.0f * std::pow (1000.0f, (float) (c + 1) / (float) kCols);
        const int b0 = juce::jlimit (1, kFftSize / 2 - 1, (int) std::floor (f0 / binHz)), b1 = juce::jlimit (b0, kFftSize / 2 - 1, (int) std::floor (f1 / binHz));
        float mag = 0.0f;
        for (int b = b0; b <= b1; ++b) mag = std::max (mag, fftData[(size_t) b]);
        const float db = on ? juce::Decibels::gainToDecibels (mag / (0.25f * (float) kFftSize), -120.0f) : -120.0f;      // 0 dB = a full scale sine
        spectrumDb[(size_t) c] += (db - spectrumDb[(size_t) c]) * (db > spectrumDb[(size_t) c] ? 0.5f : 0.12f);
    }
}

void EQ1AudioProcessorEditor::timerCallback()
{
    meter.update (proc.outMeter);
    updateSpectrum();
    if (midiUI.needsRepaint()) midiUI.tick();
    repaintDesign (panelRect (0).expanded (4.0f));
    repaintDesign (juce::Rectangle<float> (meterOrigin().x, meterOrigin().y, ui::kMeterW, ui::kMeterH).expanded (2.0f));
}

int EQ1AudioProcessorEditor::nodeAt (juce::Point<float> p, float radius) const
{
    int best = -1;
    float bestD = radius;
    for (int b = 0; b < ms1::kEqBands; ++b)
    {
        const juce::String id = "eq" + juce::String (b + 1);
        const juce::Point<float> c (xOfFreq (value ((id + "Freq").toRawUTF8())), yOfDb (value ((id + "Gain").toRawUTF8())));
        const float d = c.getDistanceFrom (p);
        if (d < bestD) { bestD = d; best = b; }
    }
    return best;
}

//==============================================================================
void EQ1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    const bool power = isOn ("power");
    ui::paintBody (g, kPal, kW, kH);
    ui::paintHeader (g, kPal, "EQ-1", "GRAPHIC EQ", "TONE SCULPTING");
    ui::paintPresetBar (g, kPal, proc.presets, proc.undoManager, power);
    ui::paintFooter (g, kPal, kH - 15.0f);
    ui::paintPanel (g, kPal, panelRect (0), "EQUALIZER");
    ui::paintPanel (g, kPal, panelRect (1), "BAND");
    ui::paintPanel (g, kPal, panelRect (2), "OUTPUT");
    ui::paintTabs (g, kPal, panelRect (1), kTabs, tab, -15.0f, 16.0f, kTabsLeft);

    // ---- the display
    const auto sc = screenRect(), pl = plotRect();
    const float fs = proc.getSampleRate() > 0.0 ? (float) proc.getSampleRate() : 48000.0f;
    ui::paintScreen (g, kPal, sc);
    {
        juce::Graphics::ScopedSaveState ss (g);                                                      // the grid: decades and 1-2-5 lines, +-6 / 12 dB
        g.reduceClipRegion (sc.reduced (3.0f).toNearestInt());
        for (const float f : { 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 70.0f, 80.0f, 90.0f, 100.0f, 200.0f, 300.0f, 400.0f, 500.0f, 600.0f, 700.0f, 800.0f, 900.0f, 1000.0f,
                               2000.0f, 3000.0f, 4000.0f, 5000.0f, 6000.0f, 7000.0f, 8000.0f, 9000.0f, 10000.0f, 20000.0f })
        {
            const bool major = f == 100.0f || f == 1000.0f || f == 10000.0f;
            g.setColour (kPal.phosphor.withAlpha (major ? 0.16f : 0.06f));
            g.drawLine (xOfFreq (f), pl.getY(), xOfFreq (f), pl.getBottom(), 1.0f);
        }
        for (const float db : { -12.0f, -6.0f, 6.0f, 12.0f })
        {
            g.setColour (kPal.phosphor.withAlpha (0.08f));
            g.drawLine (pl.getX(), yOfDb (db), pl.getRight(), yOfDb (db), 1.0f);
        }
        g.setColour (kPal.cream.withAlpha (0.45f));                                                  // the 0 dB line
        g.drawLine (pl.getX(), yOfDb (0.0f), pl.getRight(), yOfDb (0.0f), 1.2f);
    }
    for (const float f : { 20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f })
        ui::drawText (g, f >= 1000.0f ? juce::String (f / 1000.0f, 0) + "k" : juce::String ((int) f), xOfFreq (f), sc.getBottom() - 12.0f, 13.0f, kPal.phosphorDim, false, 0.0f, juce::Justification::centred, ui::Face::Mono);
    for (const float db : { -12.0f, 0.0f, 12.0f })
        ui::drawText (g, db == 0.0f ? juce::String ("0") : juce::String::formatted ("%+d", (int) db), pl.getRight() + 26.0f, yOfDb (db), 13.0f, kPal.phosphorDim, false, 0.0f, juce::Justification::centred, ui::Face::Mono);

    {   // the spectrum (the output, -90 .. 0 dB over the height of the plot)
        juce::Path sp;
        sp.startNewSubPath (pl.getX(), pl.getBottom());
        for (int c = 0; c < kCols; ++c)
            sp.lineTo (pl.getX() + pl.getWidth() * ((float) c + 0.5f) / (float) kCols, pl.getBottom() - pl.getHeight() * juce::jlimit (0.0f, 1.0f, (spectrumDb[(size_t) c] + 90.0f) / 90.0f));
        sp.lineTo (pl.getRight(), pl.getBottom());
        sp.closeSubPath();
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (pl.toNearestInt());
        g.setColour (kPal.phosphor.withAlpha (0.16f));
        g.fillPath (sp);
        g.setColour (kPal.phosphor.withAlpha (0.4f));
        g.strokePath (sp, juce::PathStrokeType (1.2f));
    }

    std::array<ms1::EqBand, ms1::kEqBands> bands;
    for (int b = 0; b < ms1::kEqBands; ++b)
    {
        const juce::String id = "eq" + juce::String (b + 1);
        bands[(size_t) b] = { value ((id + "Freq").toRawUTF8()), value ((id + "Gain").toRawUTF8()), value ((id + "Q").toRawUTF8()), choice ((id + "Mode").toRawUTF8()), isOn ((id + "On").toRawUTF8()) };
    }
    {   // the summed curve of the eight bands
        juce::Path curve;
        for (int i = 0; i <= 240; ++i)
        {
            const float f = 20.0f * std::pow (1000.0f, (float) i / 240.0f);
            float db = 0.0f;
            for (int b = 0; b < ms1::kEqBands; ++b) db += ms1::eqResponseDb (b, bands[(size_t) b], f, fs);
            const juce::Point<float> pt (pl.getX() + pl.getWidth() * (float) i / 240.0f, yOfDb (db));
            if (i == 0) curve.startNewSubPath (pt); else curve.lineTo (pt);
        }
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (pl.expanded (2.0f).toNearestInt());
        g.setColour (kPal.cream.withAlpha (power ? 0.95f : 0.35f));
        g.strokePath (curve, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    for (int b = 0; b < ms1::kEqBands; ++b)                                                             // the band nodes
    {
        const auto& e = bands[(size_t) b];
        const juce::Point<float> c (xOfFreq (e.freq), yOfDb (e.gainDb));
        const bool sel = b == selBand, live = power && e.on;
        const auto disc = juce::Rectangle<float> (22.0f, 22.0f).withCentre (c);
        g.setColour (kPal.screen);
        g.fillEllipse (disc);
        if (sel) { g.setColour (kPal.accent.withAlpha (live ? 1.0f : 0.5f)); g.fillEllipse (disc); }
        g.setColour ((sel ? kPal.accent : kPal.cream).withAlpha (live ? 0.95f : 0.35f));
        g.drawEllipse (disc, 1.6f);
        ui::drawText (g, juce::String (b + 1), c.x, c.y, 13.0f, sel ? kPal.screen.withAlpha (live ? 1.0f : 0.6f) : kPal.cream.withAlpha (live ? 0.95f : 0.4f), true, 0.0f, juce::Justification::centred, ui::Face::Mono);
        if (live && isOn ((("eq" + juce::String (b + 1)) + "Dyn").toRawUTF8()))                          // a dynamic band: a ring that fills with how much of its gain applies now
        {
            const float amt = proc.dynAmount (b);
            g.setColour (kPal.accent.withAlpha (0.25f + 0.75f * amt));
            g.drawEllipse (juce::Rectangle<float> (30.0f, 30.0f).withCentre (c), 1.0f + 2.0f * amt);
        }
    }
    {   // type, frequency, gain and Q of the selected band
        const auto& e = bands[(size_t) selBand];
        const char* type = selBand == 0 ? "LOW SHELF" : selBand == ms1::kEqBands - 1 ? "HIGH SHELF" : "BELL";
        const juce::String freq = e.freq >= 1000.0f ? juce::String (e.freq * 0.001f, 2) + " kHz" : juce::String ((int) std::lround (e.freq)) + " Hz";
        ui::drawText (g, juce::String (selBand + 1) + "  " + type + "   " + freq + "   " + juce::String::formatted ("%+.1f dB", (double) e.gainDb) + "   Q " + juce::String (e.q, 2),
                      infoRect().getX(), infoRect().getCentreY(), 16.0f, kPal.ink, true, 0.06f, juce::Justification::centredLeft);
    }

    // ---- BAND: the buttons and the knobs of the tab
    const auto id = bandId();
    const auto& e = bands[(size_t) selBand];
    if (tab == 0)
    {
        ui::paintFlatButton (g, kPal, buttonRow (0), "BAND ON", e.on, 17.0f);
        static const char* modeName[5] = { "STEREO", "L", "R", "M", "S" };
        for (int i = 0; i < 5; ++i) ui::paintFlatButton (g, kPal, modeRect (i), modeName[i], e.mode == i, 15.0f);
    }
    else
    {
        const bool dyn = isOn ((id + "Dyn").toRawUTF8()), below = isOn ((id + "DynBelow").toRawUTF8());
        ui::paintFlatButton (g, kPal, buttonRow (0), "DYNAMIC", dyn, 17.0f);
        const auto row = buttonRow (1);
        const float w = (row.getWidth() - 6.0f) * 0.5f;
        ui::paintFlatButton (g, kPal, { row.getX(), row.getY(), w, row.getHeight() }, "ABOVE", ! below, 15.0f);
        ui::paintFlatButton (g, kPal, { row.getX() + w + 6.0f, row.getY(), w, row.getHeight() }, "BELOW", below, 15.0f);
    }
    for (int b = 0; b < ms1::kEqBands; ++b) ui::paintFlatButton (g, kPal, bandSelRect (b), juce::String (b + 1), b == selBand, 16.0f);

    for (int i = 0; i < 7; ++i)
    {
        if ((i < 3) != (tab == 0)) continue;
        const auto c = knobCentre (i);
        const double v = bandKnobs[i].slider.getValue();
        juce::String txt;
        switch (i)
        {
            case 0: txt = v >= 1000.0 ? juce::String (v * 0.001, 2) + " kHz" : juce::String ((int) std::lround (v)) + " Hz"; break;
            case 1: txt = juce::String::formatted ("%+.1f dB", v); break;
            case 2: txt = juce::String (v, 2); break;
            case 3: txt = juce::String (v, 1) + " dB"; break;
            case 4: txt = juce::String (v, 1) + ":1"; break;
            case 5: txt = juce::String (v, v < 10.0 ? 1 : 0) + " ms"; break;
            default: txt = juce::String ((int) std::lround (v)) + " ms"; break;
        }
        ui::paintKnob (g, kPal, c, kKnobName[i], txt, kLo[i], kHi[i], i == 1 ? 0.0f : (float) bandKnobs[i].slider.valueToProportionOfLength (v));
        if (i == 1)                                                                                  // GAIN is bipolar: a mark at the top = 0 dB, the ring runs from there
        {
            g.setColour (kPal.inkSoft);
            g.drawLine (c.x, c.y - ui::ringRadius (ui::kKnobR) - 7.0f, c.x, c.y - ui::ringRadius (ui::kKnobR) - 1.0f, 1.6f);
            ui::paintBipolarArc (g, c, ui::kKnobR, (float) (v / kEqRange), kPal.accent, kPal.track);
        }
    }

    // ---- OUTPUT
    ui::paintKnob (g, kPal, outKnobCentre(), "GAIN", juce::String::formatted ("%+.1f dB", outKnob.slider.getValue()), "-12", "+12", (float) outKnob.slider.valueToProportionOfLength (outKnob.slider.getValue()));
    ui::paintMeter (g, kPal, meter, meterOrigin().x, meterOrigin().y, power);
}

//==============================================================================
void EQ1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / scale();
    proc.undoManager.beginNewTransaction();
    if (ui::powerRect().expanded (3.0f).contains (p)) { toggle ("power"); return; }
    if (ui::presetBarMouseDown (p, *this, proc.presets, proc.undoManager, scale(), [this] { repaint(); })) return;
    if (e.mods.isPopupMenu()) return;

    const auto tabs = tabRects();
    for (size_t i = 0; i < tabs.size(); ++i)
        if (tabs[i].contains (p)) { tab = (int) i; layoutKnobs(); repaint(); return; }
    for (int b = 0; b < ms1::kEqBands; ++b)
        if (bandSelRect (b).expanded (2.0f).contains (p)) { selectBand (b); return; }
    const auto id = bandId();
    if (buttonRow (0).expanded (2.0f).contains (p)) { toggle ((id + (tab == 0 ? "On" : "Dyn")).toRawUTF8()); repaint(); return; }
    if (tab == 0)
    {
        for (int i = 0; i < 5; ++i)
            if (modeRect (i).expanded (2.0f).contains (p)) { setChoice ((id + "Mode").toRawUTF8(), i); repaint(); return; }
    }
    else
    {
        const auto row = buttonRow (1);
        if (row.expanded (2.0f).contains (p)) { { auto* par = apvts.getParameter (id + "DynBelow"); par->beginChangeGesture(); par->setValueNotifyingHost (p.x < row.getCentreX() ? 0.0f : 1.0f); par->endChangeGesture(); } repaint(); return; }
    }
    if (const int n = nodeAt (p, 16.0f); n >= 0)
    {
        selectBand (n);
        dragBand = n;
        apvts.getParameter (bandId() + "Freq")->beginChangeGesture();
        apvts.getParameter (bandId() + "Gain")->beginChangeGesture();
        repaint();
    }
}

void EQ1AudioProcessorEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (dragBand < 0) return;
    const auto p = e.position / scale();
    const auto r = plotRect();
    const float f = 20.0f * std::pow (1000.0f, std::clamp ((p.x - r.getX()) / r.getWidth(), 0.0f, 1.0f));
    const float gain = std::clamp ((r.getCentreY() - p.y) / (r.getHeight() * 0.5f) * kEqRange, -kEqRange, kEqRange);
    const juce::String id = "eq" + juce::String (dragBand + 1);
    if (auto* par = apvts.getParameter (id + "Freq")) par->setValueNotifyingHost (par->convertTo0to1 (f));
    if (auto* par = apvts.getParameter (id + "Gain")) par->setValueNotifyingHost (par->convertTo0to1 (std::round (gain * 10.0f) / 10.0f));
    repaint();
}

void EQ1AudioProcessorEditor::endNodeDrag()
{
    if (dragBand < 0) return;
    const juce::String id = "eq" + juce::String (dragBand + 1);
    apvts.getParameter (id + "Freq")->endChangeGesture();
    apvts.getParameter (id + "Gain")->endChangeGesture();
    dragBand = -1;
}

void EQ1AudioProcessorEditor::mouseUp (const juce::MouseEvent&) { endNodeDrag(); }

void EQ1AudioProcessorEditor::mouseDoubleClick (const juce::MouseEvent& e)          // a node: back to 0 dB
{
    const auto p = e.position / scale();
    if (const int n = nodeAt (p, 16.0f); n >= 0)
    {
        endNodeDrag();
        if (auto* par = apvts.getParameter ("eq" + juce::String (n + 1) + "Gain")) { par->beginChangeGesture(); par->setValueNotifyingHost (par->convertTo0to1 (0.0f)); par->endChangeGesture(); }
        repaint();
    }
}

void EQ1AudioProcessorEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)     // over the plot: the Q of the node under the mouse (else the selected band)
{
    const auto p = e.position / scale();
    if (! plotRect().expanded (10.0f).contains (p)) return;
    const int n = nodeAt (p, 24.0f) >= 0 ? nodeAt (p, 24.0f) : selBand;
    if (n != selBand) selectBand (n);
    const float dy = w.deltaY * (w.isReversed ? -1.0f : 1.0f);
    if (std::abs (dy) < 1.0e-4f) return;
    if (auto* par = apvts.getParameter (bandId() + "Q"))
    {
        const float q = std::clamp (value ((bandId() + "Q").toRawUTF8()) * std::exp (dy * 1.2f), 0.2f, 10.0f);
        par->beginChangeGesture(); par->setValueNotifyingHost (par->convertTo0to1 (q)); par->endChangeGesture();
    }
    repaint();
}
