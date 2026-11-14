// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout GR1AudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    const auto pct = [&layout] (const char* id, const char* name, float def, const char* unit = "%")
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 100.0f, 1.0f), def, AudioParameterFloatAttributes().withLabel (unit)));
    };
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { "power", 1 }, "Power", true));
    layout.add (vibr::makeBypassParam());
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "shape", 1 }, "Envelope",
                                                        StringArray { "Soft", "Sharp", "Percussive", "Swell" }, 0));
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { "freeze", 1 }, "Freeze", false));
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { "sync", 1 }, "Sync (Density follows the host tempo)", false));
    pct ("size", "Size", 50.0f);
    pct ("density", "Density", 55.0f);
    pct ("position", "Position", 20.0f);
    pct ("spray", "Spray", 30.0f);
    pct ("pitch", "Pitch", 50.0f, "");
    pct ("random", "Random", 0.0f);
    pct ("reverse", "Reverse", 0.0f);
    pct ("spread", "Spread", 60.0f);
    pct ("mix", "Mix", 60.0f);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "level", 1 }, "Level", NormalisableRange<float> (-24.0f, 12.0f, 1.0f), 0.0f, AudioParameterFloatAttributes().withLabel ("dB")));
    return layout;
}

GR1AudioProcessor::GR1AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "GR1", createLayout())
{
    pPower   = apvts.getRawParameterValue ("power");
    pShape   = apvts.getRawParameterValue ("shape");
    pFreeze  = apvts.getRawParameterValue ("freeze");
    pSync    = apvts.getRawParameterValue ("sync");
    pSize    = apvts.getRawParameterValue ("size");
    pDensity = apvts.getRawParameterValue ("density");
    pPosition = apvts.getRawParameterValue ("position");
    pSpray   = apvts.getRawParameterValue ("spray");
    pPitch   = apvts.getRawParameterValue ("pitch");
    pRandom  = apvts.getRawParameterValue ("random");
    pReverse = apvts.getRawParameterValue ("reverse");
    pSpread  = apvts.getRawParameterValue ("spread");
    pMix     = apvts.getRawParameterValue ("mix");
    pLevel   = apvts.getRawParameterValue ("level");
}

bool GR1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void GR1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare ((float) sampleRate);
    powerSmooth.reset (sampleRate, 0.02);
    powerSmooth.setCurrentAndTargetValue (pPower->load() > 0.5f ? 1.0f : 0.0f);
    dryBuf.setSize (2, juce::jmax (samplesPerBlock, 8192));
}

void GR1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0 || buffer.getNumChannels() < 2 || n > dryBuf.getNumSamples()) return;

    const float from = powerSmooth.getCurrentValue();
    powerSmooth.setTargetValue (pPower->load() > 0.5f ? 1.0f : 0.0f);
    const float to = powerSmooth.skip (n);
    if (from == 0.0f && to == 0.0f) return;                          // off: the signal passes untouched

    for (int c = 0; c < 2; ++c) dryBuf.copyFrom (c, 0, buffer, c, 0, n);

    gr1::Params p;
    p.shape = juce::jlimit (0, gr1::kNumShapes - 1, (int) std::lround (pShape->load()));
    p.freeze = pFreeze->load() > 0.5f;
    p.sync = pSync->load() > 0.5f;
    p.bpm = 120.0f;                                                  // the host tempo (120 when the host gives none)
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) p.bpm = (float) juce::jlimit (20.0, 400.0, *b);
    p.size = pSize->load() * 0.01f;
    p.density = pDensity->load() * 0.01f;
    p.position = pPosition->load() * 0.01f;
    p.spray = pSpray->load() * 0.01f;
    p.pitch = pPitch->load() * 0.01f;
    p.random = pRandom->load() * 0.01f;
    p.reverse = pReverse->load() * 0.01f;
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

juce::AudioProcessorEditor* GR1AudioProcessor::createEditor() { return new GR1AudioProcessorEditor (*this); }

void GR1AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void GR1AudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GR1AudioProcessor(); }
