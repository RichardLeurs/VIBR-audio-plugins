// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr float kTwoPi = juce::MathConstants<float>::twoPi;
float knob01 (float v) { return juce::jlimit (0.0f, 1.0f, v / 10.0f); }
} // namespace

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout SDD1000AudioProcessor::createLayout()
{
    using FP = juce::AudioParameterFloat;
    using NR = juce::NormalisableRange<float>;
    juce::AudioProcessorValueTreeState::ParameterLayout l;

    auto knob = [&] (const char* id, const char* name, float def)
    {
        l.add (std::make_unique<FP> (juce::ParameterID { id, 1 }, name, NR (0.0f, 10.0f, 0.01f), def));
    };
    knob ("level",         "Input Level",          5.0f);
    knob ("delayTime",     "Delay Time",           4.0f);
    knob ("factor",        "Time Factor",         10.0f);
    knob ("feedback",      "Regeneration",         6.5f);   // 5 = none, 0 = -10 (inverted), 10 = +10
    knob ("hiCut",         "Hi-Cut",               3.0f);
    knob ("modIntensity",  "Modulation Intensity", 0.0f);
    knob ("modFrequency",  "Modulation Frequency", 3.0f);
    knob ("direct",        "Direct",               7.0f);
    knob ("effect",        "Effect",               5.0f);
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "bypass", 1 }, "Bypass", false));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "power", 1 },  "Power",  true));
    // tempo sync (added later, appended so existing ids / order stay put): off = the TIME / FACTOR knobs work as before;
    // on = delay time = one `division` at the host tempo (vibr::sync::ms), clamped to the 3 .. 2048 ms range of the unit
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "sync", 1 }, "Sync", false));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "division", 1 }, "Division",
                                                         vibr::sync::divisionNames(), vibr::sync::divisionNames().indexOf ("1/8")));
    return l;
}

SDD1000AudioProcessor::SDD1000AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    pLevel    = apvts.getRawParameterValue ("level");
    pTime     = apvts.getRawParameterValue ("delayTime");
    pFactor   = apvts.getRawParameterValue ("factor");
    pFeedback = apvts.getRawParameterValue ("feedback");
    pHiCut    = apvts.getRawParameterValue ("hiCut");
    pModInt   = apvts.getRawParameterValue ("modIntensity");
    pModFreq  = apvts.getRawParameterValue ("modFrequency");
    pDirect   = apvts.getRawParameterValue ("direct");
    pEffect   = apvts.getRawParameterValue ("effect");
    pBypass   = apvts.getRawParameterValue ("bypass");
    pPower    = apvts.getRawParameterValue ("power");
    pSync     = apvts.getRawParameterValue ("sync");
    pDivision = apvts.getRawParameterValue ("division");

    midiMap.setDefault ("modIntensity", 1);           // mod wheel
    midiMap.setDefault ("delayTime", 11);             // expression pedal
    midiMap.setSyncAlias ("delayTime", "sync", "division");
    midiMap.enableTap (85);                          // MIDI tap tempo: any note-on or CC85
}

void SDD1000AudioProcessor::tapAt (double ms)
{
    double iv = 0.0;
    if (! tapAvg.add (ms, iv)) return;
    // the tapped beat sets the delay time: TIME knob x FACTOR (see processBlock); the knob is found by bisection (TIME is monotonic)
    const float factorGain = 0.25f * std::pow (4.0f, juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue ("factor")->load() / 10.0f));
    double t = iv;
    while (t / factorGain > 2048.0) t *= 0.5;
    const float target = (float) t / factorGain;
    float lo = 0.0f, hi = 8.0f;
    for (int i = 0; i < 24; ++i) { const float mid = 0.5f * (lo + hi); (sdd1000::timeKnobToMs (mid) < target ? lo : hi) = mid; }
    if (auto* p = apvts.getParameter ("sync")) if (p->getValue() > 0.5f) { p->beginChangeGesture(); p->setValueNotifyingHost (0.0f); p->endChangeGesture(); }
    if (auto* p = apvts.getParameter ("delayTime")) { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (0.5f * (lo + hi))); p->endChangeGesture(); }
}

bool SDD1000AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) && in == out;
}

//==============================================================================
void SDD1000AudioProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    channels[0].delay.prepare (sr);
    channels[1].delay.prepare (sr);
    for (auto& c : channels) c.reset();

    auto init = [this] (juce::SmoothedValue<float>& s, double seconds, float v) { s.reset (sr, seconds); s.setCurrentAndTargetValue (v); };
    init (sDelayMs, 0.10, 500.0f);  init (sFeedback, 0.03, 0.0f);
    init (sInGain, 0.03, 1.0f);     init (sDirect, 0.03, 0.6f);     init (sEffect, 0.03, 0.5f);
    init (sActive, 0.02, 1.0f);     init (sModInt, 0.05, 0.0f);     init (sModRate, 0.08, 1.0f);
    init (sCutHz, 0.05, 8000.0f);
    phase = 0.0f;
}

//==============================================================================
void SDD1000AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    midiMap.process (midi);
    if (midiMap.consumeTap()) midiTap.hit();
    juce::ScopedNoDenormals noDenormals;
    const int numCh = juce::jmin (2, buffer.getNumChannels());
    const int numSamples = buffer.getNumSamples();
    for (int c = numCh; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, numSamples);

    // TIME: 16 .. 2048 ms (piecewise geometric, see timeKnobToMs) x FACTOR 0..10 -> x0.25 .. x1 (exponential).
    // SYNC on: one division at the host tempo (read once per block, 120 bpm without a host) replaces TIME x FACTOR.
    const float freeMs = sdd1000::timeKnobToMs (*pTime) * 0.25f * std::pow (4.0f, knob01 (*pFactor));
    const float timeMs = *pSync > 0.5f ? (float) vibr::sync::ms (vibr::sync::hostBpm (*this, 120.0), (int) std::lround (pDivision->load()))
                                       : freeMs;
    const float fbKnob = (*pFeedback - 5.0f) / 5.0f;                                                    // -1 .. +1

    sDelayMs.setTargetValue (juce::jlimit (3.0f, 2048.0f, timeMs));            // same clamp + 100 ms glide for both paths
    sFeedback.setTargetValue (fbKnob * 1.02f);                    // >1 near the ends self-oscillates (bounded by tanh)
    sInGain.setTargetValue (*pLevel / 5.0f);
    sDirect.setTargetValue (std::pow (knob01 (*pDirect), 1.6f) * 1.1f);
    sEffect.setTargetValue (std::pow (knob01 (*pEffect), 1.4f) * 1.3f);
    sActive.setTargetValue ((*pPower > 0.5f && *pBypass < 0.5f) ? 1.0f : 0.0f);   // power off / bypass = input passes untouched
    sModInt.setTargetValue (knob01 (*pModInt));
    sModRate.setTargetValue (0.1f * std::pow (100.0f, knob01 (*pModFreq)));          // 0.1 .. 10 Hz
    sCutHz.setTargetValue (20000.0f * std::pow (0.05f, knob01 (*pHiCut)));           // 0 = open (20 kHz), 10 = 1 kHz

    float peak = 0.0f;
    auto* data = buffer.getArrayOfWritePointers();
    for (int n = 0; n < numSamples; ++n)
    {
        const float delayMs = sDelayMs.getNextValue(), fb = sFeedback.getNextValue();
        const float gIn = sInGain.getNextValue(), dry = sDirect.getNextValue(), eff = sEffect.getNextValue();
        const float active = sActive.getNextValue(), modI = sModInt.getNextValue(), rate = sModRate.getNextValue();
        const float cutCoef = 1.0f - std::exp (-kTwoPi * juce::jmin (sCutHz.getNextValue(), 0.45f * (float) sr) / (float) sr);

        phase += kTwoPi * rate / (float) sr;
        if (phase > kTwoPi) phase -= kTwoPi;
        const float depthMs = modI * juce::jmin (6.0f, 0.25f * delayMs);

        for (int c = 0; c < numCh; ++c)
        {
            auto& ch = channels[c];
            const float raw = std::isfinite (data[c][n]) ? data[c][n] : 0.0f;
            const float in = raw * gIn;
            peak = juce::jmax (peak, std::abs (in));
            const float x = std::tanh (in);                                   // converters clip softly past full scale

            const float lfo = std::sin (phase + (float) c * 0.5f * juce::MathConstants<float>::pi);
            const float d = juce::jlimit (2.0f, ch.delay.getMaxDelay(), (delayMs + depthMs * lfo) * 0.001f * (float) sr);
            const float wet = ch.delay.read (d);

            ch.cut1 += cutCoef * (wet - ch.cut1);                             // Hi-Cut in the regeneration path
            ch.cut2 += cutCoef * (ch.cut1 - ch.cut2);
            const float regen = std::tanh (ch.cut2 * fb);                     // bounded feedback (also handles negative = inverted)

            ch.delay.write (x + regen);

            const float y = x * dry + wet * eff;
            data[c][n] = raw + (y - raw) * active;
        }
    }

    const float decay = std::pow (0.88f, (float) numSamples / 512.0f);
    meter = juce::jmax (peak, meter * decay);
    meterLevel.store (meter);
}

//==============================================================================
juce::AudioProcessorEditor* SDD1000AudioProcessor::createEditor()
{
    return new SDD1000AudioProcessorEditor (*this);
}

void SDD1000AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.appendChild (midiMap.toTree(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void SDD1000AudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto st = juce::ValueTree::fromXml (*xml);
            midiMap.fromTree (st.getChildWithName ("MIDIMAP"));
            st.removeChild (st.getChildWithName ("MIDIMAP"), nullptr);
            apvts.replaceState (st);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SDD1000AudioProcessor();
}
