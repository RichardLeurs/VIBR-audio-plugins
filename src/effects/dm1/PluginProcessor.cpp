// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
float knob01 (float v) { return juce::jlimit (0.0f, 1.0f, v / 10.0f); }

// Delay Time: knob 0..10 -> 20 ms .. 2 s, exponential (d = 0.02 * 100^(k/10)); knob 5 = 200 ms
float delayKnobToSec (float k) { return dm1::kMinDelaySec * std::pow (100.0f, knob01 (k)); }
float delaySecToKnob (float s)
{
    s = juce::jlimit (dm1::kMinDelaySec, dm1::kMaxDelaySec, s);
    return 10.0f * std::log (s / dm1::kMinDelaySec) / std::log (100.0f);
}

// Sync: delay time = one note division at the host tempo, limited to 20 ms .. 2 s. A division longer than 2 s (e.g. 1 BAR below
// 120 bpm) falls back to the longest division that still fits, so the repeats stay on the grid instead of being cut to a free 2 s.
float syncedDelaySec (double bpm, int division)
{
    double s = vibr::sync::seconds (bpm, division);
    if (s > (double) dm1::kMaxDelaySec + 1.0e-6)
    {
        double best = 0.0;
        for (int i = 0; i < vibr::sync::kNumDivisions; ++i)
        {
            const double si = vibr::sync::seconds (bpm, i);
            if (si <= (double) dm1::kMaxDelaySec + 1.0e-6 && si > best)
                best = si;
        }
        s = best > 0.0 ? best : (double) dm1::kMaxDelaySec;
    }
    return juce::jlimit (dm1::kMinDelaySec, dm1::kMaxDelaySec, (float) s);
}

const juce::StringArray& modeNames()
{
    static const juce::StringArray names { "Classic", "Vintage", "Modern", "Multi-head", "Non-linear", "Ambience",
                                           "Reflect", "Doubling+Delay", "Wide", "Dual Mod", "Pan", "Pattern" };
    jassert (names.size() == dm1::kNumModes);
    return names;
}

const juce::StringArray& divisionNames()
{
    static const juce::StringArray names { "Half", "Quarter", "Eighth", "Triplet", "Dotted 8th" };
    return names;
}
} // namespace

//==============================================================================
// tapDivision: the 5 LEDs on the panel, left to right.
//   1 = half note       (x2)
//   2 = quarter note    (x1)    <- default
//   3 = eighth note     (x1/2)
//   4 = TRI, 8th triplet (x1/3)
//   5 = DOT, dotted 8th (x3/4)
// The tapped interval is taken as one quarter-note beat.
float DM1AudioProcessor::divisionMultiplier (int division)
{
    switch (division)
    {
        case 1:  return 2.0f;
        case 3:  return 0.5f;
        case 4:  return 1.0f / 3.0f;
        case 5:  return 0.75f;
        default: return 1.0f;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout DM1AudioProcessor::createLayout()
{
    using FP = juce::AudioParameterFloat;
    using NR = juce::NormalisableRange<float>;
    juce::AudioProcessorValueTreeState::ParameterLayout l;

    auto knob = [&] (const char* id, const char* name, float def)
    {
        l.add (std::make_unique<FP> (juce::ParameterID { id, 1 }, name, NR (0.0f, 10.0f, 0.01f), def));
    };

    knob ("modRate",     "Mod Rate",     5.0f);
    knob ("modDepth",    "Mod Depth",    3.0f);
    knob ("variation",   "Variation",    5.0f);
    knob ("delayTime",   "Delay Time",   5.0f);
    knob ("intensity",   "Intensity",    4.0f);
    knob ("delayVolume", "Delay Volume", 5.0f);

    l.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "mode", 1 }, "Mode", 1, dm1::kNumModes, 1,
        juce::AudioParameterIntAttributes().withStringFromValueFunction (
            [] (int v, int) { return modeNames()[juce::jlimit (1, dm1::kNumModes, v) - 1]; })
            .withValueFromStringFunction (
                [] (const juce::String& t)
                {
                    // accepts a mode name (case-insensitive) or its number 1..12
                    const int idx = modeNames().indexOf (t.trim(), true);
                    if (idx >= 0) return idx + 1;
                    return juce::jlimit (1, dm1::kNumModes, t.getIntValue());
                })));

    l.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "tapDivision", 1 }, "Tap Division", 1, 5, 2,
        juce::AudioParameterIntAttributes().withStringFromValueFunction (
            [] (int v, int) { return divisionNames()[juce::jlimit (1, 5, v) - 1]; })));

    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "power", 1 }, "Power", true));

    // Host tempo sync (new): off = the Delay Time knob / TAP work as before; on = delay time = division at the host tempo
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "sync", 1 }, "Sync (delay time follows the host tempo)", false));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "division", 1 }, "Division", vibr::sync::divisionNames(),
                                                         juce::jmax (0, vibr::sync::divisionNames().indexOf ("1/8"))));

    l.add (vibr::makeBypassParam());
    return l;
}

DM1AudioProcessor::DM1AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    pModRate     = apvts.getRawParameterValue ("modRate");
    pModDepth    = apvts.getRawParameterValue ("modDepth");
    pVariation   = apvts.getRawParameterValue ("variation");
    pDelayTime   = apvts.getRawParameterValue ("delayTime");
    pIntensity   = apvts.getRawParameterValue ("intensity");
    pDelayVolume = apvts.getRawParameterValue ("delayVolume");
    pMode        = apvts.getRawParameterValue ("mode");
    pTapDivision = apvts.getRawParameterValue ("tapDivision");
    pPower       = apvts.getRawParameterValue ("power");
    pSync        = apvts.getRawParameterValue ("sync");
    pDivision    = apvts.getRawParameterValue ("division");

    midiMap.setDefault ("modDepth", 1);               // mod wheel
    midiMap.setDefault ("delayTime", 11);             // expression pedal
    midiMap.setSyncAlias ("delayTime", "sync", "division");
    midiMap.enableTap (85);                          // MIDI tap tempo: any note-on or CC85
}

bool DM1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) && in == out;
}

//==============================================================================
// Knob -> DSP unit mapping (all knobs 0..10)
dm1::Params DM1AudioProcessor::readParams() const
{
    dm1::Params p;
    p.mode = juce::jlimit (1, dm1::kNumModes, (int) std::lround (pMode->load()));

    // Delay Time: exponential 20 ms .. 2 s
    p.delaySec = delayKnobToSec (pDelayTime->load());

    // Intensity (feedback): 1.12 * (k/10)^1.2 -> 0 .. 1.12. Loop gain reaches 1 at ~9.1,
    // above that the repeats self-oscillate; the in-loop tanh keeps the level bounded.
    p.feedback = 1.12f * std::pow (knob01 (pIntensity->load()), 1.2f);

    // Delay Volume (wet level): 1.25 * (k/10)^1.5 -> 0 .. +1.9 dB; knob 5 = 0.44 (-7 dB)
    p.wetGain = 1.25f * std::pow (knob01 (pDelayVolume->load()), 1.5f);

    // Mod Rate: exponential 0.05 .. 8 Hz (0.05 * 160^(k/10)); knob 5 = 0.63 Hz
    p.modRateHz = 0.05f * std::pow (160.0f, knob01 (pModRate->load()));

    // Mod Depth: 0 .. 8 ms of delay-time swing, square-law taper; knob 3 = 0.72 ms
    p.modDepthSec = 0.008f * std::pow (knob01 (pModDepth->load()), 2.0f);

    // Variation: 0..1 per-mode macro (see dm1::Mode)
    p.variation = knob01 (pVariation->load());

    p.power = pPower->load() > 0.5f;
    return p;
}

// With sync on the delay time comes from the host tempo (read once per call; no host = the tapped tempo, 120 bpm until tapped)
// and the division. It goes through the same engine smoother as the knob, so tempo changes glide instead of clicking.
dm1::Params DM1AudioProcessor::readParamsSynced()
{
    auto p = readParams();
    if (syncOn())
    {
        const double bpm = vibr::sync::hostBpm (*this, (double) tappedBpm.load());
        const int div = juce::jlimit (0, vibr::sync::kNumDivisions - 1, (int) std::lround (pDivision->load()));
        p.delaySec = syncedDelaySec (bpm, div);
    }
    return p;
}

void DM1AudioProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    engine.setParams (readParamsSynced(), true);
    meterOut = 0.0f;
}

void DM1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    midiMap.process (midi);
    if (midiMap.consumeTap()) midiTap.hit();
    juce::ScopedNoDenormals noDenormals;
    const int numCh = juce::jmin (2, buffer.getNumChannels());
    const int numSamples = buffer.getNumSamples();

    for (int c = numCh; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, numSamples);

    if (numCh == 0 || numSamples == 0)
        return;

    engine.setParams (readParamsSynced());
    engine.process (buffer.getWritePointer (0), numCh > 1 ? buffer.getWritePointer (1) : nullptr, numSamples);

    // tap LED: lit for the first 20 % of each delay period
    tapLedOn.store (engine.getBeatPhase() < 0.2f);

    // metering: block peak with ~0.9 per 512 samples decay
    float peak = 0.0f;
    for (int c = 0; c < numCh; ++c)
        peak = juce::jmax (peak, buffer.getMagnitude (c, 0, numSamples));
    const float decay = std::pow (0.9f, (float) numSamples / 512.0f);
    meterOut = juce::jmax (juce::jmin (1.0f, peak), meterOut * decay);
    outputLevel.store (meterOut);
}

//==============================================================================
void DM1AudioProcessor::tapTempo (double nowSec)
{
    constexpr double kTimeoutSec = 2.5;   // longer gap starts a new tap sequence
    constexpr double kDebounceSec = 0.06; // ignore switch bounce / double clicks

    if (syncOn())
        return;   // the host tempo sets the time

    const double now = nowSec >= 0.0 ? nowSec : juce::Time::getMillisecondCounterHiRes() * 0.001;
    const double dt = now - lastTapSec;

    if (dt < kDebounceSec)
        return;

    lastTapSec = now;

    if (dt > kTimeoutSec)
    {
        numTapIntervals = 0;
        tapWriteIdx = 0;
        return;   // first tap of a sequence: nothing to measure yet
    }

    tapIntervals[tapWriteIdx] = dt;
    tapWriteIdx = (tapWriteIdx + 1) % kMaxTaps;
    numTapIntervals = juce::jmin (numTapIntervals + 1, kMaxTaps);

    double sum = 0.0;
    for (int i = 0; i < numTapIntervals; ++i)
        sum += tapIntervals[i];
    const double beat = sum / (double) numTapIntervals;
    tappedBpm.store ((float) vibr::sync::bpmClamp (60.0 / beat));   // the sync tempo when there is no host tempo (standalone)

    const int division = juce::jlimit (1, 5, (int) std::lround (pTapDivision->load()));
    const float seconds = (float) beat * divisionMultiplier (division);   // clamped to 20 ms .. 2 s below

    if (auto* param = apvts.getParameter ("delayTime"))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 (delaySecToKnob (seconds)));
        param->endChangeGesture();
    }
}

//==============================================================================
juce::AudioProcessorEditor* DM1AudioProcessor::createEditor()
{
    return new DM1AudioProcessorEditor (*this);
}

void DM1AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.appendChild (midiMap.toTree(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void DM1AudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            { auto st = juce::ValueTree::fromXml (*xml);
              midiMap.fromTree (st.getChildWithName ("MIDIMAP"));
              st.removeChild (st.getChildWithName ("MIDIMAP"), nullptr);
              vibr::BypassLink::normalise (st); apvts.replaceState (st); }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DM1AudioProcessor();
}
