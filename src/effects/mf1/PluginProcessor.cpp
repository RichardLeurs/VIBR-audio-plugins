// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout MF1AudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    const auto pct = [&layout] (const juce::String& id, const juce::String& name, float def)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 100.0f, 1.0f), def, AudioParameterFloatAttributes().withLabel ("%")));
    };
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { "power", 1 }, "Power", true));
    layout.add (vibr::makeBypassParam());
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "type", 1 }, "Filter",
                                                        StringArray { "Low pass", "Band pass", "High pass", "Notch", "Vowel" }, 1));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "source", 1 }, "Source", StringArray { "LFO", "Envelope", "Steps" }, 0));
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { "sync", 1 }, "Sync", false));
    pct ("cutoff", "Cutoff", 40.0f);
    pct ("res", "Resonance", 55.0f);
    pct ("depth", "Depth", 60.0f);
    pct ("m1", "Rate / Sens", 35.0f);
    pct ("m2", "Shape / Decay / Glide", 0.0f);
    pct ("mix", "Mix", 100.0f);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "level", 1 }, "Level", NormalisableRange<float> (-24.0f, 12.0f, 1.0f), 0.0f, AudioParameterFloatAttributes().withLabel ("dB")));
    static const float pattern[mf1::kNumSteps] = { 20, 80, 40, 100, 30, 70, 50, 90 };
    for (int i = 0; i < mf1::kNumSteps; ++i)
        pct ("step" + String (i + 1), "Step " + String (i + 1), pattern[i]);
    return layout;
}

MF1AudioProcessor::MF1AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MF1", createLayout())
{
    pPower  = apvts.getRawParameterValue ("power");
    pType   = apvts.getRawParameterValue ("type");
    pSource = apvts.getRawParameterValue ("source");
    pSync   = apvts.getRawParameterValue ("sync");
    pCutoff = apvts.getRawParameterValue ("cutoff");
    pRes    = apvts.getRawParameterValue ("res");
    pDepth  = apvts.getRawParameterValue ("depth");
    pM1     = apvts.getRawParameterValue ("m1");
    pM2     = apvts.getRawParameterValue ("m2");
    pMix    = apvts.getRawParameterValue ("mix");
    pLevel  = apvts.getRawParameterValue ("level");
    for (int i = 0; i < mf1::kNumSteps; ++i) pStep[i] = apvts.getRawParameterValue ("step" + juce::String (i + 1));
}

bool MF1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void MF1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare ((float) sampleRate);
    powerSmooth.reset (sampleRate, 0.02);
    powerSmooth.setCurrentAndTargetValue (pPower->load() > 0.5f ? 1.0f : 0.0f);
    dryBuf.setSize (2, juce::jmax (samplesPerBlock, 8192));
}

void MF1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0 || buffer.getNumChannels() < 2 || n > dryBuf.getNumSamples()) return;

    const float from = powerSmooth.getCurrentValue();
    powerSmooth.setTargetValue (pPower->load() > 0.5f ? 1.0f : 0.0f);
    const float to = powerSmooth.skip (n);
    if (from == 0.0f && to == 0.0f) return;                          // off: the signal passes untouched

    for (int c = 0; c < 2; ++c) dryBuf.copyFrom (c, 0, buffer, c, 0, n);

    mf1::Params p;
    p.type = juce::jlimit (0, mf1::kNumTypes - 1, (int) std::lround (pType->load()));
    p.source = juce::jlimit (0, mf1::kNumSources - 1, (int) std::lround (pSource->load()));
    p.sync = pSync->load() > 0.5f;
    p.cutoff = pCutoff->load() * 0.01f;
    p.res = pRes->load() * 0.01f;
    p.depth = pDepth->load() * 0.01f;
    p.m1 = pM1->load() * 0.01f;
    p.m2 = pM2->load() * 0.01f;
    for (int i = 0; i < mf1::kNumSteps; ++i) p.steps[i] = pStep[i]->load() * 0.01f;
    p.bpm = 120.0f;                                                  // the host tempo (120 when the host gives none)
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) p.bpm = (float) juce::jlimit (20.0, 400.0, *b);

    engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n, p, pMix->load() * 0.01f, juce::Decibels::decibelsToGain (pLevel->load()));
    currentStep.store (engine.currentStep());
    modulation.store (engine.lastModulation());

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

juce::AudioProcessorEditor* MF1AudioProcessor::createEditor() { return new MF1AudioProcessorEditor (*this); }

void MF1AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void MF1AudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MF1AudioProcessor(); }
