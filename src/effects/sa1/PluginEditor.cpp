// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"

namespace
{
namespace ui = vibr::ui::vintage;          // the default look (src/common/ui/VintageUI.h)

constexpr float kW = ui::kW, kH = ui::kH;
constexpr float kRow1Y = 114.0f, kRow1H = 294.0f, kRow2Y = 426.0f, kRow2H = 204.0f;       // the content area is y 114 to 630, 18 between the rows
const ui::Palette kPal { juce::Colour (181, 213, 46) };                                   // the Utility & Routing lime #B5D52E
const juce::Colour kSnapCol (255, 152, 74);                                               // the snapshot trace
constexpr float kColW = (kW - 2.0f * ui::kMargin - 2.0f * 18.0f) / 3.0f;
constexpr float kFloorDb = -200.0f;

juce::Rectangle<float> panelRect (int i)             // 0 ANALYZER, 1 ANALYSIS, 2 TRACE, 3 READOUT
{
    if (i == 0) return { ui::kMargin, kRow1Y, kW - 2.0f * ui::kMargin, kRow1H };
    return { ui::kMargin + (float) (i - 1) * (kColW + 18.0f), kRow2Y, kColW, kRow2H };
}
constexpr const char* kPanelName[4] = { "ANALYZER", "ANALYSIS", "TRACE", "READOUT" };

// the display: a phosphor screen with the plot inside it (room for the scale on the right and the labels at the bottom)
juce::Rectangle<float> screenRect() { const auto r = panelRect (0); return { r.getX() + 20.0f, r.getY() + 38.0f, r.getWidth() - 40.0f, r.getHeight() - 58.0f }; }
juce::Rectangle<float> plotRect() { return screenRect().withTrimmedLeft (16.0f).withTrimmedRight (52.0f).withTrimmedTop (20.0f).withTrimmedBottom (28.0f); }
juce::Rectangle<float> infoRect() { const auto r = panelRect (0); return { r.getX() + 20.0f, r.getY() + 23.0f, 900.0f, 14.0f }; }
juce::Rectangle<float> readoutScreen() { const auto r = panelRect (3); return { r.getX() + 20.0f, r.getY() + 26.0f, r.getWidth() - 40.0f, r.getHeight() - 46.0f }; }

// the analysis mode: tabs on the top edge of ANALYZER
const std::vector<juce::String> kModeTabs { "SPECTRUM", "SPECTROGRAM", "OCTAVE" };
const float kTabsLeft = ui::titleEnd ("ANALYZER");
std::vector<juce::Rectangle<float>> modeTabRects() { return ui::tabRects (panelRect (0), kModeTabs, -15.0f, 16.0f, kTabsLeft); }

// SNAPSHOT / COMPARE / CLEAR: three small buttons at the right end of the info line
juce::Rectangle<float> snapButton (int i)
{
    static const float w[3] = { 96.0f, 88.0f, 64.0f };
    const auto r = panelRect (0);
    float x = r.getRight() - 20.0f - (w[0] + w[1] + w[2] + 12.0f);
    for (int j = 0; j < i; ++j) x += w[j] + 6.0f;
    return { x, r.getY() + 12.0f, w[i], 22.0f };
}
constexpr const char* kSnapName[3] = { "SNAPSHOT", "COMPARE", "CLEAR" };

// the option rows of ANALYSIS (panel 1) and TRACE (panel 2): a label and a row of buttons that each set one choice parameter. TRACE follows the mode.
struct Row { const char* label; const char* id; std::vector<juce::String> names; };
const std::vector<Row>& rows (int panel, int mode)
{
    static const std::vector<Row> analysis { { "FFT SIZE", "fftSize", { "1K", "2K", "4K", "8K", "16K" } }, { "CHANNEL", "channel", { "LEFT", "RIGHT", "SUM" } }, { "RANGE", "range", { "60 dB", "90 dB", "120 dB" } } };
    static const std::vector<Row> traceSpectrum { { "SMOOTH", "smooth", { "OFF", "LOW", "MED", "HIGH" } }, { "AVERAGE", "avg", { "1x", "2x", "4x", "8x" } }, { "DISPLAY", "display", { "FILLED", "LINE" } }, { "SCALE", "scale", { "LOG", "LIN" } } };
    static const std::vector<Row> traceSpectrogram { { "SMOOTH", "smooth", { "OFF", "LOW", "MED", "HIGH" } }, { "AVERAGE", "avg", { "1x", "2x", "4x", "8x" } }, { "SCALE", "scale", { "LOG", "LIN" } }, { "SPEED", "speed", { "FAST", "MED", "SLOW" } } };
    static const std::vector<Row> traceBands { { "BANDS", "bands", { "1/3 OCTAVE", "1 OCTAVE" } }, { "AVERAGE", "avg", { "1x", "2x", "4x", "8x" } } };
    if (panel == 1) return analysis;
    return mode == 1 ? traceSpectrogram : mode == 2 ? traceBands : traceSpectrum;
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

// the nominal centres of the octave bands
constexpr float kThird[31] = { 20.0f, 25.0f, 31.5f, 40.0f, 50.0f, 63.0f, 80.0f, 100.0f, 125.0f, 160.0f, 200.0f, 250.0f, 315.0f, 400.0f, 500.0f, 630.0f, 800.0f, 1000.0f, 1250.0f, 1600.0f,
                               2000.0f, 2500.0f, 3150.0f, 4000.0f, 5000.0f, 6300.0f, 8000.0f, 10000.0f, 12500.0f, 16000.0f, 20000.0f };
constexpr float kOctave[10] = { 31.5f, 63.0f, 125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f };

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

// the spectrogram colours: from the screen colour through dark and bright green to yellow and red (t 0 .. 1 = -range .. 0 dB)
juce::Colour heat (float t)
{
    static const struct { float t; juce::uint8 r, g, b; } stops[] = { { 0.0f, 15, 20, 13 }, { 0.3f, 30, 78, 38 }, { 0.6f, 127, 216, 106 }, { 0.82f, 232, 193, 74 }, { 1.0f, 240, 87, 58 } };
    t = juce::jlimit (0.0f, 1.0f, t);
    for (int i = 1; i < 5; ++i)
        if (t <= stops[i].t)
        {
            const float k = (t - stops[i - 1].t) / (stops[i].t - stops[i - 1].t);
            return juce::Colour ((juce::uint8) juce::roundToInt ((float) stops[i - 1].r + k * (float) (stops[i].r - stops[i - 1].r)), (juce::uint8) juce::roundToInt ((float) stops[i - 1].g + k * (float) (stops[i].g - stops[i - 1].g)),
                                 (juce::uint8) juce::roundToInt ((float) stops[i - 1].b + k * (float) (stops[i].b - stops[i - 1].b)));
        }
    return juce::Colour (240, 87, 58);
}
} // namespace

//==============================================================================
SA1AudioProcessorEditor::SA1AudioProcessorEditor (SA1AudioProcessor& p)
    : LabEditor (p, kW, kH), proc (p)
{
    holdDb.fill (kFloorDb);
    colDb.fill (kFloorDb);
    snapDb.fill (kFloorDb);
    bandDb.fill (kFloorDb); bandHold.fill (kFloorDb); snapBand.fill (kFloorDb);
    specImage.clear (specImage.getBounds(), heat (0.0f));
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

float SA1AudioProcessorEditor::freqToY (float hz) const
{
    const auto r = plotRect();
    const float t = choice ("scale") == 0 ? std::log (juce::jmax (hz, 1.0f) / 20.0f) / std::log (1000.0f) : (hz - 20.0f) / 19980.0f;
    return r.getBottom() - r.getHeight() * t;
}

//==============================================================================
// power per bin -> dB per display column: the peak of the bins in the column (or the interpolated value when the column is narrower than a bin), smoothed over a fraction of an octave
void SA1AudioProcessorEditor::computeColumns (const std::vector<float>& pow, float bHz, std::array<float, kCols>& out) const
{
    const int nBins = (int) pow.size() - 1;
    std::array<float, kCols> colPow {};
    for (int c = 0; c < kCols; ++c)
    {
        const float k0 = colFreq ((float) c) / bHz, k1 = colFreq ((float) c + 1.0f) / bHz;
        if (k1 - k0 < 1.0f)
        {
            const float kc = juce::jlimit (0.0f, (float) nBins - 1.0f, 0.5f * (k0 + k1));
            const int kb = (int) kc;
            colPow[(size_t) c] = pow[(size_t) kb] + (pow[(size_t) kb + 1] - pow[(size_t) kb]) * (kc - (float) kb);
        }
        else
        {
            float m = 0.0f;
            for (int b = juce::jmax (0, (int) std::ceil (k0)); b <= juce::jmin (nBins, (int) std::floor (k1)); ++b) m = juce::jmax (m, pow[(size_t) b]);
            colPow[(size_t) c] = m;
        }
    }
    static const float halfCols[4] = { 0.0f, 2.5f, 5.0f, 10.0f };                       // smoothing: off, 1/12, 1/6, 1/3 octave (about 60 columns an octave on the log scale)
    const int h = (int) halfCols[juce::jlimit (0, 3, choice ("smooth"))];
    std::array<double, kCols + 1> prefix {};
    for (int c = 0; c < kCols; ++c) prefix[(size_t) c + 1] = prefix[(size_t) c] + (double) colPow[(size_t) c];
    for (int c = 0; c < kCols; ++c)
    {
        const int a = juce::jmax (0, c - h), b = juce::jmin (kCols, c + h + 1);
        const float pw = (float) ((prefix[(size_t) b] - prefix[(size_t) a]) / (double) (b - a));
        out[(size_t) c] = pw > 1.0e-20f ? 10.0f * std::log10 (pw) : kFloorDb;
    }
}

// power per bin -> dB per octave band: the power of the bins inside the band (a Hann window spreads a sine over 1.5 bins' worth of power, hence the division), or the
// interpolated density times the band width when the band is narrower than a bin
int SA1AudioProcessorEditor::computeBands (const std::vector<float>& pow, float bHz, std::array<float, kMaxBands>& out) const
{
    const bool third = choice ("bands") == 0;
    const int n = third ? 31 : 10, nBins = (int) pow.size() - 1;
    const float half = third ? std::pow (2.0f, 1.0f / 6.0f) : std::sqrt (2.0f);
    for (int i = 0; i < n; ++i)
    {
        const float fc = third ? kThird[i] : kOctave[i], lo = fc / half, hi = fc * half;
        const int b0 = juce::jmax (1, (int) std::ceil (lo / bHz)), b1 = juce::jmin (nBins, (int) std::floor (hi / bHz));
        double sum = 0.0;
        if (b1 >= b0) for (int b = b0; b <= b1; ++b) sum += (double) pow[(size_t) b];
        else
        {
            const float kc = juce::jlimit (0.0f, (float) nBins - 1.0f, fc / bHz);
            const int kb = (int) kc;
            sum = (double) (pow[(size_t) kb] + (pow[(size_t) kb + 1] - pow[(size_t) kb]) * (kc - (float) kb)) * (double) ((hi - lo) / bHz);
        }
        sum /= 1.5;
        out[(size_t) i] = sum > 1.0e-20 ? (float) (10.0 * std::log10 (sum)) : kFloorDb;
    }
    for (int i = n; i < kMaxBands; ++i) out[(size_t) i] = kFloorDb;
    return n;
}

void SA1AudioProcessorEditor::pushSpectrogramColumn()
{
    const float rangeDb = choice ("range") == 0 ? 60.0f : choice ("range") == 1 ? 90.0f : 120.0f;
    specImage.moveImageSection (0, 0, 1, 0, kSpecW - 1, kSpecH);                        // scroll one pixel to the left, the newest column goes in on the right
    juce::Image::BitmapData bd (specImage, kSpecW - 1, 0, 1, kSpecH, juce::Image::BitmapData::writeOnly);
    for (int r = 0; r < kSpecH; ++r)
    {
        const float up = 1.0f - ((float) r + 0.5f) / (float) kSpecH;                    // row 0 is the top = the highest frequency
        const int c = juce::jlimit (0, kCols - 1, (int) (up * (float) kCols));
        bd.setPixelColour (0, r, heat ((colDb[(size_t) c] + rangeDb) / rangeDb));
    }
}

//==============================================================================
// one analysis frame (30 times a second): a Hann-windowed FFT of the last N samples of the chosen channel, the power averaged over 1 / 2 / 4 / 8 frames, the display columns,
// the octave bands, the peak hold, the peak finder and the fundamental (a harmonic product spectrum)
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
    if (hasSnapshot()) { computeColumns (proc.snapPow, proc.snapBinHz, snapDb); computeBands (proc.snapPow, proc.snapBinHz, snapBand); }      // the snapshot follows the scale and the smoothing
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

    computeColumns (avgPow, binHz, colDb);
    numBands = computeBands (avgPow, binHz, bandDb);
    const bool hold = isOn ("hold");
    for (int c = 0; c < kCols; ++c) holdDb[(size_t) c] = hold ? juce::jmax (holdDb[(size_t) c], colDb[(size_t) c]) : kFloorDb;
    for (int i = 0; i < kMaxBands; ++i) bandHold[(size_t) i] = hold ? juce::jmax (bandHold[(size_t) i], bandDb[(size_t) i]) : kFloorDb;

    if (choice ("mode") == 1)                                                   // the spectrogram: one column every 1, 2 or 4 frames (16, 32 or 64 s across)
    {
        static const int framesPerColumn[3] = { 1, 2, 4 };
        if (++specTick >= framesPerColumn[juce::jlimit (0, 2, choice ("speed"))]) { specTick = 0; pushSpectrogramColumn(); }
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
    const int mode = choice ("mode");
    const bool linear = choice ("scale") == 1;
    const float rangeDb = choice ("range") == 0 ? 60.0f : choice ("range") == 1 ? 90.0f : 120.0f;
    const auto yOfDb = [&] (float db) { return pl.getY() + pl.getHeight() * juce::jlimit (0.0f, 1.0f, -db / rangeDb); };
    const auto label = [&] (const juce::String& t, float cx, float cy) { ui::drawText (g, t, cx, cy, 13.0f, kPal.phosphorDim, false, 0.0f, juce::Justification::centred, ui::Face::Mono); };
    const auto freqLabel = [] (float f) { return f >= 1000.0f ? juce::String (f / 1000.0f, 0) + "k" : juce::String ((int) f); };

    ui::paintScreen (g, kPal, sc);
    {
        juce::Graphics::ScopedSaveState ss (g);                                                 // the grid
        g.reduceClipRegion (sc.reduced (3.0f).toNearestInt());
        if (mode == 0)
        {
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
        }
        if (mode != 1)
        {
            const float step = rangeDb > 100.0f ? 20.0f : 10.0f;
            for (float d = 0.0f; d <= rangeDb + 0.1f; d += step)
            {
                g.setColour (kPal.phosphor.withAlpha (d == 0.0f ? 0.22f : 0.08f));
                g.drawLine (pl.getX(), yOfDb (-d), pl.getRight(), yOfDb (-d), 1.0f);
            }
        }
    }

    if (mode == 0) paintSpectrum (g, pl, rangeDb);
    else if (mode == 1) paintSpectrogram (g, pl, rangeDb);
    else { paintBands (g, pl, rangeDb); paintBandLabels (g, pl); }

    // the scales: dB on the right (spectrum, octave) or frequency on the right and time at the bottom (spectrogram)
    if (mode == 1)
    {
        for (const float f : linear ? std::vector<float> { 20.0f, 4000.0f, 8000.0f, 12000.0f, 16000.0f, 20000.0f } : std::vector<float> { 20.0f, 100.0f, 1000.0f, 10000.0f, 20000.0f })
            label (freqLabel (f), pl.getRight() + 26.0f, juce::jlimit (pl.getY() + 4.0f, pl.getBottom() - 4.0f, freqToY (f)));
        static const int seconds[3] = { 16, 32, 64 };
        label ("-" + juce::String (seconds[juce::jlimit (0, 2, choice ("speed"))]) + " s", pl.getX() + 16.0f, sc.getBottom() - 12.0f);
        label ("NOW", pl.getRight() - 12.0f, sc.getBottom() - 12.0f);
    }
    else
    {
        const float step = rangeDb > 100.0f ? 20.0f : 10.0f;
        for (float d = 0.0f; d <= rangeDb + 0.1f; d += step * (rangeDb > 100.0f ? 1.0f : 2.0f))
            label (d == 0.0f ? juce::String ("0") : juce::String ((int) -d), pl.getRight() + 26.0f, yOfDb (-d));
        if (mode == 0)
            for (const float f : linear ? std::vector<float> { 20.0f, 4000.0f, 8000.0f, 12000.0f, 16000.0f, 20000.0f } : std::vector<float> { 20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f })
                label (freqLabel (f), freqToX (f), sc.getBottom() - 12.0f);
    }

    {   // the info line above the screen
        static const char* chName[3] = { "LEFT", "RIGHT", "SUM" };
        const int N = 1 << (10 + choice ("fftSize"));
        ui::drawText (g, "FFT " + juce::String (N) + "   " + juce::String (binHz, 2) + " Hz / BIN   " + chName[juce::jlimit (0, 2, choice ("channel"))] + "   AVG " + juce::String (1 << choice ("avg")) + "x"
                         + (isOn ("freeze") ? "   FROZEN" : "") + (isOn ("hold") && mode != 1 ? "   HOLD" : ""),
                      infoRect().getX(), infoRect().getCentreY(), 16.0f, kPal.ink, true, 0.06f, juce::Justification::centredLeft);
    }
}

void SA1AudioProcessorEditor::paintSpectrum (juce::Graphics& g, juce::Rectangle<float> pl, float rangeDb)
{
    const bool power = isOn ("power"), line = choice ("display") == 1, compare = isOn ("compare") && hasSnapshot();
    const auto yOf = [&] (float db) { return pl.getY() + pl.getHeight() * juce::jlimit (0.0f, 1.0f, -db / rangeDb); };
    const auto pathOf = [&] (const std::array<float, kCols>& v)
    {
        juce::Path p;
        for (int c = 0; c < kCols; ++c)
        {
            const juce::Point<float> pt (pl.getX() + pl.getWidth() * ((float) c + 0.5f) / (float) kCols, yOf (v[(size_t) c]));
            if (c == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
        return p;
    };
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
    const auto sp = pathOf (colDb);
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
    if (compare)                                                                            // the snapshot over the live spectrum
    {
        g.setColour (kSnapCol.withAlpha (0.95f));
        g.strokePath (pathOf (snapDb), juce::PathStrokeType (1.6f));
    }
    if (isOn ("hold"))                                                                      // the peak hold line
    {
        g.setColour (kPal.cream.withAlpha (0.8f));
        g.strokePath (pathOf (holdDb), juce::PathStrokeType (1.2f));
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
    if (hoverP.x >= 0.0f)                                                                   // the cursor
    {
        g.setColour (kPal.cream.withAlpha (0.55f));
        g.drawLine (hoverP.x, pl.getY(), hoverP.x, pl.getBottom(), 1.0f);
    }
    if (compare)                                                                            // the key of the two traces
    {
        ui::drawText (g, "LIVE", pl.getRight() - 90.0f, pl.getBottom() - 30.0f, 13.0f, kPal.phosphor, true, 0.0f, juce::Justification::centredRight, ui::Face::Mono);
        ui::drawText (g, "SNAPSHOT", pl.getRight() - 90.0f, pl.getBottom() - 14.0f, 13.0f, kSnapCol, true, 0.0f, juce::Justification::centredRight, ui::Face::Mono);
    }
}

void SA1AudioProcessorEditor::paintSpectrogram (juce::Graphics& g, juce::Rectangle<float> pl, float)
{
    g.setImageResamplingQuality (juce::Graphics::mediumResamplingQuality);
    g.drawImage (specImage, pl, juce::RectanglePlacement::stretchToFit);
    if (hoverP.x >= 0.0f)
    {
        g.setColour (kPal.cream.withAlpha (0.55f));
        g.drawLine (pl.getX(), hoverP.y, pl.getRight(), hoverP.y, 1.0f);
    }
}

void SA1AudioProcessorEditor::paintBands (juce::Graphics& g, juce::Rectangle<float> pl, float rangeDb)
{
    const bool compare = isOn ("compare") && hasSnapshot(), third = choice ("bands") == 0;
    const auto yOf = [&] (float db) { return pl.getY() + pl.getHeight() * juce::jlimit (0.0f, 1.0f, -db / rangeDb); };
    const float slot = pl.getWidth() / (float) numBands, barW = slot * (third ? 0.72f : 0.6f);
    juce::Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (pl.expanded (1.0f, 2.0f).toNearestInt());
    for (int i = 0; i < numBands; ++i)
    {
        const float cx = pl.getX() + slot * ((float) i + 0.5f), y = yOf (bandDb[(size_t) i]);
        const juce::Rectangle<float> bar (cx - barW * 0.5f, y, barW, pl.getBottom() - y);
        g.setColour (kPal.phosphor.withAlpha (0.55f));
        g.fillRect (bar);
        g.setColour (kPal.phosphor);
        g.drawRect (bar, 1.0f);
        if (isOn ("hold")) { g.setColour (kPal.cream.withAlpha (0.85f)); g.fillRect (cx - barW * 0.5f, yOf (bandHold[(size_t) i]) - 1.0f, barW, 2.0f); }
        if (compare) { g.setColour (kSnapCol); g.fillRect (cx - barW * 0.5f - 2.0f, yOf (snapBand[(size_t) i]) - 1.5f, barW + 4.0f, 3.0f); }
    }
    if (compare)
    {
        ui::drawText (g, "LIVE", pl.getRight() - 90.0f, pl.getY() + 12.0f, 13.0f, kPal.phosphor, true, 0.0f, juce::Justification::centredRight, ui::Face::Mono);
        ui::drawText (g, "SNAPSHOT", pl.getRight() - 90.0f, pl.getY() + 28.0f, 13.0f, kSnapCol, true, 0.0f, juce::Justification::centredRight, ui::Face::Mono);
    }
    if (hoverP.x >= 0.0f)
    {
        const int i = juce::jlimit (0, numBands - 1, (int) ((hoverP.x - pl.getX()) / slot));
        g.setColour (kPal.cream.withAlpha (0.4f));
        g.drawRect (juce::Rectangle<float> (slot, pl.getHeight()).withPosition (pl.getX() + slot * (float) i, pl.getY()), 1.0f);
    }
}

// the centre frequencies under the bars (the octave centres; drawn outside the clipped plot)
void SA1AudioProcessorEditor::paintBandLabels (juce::Graphics& g, juce::Rectangle<float> pl)
{
    const bool third = choice ("bands") == 0;
    const float slot = pl.getWidth() / (float) numBands;
    for (int i = 0; i < numBands; ++i)
    {
        const float fc = third ? kThird[i] : kOctave[i];
        bool labelled = ! third;
        if (third) for (const float o : kOctave) if (std::abs (o - fc) < 0.5f) labelled = true;
        if (labelled)
            ui::drawText (g, fc >= 1000.0f ? juce::String (fc / 1000.0f, fc == 1000.0f || fc == 2000.0f || fc == 4000.0f || fc == 8000.0f || fc == 16000.0f ? 0 : 1) + "k" : (fc == 31.5f ? juce::String ("31") : juce::String ((int) fc)),
                          pl.getX() + slot * ((float) i + 0.5f), pl.getBottom() + 14.0f, 13.0f, kPal.phosphorDim, false, 0.0f, juce::Justification::centred, ui::Face::Mono);
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
    const int mode = choice ("mode");
    const bool compare = isOn ("compare") && hasSnapshot();
    const auto signedDb = [] (float v) { return (v >= 0.0f ? "+" : "") + juce::String (v, 1); };
    if (hoverP.x < 0.0f || ! pl.expanded (0.0f, 30.0f).contains (hoverP)) line ("CURSOR --", kPal.phosphorDim);
    else if (mode == 2)                                                                    // a band
    {
        const int i = juce::jlimit (0, numBands - 1, (int) ((hoverP.x - pl.getX()) / (pl.getWidth() / (float) numBands)));
        const float fc = choice ("bands") == 0 ? kThird[i] : kOctave[i], v = bandDb[(size_t) i];
        line ("BAND " + hzText (fc) + "  " + (v > -150.0f ? juce::String (v, 1) + " dB" : juce::String ("-inf")) + (compare ? " (" + signedDb (v - snapBand[(size_t) i]) + ")" : juce::String()), kPal.cream);
    }
    else
    {
        const float col = mode == 1 ? (pl.getBottom() - hoverP.y) / pl.getHeight() * (float) kCols : (hoverP.x - pl.getX()) / pl.getWidth() * (float) kCols;
        const float hz = colFreq (juce::jlimit (0.0f, (float) kCols - 1.0f, col));
        const int ci = juce::jlimit (0, kCols - 1, (int) col);
        const float v = colDb[(size_t) ci];
        line ("CUR " + hzText (hz) + " " + noteName (hz) + " " + (v > -150.0f ? juce::String (v, 1) : juce::String ("-inf")) + (compare && mode == 0 ? " (" + signedDb (v - snapDb[(size_t) ci]) + ")" : juce::String()), kPal.cream);
    }
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
    ui::paintTabs (g, kPal, panelRect (0), kModeTabs, juce::jlimit (0, 2, choice ("mode")), -15.0f, 16.0f, kTabsLeft);
    paintDisplay (g);
    paintReadout (g);

    const bool snap = hasSnapshot();                                                        // SNAPSHOT / COMPARE / CLEAR (COMPARE and CLEAR need a snapshot)
    ui::paintFlatButton (g, kPal, snapButton (0), kSnapName[0], false, 13.0f);
    ui::paintFlatButton (g, kPal, snapButton (1), kSnapName[1], isOn ("compare") && snap, 13.0f, snap ? 1.0f : 0.4f);
    ui::paintFlatButton (g, kPal, snapButton (2), kSnapName[2], false, 13.0f, snap ? 1.0f : 0.4f);

    for (int panel = 1; panel <= 2; ++panel)
    {
        const auto& rs = rows (panel, choice ("mode"));
        for (int r = 0; r < (int) rs.size(); ++r)
        {
            const auto& row = rs[(size_t) r];
            const auto first = optionRect (panel, r, 0, (int) row.names.size());
            ui::drawText (g, row.label, panelRect (panel).getX() + 20.0f, first.getCentreY(), 15.0f, kPal.ink, true, 0.1f, juce::Justification::centredLeft);
            for (int i = 0; i < (int) row.names.size(); ++i)
                ui::paintFlatButton (g, kPal, optionRect (panel, r, i, (int) row.names.size()), row.names[(size_t) i], choice (row.id) == i, 15.0f);
        }
    }
    for (int i = 0; i < 3; ++i) ui::paintFlatButton (g, kPal, toggleRect (i), kToggleName[i], isOn (kToggleId[i]), 15.0f, i == 2 && choice ("mode") != 0 ? 0.4f : 1.0f);      // HARMONICS is for the spectrum
}

//==============================================================================
void SA1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / scale();
    proc.undoManager.beginNewTransaction();
    if (ui::powerRect().expanded (3.0f).contains (p)) { toggle ("power"); return; }
    if (ui::presetBarMouseDown (p, *this, proc.presets, proc.undoManager, scale(), [this] { repaint(); })) return;

    const auto tabs = modeTabRects();
    for (size_t i = 0; i < tabs.size(); ++i)
        if (tabs[i].contains (p)) { setChoice ("mode", (int) i); specTick = 0; repaint(); return; }

    if (snapButton (0).expanded (2.0f).contains (p))                                        // SNAPSHOT: keep the averaged spectrum as it is now
    {
        proc.snapPow = avgPow; proc.snapBinHz = binHz;
        repaint();
        return;
    }
    if (snapButton (1).expanded (2.0f).contains (p)) { if (hasSnapshot()) toggle ("compare"); repaint(); return; }
    if (snapButton (2).expanded (2.0f).contains (p))
    {
        proc.snapPow.clear(); proc.snapBinHz = 0.0f;
        if (isOn ("compare")) toggle ("compare");
        repaint();
        return;
    }

    for (int panel = 1; panel <= 2; ++panel)
    {
        const auto& rs = rows (panel, choice ("mode"));
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
    hoverP = plotRect().expanded (0.0f, 30.0f).contains (p) ? p : juce::Point<float> (-1.0f, -1.0f);
}

void SA1AudioProcessorEditor::mouseExit (const juce::MouseEvent&) { hoverP = { -1.0f, -1.0f }; }
