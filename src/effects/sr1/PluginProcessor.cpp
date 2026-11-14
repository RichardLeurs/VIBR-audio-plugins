// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout SR1AudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    const auto pct = [&layout] (const char* id, const char* name, float def, const char* unit = "%")
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 100.0f, 1.0f), def, AudioParameterFloatAttributes().withLabel (unit)));
    };
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { "power", 1 }, "Power", true));
    layout.add (vibr::makeBypassParam());
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "root", 1 }, "Root",
                                                        StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "scale", 1 }, "Scale",
                                                        StringArray { "Major", "Minor", "Major pentatonic", "Minor pentatonic", "Dorian", "Whole tone" }, 2));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "texture", 1 }, "Texture", StringArray { "Wood", "Metal", "Glass" }, 1));
    pct ("strings", "Strings", 60.0f);
    pct ("decay", "Decay", 55.0f);
    pct ("damping", "Damping", 40.0f);
    pct ("tension", "Tension", 25.0f);
    pct ("motion", "Motion", 30.0f);
    pct ("fine", "Fine", 50.0f, "");
    pct ("mix", "Blend", 50.0f);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "level", 1 }, "Level", NormalisableRange<float> (-24.0f, 12.0f, 1.0f), 0.0f, AudioParameterFloatAttributes().withLabel ("dB")));
    return layout;
}

SR1AudioProcessor::SR1AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SR1", createLayout())
{
    midiMap.setDefault ("mix", 1);
    midiMap.setDefault ("decay", 11);
    pPower   = apvts.getRawParameterValue ("power");
    pRoot    = apvts.getRawParameterValue ("root");
    pScale   = apvts.getRawParameterValue ("scale");
    pTexture = apvts.getRawParameterValue ("texture");
    pStrings = apvts.getRawParameterValue ("strings");
    pDecay   = apvts.getRawParameterValue ("decay");
    pDamping = apvts.getRawParameterValue ("damping");
    pTension = apvts.getRawParameterValue ("tension");
    pMotion  = apvts.getRawParameterValue ("motion");
    pFine    = apvts.getRawParameterValue ("fine");
    pMix     = apvts.getRawParameterValue ("mix");
    pLevel   = apvts.getRawParameterValue ("level");
}

bool SR1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void SR1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare ((float) sampleRate);
    powerSmooth.reset (sampleRate, 0.02);
    powerSmooth.setCurrentAndTargetValue (pPower->load() > 0.5f ? 1.0f : 0.0f);
    dryBuf.setSize (2, juce::jmax (samplesPerBlock, 8192));
}

void SR1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
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

    sr1::Params p;
    p.root = juce::jlimit (0, 11, (int) std::lround (pRoot->load()));
    p.scale = juce::jlimit (0, sr1::kNumScales - 1, (int) std::lround (pScale->load()));
    p.texture = juce::jlimit (0, sr1::kNumTextures - 1, (int) std::lround (pTexture->load()));
    p.strings = pStrings->load() * 0.01f;
    p.decay = pDecay->load() * 0.01f;
    p.damping = pDamping->load() * 0.01f;
    p.tension = pTension->load() * 0.01f;
    p.motion = pMotion->load() * 0.01f;
    p.fine = pFine->load() * 0.01f;
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

juce::AudioProcessorEditor* SR1AudioProcessor::createEditor() { return new SR1AudioProcessorEditor (*this); }

void SR1AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.appendChild (midiMap.toTree(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void SR1AudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto st = juce::ValueTree::fromXml (*xml);
            midiMap.fromTree (st.getChildWithName ("MIDIMAP"));
            st.removeChild (st.getChildWithName ("MIDIMAP"), nullptr);
            vibr::BypassLink::normalise (st);
            apvts.replaceState (st);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SR1AudioProcessor(); }
