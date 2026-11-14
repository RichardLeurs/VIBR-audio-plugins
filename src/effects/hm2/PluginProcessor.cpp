// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout HM2AudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    const auto pct = [&layout] (const char* id, const char* name, float def)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 100.0f, 1.0f), def, AudioParameterFloatAttributes().withLabel ("%")));
    };
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { "power", 1 }, "Power", true));
    layout.add (vibr::makeBypassParam());
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "character", 1 }, "Character",
                                                        StringArray { "Tube", "Tape", "Transistor", "Fold" }, 0));
    pct ("drive", "Drive", 40.0f);
    pct ("oddeven", "Odd / Even", 50.0f);
    pct ("octave", "Octave", 0.0f);
    pct ("sub", "Sub", 0.0f);
    pct ("focus", "Focus", 40.0f);
    pct ("tilt", "Tilt", 50.0f);
    pct ("tone", "Tone", 80.0f);
    pct ("mix", "Mix", 100.0f);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "level", 1 }, "Level", NormalisableRange<float> (-24.0f, 12.0f, 1.0f), 0.0f, AudioParameterFloatAttributes().withLabel ("dB")));
    return layout;
}

HM2AudioProcessor::HM2AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "HM2", createLayout())
{
    pPower   = apvts.getRawParameterValue ("power");
    pCharacter = apvts.getRawParameterValue ("character");
    pDrive   = apvts.getRawParameterValue ("drive");
    pOddEven = apvts.getRawParameterValue ("oddeven");
    pOctave  = apvts.getRawParameterValue ("octave");
    pSub     = apvts.getRawParameterValue ("sub");
    pFocus   = apvts.getRawParameterValue ("focus");
    pTilt    = apvts.getRawParameterValue ("tilt");
    pTone    = apvts.getRawParameterValue ("tone");
    pMix     = apvts.getRawParameterValue ("mix");
    pLevel   = apvts.getRawParameterValue ("level");
}

bool HM2AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void HM2AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare ((float) sampleRate);
    powerSmooth.reset (sampleRate, 0.02);
    powerSmooth.setCurrentAndTargetValue (pPower->load() > 0.5f ? 1.0f : 0.0f);
    dryBuf.setSize (2, juce::jmax (samplesPerBlock, 8192));
}

void HM2AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0 || buffer.getNumChannels() < 2 || n > dryBuf.getNumSamples()) return;

    const float from = powerSmooth.getCurrentValue();
    powerSmooth.setTargetValue (pPower->load() > 0.5f ? 1.0f : 0.0f);
    const float to = powerSmooth.skip (n);
    if (from == 0.0f && to == 0.0f) return;                          // off: the signal passes untouched

    for (int c = 0; c < 2; ++c) dryBuf.copyFrom (c, 0, buffer, c, 0, n);

    hm2::Params p;
    p.character = juce::jlimit (0, hm2::kNumCharacters - 1, (int) std::lround (pCharacter->load()));
    p.drive = pDrive->load() * 0.01f;
    p.oddEven = pOddEven->load() * 0.01f;
    p.octave = pOctave->load() * 0.01f;
    p.sub = pSub->load() * 0.01f;
    p.focus = pFocus->load() * 0.01f;
    p.tilt = pTilt->load() * 0.01f;
    p.tone = pTone->load() * 0.01f;
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

juce::AudioProcessorEditor* HM2AudioProcessor::createEditor() { return new HM2AudioProcessorEditor (*this); }

void HM2AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void HM2AudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) { auto st = juce::ValueTree::fromXml (*xml); vibr::BypassLink::normalise (st); apvts.replaceState (st); }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new HM2AudioProcessor(); }
