// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout VC1AudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    const auto pct = [&layout] (const juce::String& id, const juce::String& name, float def)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 100.0f, 1.0f), def, AudioParameterFloatAttributes().withLabel ("%")));
    };
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { "power", 1 }, "Power", true));
    layout.add (vibr::makeBypassParam());
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { "sync", 1 }, "Sync (rates follow the host tempo)", false));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "type", 1 }, "Filter", StringArray { "Low pass", "Band pass", "High pass" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "shape1", 1 }, "LFO 1 shape", StringArray { "Sine", "Triangle", "Square", "Ramp" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "shape2", 1 }, "LFO 2 shape", StringArray { "Sine", "Triangle", "Square", "Ramp" }, 1));
    pct ("cutoff", "Cutoff", 50.0f);
    pct ("res", "Resonance", 40.0f);
    pct ("envsens", "Envelope sensitivity", 50.0f);
    pct ("envrel", "Envelope release", 40.0f);
    pct ("rate1", "LFO 1 rate", 45.0f);
    pct ("rate2", "LFO 2 rate", 30.0f);
    pct ("rand", "Random rate", 40.0f);
    pct ("xmod", "Cross-modulation", 0.0f);
    pct ("mix", "Mix", 100.0f);
    for (int s = 0; s < vc1::kNumSources; ++s)
        for (int d = 0; d < vc1::kNumDests; ++d)
        {
            float def = 50.0f;                                                  // a starting patch: LFO 1 -> filter, envelope -> filter
            if (s == vc1::Lfo1 && d == vc1::Filter) def = 80.0f;
            if (s == vc1::Env && d == vc1::Filter) def = 70.0f;
            pct ("m" + String (s) + String (d), String (vc1::sourceName (s)) + " > " + vc1::destName (d), def);
        }
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "level", 1 }, "Level", NormalisableRange<float> (-24.0f, 12.0f, 1.0f), 0.0f, AudioParameterFloatAttributes().withLabel ("dB")));
    return layout;
}

VC1AudioProcessor::VC1AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VC1", createLayout())
{
    pPower   = apvts.getRawParameterValue ("power");
    pSync    = apvts.getRawParameterValue ("sync");
    pType    = apvts.getRawParameterValue ("type");
    pShape1  = apvts.getRawParameterValue ("shape1");
    pShape2  = apvts.getRawParameterValue ("shape2");
    pCutoff  = apvts.getRawParameterValue ("cutoff");
    pRes     = apvts.getRawParameterValue ("res");
    pEnvSens = apvts.getRawParameterValue ("envsens");
    pEnvRel  = apvts.getRawParameterValue ("envrel");
    pRate1   = apvts.getRawParameterValue ("rate1");
    pRate2   = apvts.getRawParameterValue ("rate2");
    pRand    = apvts.getRawParameterValue ("rand");
    pXmod    = apvts.getRawParameterValue ("xmod");
    for (int s = 0; s < vc1::kNumSources; ++s)
        for (int d = 0; d < vc1::kNumDests; ++d) pMatrix[s][d] = apvts.getRawParameterValue ("m" + juce::String (s) + juce::String (d));
    pMix     = apvts.getRawParameterValue ("mix");
    pLevel   = apvts.getRawParameterValue ("level");
}

bool VC1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void VC1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare ((float) sampleRate);
    powerSmooth.reset (sampleRate, 0.02);
    powerSmooth.setCurrentAndTargetValue (pPower->load() > 0.5f ? 1.0f : 0.0f);
    dryBuf.setSize (2, juce::jmax (samplesPerBlock, 8192));
}

void VC1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0 || buffer.getNumChannels() < 2 || n > dryBuf.getNumSamples()) return;

    const float from = powerSmooth.getCurrentValue();
    powerSmooth.setTargetValue (pPower->load() > 0.5f ? 1.0f : 0.0f);
    const float to = powerSmooth.skip (n);
    if (from == 0.0f && to == 0.0f) return;                          // off: the signal passes untouched

    for (int c = 0; c < 2; ++c) dryBuf.copyFrom (c, 0, buffer, c, 0, n);

    for (const auto meta : midi)                                       // mod wheel, expression and pressure drive the MIDI source
    {
        const auto m = meta.getMessage();
        if (m.isController() && (m.getControllerNumber() == 1 || m.getControllerNumber() == 11)) midiValue = (float) m.getControllerValue() / 127.0f;
        else if (m.isChannelPressure()) midiValue = (float) m.getChannelPressureValue() / 127.0f;
    }

    vc1::Params p;
    p.type = juce::jlimit (0, vc1::kNumTypes - 1, (int) std::lround (pType->load()));
    p.shape1 = juce::jlimit (0, vc1::kNumShapes - 1, (int) std::lround (pShape1->load()));
    p.shape2 = juce::jlimit (0, vc1::kNumShapes - 1, (int) std::lround (pShape2->load()));
    p.cutoff = pCutoff->load() * 0.01f;
    p.res = pRes->load() * 0.01f;
    p.envSens = pEnvSens->load() * 0.01f;
    p.envRel = pEnvRel->load() * 0.01f;
    p.rate1 = pRate1->load() * 0.01f;
    p.rate2 = pRate2->load() * 0.01f;
    p.randRate = pRand->load() * 0.01f;
    p.xmod = pXmod->load() * 0.01f;
    p.sync = pSync->load() > 0.5f;
    p.bpm = 120.0f;                                                  // the host tempo (120 when the host gives none)
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) p.bpm = (float) juce::jlimit (20.0, 400.0, *b);
    p.mix = pMix->load() * 0.01f;
    p.midi = midiValue;
    for (int s = 0; s < vc1::kNumSources; ++s)
        for (int d = 0; d < vc1::kNumDests; ++d) p.matrix[s][d] = (pMatrix[s][d]->load() - 50.0f) * 0.02f;
    engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n, p, juce::Decibels::decibelsToGain (pLevel->load()));

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

juce::AudioProcessorEditor* VC1AudioProcessor::createEditor() { return new VC1AudioProcessorEditor (*this); }

void VC1AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void VC1AudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VC1AudioProcessor(); }
