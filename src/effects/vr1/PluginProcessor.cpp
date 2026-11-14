// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout VR1AudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    const auto pct = [&layout] (const char* id, const char* name, float def)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 100.0f, 1.0f), def, AudioParameterFloatAttributes().withLabel ("%")));
    };
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { "power", 1 }, "Power", true));
    layout.add (vibr::makeBypassParam());
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode", StringArray { "Uni-Vibe", "Rotary", "Ring Mod" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "variant", 1 }, "Type (Chorus / Vibrato, Slow / Fast, Sine / Triangle / Square)",
                                                        StringArray { "Type 1", "Type 2", "Type 3" }, 0));
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { "sync", 1 }, "Sync (Uni-Vibe rate follows the host tempo)", false));
    pct ("rate", "Rate / Speed / Freq", 55.0f);
    pct ("depth", "Depth / Intensity", 70.0f);
    pct ("mix", "Mix", 100.0f);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "level", 1 }, "Level", NormalisableRange<float> (-24.0f, 12.0f, 1.0f), 0.0f, AudioParameterFloatAttributes().withLabel ("dB")));
    return layout;
}

VR1AudioProcessor::VR1AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VR1", createLayout())
{
    pPower   = apvts.getRawParameterValue ("power");
    pMode    = apvts.getRawParameterValue ("mode");
    pVariant = apvts.getRawParameterValue ("variant");
    pSync    = apvts.getRawParameterValue ("sync");
    pRate    = apvts.getRawParameterValue ("rate");
    pDepth   = apvts.getRawParameterValue ("depth");
    pMix     = apvts.getRawParameterValue ("mix");
    pLevel   = apvts.getRawParameterValue ("level");
    midiMap.setDefault ("depth", 1);
    midiMap.setDefault ("rate", 11);
}

bool VR1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void VR1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare ((float) sampleRate);
    powerSmooth.reset (sampleRate, 0.02);
    powerSmooth.setCurrentAndTargetValue (pPower->load() > 0.5f ? 1.0f : 0.0f);
    dryBuf.setSize (2, juce::jmax (samplesPerBlock, 8192));
}

void VR1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    midiMap.process (midi);
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0 || buffer.getNumChannels() < 2 || n > dryBuf.getNumSamples()) return;

    const float from = powerSmooth.getCurrentValue();
    powerSmooth.setTargetValue (pPower->load() > 0.5f ? 1.0f : 0.0f);
    const float to = powerSmooth.skip (n);
    if (from == 0.0f && to == 0.0f) return;                          // off: the signal passes untouched

    for (int c = 0; c < 2; ++c) dryBuf.copyFrom (c, 0, buffer, c, 0, n);

    vr1::Params p;
    p.mode = juce::jlimit (0, vr1::kNumModes - 1, (int) std::lround (pMode->load()));
    p.variant = juce::jlimit (0, vr1::variantCount (p.mode) - 1, (int) std::lround (pVariant->load()));
    p.sync = pSync->load() > 0.5f;
    p.bpm = 120.0f;                                                  // the host tempo (120 when the host gives none)
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) p.bpm = (float) juce::jlimit (20.0, 400.0, *b);
    p.rate = pRate->load() * 0.01f;
    p.depth = pDepth->load() * 0.01f;
    p.mix = pMix->load() * 0.01f;
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

juce::AudioProcessorEditor* VR1AudioProcessor::createEditor() { return new VR1AudioProcessorEditor (*this); }

void VR1AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.appendChild (midiMap.toTree(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void VR1AudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) {
            auto st = juce::ValueTree::fromXml (*xml);
            midiMap.fromTree (st.getChildWithName ("MIDIMAP"));
            st.removeChild (st.getChildWithName ("MIDIMAP"), nullptr);
            vibr::BypassLink::normalise (st);
            apvts.replaceState (st);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VR1AudioProcessor(); }
