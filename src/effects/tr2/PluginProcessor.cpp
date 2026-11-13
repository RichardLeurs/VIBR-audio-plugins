// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout TR2AudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    const auto knob = [&] (const char* id, const char* name, float def)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 10.0f, 0.01f), def));
    };
    knob ("rate",  "Rate",  4.0f);
    knob ("depth", "Depth", 6.0f);
    knob ("wave",  "Wave",  0.0f);
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "bypass", 1 }, "Bypass", false));
    return layout;
}

TR2AudioProcessor::TR2AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TR2", createLayout())
{
    pRate  = apvts.getRawParameterValue ("rate");
    pDepth = apvts.getRawParameterValue ("depth");
    pWave  = apvts.getRawParameterValue ("wave");
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("bypass"));
}

bool TR2AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) && out == layouts.getMainInputChannelSet();
}

void TR2AudioProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    phase = 0.0;
    gainState = 1.0f;
    rateSmooth.reset (sampleRate, 0.03);  rateSmooth.setCurrentAndTargetValue (pRate->load() * 0.1f);
    depthSmooth.reset (sampleRate, 0.03); depthSmooth.setCurrentAndTargetValue (pDepth->load() * 0.1f);
    waveSmooth.reset (sampleRate, 0.03);  waveSmooth.setCurrentAndTargetValue (pWave->load() * 0.1f);
    wetSmooth.reset (sampleRate, 0.01);   wetSmooth.setCurrentAndTargetValue (bypassParam->get() ? 0.0f : 1.0f);
}

void TR2AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int channels = juce::jmin (buffer.getNumChannels(), kMaxChannels);
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, n);
    if (n == 0 || channels == 0) return;

    rateSmooth.setTargetValue (pRate->load() * 0.1f);
    depthSmooth.setTargetValue (pDepth->load() * 0.1f);
    waveSmooth.setTargetValue (pWave->load() * 0.1f);
    wetSmooth.setTargetValue (bypassParam->get() ? 0.0f : 1.0f);

    const float smoothCoef = std::exp (-1.0f / (0.0015f * (float) sr));      // 1.5 ms: takes the click off a square wave
    for (int i = 0; i < n; ++i)
    {
        const float rate = rateSmooth.getNextValue(), depth = depthSmooth.getNextValue(), wave = waveSmooth.getNextValue(), bypassMix = wetSmooth.getNextValue();
        phase += (double) tr2::rateHz (rate) / sr;
        if (phase >= 1.0) phase -= 1.0;
        const float target = tr2::gain (tr2::lfo ((float) phase, wave), depth);
        gainState = smoothCoef * gainState + (1.0f - smoothCoef) * target;

        for (int c = 0; c < channels; ++c)
        {
            auto* x = buffer.getWritePointer (c);
            const float in = x[i];
            x[i] = in + (in * gainState - in) * bypassMix;
        }
    }
}

juce::AudioProcessorEditor* TR2AudioProcessor::createEditor() { return new TR2AudioProcessorEditor (*this); }

void TR2AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void TR2AudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TR2AudioProcessor(); }
