// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
// the factory presets (values are plain host values; size / decay / diffusion / tone / motion / texture / mix 0 .. 100, level in dB, mode 0 .. 4)
std::vector<vibr::PresetManager::Factory> sp1FactoryPresets()
{
    return {
        { "Init", "Space Engine at its defaults", {} },
        { "Angel Shimmer", "Octave-up bloom", { { "mode", 0 }, { "size", 70 }, { "decay", 75 }, { "texture", 60 }, { "mix", 45 } } },
        { "Endless Hall", "A tail that never ends", { { "mode", 1 }, { "size", 80 }, { "decay", 90 }, { "diffusion", 80 }, { "mix", 50 } } },
        { "Spectral Cloud", "Smeared, glassy spectrum", { { "mode", 2 }, { "size", 65 }, { "decay", 70 }, { "motion", 50 }, { "texture", 65 }, { "mix", 55 } } },
        { "Slow Drift", "A chorused, moving space", { { "mode", 3 }, { "decay", 65 }, { "motion", 60 }, { "texture", 55 }, { "mix", 40 } } },
        { "Small Room", "Tight and natural", { { "mode", 4 }, { "size", 30 }, { "decay", 35 }, { "diffusion", 60 }, { "mix", 30 } } },
        { "Frozen Chord", "Freeze what just played", { { "mode", 1 }, { "freeze", 1 }, { "size", 75 }, { "decay", 85 }, { "mix", 70 } } },
    };
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout SP1AudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    const auto pct = [&layout] (const char* id, const char* name, float def, const char* unit = "%")
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 100.0f, 1.0f), def, AudioParameterFloatAttributes().withLabel (unit)));
    };
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { "power", 1 }, "Power", true, juce::AudioParameterBoolAttributes().withMeta (true)));
    layout.add (vibr::makeBypassParam());
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode",
                                                        StringArray { "Shimmer", "Infinite", "Spectral", "Modulated", "Rooms" }, 0));
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { "freeze", 1 }, "Freeze", false));
    pct ("size", "Size", 60.0f);
    pct ("decay", "Decay", 60.0f);
    pct ("diffusion", "Diffusion", 70.0f);
    pct ("tone", "Tone", 55.0f);
    pct ("motion", "Motion", 35.0f);
    pct ("texture", "Texture", 50.0f);
    pct ("mix", "Mix", 40.0f);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "level", 1 }, "Level", NormalisableRange<float> (-24.0f, 12.0f, 1.0f), 0.0f, AudioParameterFloatAttributes().withLabel ("dB")));
    return layout;
}

SP1AudioProcessor::SP1AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "SP1", createLayout()),
      presets (*this, apvts, undoManager, "SP-1 Presets", sp1FactoryPresets(), { "power", "bypass" })
{
    pPower   = apvts.getRawParameterValue ("power");
    pMode    = apvts.getRawParameterValue ("mode");
    pFreeze  = apvts.getRawParameterValue ("freeze");
    pSize    = apvts.getRawParameterValue ("size");
    pDecay   = apvts.getRawParameterValue ("decay");
    pDiff    = apvts.getRawParameterValue ("diffusion");
    pTone    = apvts.getRawParameterValue ("tone");
    pMotion  = apvts.getRawParameterValue ("motion");
    pTexture = apvts.getRawParameterValue ("texture");
    pMix     = apvts.getRawParameterValue ("mix");
    pLevel   = apvts.getRawParameterValue ("level");

    midiMap.setDefault ("motion", 1);
    midiMap.setDefault ("decay", 11);
    midiMap.setDefault ("freeze", 64);
}

bool SP1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void SP1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare ((float) sampleRate);
    powerSmooth.reset (sampleRate, 0.02);
    powerSmooth.setCurrentAndTargetValue (pPower->load() > 0.5f ? 1.0f : 0.0f);
    dryBuf.setSize (2, juce::jmax (samplesPerBlock, 8192));
}

void SP1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    midiMap.process (midi);
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0 || buffer.getNumChannels() < 2 || n > dryBuf.getNumSamples()) return;

    const float from = powerSmooth.getCurrentValue();
    powerSmooth.setTargetValue (pPower->load() > 0.5f ? 1.0f : 0.0f);
    const float to = powerSmooth.skip (n);
    if (from == 0.0f && to == 0.0f) { outMeter.measure (buffer, n); return; }                        // off: the signal passes untouched

    for (int c = 0; c < 2; ++c) dryBuf.copyFrom (c, 0, buffer, c, 0, n);

    sp1::Params p;
    p.mode = juce::jlimit (0, sp1::kNumModes - 1, (int) std::lround (pMode->load()));
    p.freeze = pFreeze->load() > 0.5f;
    p.size = pSize->load() * 0.01f;
    p.decay = pDecay->load() * 0.01f;
    p.diffusion = pDiff->load() * 0.01f;
    p.tone = pTone->load() * 0.01f;
    p.motion = pMotion->load() * 0.01f;
    p.texture = pTexture->load() * 0.01f;
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
    outMeter.measure (buffer, n);
}

juce::AudioProcessorEditor* SP1AudioProcessor::createEditor() { return new SP1AudioProcessorEditor (*this); }

void SP1AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.appendChild (midiMap.toTree(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void SP1AudioProcessor::setStateInformation (const void* data, int size)
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

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SP1AudioProcessor(); }
