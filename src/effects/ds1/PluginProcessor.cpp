// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout DS1AudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    const auto knob = [&] (const char* id, const char* name, float def)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 10.0f, 0.01f), def));
    };
    knob ("tone",  "Tone",  5.0f);
    knob ("level", "Level", 5.0f);
    knob ("dist",  "Dist",  5.0f);
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "bypass", 1 }, "Bypass", false));
    return layout;
}

DS1AudioProcessor::DS1AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "DS1", createLayout())
{
    pTone  = apvts.getRawParameterValue ("tone");
    pLevel = apvts.getRawParameterValue ("level");
    pDist  = apvts.getRawParameterValue ("dist");
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("bypass"));
}

bool DS1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) && out == layouts.getMainInputChannelSet();
}

void DS1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    const int channels = juce::jmax (1, getTotalNumOutputChannels());
    oversampling = std::make_unique<juce::dsp::Oversampling<float>> ((size_t) channels, (size_t) kOsFactor,
                                                                      juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    oversampling->initProcessing ((size_t) samplesPerBlock);
    oversampling->reset();

    const float osRate = (float) (sampleRate * (double) (1 << kOsFactor));
    for (int c = 0; c < kMaxChannels; ++c)
    {
        dist[c].prepare (osRate);
        tone[c].prepare ((float) sampleRate);
    }

    latency = (int) std::ceil (oversampling->getLatencyInSamples());
    setLatencySamples (latency);
    for (auto& d : dryBuf) d.assign ((size_t) juce::jmax (1, latency), 0.0f);
    for (auto& d : dryOut) d.assign ((size_t) samplesPerBlock, 0.0f);
    dryPos = 0;

    gainOs.assign ((size_t) samplesPerBlock << kOsFactor, 1.4f);
    toneBlock.assign ((size_t) samplesPerBlock, 0.5f);
    levelBlock.assign ((size_t) samplesPerBlock, 0.5f);
    wetBlock.assign ((size_t) samplesPerBlock, 1.0f);

    gainSmooth.reset (osRate, 0.03);   gainSmooth.setCurrentAndTargetValue (ds1::distGain (pDist->load() * 0.1f));
    toneSmooth.reset (sampleRate, 0.02);  toneSmooth.setCurrentAndTargetValue (pTone->load() * 0.1f);
    levelSmooth.reset (sampleRate, 0.02); levelSmooth.setCurrentAndTargetValue (ds1::levelGain (pLevel->load() * 0.1f));
    wetSmooth.reset (sampleRate, 0.01);   wetSmooth.setCurrentAndTargetValue (bypassParam->get() ? 0.0f : 1.0f);
}

void DS1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int channels = juce::jmin (buffer.getNumChannels(), kMaxChannels);
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, n);
    if (n == 0 || channels == 0 || oversampling == nullptr || n > (int) toneBlock.size()) return;

    // ---- control signals for the block (smoothed)
    gainSmooth.setTargetValue (ds1::distGain (pDist->load() * 0.1f));
    toneSmooth.setTargetValue (pTone->load() * 0.1f);
    levelSmooth.setTargetValue (ds1::levelGain (pLevel->load() * 0.1f));
    wetSmooth.setTargetValue (bypassParam->get() ? 0.0f : 1.0f);
    for (int i = 0; i < (n << kOsFactor); ++i) gainOs[(size_t) i] = gainSmooth.getNextValue();
    for (int i = 0; i < n; ++i)
    {
        toneBlock[(size_t) i]  = toneSmooth.getNextValue();
        levelBlock[(size_t) i] = levelSmooth.getNextValue();
        wetBlock[(size_t) i]   = wetSmooth.getNextValue();
    }
    const float gEnd = gainOs[(size_t) ((n << kOsFactor) - 1)];
    for (int c = 0; c < channels; ++c) dist[c].opamp.setBandwidth (gEnd);

    // ---- dry copy, delayed by the oversampler's latency (read the oldest sample, then store the new one)
    for (int c = 0; c < channels; ++c)
    {
        auto* x = buffer.getReadPointer (c);
        auto& d = dryBuf[c];
        auto& out = dryOut[c];
        int pos = dryPos;
        for (int i = 0; i < n; ++i)
        {
            if (latency > 0) { out[(size_t) i] = d[(size_t) pos]; d[(size_t) pos] = x[i]; pos = (pos + 1) % latency; }
            else             { out[(size_t) i] = x[i]; }
        }
    }
    if (latency > 0) dryPos = (dryPos + n) % latency;

    // ---- wet path: oversampled nonlinear stage, then tone and level at the host rate
    juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) channels, (size_t) n);
    auto up = oversampling->processSamplesUp (block);
    for (int c = 0; c < channels; ++c)
    {
        auto* u = up.getChannelPointer ((size_t) c);
        for (size_t i = 0; i < up.getNumSamples(); ++i)
            u[i] = dist[c].process (u[i], gainOs[i]);
    }
    oversampling->processSamplesDown (block);

    for (int c = 0; c < channels; ++c)
    {
        auto* x = buffer.getWritePointer (c);
        const auto& dry = dryOut[c];
        for (int i = 0; i < n; ++i)
        {
            const float wet = tone[c].process (x[i], toneBlock[(size_t) i]) * levelBlock[(size_t) i];
            const float w = wetBlock[(size_t) i];
            x[i] = dry[(size_t) i] * (1.0f - w) + wet * w;
        }
    }
}

juce::AudioProcessorEditor* DS1AudioProcessor::createEditor() { return new DS1AudioProcessorEditor (*this); }

void DS1AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void DS1AudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DS1AudioProcessor(); }
