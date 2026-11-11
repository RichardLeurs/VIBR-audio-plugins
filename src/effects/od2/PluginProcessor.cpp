// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout OD2AudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    const auto knob = [&] (const char* id, const char* name, float def)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 10.0f, 0.01f), def));
    };
    StringArray names;
    for (int i = 0; i < od2::kNumTypes; ++i) names.add (od2::typeDef (i).name);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "type", 1 }, "Type", names, od2::Overdrive));
    knob ("param",  "Param",  5.0f);
    knob ("drive",  "Drive",  5.0f);
    knob ("level",  "Level",  5.0f);
    knob ("low",    "Low",    5.0f);
    knob ("middle", "Middle", 5.0f);
    knob ("high",   "High",   5.0f);
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "boostOn", 1 }, "Boost On", false));
    knob ("boostPre",   "Boost Pre",   4.0f);       // extra drive before the clipping, up to +12 dB
    knob ("boostPost",  "Boost Post",  3.0f);       // extra level after it, up to +9 dB
    knob ("gateThresh", "Gate Threshold", 0.0f);    // 0 = gate off
    knob ("gateDecay",  "Gate Decay", 5.0f);
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "bypass", 1 }, "Bypass", false));
    return layout;
}

OD2AudioProcessor::OD2AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "OD2", createLayout())
{
    pType = apvts.getRawParameterValue ("type");       pParam = apvts.getRawParameterValue ("param");
    pDrive = apvts.getRawParameterValue ("drive");     pLevel = apvts.getRawParameterValue ("level");
    pLow = apvts.getRawParameterValue ("low");         pMiddle = apvts.getRawParameterValue ("middle");
    pHigh = apvts.getRawParameterValue ("high");       pBoostOn = apvts.getRawParameterValue ("boostOn");
    pBoostPre = apvts.getRawParameterValue ("boostPre");   pBoostPost = apvts.getRawParameterValue ("boostPost");
    pGateThresh = apvts.getRawParameterValue ("gateThresh"); pGateDecay = apvts.getRawParameterValue ("gateDecay");
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("bypass"));
}

bool OD2AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) && out == layouts.getMainInputChannelSet();
}

namespace
{
float driveGainLinear (int type, float drive01, bool boost, float boostPre01)
{
    const auto& d = od2::typeDef (type);
    return od2::dbToGain (d.gainMinDb + (d.gainMaxDb - d.gainMinDb) * drive01 + (boost ? 12.0f * boostPre01 : 0.0f));
}
float outputGain (int type, float level01, bool boost, float boostPost01)
{
    const float l = 2.0f * level01;
    return l * l * od2::dbToGain (od2::typeDef (type).trimDb + (boost ? 9.0f * boostPost01 : 0.0f));
}
}

void OD2AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    const int channels = juce::jmax (1, getTotalNumOutputChannels());
    oversampling = std::make_unique<juce::dsp::Oversampling<float>> ((size_t) channels, (size_t) kOsFactor,
                                                                      juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    oversampling->initProcessing ((size_t) samplesPerBlock);
    oversampling->reset();

    const float osRate = (float) (sampleRate * (double) (1 << kOsFactor));
    for (int c = 0; c < kMaxChannels; ++c) { drive[c].prepare (osRate); tone[c].reset(); gate[c].reset(); }

    latency = (int) std::ceil (oversampling->getLatencyInSamples());
    setLatencySamples (latency);
    for (auto& d : dryBuf) d.assign ((size_t) juce::jmax (1, latency), 0.0f);
    for (auto& d : dryOut) d.assign ((size_t) samplesPerBlock, 0.0f);
    dryPos = 0;

    gainOs.assign ((size_t) samplesPerBlock << kOsFactor, 1.0f);
    levelBlock.assign ((size_t) samplesPerBlock, 1.0f);
    wetBlock.assign ((size_t) samplesPerBlock, 1.0f);

    activeType = juce::jlimit (0, od2::kNumTypes - 1, (int) std::lround (pType->load()));
    typeFade = 1.0f;
    const bool boost = pBoostOn->load() > 0.5f;
    gainSmooth.reset (osRate, 0.03);
    gainSmooth.setCurrentAndTargetValue (driveGainLinear (activeType, pDrive->load() * 0.1f, boost, pBoostPre->load() * 0.1f));
    paramSmooth.reset (sampleRate, 0.05);  paramSmooth.setCurrentAndTargetValue (pParam->load() * 0.1f);
    lowSmooth.reset (sampleRate, 0.03);    lowSmooth.setCurrentAndTargetValue (pLow->load());
    midSmooth.reset (sampleRate, 0.03);    midSmooth.setCurrentAndTargetValue (pMiddle->load());
    highSmooth.reset (sampleRate, 0.03);   highSmooth.setCurrentAndTargetValue (pHigh->load());
    levelSmooth.reset (sampleRate, 0.02);  levelSmooth.setCurrentAndTargetValue (outputGain (activeType, pLevel->load() * 0.1f, boost, pBoostPost->load() * 0.1f));
    wetSmooth.reset (sampleRate, 0.01);    wetSmooth.setCurrentAndTargetValue (bypassParam->get() ? 0.0f : 1.0f);
}

void OD2AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int channels = juce::jmin (buffer.getNumChannels(), kMaxChannels);
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, n);
    if (n == 0 || channels == 0 || oversampling == nullptr || n > (int) levelBlock.size()) return;

    // ---- type changes dip the wet signal to zero over one block, then switch (no click)
    const int wantedType = juce::jlimit (0, od2::kNumTypes - 1, (int) std::lround (pType->load()));
    const bool switching = wantedType != activeType;
    const float fadeStart = typeFade, fadeEnd = switching ? 0.0f : 1.0f;
    typeFade = fadeEnd;

    const bool boost = pBoostOn->load() > 0.5f;
    const float drive01 = pDrive->load() * 0.1f, level01 = pLevel->load() * 0.1f;
    gainSmooth.setTargetValue (driveGainLinear (activeType, drive01, boost, pBoostPre->load() * 0.1f));
    levelSmooth.setTargetValue (outputGain (activeType, level01, boost, pBoostPost->load() * 0.1f));
    paramSmooth.setTargetValue (pParam->load() * 0.1f);
    lowSmooth.setTargetValue (pLow->load()); midSmooth.setTargetValue (pMiddle->load()); highSmooth.setTargetValue (pHigh->load());
    wetSmooth.setTargetValue (bypassParam->get() ? 0.0f : 1.0f);
    for (int i = 0; i < (n << kOsFactor); ++i) gainOs[(size_t) i] = gainSmooth.getNextValue();
    for (int i = 0; i < n; ++i)
    {
        levelBlock[(size_t) i] = levelSmooth.getNextValue() * (fadeStart + (fadeEnd - fadeStart) * (float) (i + 1) / (float) n);
        wetBlock[(size_t) i] = wetSmooth.getNextValue();
    }
    paramSmooth.skip (n); lowSmooth.skip (n); midSmooth.skip (n); highSmooth.skip (n);
    const float param01 = paramSmooth.getCurrentValue();
    for (int c = 0; c < channels; ++c)
    {
        drive[c].configure (activeType, param01);
        tone[c].set (lowSmooth.getCurrentValue(), midSmooth.getCurrentValue(), highSmooth.getCurrentValue(), (float) sr);
    }

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

    // ---- noise gate on the wet path, then the oversampled nonlinear stage
    const float thr = pGateThresh->load() * 0.1f, dec = pGateDecay->load() * 0.1f;
    for (int c = 0; c < channels; ++c)
    {
        auto* x = buffer.getWritePointer (c);
        for (int i = 0; i < n; ++i) x[i] *= gate[c].process (x[i], thr, dec, (float) sr);
    }
    juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) channels, (size_t) n);
    auto up = oversampling->processSamplesUp (block);
    for (int c = 0; c < channels; ++c)
    {
        auto* u = up.getChannelPointer ((size_t) c);
        for (size_t i = 0; i < up.getNumSamples(); ++i)
            u[i] = drive[c].process (u[i], activeType, gainOs[i], param01);
    }
    oversampling->processSamplesDown (block);

    // ---- tone stack and level at the host rate, mixed with the delayed dry signal
    for (int c = 0; c < channels; ++c)
    {
        auto* x = buffer.getWritePointer (c);
        const auto& dry = dryOut[c];
        for (int i = 0; i < n; ++i)
        {
            const float wet = tone[c].process (x[i]) * levelBlock[(size_t) i];
            const float w = wetBlock[(size_t) i];
            x[i] = dry[(size_t) i] * (1.0f - w) + wet * w;
        }
    }

    if (switching)                                  // the wet signal is now silent: change the type and clear the filters
    {
        activeType = wantedType;
        for (int c = 0; c < channels; ++c) { drive[c].prepare ((float) (sr * (double) (1 << kOsFactor))); tone[c].reset(); }
        gainSmooth.setCurrentAndTargetValue (driveGainLinear (activeType, drive01, boost, pBoostPre->load() * 0.1f));
        levelSmooth.setCurrentAndTargetValue (outputGain (activeType, level01, boost, pBoostPost->load() * 0.1f));
    }
}

juce::AudioProcessorEditor* OD2AudioProcessor::createEditor() { return new OD2AudioProcessorEditor (*this); }

void OD2AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void OD2AudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new OD2AudioProcessor(); }
