// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
// host label and readable text for a float parameter (the stored value is the shown value; Hz above 1000 reads as kHz)
juce::AudioParameterFloatAttributes unitAttrs (const juce::String& unit, int decimals)
{
    return juce::AudioParameterFloatAttributes()
        .withLabel (unit)
        .withStringFromValueFunction ([unit, decimals] (float v, int)
        {
            if (unit == "Hz" && std::abs (v) >= 1000.0f) return juce::String (v / 1000.0f, 2) + " kHz";
            return juce::String (v, decimals) + (unit.isEmpty() ? juce::String() : " " + unit);
        })
        .withValueFromStringFunction ([unit] (const juce::String& s)
        {
            const float v = s.retainCharacters ("0123456789.-+").getFloatValue();
            return (unit == "Hz" && s.containsIgnoreCase ("k")) ? v * 1000.0f : v;
        });
}


} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout RV1AudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    const auto pct = [&layout] (const char* id, const char* name, float def)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 100.0f, 1.0f), def, unitAttrs ("%", 0)));
    };
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { "power", 1 }, "Power", true));
    layout.add (vibr::makeBypassParam());
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode",
                                                        StringArray { "Reverse", "Scrub", "Stutter", "Glitch", "Repeat" }, 0));
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { "freeze", 1 }, "Freeze", false));
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { "sync", 1 }, "Sync (Length follows the host tempo)", false));
    pct ("length", "Length", 50.0f);
    pct ("speed", "Speed", 50.0f);
    pct ("smooth", "Smooth", 40.0f);
    pct ("random", "Random", 30.0f);
    pct ("repeats", "Repeats", 50.0f);
    pct ("mix", "Mix", 70.0f);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "level", 1 }, "Level", NormalisableRange<float> (-24.0f, 12.0f, 1.0f), 0.0f, unitAttrs ("dB", 0)));
    return layout;
}

namespace
{
// the factory presets (values are plain host values; length / speed / smooth / random / repeats / mix 0 .. 100)
std::vector<vibr::PresetManager::Factory> rv1FactoryPresets()
{
    return {
        { "Init", "Reverse at its defaults", {} },
        { "Reverse Echo", "Short reversed slices", { { "mode", 0 }, { "length", 45 }, { "speed", 50 }, { "smooth", 50 }, { "repeats", 35 }, { "mix", 60 } } },
        { "Half-Speed Reverse", "Slow, dark reversed swells", { { "mode", 0 }, { "length", 70 }, { "speed", 25 }, { "smooth", 60 }, { "mix", 70 } } },
        { "Tape Scrub", "The read position wanders", { { "mode", 1 }, { "length", 55 }, { "random", 45 }, { "repeats", 40 }, { "mix", 70 } } },
        { "Stutter Gate", "A short repeat", { { "mode", 2 }, { "length", 20 }, { "random", 10 }, { "repeats", 60 }, { "mix", 80 } } },
        { "Glitch Machine", "Chopped, random slices", { { "mode", 3 }, { "length", 30 }, { "random", 70 }, { "repeats", 60 }, { "mix", 85 } } },
        { "Beat Repeat", "A repeat locked to the tempo", { { "mode", 4 }, { "sync", 1 }, { "length", 50 }, { "repeats", 50 }, { "mix", 90 } } },
        { "Frozen Slice", "Freeze what just played", { { "mode", 0 }, { "freeze", 1 }, { "length", 60 }, { "mix", 100 } } },
    };
}
} // namespace

RV1AudioProcessor::RV1AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "RL1", createLayout()),
      presets (*this, apvts, undoManager, "RV-1 Presets", rv1FactoryPresets(), { "power", "bypass" })
{
    pPower   = apvts.getRawParameterValue ("power");
    pMode    = apvts.getRawParameterValue ("mode");
    pFreeze  = apvts.getRawParameterValue ("freeze");
    pSync    = apvts.getRawParameterValue ("sync");
    pLength  = apvts.getRawParameterValue ("length");
    pSpeed   = apvts.getRawParameterValue ("speed");
    pSmooth  = apvts.getRawParameterValue ("smooth");
    pRandom  = apvts.getRawParameterValue ("random");
    pRepeats = apvts.getRawParameterValue ("repeats");
    pMix     = apvts.getRawParameterValue ("mix");
    pLevel   = apvts.getRawParameterValue ("level");
}

bool RV1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void RV1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare ((float) sampleRate);
    powerSmooth.reset (sampleRate, 0.02);
    powerSmooth.setCurrentAndTargetValue (pPower->load() > 0.5f ? 1.0f : 0.0f);
    dryBuf.setSize (2, juce::jmax (samplesPerBlock, 8192));
}

void RV1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0 || buffer.getNumChannels() < 2 || n > dryBuf.getNumSamples()) return;

    const float from = powerSmooth.getCurrentValue();
    powerSmooth.setTargetValue (pPower->load() > 0.5f ? 1.0f : 0.0f);
    const float to = powerSmooth.skip (n);
    if (from == 0.0f && to == 0.0f) return;                          // off: the signal passes untouched

    for (int c = 0; c < 2; ++c) dryBuf.copyFrom (c, 0, buffer, c, 0, n);

    rv1::Params p;
    p.mode = juce::jlimit (0, rv1::kNumModes - 1, (int) std::lround (pMode->load()));
    p.freeze = pFreeze->load() > 0.5f;
    p.sync = pSync->load() > 0.5f;
    p.bpm = 120.0f;                                                  // the host tempo (120 when the host gives none)
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) p.bpm = (float) juce::jlimit (20.0, 400.0, *b);
    p.length = pLength->load() * 0.01f;
    p.speed = pSpeed->load() * 0.01f;
    p.smooth = pSmooth->load() * 0.01f;
    p.random = pRandom->load() * 0.01f;
    p.repeats = pRepeats->load() * 0.01f;
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

juce::AudioProcessorEditor* RV1AudioProcessor::createEditor() { return new RV1AudioProcessorEditor (*this); }

void RV1AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void RV1AudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RV1AudioProcessor(); }
