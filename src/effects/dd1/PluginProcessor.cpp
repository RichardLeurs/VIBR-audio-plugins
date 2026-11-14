// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout DD1AudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    const auto knob = [&] (const juce::String& id, const juce::String& name, float def)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 10.0f, 0.01f), def));
    };
    const auto delay = [&] (const juce::String& p, const juce::String& n, int div, float time, float rep, float level)
    {
        layout.add (std::make_unique<AudioParameterInt> (ParameterID { p + "Div", 1 }, n + " Division", 0, dd1::kNumDivisions - 1, div));
        knob (p + "Time",  n + " Time", time);
        knob (p + "Rep",   n + " Repeats", rep);
        knob (p + "Level", n + " Level", level);
        knob (p + "Mod",   n + " Mod Depth", 2.0f);
        knob (p + "Rate",  n + " Mod Rate", 3.0f);
    };
    delay ("a", "Delay A", 4, 5.0f, 4.0f, 6.0f);
    delay ("b", "Delay B", 3, 5.0f, 3.0f, 4.0f);      // B division = ratio of Delay A (3/4 of A = dotted 1/8 when A is 1/4)
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "boostOn", 1 }, "Boost On", false));
    knob ("drive",      "Boost Drive", 3.0f);
    knob ("boostTone",  "Boost Tone", 6.0f);
    knob ("boostLevel", "Boost Level", 5.0f);
    knob ("aLowCut", "Delay A Repeat Low Cut", 2.0f);
    knob ("aHiCut",  "Delay A Repeat Hi Cut", 6.0f);
    knob ("lowCut", "Delay B Repeat Low Cut", 2.0f);       // ids kept from the first (shared) filter
    knob ("hiCut",  "Delay B Repeat Hi Cut", 6.0f);
    knob ("chRate",  "Chorus Rate", 3.0f);
    knob ("chDepth", "Chorus Depth", 5.0f);
    knob ("chLevel", "Chorus Level", 0.0f);
    knob ("rvDecay", "Reverb Decay", 5.0f);
    knob ("rvTone",  "Reverb Tone", 5.0f);
    knob ("rvLevel", "Reverb Level", 0.0f);
    knob ("rDecay", "Shimmer Decay", 5.0f);
    knob ("rTone",  "Shimmer Tone", 5.0f);
    knob ("rShim",  "Shimmer Amount", 5.0f);
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "rInt", 1 }, "Shimmer Interval", 0, 4, 2));
    knob ("rLevel", "Shimmer Level", 3.0f);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "tempo", 1 }, "Tempo (tap)", NormalisableRange<float> (30.0f, 300.0f, 0.1f), 120.0f, AudioParameterFloatAttributes().withLabel ("bpm")));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "hostSync", 1 }, "Follow Host Tempo (off = Tap)", false));
    knob ("spread", "Stereo Spread", 5.0f);
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "pingA", 1 }, "Delay A Ping-Pong", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "pingB", 1 }, "Delay B Ping-Pong", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "series", 1 }, "Routing Series (off = Parallel)", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "bypass", 1 }, "Bypass", false));
    return layout;
}

DD1AudioProcessor::DD1AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "DD1", createLayout())
{
    seedFactoryMemories();
    const auto bind = [this] (DelayParams& d, const char* p)
    {
        const juce::String s (p);
        d.div = apvts.getRawParameterValue (s + "Div");   d.time = apvts.getRawParameterValue (s + "Time");
        d.rep = apvts.getRawParameterValue (s + "Rep");   d.level = apvts.getRawParameterValue (s + "Level");
        d.mod = apvts.getRawParameterValue (s + "Mod");   d.rate = apvts.getRawParameterValue (s + "Rate");
    };
    bind (pa, "a"); bind (pb, "b");
    pDecay = apvts.getRawParameterValue ("rDecay");  pTone = apvts.getRawParameterValue ("rTone");
    pShim  = apvts.getRawParameterValue ("rShim");   pInt = apvts.getRawParameterValue ("rInt");
    pRevLevel = apvts.getRawParameterValue ("rLevel");
    pSeries = apvts.getRawParameterValue ("series");
    pSpread = apvts.getRawParameterValue ("spread");
    pTempo = apvts.getRawParameterValue ("tempo");
    pBoostOn = apvts.getRawParameterValue ("boostOn");
    pDrive = apvts.getRawParameterValue ("drive");
    pBoostTone = apvts.getRawParameterValue ("boostTone");
    pBoostLevel = apvts.getRawParameterValue ("boostLevel");
    pChRate = apvts.getRawParameterValue ("chRate");   pChDepth = apvts.getRawParameterValue ("chDepth");
    pChLevel = apvts.getRawParameterValue ("chLevel");
    pRvDecay = apvts.getRawParameterValue ("rvDecay"); pRvTone = apvts.getRawParameterValue ("rvTone");
    pRvLevel = apvts.getRawParameterValue ("rvLevel");
    pLowCutA = apvts.getRawParameterValue ("aLowCut");
    pHiCutA = apvts.getRawParameterValue ("aHiCut");
    pLowCut = apvts.getRawParameterValue ("lowCut");
    pHiCut = apvts.getRawParameterValue ("hiCut");
    pHostSync = apvts.getRawParameterValue ("hostSync");
    pPingA = apvts.getRawParameterValue ("pingA");
    pPingB = apvts.getRawParameterValue ("pingB");
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("bypass"));
}

bool DD1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) && out == layouts.getMainInputChannelSet();
}

void DD1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sampleRateHz = sampleRate;
    delayA.prepare ((float) sampleRate, 0.0f);
    delayB.prepare ((float) sampleRate, 2.1f);
    cleanA.prepare ((float) sampleRate, 0.0f);
    cleanB.prepare ((float) sampleRate, 0.0f);
    delayA.setFilter (40.0f, 4700.0f); delayB.setFilter (40.0f, 4700.0f);
    cleanA.setFilter (40.0f, 4700.0f); cleanB.setFilter (40.0f, 4700.0f);
    reverb.prepare ((float) sampleRate);
    hall.prepare ((float) sampleRate);
    chorus.prepare ((float) sampleRate);
    const auto cap = (size_t) juce::jmax (samplesPerBlock, 1);
    revL.assign (cap, 0.0f); revR.assign (cap, 0.0f); dlL.assign (cap, 0.0f); dlR.assign (cap, 0.0f); hallL.assign (cap, 0.0f); hallR.assign (cap, 0.0f);

    // settle the reverb's type / mix ramps so no dry signal leaks through at the start
    std::vector<float> z (4096, 0.0f), z2 (4096, 0.0f);
    bigsky::Params rp; rp.type = bigsky::Shimmer; rp.mix = 1.0f;
    reverb.process (z.data(), z2.data(), (int) z.size(), rp);
    rp.type = bigsky::Hall;
    hall.process (z.data(), z2.data(), (int) z.size(), rp);

    levA = levB = levR = levCh = levH = 0.0f;
    boostL = {}; boostR = {}; boostG = 0.0f;
    spreadS = pSpread->load() * 0.1f;
    engage = bypassParam->get() ? 0.0f : 1.0f;
}

void DD1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int total = buffer.getNumSamples();
    const int channels = juce::jmin (buffer.getNumChannels(), 2);
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, total);

    // tempo: the tapped value, or the host's when "host" is selected and the host provides one
    double bpm = (double) pTempo->load();
    if (pHostSync->load() > 0.5f)
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
                if (auto hb = pos->getBpm()) if (*hb > 20.0) bpm = *hb;
    bpmNow.store ((float) bpm);
    const float fs = (float) sampleRateHz;
    const float maxT = (dd1::kMaxDelaySeconds - 0.05f) * fs;
    const float quarterMs = (float) (60000.0 / bpm);

    // Delay A: FREE = the Time knob; a note division = that fraction of the tempo, nudged +-25% by the Time knob
    // (centre = exact). Delay B has no time knob: its selector is a ratio of Delay A's time (1/1 = the same as A).
    const int divA = (int) std::lround (pa.div->load()), divB = (int) std::lround (pb.div->load());
    const float tA = pa.time->load();
    const float msA = dd1::divisionBeats (divA) > 0.0f ? dd1::divisionBeats (divA) * quarterMs * (1.0f + (tA - 5.0f) * 0.05f)
                                                       : dd1::freeTimeMs (tA * 0.1f);
    const float msB = msA * dd1::ratioOfA (divB);

    struct Setup { float time, rep, level, depth, rate; };
    const auto setup = [&] (const DelayParams& d, float ms)
    {
        return Setup { juce::jmin (ms * 0.001f * fs, maxT), d.rep->load() * 0.1f * 1.05f,
                       d.level->load() * 0.1f, dd1::modDepthMs (d.mod->load() * 0.1f) * 0.001f * fs,
                       dd1::modRateHz (d.rate->load() * 0.1f) };
    };
    const Setup sa = setup (pa, msA), sb = setup (pb, msB);
    delayA.setFilter (dd1::lowCutHz (pLowCutA->load() * 0.1f), dd1::hiCutHz (pHiCutA->load() * 0.1f));      // each delay has its own repeat filter
    delayB.setFilter (dd1::lowCutHz (pLowCut->load() * 0.1f), dd1::hiCutHz (pHiCut->load() * 0.1f));
    cleanA.setFilter (dd1::lowCutHz (pLowCutA->load() * 0.1f), dd1::hiCutHz (pHiCutA->load() * 0.1f));
    cleanB.setFilter (dd1::lowCutHz (pLowCut->load() * 0.1f), dd1::hiCutHz (pHiCut->load() * 0.1f));
    timeMs[0].store (sa.time / fs * 1000.0f);
    timeMs[1].store (sb.time / fs * 1000.0f);
    const bool series = pSeries->load() > 0.5f;
    const bool pingA = pPingA->load() > 0.5f, pingB = pPingB->load() > 0.5f;
    const float spreadT = pSpread->load() * 0.1f;
    const float engageT = bypassParam->get() ? 0.0f : 1.0f;
    const float boostT = (pBoostOn->load() > 0.5f && ! bypassParam->get()) ? 1.0f : 0.0f;
    const float bGain = dd1::driveGain (pDrive->load() * 0.1f), bLp = 1.0f - bigsky::coef (dd1::boostToneHz (pBoostTone->load() * 0.1f), fs);
    const float bHp = bigsky::coef (90.0f, fs), bOut = dd1::boostLevelGain (pBoostLevel->load() * 0.1f);      // bypass stops new sound entering; tails ring out

    bigsky::Params rp;
    rp.type = bigsky::Shimmer; rp.mix = 1.0f; rp.value = 0.5f; rp.mod = 0.15f; rp.predelay = 0.0f;
    rp.decay = pDecay->load() * 0.1f; rp.tone = pTone->load() * 0.1f; rp.p1 = pShim->load() * 0.1f;
    rp.p2 = ((float) std::lround (pInt->load()) + 0.5f) / 5.0f;
    bigsky::Params hp;                                           // the normal reverb: a hall
    hp.type = bigsky::Hall; hp.mix = 1.0f; hp.value = 0.5f; hp.mod = 0.5f; hp.predelay = 0.2f;
    hp.decay = pRvDecay->load() * 0.1f; hp.tone = pRvTone->load() * 0.1f; hp.p1 = 0.5f; hp.p2 = 0.4f;
    const float levHT = pRvLevel->load() * 0.12f;
    const float chRate = dd1::chorusRateHz (pChRate->load() * 0.1f), chDepth = pChDepth->load() * 0.1f, levChT = pChLevel->load() * 0.1f;
    const float levAT = sa.level, levBT = sb.level, levRT = pRevLevel->load() * 0.12f;

    const int cap = (int) revL.size();
    auto* l = buffer.getWritePointer (0);
    auto* r = channels > 1 ? buffer.getWritePointer (1) : l;
    for (int off = 0; off < total; off += cap)
    {
        const int n = juce::jmin (cap, total - off);
        float* pl = l + off;
        float* pr = r + off;
        for (int i = 0; i < n; ++i)
        {
            // boost / drive in front of everything (also colours the dry signal)
            boostG += 0.002f * (boostT - boostG);
            if (boostG > 0.0005f)
            {
                const float il = pl[i];
                pl[i] = il + boostG * (boostL.process (il, bGain, bHp, bLp, bOut) - il);
                if (channels > 1)
                {
                    const float ir = pr[i];
                    pr[i] = ir + boostG * (boostR.process (ir, bGain, bHp, bLp, bOut) - ir);
                }
            }
            engage += 0.002f * (engageT - engage);
            // chorus in front of the delays (colours the dry signal too): in + level * chorus voices
            levCh += 0.002f * (levChT * engage - levCh);
            if (levCh > 0.0005f)
            {
                float wl, wr;
                chorus.process (pl[i], channels > 1 ? pr[i] : pl[i], chDepth, chRate, wl, wr);
                pl[i] += 0.8f * levCh * wl;
                if (channels > 1) pr[i] += 0.8f * levCh * wr;
            }
            else
            {
                float wl, wr;
                chorus.process (pl[i], channels > 1 ? pr[i] : pl[i], chDepth, chRate, wl, wr);   // keep the lines running
            }
            levA += 0.002f * (levAT - levA); levB += 0.002f * (levBT - levB);
            spreadS += 0.002f * (spreadT - spreadS);
            const float x = 0.5f * (pl[i] + pr[i]) * engage;
            float a1, a2, b1, b2;
            delayA.process (x, sa.time, sa.rep, sa.depth, sa.rate, pingA, a1, a2);
            const float aMono = pingA ? 0.7f * (a1 + a2) : a1;
            delayB.process (series ? aMono : x, sb.time, sb.rep, sb.depth, sb.rate, pingB, b1, b2);

            // stereo placement: mono delays are panned by Spread (A left, B right); ping-pong delays bounce L/R,
            // Spread sets how wide (A starts on the left, B on the right)
            const float gAL = std::cos ((1.0f - spreadS) * 0.25f * juce::MathConstants<float>::pi), gAR = std::sin ((1.0f - spreadS) * 0.25f * juce::MathConstants<float>::pi);
            float aL, aR, bL, bR;
            if (pingA) { aL = a1 * (0.5f + 0.5f * spreadS) + a2 * (0.5f - 0.5f * spreadS); aR = a2 * (0.5f + 0.5f * spreadS) + a1 * (0.5f - 0.5f * spreadS); }
            else       { aL = a1 * gAL; aR = a1 * gAR; }
            if (pingB) { bR = b1 * (0.5f + 0.5f * spreadS) + b2 * (0.5f - 0.5f * spreadS); bL = b2 * (0.5f + 0.5f * spreadS) + b1 * (0.5f - 0.5f * spreadS); }
            else       { bL = b1 * gAR; bR = b1 * gAL; }
            dlL[(size_t) i] = aL * levA + bL * levB;
            dlR[(size_t) i] = aR * levA + bR * levB;
            // the shimmer hears dry + the CLEAN echoes (same timing and repeats, but no modulation = no wobble)
            float ca1, ca2, cb1, cb2;
            cleanA.process (x, sa.time, sa.rep, 0.0f, sa.rate, false, ca1, ca2);
            cleanB.process (series ? ca1 : x, sb.time, sb.rep, 0.0f, sb.rate, false, cb1, cb2);
            revL[(size_t) i] = revR[(size_t) i] = x + ca1 * levA + cb1 * levB;
            hallL[(size_t) i] = hallR[(size_t) i] = revL[(size_t) i];
        }
        reverb.process (revL.data(), revR.data(), n, rp);
        hall.process (hallL.data(), hallR.data(), n, hp);
        for (int i = 0; i < n; ++i)
        {
            levR += 0.002f * (levRT - levR);
            levH += 0.002f * (levHT - levH);
            const float wl = dlL[(size_t) i] + revL[(size_t) i] * levR + hallL[(size_t) i] * levH;
            const float wr = dlR[(size_t) i] + revR[(size_t) i] * levR + hallR[(size_t) i] * levH;
            if (channels > 1) { pl[i] += wl; pr[i] += wr; }
            else pl[i] += 0.5f * (wl + wr);
        }
    }
}

//==============================================================================
// Factory presets: only the differences from the default settings are listed.
namespace
{
using PV = std::vector<std::pair<const char*, float>>;
struct Preset { const char* name; PV values; };

// aDiv: 0 FREE, 1 1/1, 2 1/2, 3 1/4., 4 1/4, 5 1/4T, 6 1/8., 7 1/8, 8 1/8T, 9 1/16.
// bDiv (ratio of A): 0 2/1, 1 3/2, 2 1/1, 3 3/4, 4 2/3, 5 1/2, 6 3/8, 7 1/3, 8 1/4, 9 1/8.   A = 1/4 with B = 3/4 is the dotted eighth.
const std::vector<Preset>& presets()
{
    static const std::vector<Preset> list = {
        { "CLEAN",         { { "aLevel", 0.0f }, { "bLevel", 0.0f }, { "rLevel", 0.0f } } },
        { "STREETS",       { { "aDiv", 4 }, { "bDiv", 3 }, { "aRep", 3.5f }, { "bRep", 4.5f }, { "aLevel", 5.0f }, { "bLevel", 6.5f },
                             { "aMod", 1.2f }, { "bMod", 1.2f }, { "aLowCut", 3.0f }, { "lowCut", 3.0f }, { "aHiCut", 7.0f }, { "hiCut", 7.0f },
                             { "spread", 6.0f }, { "boostOn", 1 }, { "drive", 3.5f }, { "boostTone", 6.5f }, { "boostLevel", 5.5f },
                             { "rLevel", 0.0f }, { "rvLevel", 1.2f }, { "rvDecay", 3.5f } } },
        { "PRIDE",         { { "aDiv", 4 }, { "bDiv", 3 }, { "aRep", 4.0f }, { "bRep", 5.0f }, { "aLevel", 5.0f }, { "bLevel", 6.0f },
                             { "aMod", 1.5f }, { "bMod", 1.5f }, { "pingB", 1 }, { "spread", 8.0f }, { "boostOn", 1 }, { "drive", 3.0f },
                             { "chLevel", 2.0f }, { "chDepth", 3.0f }, { "chRate", 2.5f }, { "rLevel", 0.0f }, { "rvLevel", 1.5f } } },
        { "SUNDAY",        { { "aDiv", 6 }, { "bDiv", 4 }, { "aRep", 3.0f }, { "bRep", 3.0f }, { "aLevel", 5.0f }, { "bLevel", 5.0f },
                             { "aMod", 0.0f }, { "bMod", 0.0f }, { "aHiCut", 7.5f }, { "hiCut", 7.5f }, { "spread", 4.0f },
                             { "boostOn", 1 }, { "drive", 5.0f }, { "boostLevel", 5.0f }, { "rLevel", 0.0f }, { "rvLevel", 0.8f } } },
        { "NEW YEAR",      { { "aDiv", 4 }, { "bDiv", 3 }, { "aRep", 3.0f }, { "bRep", 4.0f }, { "aLevel", 5.0f }, { "bLevel", 5.0f },
                             { "aMod", 0.8f }, { "bMod", 0.8f }, { "aLowCut", 4.0f }, { "lowCut", 4.0f }, { "aHiCut", 8.0f }, { "hiCut", 8.0f },
                             { "pingA", 1 }, { "pingB", 1 }, { "spread", 9.0f }, { "boostOn", 1 }, { "drive", 4.0f }, { "rLevel", 0.0f }, { "rvLevel", 1.0f } } },
        { "ELEVATION",     { { "aDiv", 4 }, { "bDiv", 3 }, { "aRep", 3.0f }, { "bRep", 3.5f }, { "aLevel", 4.0f }, { "bLevel", 5.0f },
                             { "aMod", 0.5f }, { "bMod", 0.5f }, { "boostOn", 1 }, { "drive", 7.0f }, { "boostTone", 5.0f }, { "boostLevel", 4.5f },
                             { "rLevel", 0.0f }, { "rvLevel", 1.0f } } },
        { "I WILL FOLLOW", { { "aDiv", 7 }, { "bDiv", 3 }, { "aRep", 3.0f }, { "bRep", 3.0f }, { "aLevel", 5.0f }, { "bLevel", 5.0f },
                             { "aMod", 0.0f }, { "bMod", 0.0f }, { "aLowCut", 3.5f }, { "lowCut", 3.5f }, { "boostOn", 1 }, { "drive", 5.0f },
                             { "rLevel", 0.0f }, { "rvLevel", 0.6f } } },
        { "MYSTERIOUS",    { { "aDiv", 4 }, { "bDiv", 3 }, { "aRep", 4.0f }, { "bRep", 4.0f }, { "aLevel", 5.0f }, { "bLevel", 5.5f },
                             { "aMod", 4.0f }, { "bMod", 5.0f }, { "aRate", 3.5f }, { "bRate", 4.5f }, { "chLevel", 4.0f }, { "chDepth", 5.0f },
                             { "chRate", 4.0f }, { "boostOn", 1 }, { "drive", 3.0f }, { "rLevel", 0.0f }, { "rvLevel", 2.0f } } },
        { "BAD",           { { "aDiv", 4 }, { "bDiv", 1 }, { "aRep", 6.0f }, { "bRep", 5.0f }, { "aLevel", 5.0f }, { "bLevel", 4.0f },
                             { "aMod", 3.0f }, { "bMod", 3.0f }, { "aHiCut", 5.0f }, { "hiCut", 5.0f }, { "spread", 7.0f },
                             { "chLevel", 1.5f }, { "rLevel", 3.0f }, { "rDecay", 7.0f }, { "rShim", 6.0f }, { "rInt", 2 },
                             { "rvLevel", 2.5f }, { "rvDecay", 6.0f } } },
        { "UNFORGETTABLE", { { "aDiv", 4 }, { "bDiv", 3 }, { "aRep", 5.5f }, { "bRep", 5.5f }, { "aLevel", 5.0f }, { "bLevel", 5.0f },
                             { "aMod", 3.5f }, { "bMod", 4.0f }, { "aRate", 2.0f }, { "bRate", 2.5f }, { "spread", 8.0f }, { "chLevel", 3.0f },
                             { "chDepth", 6.0f }, { "chRate", 2.0f }, { "boostOn", 1 }, { "drive", 2.0f }, { "rLevel", 5.0f }, { "rDecay", 8.0f },
                             { "rShim", 7.0f }, { "rInt", 2 }, { "rvLevel", 3.0f }, { "rvDecay", 7.0f } } },
    };
    return list;
}
} // namespace

int DD1AudioProcessor::getNumPresets() { return (int) presets().size(); }
const char* DD1AudioProcessor::getPresetName (int index) { return presets()[(size_t) juce::jlimit (0, getNumPresets() - 1, index)].name; }

void DD1AudioProcessor::applyPreset (int index)
{
    index = juce::jlimit (0, getNumPresets() - 1, index);
    const auto& pr = presets()[(size_t) index];
    const auto set = [] (juce::RangedAudioParameter* p, float normalised)
    {
        p->beginChangeGesture(); p->setValueNotifyingHost (normalised); p->endChangeGesture();
    };
    for (auto* base : getParameters())                         // everything back to its default first ...
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (base))
        {
            const auto id = p->paramID;
            if (id == "tempo" || id == "hostSync" || id == "bypass" || id == "bTime") continue;   // not part of a preset
            set (p, p->getDefaultValue());
        }
    for (const auto& kv : pr.values)                           // ... then the preset's own values
        if (auto* p = apvts.getParameter (kv.first))
            set (p, p->convertTo0to1 (kv.second));
    presetIndex.store (index);
}

// The four panel memories start with factory settings (Edge-style), sorted from the fastest to the slowest delay.
// They are written as snapshots into the state (child "memory"), exactly like a user write; tempo, host/tap source,
// delay B time and bypass are not part of a snapshot. Only done when the state has no memory yet.
void DD1AudioProcessor::seedFactoryMemories()
{
    if (apvts.state.getChildWithName ("memory").isValid())
        return;

    struct Slot { const char* preset; PV extra; };
    const Slot slots[4] = {
        { "I WILL FOLLOW", {} },                               // 1/8 + dotted 16th: fastest
        { "SUNDAY", {} },                                      // dotted 1/8
        { "STREETS", {} },                                     // 1/4 + dotted 8th
        { "UNFORGETTABLE", { { "aDiv", 3 } } },                // dotted 1/4, long repeats, shimmer: slowest
    };
    juce::ValueTree memory ("memory");
    memory.setProperty ("current", 0, nullptr);
    for (int i = 0; i < 4; ++i)
    {
        const Preset* pr = nullptr;
        for (const auto& q : presets())
            if (juce::String (q.name) == slots[i].preset) pr = &q;
        if (pr == nullptr) continue;

        juce::ValueTree snap ("slot" + juce::String (i + 1));
        for (auto* base : getParameters())
            if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (base))
            {
                const juce::String id = p->paramID;
                if (id == "tempo" || id == "hostSync" || id == "bypass" || id == "bTime") continue;
                float v = p->getDefaultValue();
                for (const auto* list : { &pr->values, &slots[i].extra })
                    for (const auto& kv : *list)
                        if (id == kv.first) v = p->convertTo0to1 (kv.second);
                snap.setProperty (id, v, nullptr);
            }
        memory.addChild (snap, -1, nullptr);
    }
    apvts.state.addChild (memory, -1, nullptr);
}

juce::AudioProcessorEditor* DD1AudioProcessor::createEditor() { return new DD1AudioProcessorEditor (*this); }

void DD1AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    apvts.state.setProperty ("preset", presetIndex.load(), nullptr);
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void DD1AudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            presetIndex.store (juce::jlimit (0, getNumPresets() - 1, (int) apvts.state.getProperty ("preset", 0)));
            seedFactoryMemories();                             // sessions saved before the memory panel
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DD1AudioProcessor(); }
