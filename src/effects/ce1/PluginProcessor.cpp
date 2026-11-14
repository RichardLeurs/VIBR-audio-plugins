// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr float kTwoPi = juce::MathConstants<float>::twoPi;
float knobRange (float v) { return juce::jlimit (0.0f, 1.0f, v / 10.0f); }
} // namespace

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout CE1AudioProcessor::createLayout()
{
    using FP = juce::AudioParameterFloat;
    using NR = juce::NormalisableRange<float>;
    juce::AudioProcessorValueTreeState::ParameterLayout l;

    auto knob = [&] (const char* id, const char* name, float def)
    {
        l.add (std::make_unique<FP> (juce::ParameterID { id, 1 }, name, NR (0.0f, 10.0f, 0.01f), def));
    };
    knob ("level",           "Level Control",    6.0f);
    knob ("chorusIntensity", "Chorus Intensity", 5.0f);
    knob ("vibratoDepth",    "Vibrato Depth",    5.0f);
    knob ("vibratoRate",     "Vibrato Rate",     5.0f);
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "vibrato", 1 },   "Effect (off = Chorus, on = Vibrato)", false));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "inputHigh", 1 }, "Input (off = Low, on = High)", true));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "power", 1 },     "Power", true));
    l.add (vibr::makeBypassParam());
    return l;
}

CE1AudioProcessor::CE1AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    pLevel     = apvts.getRawParameterValue ("level");
    pChorus    = apvts.getRawParameterValue ("chorusIntensity");
    pDepth     = apvts.getRawParameterValue ("vibratoDepth");
    pRate      = apvts.getRawParameterValue ("vibratoRate");
    pVibrato   = apvts.getRawParameterValue ("vibrato");
    pInputHigh = apvts.getRawParameterValue ("inputHigh");
    pPower     = apvts.getRawParameterValue ("power");
}

bool CE1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) && in == out;
}

//==============================================================================
void CE1AudioProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    for (auto& c : channels)
    {
        c.delay.prepare (sr);
        c.reset();
    }

    auto init = [this] (juce::SmoothedValue<float>& s, double seconds, float v) { s.reset (sr, seconds); s.setCurrentAndTargetValue (v); };
    init (sDry, 0.03, 0.62f);       init (sWet, 0.03, 0.4f);
    init (sCentreMs, 0.08, 6.5f);   init (sDepthMs, 0.05, 1.5f);
    init (sRateHz, 0.08, 0.85f);    init (sInGain, 0.03, 1.0f);
    init (sOutGain, 0.03, 0.8f);    init (sPower, 0.02, *pPower > 0.5f ? 1.0f : 0.0f);

    lpCoef = 1.0f - std::exp (-kTwoPi * 7000.0f / (float) sr);   // BBD-style darkening above ~7 kHz
    phase = 0.0f;
}

//==============================================================================
void CE1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = juce::jmin (2, buffer.getNumChannels());
    const int numSamples = buffer.getNumSamples();
    for (int c = numCh; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, numSamples);

    const bool vibrato = *pVibrato > 0.5f;
    const float chorusI = knobRange (*pChorus);
    const float depthK = knobRange (*pDepth);
    const float rateK = knobRange (*pRate);
    const float level = knobRange (*pLevel);

    if (! vibrato)
    {
        // Chorus: fixed slow rate, depth and wet amount follow Chorus Intensity (Vibrato knobs are not used)
        sCentreMs.setTargetValue (6.5f);
        sDepthMs.setTargetValue (0.5f + 2.8f * chorusI);
        sRateHz.setTargetValue (0.85f);
        sDry.setTargetValue (0.62f);
        sWet.setTargetValue (0.20f + 0.50f * chorusI);
    }
    else
    {
        // Vibrato: 100% wet pitch modulation; Depth up to 3 ms, Rate 0.4 .. 10 Hz (Chorus Intensity is not used)
        sCentreMs.setTargetValue (4.0f);
        sDepthMs.setTargetValue (3.0f * depthK);
        sRateHz.setTargetValue (0.4f * std::pow (25.0f, rateK));
        sDry.setTargetValue (0.0f);
        sWet.setTargetValue (1.0f);
    }
    sInGain.setTargetValue (*pInputHigh > 0.5f ? 1.0f : 0.35f);   // LOW pads the input
    sOutGain.setTargetValue (level * level * 2.2f);
    sPower.setTargetValue (*pPower > 0.5f ? 1.0f : 0.0f);

    float peak = 0.0f;
    auto* data = buffer.getArrayOfWritePointers();
    for (int n = 0; n < numSamples; ++n)
    {
        const float rate = sRateHz.getNextValue();
        phase += kTwoPi * rate / (float) sr;
        if (phase > kTwoPi) phase -= kTwoPi;

        const float centre = sCentreMs.getNextValue(), depth = sDepthMs.getNextValue();
        const float dry = sDry.getNextValue(), wet = sWet.getNextValue();
        const float gIn = sInGain.getNextValue(), gOut = sOutGain.getNextValue(), gPower = sPower.getNextValue();

        for (int c = 0; c < numCh; ++c)
        {
            auto& ch = channels[c];
            const float raw = std::isfinite (data[c][n]) ? data[c][n] : 0.0f;
            const float in = raw * gIn;
            peak = juce::jmax (peak, std::abs (in));

            const float x = std::tanh (in * 1.3f) / 1.3f;                 // input stage
            ch.delay.write (x);

            const float lfo = std::sin (phase + (float) c * 0.5f * juce::MathConstants<float>::pi);   // right channel 90 degrees later
            const float delaySamples = juce::jmax (2.0f, (centre + depth * lfo) * 0.001f * (float) sr);
            float d = ch.delay.read (delaySamples);
            ch.lp1 += lpCoef * (d - ch.lp1);
            ch.lp2 += lpCoef * (ch.lp1 - ch.lp2);
            d = ch.lp2;

            const float y = (dry * x + wet * d) * gOut;
            data[c][n] = raw + (y - raw) * gPower;                         // power = smooth bypass
        }
    }

    if (peak > inputPeak.load()) inputPeak.store (peak);
}

//==============================================================================
juce::AudioProcessorEditor* CE1AudioProcessor::createEditor()
{
    return new CE1AudioProcessorEditor (*this);
}

void CE1AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void CE1AudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CE1AudioProcessor();
}
