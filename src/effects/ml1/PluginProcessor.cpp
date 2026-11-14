// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
// host label and readable text for a float parameter (the stored value is the shown value, the host shows the label next to it)
juce::AudioParameterFloatAttributes unitAttrs (const juce::String& unit, int decimals)
{
    return juce::AudioParameterFloatAttributes()
        .withLabel (unit)
        .withStringFromValueFunction ([decimals] (float v, int)
        {
            return juce::String (v, decimals);                       // the number only: the host adds the label (unit) itself
        })
        .withValueFromStringFunction ([] (const juce::String& s)
        {
            return s.retainCharacters ("0123456789.-+").getFloatValue();
        });
}


} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout ML1AudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    const auto pct = [&layout] (const char* id, const char* name, float def)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 100.0f, 1.0f), def, unitAttrs ("%", 0)));
    };
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { "power", 1 }, "Power", true));
    layout.add (vibr::makeBypassParam());
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode", StringArray { "Chorus", "Vibrato", "Flanger", "Phaser" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "variant", 1 }, "Type (2-4 voices, Sine / Triangle, Positive / Negative feedback, 4-8 stages)",
                                                        StringArray { "Type 1", "Type 2", "Type 3" }, 0));
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { "sync", 1 }, "Sync (rate follows the host tempo)", false));
    pct ("rate", "Rate", 40.0f);
    pct ("depth", "Depth", 50.0f);
    pct ("extra", "Width (Chorus, Vibrato) / Feedback (Flanger, Phaser)", 50.0f);
    pct ("mix", "Mix", 100.0f);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "level", 1 }, "Level", NormalisableRange<float> (-24.0f, 12.0f, 1.0f), 0.0f, unitAttrs ("dB", 0)));
    return layout;
}

namespace
{
// the factory presets (values are plain host values; rate / depth / extra / mix 0 .. 100)
std::vector<vibr::PresetManager::Factory> ml1FactoryPresets()
{
    return {
        { "Init", "A chorus at its defaults", {} },
        { "Lush Chorus", "Four voices, wide", { { "mode", 0 }, { "variant", 2 }, { "rate", 35 }, { "depth", 55 }, { "extra", 70 }, { "mix", 50 } } },
        { "Tight Chorus", "Two voices, subtle", { { "mode", 0 }, { "variant", 0 }, { "rate", 45 }, { "depth", 30 }, { "extra", 40 }, { "mix", 40 } } },
        { "Soft Vibrato", "A gentle sine pitch wobble", { { "mode", 1 }, { "variant", 0 }, { "rate", 40 }, { "depth", 35 }, { "mix", 100 } } },
        { "Triangle Vibrato", "A triangle wobble", { { "mode", 1 }, { "variant", 1 }, { "rate", 55 }, { "depth", 45 }, { "mix", 100 } } },
        { "Jet Flanger", "A slow, deep flanger with feedback", { { "mode", 2 }, { "variant", 0 }, { "rate", 20 }, { "depth", 75 }, { "extra", 65 }, { "mix", 50 } } },
        { "Through-Zero Feel", "Negative feedback flanging", { { "mode", 2 }, { "variant", 1 }, { "rate", 25 }, { "depth", 70 }, { "extra", 60 }, { "mix", 50 } } },
        { "Four-Stage Phaser", "A classic phaser", { { "mode", 3 }, { "variant", 0 }, { "rate", 30 }, { "depth", 60 }, { "extra", 40 }, { "mix", 50 } } },
        { "Deep Phaser", "Eight stages with feedback", { { "mode", 3 }, { "variant", 2 }, { "rate", 25 }, { "depth", 75 }, { "extra", 60 }, { "mix", 55 } } },
    };
}
} // namespace

ML1AudioProcessor::ML1AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "ML1", createLayout()),
      presets (*this, apvts, undoManager, "ML-1 Presets", ml1FactoryPresets(), { "power", "bypass" })
{
    pPower   = apvts.getRawParameterValue ("power");
    pMode    = apvts.getRawParameterValue ("mode");
    pVariant = apvts.getRawParameterValue ("variant");
    pSync    = apvts.getRawParameterValue ("sync");
    pRate    = apvts.getRawParameterValue ("rate");
    pDepth   = apvts.getRawParameterValue ("depth");
    pExtra   = apvts.getRawParameterValue ("extra");
    pMix     = apvts.getRawParameterValue ("mix");
    pLevel   = apvts.getRawParameterValue ("level");
    midiMap.setDefault ("depth", 1);
    midiMap.setDefault ("rate", 11);
}

bool ML1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void ML1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare ((float) sampleRate);
    powerSmooth.reset (sampleRate, 0.02);
    powerSmooth.setCurrentAndTargetValue (pPower->load() > 0.5f ? 1.0f : 0.0f);
    dryBuf.setSize (2, juce::jmax (samplesPerBlock, 8192));
}

void ML1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
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

    ml1::Params p;
    p.mode = juce::jlimit (0, ml1::kNumModes - 1, (int) std::lround (pMode->load()));
    p.variant = juce::jlimit (0, ml1::variantCount (p.mode) - 1, (int) std::lround (pVariant->load()));
    p.sync = pSync->load() > 0.5f;
    p.bpm = 120.0f;                                                  // the host tempo (120 when the host gives none)
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) p.bpm = (float) juce::jlimit (20.0, 400.0, *b);
    p.rate = pRate->load() * 0.01f;
    p.depth = pDepth->load() * 0.01f;
    p.extra = pExtra->load() * 0.01f;
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

juce::AudioProcessorEditor* ML1AudioProcessor::createEditor() { return new ML1AudioProcessorEditor (*this); }

void ML1AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.appendChild (midiMap.toTree(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void ML1AudioProcessor::setStateInformation (const void* data, int size)
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

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ML1AudioProcessor(); }
