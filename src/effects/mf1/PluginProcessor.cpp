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
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { "power", 1 }, "Power", true, juce::AudioParameterBoolAttributes().withMeta (true)));
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

namespace
{
// the factory presets (plain host values: type 0 low, 1 band, 2 high, 3 notch, 4 vowel; source 0 LFO, 1 envelope, 2 steps; cutoff / res / depth / m1 / m2 / mix / steps 0 .. 100)
std::vector<vibr::PresetManager::Factory> mf1FactoryPresets()
{
    return {
        { "Init", "Motion Filter at its defaults", {} },
        { "Slow Sweep", "A band pass drifting up and down", { { "type", 1 }, { "source", 0 }, { "cutoff", 45 }, { "res", 50 }, { "depth", 70 }, { "m1", 15 }, { "m2", 0 }, { "mix", 100 } } },
        { "Wobble Bass", "Low pass, fast wobble", { { "type", 0 }, { "source", 0 }, { "cutoff", 30 }, { "res", 70 }, { "depth", 80 }, { "m1", 55 }, { "m2", 0 }, { "mix", 100 } } },
        { "Auto Wah", "The playing opens the filter", { { "type", 1 }, { "source", 1 }, { "cutoff", 25 }, { "res", 65 }, { "depth", 85 }, { "m1", 55 }, { "m2", 35 }, { "mix", 100 } } },
        { "Envelope Pluck", "A fast decay on a low pass", { { "type", 0 }, { "source", 1 }, { "cutoff", 20 }, { "res", 60 }, { "depth", 90 }, { "m1", 70 }, { "m2", 20 }, { "mix", 100 } } },
        { "Step Groove", "Eight steps, locked to the tempo", { { "type", 0 }, { "source", 2 }, { "sync", 1 }, { "cutoff", 35 }, { "res", 60 }, { "depth", 80 }, { "m1", 50 }, { "m2", 10 }, { "mix", 100 } } },
        { "Vowel Talker", "Vowel shapes moved by the LFO", { { "type", 4 }, { "source", 0 }, { "cutoff", 50 }, { "res", 60 }, { "depth", 75 }, { "m1", 25 }, { "m2", 0 }, { "mix", 100 } } },
        { "Notch Phase", "A swirling notch, half dry", { { "type", 3 }, { "source", 0 }, { "cutoff", 55 }, { "res", 55 }, { "depth", 60 }, { "m1", 20 }, { "m2", 0 }, { "mix", 60 } } },
    };
}
} // namespace

MF1AudioProcessor::MF1AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "MF1", createLayout()),
      presets (*this, apvts, undoManager, "MF-1 Presets", mf1FactoryPresets(), { "power", "bypass" })
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
    midiMap.setDefault ("depth", 1);
    midiMap.setDefault ("cutoff", 11);
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

void MF1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    midiMap.process (midi);
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0 || buffer.getNumChannels() < 2 || n > dryBuf.getNumSamples()) return;

    const float from = powerSmooth.getCurrentValue();
    powerSmooth.setTargetValue (pPower->load() > 0.5f ? 1.0f : 0.0f);
    const float to = powerSmooth.skip (n);
    if (from == 0.0f && to == 0.0f) { outMeter.measure (buffer, n); return; }                          // off: the signal passes untouched

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
    outMeter.measure (buffer, n);
}

juce::AudioProcessorEditor* MF1AudioProcessor::createEditor() { return new MF1AudioProcessorEditor (*this); }

void MF1AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.appendChild (midiMap.toTree(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void MF1AudioProcessor::setStateInformation (const void* data, int size)
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

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MF1AudioProcessor(); }
