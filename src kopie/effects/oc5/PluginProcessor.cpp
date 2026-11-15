// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout OC5AudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    const auto knob = [&] (const char* id, const char* name, float def)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 10.0f, 0.01f), def));
    };
    knob ("direct", "Direct Level", 6.0f);
    knob ("up1",    "+1 Oct Level", 0.0f);
    knob ("down1",  "-1 Oct Level", 5.0f);
    knob ("down2",  "-2 Oct Level / Range", 3.0f);
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "poly", 1 }, "Poly Mode (off = Vintage)", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "bypass", 1 }, "Bypass", false));
    return layout;
}

OC5AudioProcessor::OC5AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "OC5", createLayout())
{
    midiMap.setDefault ("up1", 1);
    midiMap.setDefault ("down1", 11);
    pDirect = apvts.getRawParameterValue ("direct");
    pUp1    = apvts.getRawParameterValue ("up1");
    pDown1  = apvts.getRawParameterValue ("down1");
    pDown2  = apvts.getRawParameterValue ("down2");
    pPoly   = apvts.getRawParameterValue ("poly");
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("bypass"));
}

bool OC5AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) && out == layouts.getMainInputChannelSet();
}

void OC5AudioProcessor::prepareToPlay (double sampleRate, int)
{
    for (auto& e : engine) e.prepare ((float) sampleRate);
    wetSmooth.reset (sampleRate, 0.01);
    wetSmooth.setCurrentAndTargetValue (bypassParam->get() ? 0.0f : 1.0f);
}

void OC5AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    midiMap.process (midi);
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int channels = juce::jmin (buffer.getNumChannels(), kMaxChannels);
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, n);

    oc5::Params p;
    p.direct = pDirect->load() * 0.1f;
    p.up1    = pUp1->load() * 0.1f;
    p.down1  = pDown1->load() * 0.1f;
    p.down2  = pDown2->load() * 0.1f;
    p.range  = pDown2->load() * 0.1f;               // in POLY the same knob is the RANGE control
    p.poly   = pPoly->load() > 0.5f;
    wetSmooth.setTargetValue (bypassParam->get() ? 0.0f : 1.0f);

    // one shared bypass ramp for all channels
    for (int i = 0; i < n; ++i)
    {
        const float w = wetSmooth.getNextValue();
        for (int c = 0; c < channels; ++c)
        {
            auto* x = buffer.getWritePointer (c);
            const float dry = x[i];
            x[i] = dry * (1.0f - w) + engine[c].process (dry, p) * w;
        }
    }
}

juce::AudioProcessorEditor* OC5AudioProcessor::createEditor() { return new OC5AudioProcessorEditor (*this); }

void OC5AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.appendChild (midiMap.toTree(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void OC5AudioProcessor::setStateInformation (const void* data, int size)
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

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new OC5AudioProcessor(); }
