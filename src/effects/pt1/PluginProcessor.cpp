// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout PT1AudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    const auto pct = [&layout] (const juce::String& id, const juce::String& name, float def)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 100.0f, 1.0f), def));
    };
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { "power", 1 }, "Power", true));
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { "intel", 1 }, "Intelligent", true));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "root", 1 }, "Key",
                                                        StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "scale", 1 }, "Scale",
                                                        StringArray { "Major", "Minor", "Major pentatonic", "Minor pentatonic", "Dorian", "Whole tone" }, 0));
    static const float lvl[pt1::kNumVoices] = { 70, 60, 50, 50 }, det[pt1::kNumVoices] = { 56, 44, 58, 42 };
    for (int v = 0; v < pt1::kNumVoices; ++v)
    {
        const String n (v + 1);
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "int" + n, 1 }, "Interval " + n, NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f * pt1::intervalValue (pt1::chordSemis (0, v))));
        pct ("det" + n, "Detune " + n, det[v]);
        pct ("lvl" + n, "Level " + n, lvl[v]);
    }
    pct ("spread", "Spread", 70.0f);
    pct ("mix", "Mix", 50.0f);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "level", 1 }, "Level", NormalisableRange<float> (-24.0f, 12.0f, 1.0f), 0.0f));
    return layout;
}

PT1AudioProcessor::PT1AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PT1", createLayout())
{
    pPower   = apvts.getRawParameterValue ("power");
    pIntel   = apvts.getRawParameterValue ("intel");
    pRoot    = apvts.getRawParameterValue ("root");
    pScale   = apvts.getRawParameterValue ("scale");
    pSpread  = apvts.getRawParameterValue ("spread");
    for (int v = 0; v < pt1::kNumVoices; ++v)
    {
        const juce::String n (v + 1);
        pInt[v] = apvts.getRawParameterValue ("int" + n);
        pDet[v] = apvts.getRawParameterValue ("det" + n);
        pLvl[v] = apvts.getRawParameterValue ("lvl" + n);
    }
    pMix     = apvts.getRawParameterValue ("mix");
    pLevel   = apvts.getRawParameterValue ("level");
}

bool PT1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void PT1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare ((float) sampleRate);
    powerSmooth.reset (sampleRate, 0.02);
    powerSmooth.setCurrentAndTargetValue (pPower->load() > 0.5f ? 1.0f : 0.0f);
    dryBuf.setSize (2, juce::jmax (samplesPerBlock, 8192));
}

void PT1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0 || buffer.getNumChannels() < 2 || n > dryBuf.getNumSamples()) return;

    const float from = powerSmooth.getCurrentValue();
    powerSmooth.setTargetValue (pPower->load() > 0.5f ? 1.0f : 0.0f);
    const float to = powerSmooth.skip (n);
    if (from == 0.0f && to == 0.0f) return;                          // off: the signal passes untouched

    for (int c = 0; c < 2; ++c) dryBuf.copyFrom (c, 0, buffer, c, 0, n);

    pt1::Params p;
    p.intelligent = pIntel->load() > 0.5f;
    p.root = juce::jlimit (0, 11, (int) std::lround (pRoot->load()));
    p.scale = juce::jlimit (0, pt1::kNumScales - 1, (int) std::lround (pScale->load()));
    for (int v = 0; v < pt1::kNumVoices; ++v)
    {
        p.interval[v] = pt1::intervalSemis (pInt[v]->load() * 0.01f);
        p.detune[v] = pt1::detuneCents (pDet[v]->load() * 0.01f);
        p.level[v] = pLvl[v]->load() * 0.01f;
    }
    p.spread = pSpread->load() * 0.01f;
    engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n, p, pMix->load() * 0.01f, juce::Decibels::decibelsToGain (pLevel->load()));

    if (from != 1.0f || to != 1.0f)                                  // power switch: crossfade to the untouched signal
        for (int c = 0; c < 2; ++c)
        {
            auto* d = buffer.getWritePointer (c);
            const auto* s = dryBuf.getReadPointer (c);
            for (int i = 0; i < n; ++i)
            {
                const float t = from + (to - from) * (float) i / (float) n;
                d[i] = s[i] + (d[i] - s[i]) * t;
            }
        }
}

juce::AudioProcessorEditor* PT1AudioProcessor::createEditor() { return new PT1AudioProcessorEditor (*this); }

void PT1AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void PT1AudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PT1AudioProcessor(); }
