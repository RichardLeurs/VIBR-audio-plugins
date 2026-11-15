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
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { "power", 1 }, "Power", true, juce::AudioParameterBoolAttributes().withMeta (true)));
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

namespace
{
// the factory presets (values are plain host values; every matrix depth m<source><destination> is 0 .. 100 with 50 = no modulation, 100 = +100 %)
std::vector<vibr::PresetManager::Factory> vc1FactoryPresets()
{
    return {
        { "Init", "A starting patch: LFO 1 and the envelope on the filter", {} },
        { "Auto Wah", "The playing opens the filter", { { "cutoff", 25 }, { "res", 65 }, { "envsens", 60 }, { "envrel", 45 }, { "m00", 95 }, { "m10", 50 }, { "type", 1 } } },
        { "Slow Sweep", "A slow LFO moves a low pass", { { "cutoff", 40 }, { "res", 55 }, { "rate1", 15 }, { "m00", 50 }, { "m10", 90 } } },
        { "Tremolo Pan", "LFO 1 on the level, LFO 2 on the pan", { { "cutoff", 100 }, { "res", 0 }, { "rate1", 55 }, { "rate2", 35 }, { "m00", 50 }, { "m10", 50 }, { "m11", 85 }, { "m23", 90 } } },
        { "Vibrato Drift", "Random and LFO move the pitch", { { "cutoff", 100 }, { "res", 0 }, { "rate1", 60 }, { "m00", 50 }, { "m10", 50 }, { "m12", 62 }, { "m32", 58 } } },
        { "Envelope Ducker", "Loud notes pull the level down", { { "cutoff", 100 }, { "res", 0 }, { "envsens", 55 }, { "envrel", 55 }, { "m00", 50 }, { "m10", 50 }, { "m01", 10 } } },
        { "Random Filter", "Stepped random on the cutoff", { { "cutoff", 45 }, { "res", 60 }, { "rand", 55 }, { "m00", 50 }, { "m10", 50 }, { "m30", 92 } } },
        { "Cross-Mod Wobble", "LFO 2 bends the rate of LFO 1", { { "cutoff", 40 }, { "res", 60 }, { "rate1", 50 }, { "rate2", 40 }, { "xmod", 65 }, { "m10", 88 }, { "m20", 60 } } },
        { "Wheel Filter", "The mod wheel opens the filter", { { "cutoff", 20 }, { "res", 55 }, { "m00", 50 }, { "m10", 50 }, { "m40", 100 } } },
    };
}
} // namespace

VC1AudioProcessor::VC1AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "VC1", createLayout()),
      presets (*this, apvts, undoManager, "VC-1 Presets", vc1FactoryPresets(), { "power", "bypass" })
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
    midiMap.process (midi);
    if (n == 0 || buffer.getNumChannels() < 2 || n > dryBuf.getNumSamples()) return;

    const float from = powerSmooth.getCurrentValue();
    powerSmooth.setTargetValue (pPower->load() > 0.5f ? 1.0f : 0.0f);
    const float to = powerSmooth.skip (n);
    if (from == 0.0f && to == 0.0f) { outMeter.measure (buffer, n); return; }                          // off: the signal passes untouched

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
    outMeter.measure (buffer, n);
}

juce::AudioProcessorEditor* VC1AudioProcessor::createEditor() { return new VC1AudioProcessorEditor (*this); }

void VC1AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.appendChild (midiMap.toTree(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void VC1AudioProcessor::setStateInformation (const void* data, int size)
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

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VC1AudioProcessor(); }
