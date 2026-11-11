// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout BigSkyAudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "type", 1 }, "Type", 0, bigsky::kNumTypes - 1, 0));
    const auto knob = [&] (const char* id, const char* name, float def)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 10.0f, 0.01f), def));
    };
    knob ("value",    "Value (wet level)", 5.0f);
    knob ("decay",    "Decay", 5.0f);
    knob ("predelay", "Pre-Delay", 1.5f);
    knob ("mix",      "Mix", 5.0f);
    knob ("tone",     "Tone", 5.0f);
    knob ("param1",   "Param 1", 5.0f);
    knob ("param2",   "Param 2", 5.0f);
    knob ("mod",      "Mod", 5.0f);
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "bypass", 1 }, "Bypass", false));
    return layout;
}

BigSkyAudioProcessor::BigSkyAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BIGSKY", createLayout())
{
    pType  = apvts.getRawParameterValue ("type");
    pDecay = apvts.getRawParameterValue ("decay");
    pPre   = apvts.getRawParameterValue ("predelay");
    pMix   = apvts.getRawParameterValue ("mix");
    pTone  = apvts.getRawParameterValue ("tone");
    pP1    = apvts.getRawParameterValue ("param1");
    pP2    = apvts.getRawParameterValue ("param2");
    pMod   = apvts.getRawParameterValue ("mod");
    pValue = apvts.getRawParameterValue ("value");
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("bypass"));
}

bool BigSkyAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) && out == layouts.getMainInputChannelSet();
}

void BigSkyAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare ((float) sampleRate);
    const auto cap = (size_t) juce::jmax (samplesPerBlock, 1);
    dryL.assign (cap, 0.0f); dryR.assign (cap, 0.0f); wetR.assign (cap, 0.0f);
    wetSmooth.reset (sampleRate, 0.02);
    wetSmooth.setCurrentAndTargetValue (bypassParam->get() ? 0.0f : 1.0f);
}

void BigSkyAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int total = buffer.getNumSamples();
    const int channels = juce::jmin (buffer.getNumChannels(), 2);
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, total);

    bigsky::Params p;
    p.type     = (int) std::lround (pType->load());
    p.decay    = pDecay->load() * 0.1f;
    p.predelay = pPre->load() * 0.1f;
    p.mix      = pMix->load() * 0.1f;
    p.tone     = pTone->load() * 0.1f;
    p.p1       = pP1->load() * 0.1f;
    p.p2       = pP2->load() * 0.1f;
    p.mod      = pMod->load() * 0.1f;
    p.value    = pValue->load() * 0.1f;
    wetSmooth.setTargetValue (bypassParam->get() ? 0.0f : 1.0f);

    const int cap = (int) dryL.size();
    auto* l = buffer.getWritePointer (0);
    auto* r = channels > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int off = 0; off < total; off += cap)
    {
        const int n = juce::jmin (cap, total - off);
        float* pl = l + off;
        float* pr = r != nullptr ? r + off : wetR.data();
        if (r == nullptr) std::copy (pl, pl + n, wetR.begin());
        std::copy (pl, pl + n, dryL.begin());
        std::copy (pr, pr + n, dryR.begin());

        engine.process (pl, pr, n, p);

        for (int i = 0; i < n; ++i)
        {
            const float w = wetSmooth.getNextValue();
            if (r == nullptr)
                pl[i] = dryL[(size_t) i] * (1.0f - w) + 0.5f * (pl[i] + pr[i]) * w;
            else
            {
                pl[i] = dryL[(size_t) i] * (1.0f - w) + pl[i] * w;
                pr[i] = dryR[(size_t) i] * (1.0f - w) + pr[i] * w;
            }
        }
    }
}

juce::AudioProcessorEditor* BigSkyAudioProcessor::createEditor() { return new BigSkyAudioProcessorEditor (*this); }

void BigSkyAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void BigSkyAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BigSkyAudioProcessor(); }
