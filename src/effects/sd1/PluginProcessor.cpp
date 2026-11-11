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
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "hold", 1 },   "Hold",   false));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "bypass", 1 }, "Bypass", false));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "power", 1 },  "Power",  true));
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
    pHold     = apvts.getRawParameterValue ("hold");
    pBypass   = apvts.getRawParameterValue ("bypass");
    pPower    = apvts.getRawParameterValue ("power");
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
    init (sDelayMs, 0.10, 500.0f);  init (sFeedback, 0.03, 0.0f);   init (sHoldIn, 0.03, 1.0f);
    init (sInGain, 0.03, 1.0f);     init (sDirect, 0.03, 0.6f);     init (sEffect, 0.03, 0.5f);
    init (sActive, 0.02, 1.0f);     init (sModInt, 0.05, 0.0f);     init (sModRate, 0.08, 1.0f);
    init (sCutHz, 0.05, 8000.0f);
    phase = 0.0f;
}

//==============================================================================
void SDD1000AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = juce::jmin (2, buffer.getNumChannels());
    const int numSamples = buffer.getNumSamples();
    for (int c = numCh; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, numSamples);

    const bool hold = *pHold > 0.5f;
    const float timeMs = sdd1000::timeKnobToMs (*pTime) * 0.25f * std::pow (4.0f, knob01 (*pFactor));   // Factor x0.25 .. x1
    const float fbKnob = (*pFeedback - 5.0f) / 5.0f;                                                    // -1 .. +1

    sDelayMs.setTargetValue (juce::jlimit (3.0f, 2048.0f, timeMs));
    sFeedback.setTargetValue (hold ? 0.9995f : fbKnob * 1.02f);   // HOLD freezes the loop; >1 near the ends self-oscillates (bounded by tanh)
    sHoldIn.setTargetValue (hold ? 0.0f : 1.0f);                  // ... and stops new input entering it
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
        const float delayMs = sDelayMs.getNextValue(), fb = sFeedback.getNextValue(), holdIn = sHoldIn.getNextValue();
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

            ch.delay.write (x * holdIn + regen);

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
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void SDD1000AudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SDD1000AudioProcessor();
}
