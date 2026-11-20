// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"

namespace
{
namespace ui = vibr::ui::vintage;          // the default look (src/common/ui/VintageUI.h)

constexpr float kW = ui::kW, kH = ui::kH;
constexpr float kRow1Y = 114.0f, kRow1H = 294.0f, kRow2Y = 426.0f, kRow2H = 204.0f;       // the content area is y 114 to 630, 18 between the rows
const ui::Palette kPal { juce::Colour (181, 213, 46) };                                   // the Utility & Routing lime #B5D52E
constexpr float kColW = (kW - 2.0f * ui::kMargin - 2.0f * 18.0f) / 3.0f;
constexpr float kFloorDb = -200.0f;

juce::Rectangle<float> panelRect (int i)             // 0 ANALYZER, 1 ANALYSIS, 2 TRACE, 3 READOUT
{
    if (i == 0) return { ui::kMargin, kRow1Y, kW - 2.0f * ui::kMargin, kRow1H };
    return { ui::kMargin + (float) (i - 1) * (kColW + 18.0f), kRow2Y, kColW, kRow2H };
}
constexpr const char* kPanelName[4] = { "ANALYZER", "ANALYSIS", "TRACE", "READOUT" };

// the display: a phosphor screen with the plot inside it (room for the dB scale on the right and the frequencies at the bottom)
juce::Rectangle<float> screenRect() { const auto r = panelRect (0); return { r.getX() + 20.0f, r.getY() + 38.0f, r.getWidth() - 40.0f, r.getHeight() - 58.0f }; }
juce::Rectangle<float> plotRect() { return screenRect().withTrimmedLeft (16.0f).withTrimmedRight (52.0f).withTrimmedTop (20.0f).withTrimmedBottom (28.0f); }
juce::Rectangle<float> infoRect() { const auto r = panelRect (0); return { r.getX() + 20.0f, r.getY() + 23.0f, 900.0f, 14.0f }; }
juce::Rectangle<float> readoutScreen() { const auto r = panelRect (3); return { r.getX() + 20.0f, r.getY() + 26.0f, r.getWidth() - 40.0f, r.getHeight() - 46.0f }; }

// the option rows of ANALYSIS (panel 1) and TRACE (panel 2): a label and a row of buttons that each set one choice parameter
struct Row { const char* label; const char* id; std::vector<juce::String> names; };
const std::vector<Row>& rows (int panel)
{
    static const std::vector<Row> analysis { { "FFT SIZE", "fftSize", { "1K", "2K", "4K", "8K", "16K" } }, { "CHANNEL", "channel", { "LEFT", "RIGHT", "SUM" } }, { "RANGE", "range", { "60 dB", "90 dB", "120 dB" } } };
    static const std::vector<Row> trace { { "SMOOTH", "smooth", { "OFF", "LOW", "MED", "HIGH" } }, { "AVERAGE", "avg", { "1x", "2x", "4x", "8x" } }, { "DISPLAY", "display", { "FILLED", "LINE" } }, { "SCALE", "scale", { "LOG", "LIN" } } };
    return panel == 1 ? analysis : trace;
}
constexpr const char* kToggleId[3] = { "hold", "freeze", "harmonics" };
constexpr const char* kToggleName[3] = { "PEAK HOLD", "FREEZE", "HARMONICS" };
constexpr float kRowTop = 32.0f, kRowPitch = 42.0f, kBtnH = 30.0f, kLabelW = 76.0f;
juce::Rectangle<float> optionRect (int panel, int row, int i, int n)
{
    const auto r = panelRect (panel);
    const float x0 = r.getX() + 20.0f + kLabelW, width = r.getRight() - 20.0f - x0, gap = 4.0f, w = (width - (float) (n - 1) * gap) / (float) n;
    return { x0 + (float) i * (w + gap), r.getY() + kRowTop + kRowPitch * (float) row, w, kBtnH };
}
juce::Rectangle<float> toggleRect (int i)                // the three switches on the last row of ANALYSIS, as wide as the panel
{
    const auto r = panelRect (1);
    const float x0 = r.getX() + 20.0f, width = r.getWidth() - 40.0f, gap = 4.0f, w = (width - 2.0f * gap) / 3.0f;
    return { x0 + (float) i * (w + gap), r.getY() + kRowTop + kRowPitch * 3.0f, w, kBtnH };
}

juce::String noteName (float hz, int* centsOut = nullptr)
{
    static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    if (hz <= 0.0f) return "--";
    const float midi = 69.0f + 12.0f * std::log2 (hz / 440.0f);
    const int n = (int) std::lround (midi);
    if (centsOut != nullptr) *centsOut = (int) std::lround ((midi - (float) n) * 100.0f);
    return juce::String (names[((n % 12) + 12) % 12]) + juce::String (n / 12 - 1);
}
juce::String hzText (float hz) { return hz >= 1000.0f ? juce::String (hz * 0.001f, 2) + " kHz" : hz < 100.0f ? juce::String (hz, 1) + " Hz" : juce::String (juce::roundToInt (hz)) + " Hz"; }
} // namespace

//==============================================================================
SA1AudioProcessorEditor::SA1AudioProcessorEditor (SA1AudioProcessor& p)
    : LabEditor (p, kW, kH), proc (p)
{
    holdDb.fill (kFloorDb);
    colDb.fill (kFloorDb);
    setMouseClickGrabsKeyboardFocus (false);
    startTimerHz (30);
}

SA1AudioProcessorEditor::~SA1AudioProcessorEditor() { stopTimer(); }

float SA1AudioProcessorEditor::colFreq (float col) const
{
    const float t = col / (float) kCols;
    return choice ("scale") == 0 ? 20.0f * std::pow (1000.0f, t) : 20.0f + 19980.0f * t;
}

float SA1AudioProcessorEditor::freqToX (float hz) const
{
    const auto r = plotRect();
    const float t = choice ("scale") == 0 ? std::log (juce::jmax (hz, 1.0f) / 20.0f) / std::log (1000.0f) : (hz - 20.0f) / 19980.0f;
    return r.getX() + r.getWidth() * t;
}

//==============================================================================
// one analysis frame (30 times a second): a Hann-windowed FFT of the last N samples of the chosen channel, the power averaged over 1 / 2 / 4 / 8 frames, the display columns
// (the peak of the bins in each column, smoothed over a fraction of an octave), the peak hold, the peak finder and the fundamental (a harmonic product spectrum)
void SA1AudioProcessorEditor::analyse()
{
    const int order = 10 + choice ("fftSize"), N = 1 << order;
    const double fsNow = proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 48000.0;
    if (order != fftOrder)
    {
        fftOrder = order;
        fft = std::make_unique<juce::dsp::FFT> (order);
        window = std::make_unique<juce::dsp::WindowingFunction<float>> ((size_t) N, juce::dsp::WindowingFunction<float>::hann);
        fftData.assign ((size_t) N * 2, 0.0f);
        avgPow.assign ((size_t) N / 2 + 1, 0.0f);
    }
    binHz = (float) fsNow / (float) N;
    const bool power = isOn ("power");
    if (isOn ("freeze") && power) return;                                       // a frozen spectrum stays as it is

    const int ch = choice ("channel"), nBins = N / 2;
    const int pos = proc.ringPos.load (std::memory_order_acquire), ringMask = SA1AudioProcessor::kRing - 1;
    for (int i = 0; i < N; ++i)
    {
        const int idx = (pos - N + i) & ringMask;
        const float l = proc.ringL[idx].load (std::memory_order_relaxed), r = proc.ringR[idx].load (std::memory_order_relaxed);
        fftData[(size_t) i] = ch == 0 ? l : ch == 1 ? r : 0.5f * (l + r);
    }
    std::fill (fftData.begin() + N, fftData.end(), 0.0f);
    window->multiplyWithWindowingTable (fftData.data(), (size_t) N);
    fft->performFrequencyOnlyForwardTransform (fftData.data());

    static const int avgFrames[4] = { 1, 2, 4, 8 };
    const float k = 1.0f / (float) avgFrames[juce::jlimit (0, 3, choice ("avg"))], norm = 1.0f / (0.25f * (float) N);        // 0 dB = a full scale sine
    for (int b = 0; b <= nBins; ++b)
    {
        const float m = fftData[(size_t) b] * norm, pw = power ? m * m : 0.0f;
        avgPow[(size_t) b] += (pw - avgPow[(size_t) b]) * k;
    }

    // the display columns: the peak of the bins in the column, or the interpolated value when the column is narrower than a bin
    std::array<float, kCols> colPow {};
    for (int c = 0; c < kCols; ++c)
    {
        const float k0 = colFreq ((float) c) / binHz, k1 = colFreq ((float) c + 1.0f) / binHz;
        if (k1 - k0 < 1.0f)
        {
            const float kc = juce::jlimit (0.0f, (float) nBins - 1.0f, 0.5f * (k0 + k1));
            const int kb = (int) kc;
            colPow[(size_t) c] = avgPow[(size_t) kb] + (avgPow[(size_t) kb + 1] - avgPow[(size_t) kb]) * (kc - (float) kb);
        }
        else
        {
            float m = 0.0f;
            for (int b = juce::jmax (0, (int) std::ceil (k0)); b <= juce::jmin (nBins, (int) std::floor (k1)); ++b) m = juce::jmax (m, avgPow[(size_t) b]);
            colPow[(size_t) c] = m;
        }
    }
    static const float halfCols[4] = { 0.0f, 2.5f, 5.0f, 10.0f };                       // smoothing: off, 1/12, 1/6, 1/3 octave (about 60 columns an octave on the log scale)
    const int h = (int) halfCols[juce::jlimit (0, 3, choice ("smooth"))];
    std::array<double, kCols + 1> prefix {};
    for (int c = 0; c < kCols; ++c) prefix[(size_t) c + 1] = prefix[(size_t) c] + (double) colPow[(size_t) c];
    const bool hold = isOn ("hold");
    for (int c = 0; c < kCols; ++c)
    {
        const int a = juce::jmax (0, c - h), b = juce::jmin (kCols, c + h + 1);
        const float pw = (float) ((prefix[(size_t) b] - prefix[(size_t) a]) / (double) (b - a));
        colDb[(size_t) c] = pw > 1.0e-20f ? 10.0f * std::log10 (pw) : kFloorDb;
        holdDb[(size_t) c] = hold ? juce::jmax (holdDb[(size_t) c], colDb[(size_t) c]) : kFloorDb;
    }

    // the dB values of the bins, for the peak finder and the fundamental
    std::vector<float> db ((size_t) nBins + 1);
    float maxDb = kFloorDb;
    const int bLo = juce::jmax (2, (int) (30.0f / binHz)), bHi = juce::jmin (nBins - 2, (int) (12000.0f / binHz));
    for (int b = 0; b <= nBins; ++b) db[(size_t) b] = avgPow[(size_t) b] > 1.0e-20f ? 10.0f * std::log10 (avgPow[(size_t) b]) : kFloorDb;
    for (int b = bLo; b <= bHi; ++b) maxDb = juce::jmax (maxDb, db[(size_t) b]);

    numPeaks = 0; fundHz = 0.0f; fundDb = kFloorDb;
    if (! power || maxDb < -80.0f) return;

    std::vector<Peak> found;                                                            // local maxima within 50 dB of the strongest, parabolic interpolation
    for (int b = bLo; b <= bHi; ++b)
        if (db[(size_t) b] > db[(size_t) b - 1] && db[(size_t) b] >= db[(size_t) b + 1] && db[(size_t) b] > maxDb - 50.0f)
        {
            const float a = db[(size_t) b - 1], m = db[(size_t) b], c = db[(size_t) b + 1], den = a - 2.0f * m + c, d = std::abs (den) > 1.0e-6f ? 0.5f * (a - c) / den : 0.0f;
            found.push_back ({ ((float) b + d) * binHz, m - 0.25f * (a - c) * d });
        }
    std::sort (found.begin(), found.end(), [] (const Peak& x, const Peak& y) { return x.db > y.db; });
    for (const auto& pk : found)                                                        // the strongest first, at least a semitone apart
    {
        bool near = false;
        for (int i = 0; i < numPeaks; ++i) if (std::abs (std::log2 (pk.hz / peaks[(size_t) i].hz)) < 1.0f / 12.0f) near = true;
        if (! near && numPeaks < 5) peaks[(size_t) numPeaks++] = pk;
    }

    // the fundamental: the bin whose first five harmonics add up to the most (a harmonic product spectrum in dB), 60 Hz .. 1.5 kHz
    const int fLo = juce::jmax (2, (int) (60.0f / binHz)), fHi = juce::jmin (nBins / 5 - 1, (int) (1500.0f / binHz));
    float best = -1.0e9f; int bestBin = -1;
    for (int b = fLo; b <= fHi; ++b)
    {
        float s = 0.0f;
        for (int hh = 1; hh <= 5; ++hh) s += juce::jmax (db[(size_t) (b * hh)], -100.0f);
        if (s > best) { best = s; bestBin = b; }
    }
    if (bestBin > 0)
    {
        int pk = bestBin;                                                               // the local peak next to the winning bin
        for (int b = bestBin - 2; b <= bestBin + 2; ++b) if (b > 1 && b < nBins && db[(size_t) b] > db[(size_t) pk]) pk = b;
        if (db[(size_t) pk] > maxDb - 35.0f)
        {
            const float a = db[(size_t) pk - 1], m = db[(size_t) pk], c = db[(size_t) pk + 1], den = a - 2.0f * m + c, d = std::abs (den) > 1.0e-6f ? 0.5f * (a - c) / den : 0.0f;
            fundHz = ((float) pk + d) * binHz; fundDb = m;
        }
    }
}

void SA1AudioProcessorEditor::timerCallback()
{
    analyse();
    repaintDesign (panelRect (0).expanded (4.0f));
    repaintDesign (panelRect (3).expanded (4.0f));
}

//==============================================================================
void SA1AudioProcessorEditor::paintDisplay (juce::Graphics& g)
{
    const auto sc = screenRect(), pl = plotRect();
    const bool power = isOn ("power"), linear = choice ("scale") == 1, line = choice ("display") == 1;
    const float rangeDb = choice ("range") == 0 ? 60.0f : choice ("range") == 1 ? 90.0f : 120.0f;
    const auto yOf = [&] (float db) { return pl.getY() + pl.getHeight() * juce::jlimit (0.0f, 1.0f, -db / rangeDb); };

    ui::paintScreen (g, kPal, sc);
    {
        juce::Graphics::ScopedSaveState ss (g);                                                 // the grid
        g.reduceClipRegion (sc.reduced (3.0f).toNearestInt());
        if (! linear)
            for (const float f : { 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 70.0f, 80.0f, 90.0f, 100.0f, 200.0f, 300.0f, 400.0f, 500.0f, 600.0f, 700.0f, 800.0f, 900.0f, 1000.0f,
                                   2000.0f, 3000.0f, 4000.0f, 5000.0f, 6000.0f, 7000.0f, 8000.0f, 9000.0f, 10000.0f, 20000.0f })
            {
                g.setColour (kPal.phosphor.withAlpha (f == 100.0f || f == 1000.0f || f == 10000.0f ? 0.16f : 0.06f));
                g.drawLine (freqToX (f), pl.getY(), freqToX (f), pl.getBottom(), 1.0f);
            }
        else
            for (float f = 2000.0f; f <= 20000.0f; f += 2000.0f)
            {
                g.setColour (kPal.phosphor.withAlpha (0.1f));
                g.drawLine (freqToX (f), pl.getY(), freqToX (f), pl.getBottom(), 1.0f);
            }
        const float step = rangeDb > 100.0f ? 20.0f : 10.0f;
        for (float d = 0.0f; d <= rangeDb + 0.1f; d += step)
        {
            g.setColour (kPal.phosphor.withAlpha (d == 0.0f ? 0.22f : 0.08f));
            g.drawLine (pl.getX(), yOf (-d), pl.getRight(), yOf (-d), 1.0f);
        }
    }
    for (const float f : linear ? std::vector<float> { 20.0f, 4000.0f, 8000.0f, 12000.0f, 16000.0f, 20000.0f } : std::vector<float> { 20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f })
        ui::drawText (g, f >= 1000.0f ? juce::String (f / 1000.0f, 0) + "k" : juce::String ((int) f), freqToX (f), sc.getBottom() - 12.0f, 13.0f, kPal.phosphorDim, false, 0.0f, juce::Justification::centred, ui::Face::Mono);
    {
        const float step = rangeDb > 100.0f ? 20.0f : 10.0f;
        for (float d = 0.0f; d <= rangeDb + 0.1f; d += step * (rangeDb > 100.0f ? 1.0f : 2.0f))
            ui::drawText (g, d == 0.0f ? juce::String ("0") : juce::String ((int) -d), pl.getRight() + 26.0f, yOf (-d), 13.0f, kPal.phosphorDim, false, 0.0f, juce::Justification::centred, ui::Face::Mono);
    }

    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (pl.expanded (1.0f, 2.0f).toNearestInt());
        if (isOn ("harmonics") && fundHz > 0.0f && power)                                       // harmonic markers: dotted lines at n x the fundamental
        {
            float lastLabelX = -100.0f;
            for (int n = 1; n <= 16 && (float) n * fundHz < 20000.0f; ++n)
            {
                const float x = freqToX ((float) n * fundHz);
                g.setColour (kPal.accent.withAlpha (n == 1 ? 0.9f : 0.55f));
                for (float y = pl.getY(); y < pl.getBottom(); y += 8.0f) g.drawLine (x, y, x, juce::jmin (y + 3.0f, pl.getBottom()), 1.0f);
                if (x - lastLabelX >= 18.0f)                                                    // the numbers of crowded harmonics are left out
                {
                    ui::drawText (g, juce::String (n), x, pl.getY() + 8.0f, 13.0f, kPal.accent, true, 0.0f, juce::Justification::centred, ui::Face::Mono);
                    lastLabelX = x;
                }
            }
        }
        juce::Path sp;
        for (int c = 0; c < kCols; ++c)
        {
            const juce::Point<float> pt (pl.getX() + pl.getWidth() * ((float) c + 0.5f) / (float) kCols, yOf (colDb[(size_t) c]));
            if (c == 0) sp.startNewSubPath (pt); else sp.lineTo (pt);
        }
        if (! line)
        {
            juce::Path fill (sp);
            fill.lineTo (pl.getRight(), pl.getBottom());
            fill.lineTo (pl.getX(), pl.getBottom());
            fill.closeSubPath();
            g.setColour (kPal.phosphor.withAlpha (0.2f));
            g.fillPath (fill);
        }
        g.setColour (kPal.phosphor.withAlpha (0.95f));
        g.strokePath (sp, juce::PathStrokeType (line ? 1.8f : 1.4f));
        if (isOn ("hold"))                                                                      // the peak hold line
        {
            juce::Path hp;
            for (int c = 0; c < kCols; ++c)
            {
                const juce::Point<float> pt (pl.getX() + pl.getWidth() * ((float) c + 0.5f) / (float) kCols, yOf (holdDb[(size_t) c]));
                if (c == 0) hp.startNewSubPath (pt); else hp.lineTo (pt);
            }
            g.setColour (kPal.cream.withAlpha (0.8f));
            g.strokePath (hp, juce::PathStrokeType (1.2f));
        }
        if (power)
            for (int i = 0; i < numPeaks; ++i)                                                  // the peak finder: a marker and a number over each of the strongest peaks
            {
                const float x = freqToX (peaks[(size_t) i].hz), y = yOf (peaks[(size_t) i].db);
                juce::Path tri;
                tri.addTriangle (x - 5.0f, y - 12.0f, x + 5.0f, y - 12.0f, x, y - 4.0f);
                g.setColour (kPal.segYellow);
                g.fillPath (tri);
                ui::drawText (g, juce::String (i + 1), x, y - 20.0f, 13.0f, kPal.segYellow, true, 0.0f, juce::Justification::centred, ui::Face::Mono);
            }
        if (hoverX >= 0.0f)                                                                     // the cursor
        {
            g.setColour (kPal.cream.withAlpha (0.55f));
            g.drawLine (hoverX, pl.getY(), hoverX, pl.getBottom(), 1.0f);
        }
    }
    {   // the info line above the screen
        static const char* chName[3] = { "LEFT", "RIGHT", "SUM" };
        const int N = 1 << (10 + choice ("fftSize"));
        ui::drawText (g, "FFT " + juce::String (N) + "   " + juce::String (binHz, 2) + " Hz / BIN   " + chName[juce::jlimit (0, 2, choice ("channel"))] + "   AVG " + juce::String (1 << choice ("avg")) + "x"
                         + (isOn ("freeze") ? "   FROZEN" : "") + (isOn ("hold") ? "   HOLD" : ""),
                      infoRect().getX(), infoRect().getCentreY(), 16.0f, kPal.ink, true, 0.06f, juce::Justification::centredLeft);
    }
}

void SA1AudioProcessorEditor::paintReadout (juce::Graphics& g)
{
    const auto sc = readoutScreen();
    ui::paintScreen (g, kPal, sc);
    const float x = sc.getX() + 14.0f;
    float y = sc.getY() + 16.0f;
    const auto line = [&] (const juce::String& t, juce::Colour c, float size = 13.0f) { ui::drawText (g, t, x, y, size, c, false, 0.0f, juce::Justification::centredLeft, ui::Face::Mono); y += 18.0f; };
    const auto pl = plotRect();
    // the cursor: frequency, note and level under the mouse
    if (hoverX >= pl.getX() && hoverX <= pl.getRight())
    {
        const float col = (hoverX - pl.getX()) / pl.getWidth() * (float) kCols, hz = colFreq (col);
        const float db = colDb[(size_t) juce::jlimit (0, kCols - 1, (int) col)];
        line ("CURSOR " + hzText (hz) + "  " + noteName (hz) + "  " + (db > -150.0f ? juce::String (db, 1) + " dB" : juce::String ("-inf")), kPal.cream);
    }
    else line ("CURSOR  --", kPal.phosphorDim);
    if (fundHz > 0.0f && isOn ("power"))
    {
        int cents = 0;
        const auto n = noteName (fundHz, &cents);
        line ("FUND   " + n + "  " + hzText (fundHz) + "  " + (cents >= 0 ? "+" : "") + juce::String (cents) + " c", kPal.phosphor);
    }
    else line ("FUND   --", kPal.phosphorDim);
    line ("PEAKS", kPal.phosphorDim);
    for (int i = 0; i < 5; ++i)
        if (i < numPeaks && isOn ("power")) line (juce::String (i + 1) + "  " + hzText (peaks[(size_t) i].hz).paddedRight (' ', 8) + noteName (peaks[(size_t) i].hz).paddedRight (' ', 4) + juce::String (peaks[(size_t) i].db, 1), kPal.segYellow);
        else line (juce::String (i + 1) + "  --", kPal.phosphorDim);
}

void SA1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    ui::paintBody (g, kPal, kW, kH);
    ui::paintHeader (g, kPal, "SA-1", "SPECTRUM ANALYZER", "SPECTRAL VISION");
    ui::paintPresetBar (g, kPal, proc.presets, proc.undoManager, isOn ("power"));
    ui::paintFooter (g, kPal, kH - 15.0f);
    for (int i = 0; i < 4; ++i) ui::paintPanel (g, kPal, panelRect (i), kPanelName[i]);
    paintDisplay (g);
    paintReadout (g);

    for (int panel = 1; panel <= 2; ++panel)
    {
        const auto& rs = rows (panel);
        for (int r = 0; r < (int) rs.size(); ++r)
        {
            const auto& row = rs[(size_t) r];
            const auto first = optionRect (panel, r, 0, (int) row.names.size());
            ui::drawText (g, row.label, panelRect (panel).getX() + 20.0f, first.getCentreY(), 15.0f, kPal.ink, true, 0.1f, juce::Justification::centredLeft);
            for (int i = 0; i < (int) row.names.size(); ++i)
                ui::paintFlatButton (g, kPal, optionRect (panel, r, i, (int) row.names.size()), row.names[(size_t) i], choice (row.id) == i, 15.0f);
        }
    }
    for (int i = 0; i < 3; ++i) ui::paintFlatButton (g, kPal, toggleRect (i), kToggleName[i], isOn (kToggleId[i]), 15.0f);
}

//==============================================================================
void SA1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / scale();
    proc.undoManager.beginNewTransaction();
    if (ui::powerRect().expanded (3.0f).contains (p)) { toggle ("power"); return; }
    if (ui::presetBarMouseDown (p, *this, proc.presets, proc.undoManager, scale(), [this] { repaint(); })) return;
    for (int panel = 1; panel <= 2; ++panel)
    {
        const auto& rs = rows (panel);
        for (int r = 0; r < (int) rs.size(); ++r)
            for (int i = 0; i < (int) rs[(size_t) r].names.size(); ++i)
                if (optionRect (panel, r, i, (int) rs[(size_t) r].names.size()).expanded (2.0f).contains (p)) { setChoice (rs[(size_t) r].id, i); repaint(); return; }
    }
    for (int i = 0; i < 3; ++i)
        if (toggleRect (i).expanded (2.0f).contains (p)) { toggle (kToggleId[i]); repaint(); return; }
}

void SA1AudioProcessorEditor::mouseMove (const juce::MouseEvent& e)
{
    const auto p = e.position / scale();
    hoverX = plotRect().expanded (0.0f, 40.0f).contains (p) ? p.x : -1.0f;
}

void SA1AudioProcessorEditor::mouseExit (const juce::MouseEvent&) { hoverX = -1.0f; }
