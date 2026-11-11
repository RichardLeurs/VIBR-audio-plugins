// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr float kTwoPi = juce::MathConstants<float>::twoPi;
float knob01 (float v) { return juce::jlimit (0.0f, 1.0f, v / 10.0f); }
} // namespace

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout MemoryManAudioProcessor::createLayout()
{
    using FP = juce::AudioParameterFloat;
    using NR = juce::NormalisableRange<float>;
    juce::AudioProcessorValueTreeState::ParameterLayout l;

    auto knob = [&] (const char* id, const char* name, float def)
    {
        l.add (std::make_unique<FP> (juce::ParameterID { id, 1 }, name, NR (0.0f, 10.0f, 0.01f), def));
    };
    knob ("blend",    "Blend",         5.0f);   // 0 = dry only ... 10 = echo only
    knob ("level",    "Level",         5.0f);
    knob ("feedback", "Feedback",      4.0f);
    knob ("chrsVibr", "Chrs/Vibr",     3.0f);   // modulation rate
    knob ("depth",    "Depth",         2.0f);   // modulation depth
    knob ("delay",    "Delay",         5.0f);
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "effect", 1 }, "Effect On", true));   // footswitch
    return l;
}

MemoryManAudioProcessor::MemoryManAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    pBlend    = apvts.getRawParameterValue ("blend");
    pLevel    = apvts.getRawParameterValue ("level");
    pFeedback = apvts.getRawParameterValue ("feedback");
    pRate     = apvts.getRawParameterValue ("chrsVibr");
    pDepth    = apvts.getRawParameterValue ("depth");
    pDelay    = apvts.getRawParameterValue ("delay");
    pEffect   = apvts.getRawParameterValue ("effect");
}

bool MemoryManAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) && in == out;
}

//==============================================================================
void MemoryManAudioProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    channels[0].delay.prepare (sr);
    channels[1].delay.prepare (sr);
    for (auto& c : channels) c.reset();

    auto init = [this] (juce::SmoothedValue<float>& s, double seconds, float v) { s.reset (sr, seconds); s.setCurrentAndTargetValue (v); };
    init (sDelayMs, 0.12, 250.0f);  init (sFeedback, 0.03, 0.0f);   init (sDry, 0.03, 0.7f);
    init (sWet, 0.03, 0.7f);        init (sOut, 0.03, 0.8f);         init (sActive, 0.02, *pEffect > 0.5f ? 1.0f : 0.0f);
    init (sDepth, 0.05, 0.0f);      init (sRate, 0.08, 1.0f);        init (sCutHz, 0.1, 6000.0f);
    phase = 0.0f;
}

//==============================================================================
void MemoryManAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = juce::jmin (2, buffer.getNumChannels());
    const int numSamples = buffer.getNumSamples();
    for (int c = numCh; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, numSamples);

    // Delay knob: 40 ms .. 550 ms (exponential)
    const float delayMs = 40.0f * std::pow (550.0f / 40.0f, knob01 (pDelay->load()));
    sDelayMs.setTargetValue (delayMs);
    sFeedback.setTargetValue (knob01 (pFeedback->load()) * 1.03f);       // top of the knob self-oscillates (bounded by tanh)
    const float blend = knob01 (pBlend->load());                          // equal-power: dry <-> echo (full CW = vibrato / echo only)
    sDry.setTargetValue (std::cos (blend * 0.5f * juce::MathConstants<float>::pi));
    sWet.setTargetValue (std::sin (blend * 0.5f * juce::MathConstants<float>::pi) * 1.25f);
    sOut.setTargetValue (std::pow (knob01 (pLevel->load()), 1.5f) * 2.2f);
    sActive.setTargetValue (pEffect->load() > 0.5f ? 1.0f : 0.0f);        // footswitch = true bypass
    sDepth.setTargetValue (knob01 (pDepth->load()));
    sRate.setTargetValue (0.15f * std::pow (60.0f, knob01 (pRate->load())));   // 0.15 .. 9 Hz

    float peak = 0.0f;
    auto* data = buffer.getArrayOfWritePointers();
    for (int n = 0; n < numSamples; ++n)
    {
        const float dMs = sDelayMs.getNextValue(), fb = sFeedback.getNextValue();
        const float dry = sDry.getNextValue(), wet = sWet.getNextValue(), gOut = sOut.getNextValue();
        const float active = sActive.getNextValue(), depth = sDepth.getNextValue(), rate = sRate.getNextValue();

        // BBD-style: longer delays = lower clock = darker repeats
        const float cutHz = juce::jlimit (2200.0f, 9000.0f, 14000.0f * std::sqrt (45.0f / dMs));
        sCutHz.setTargetValue (cutHz);
        const float coef = 1.0f - std::exp (-kTwoPi * juce::jmin (sCutHz.getNextValue(), 0.45f * (float) sr) / (float) sr);

        phase += kTwoPi * rate / (float) sr;
        if (phase > kTwoPi) phase -= kTwoPi;
        const float depthMs = depth * depth * juce::jmin (9.0f, 0.30f * dMs);

        for (int c = 0; c < numCh; ++c)
        {
            auto& ch = channels[c];
            const float raw = std::isfinite (data[c][n]) ? data[c][n] : 0.0f;
            peak = juce::jmax (peak, std::abs (raw * 1.3f));
            const float x = std::tanh (raw * 1.3f) / 1.3f;                  // input stage (the Overload LED lights near here)

            const float lfo = std::sin (phase + (float) c * 0.5f * juce::MathConstants<float>::pi);
            const float d = juce::jlimit (2.0f, ch.delay.getMaxDelay(), (dMs + depthMs * lfo) * 0.001f * (float) sr);
            const float echo = ch.delay.read (d);

            ch.lp1 += coef * (echo - ch.lp1);
            ch.lp2 += coef * (ch.lp1 - ch.lp2);
            ch.lp3 += coef * (ch.lp2 - ch.lp3);
            const float filtered = ch.lp3;

            ch.delay.write (x + std::tanh (filtered * fb));                 // regeneration (companded / bounded)

            const float y = (x * dry + filtered * wet) * gOut;
            data[c][n] = raw + (y - raw) * active;
        }
    }

    if (peak > overloadPeak.load()) overloadPeak.store (peak);
}

//==============================================================================
juce::AudioProcessorEditor* MemoryManAudioProcessor::createEditor()
{
    return new MemoryManAudioProcessorEditor (*this);
}

void MemoryManAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void MemoryManAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MemoryManAudioProcessor();
}
