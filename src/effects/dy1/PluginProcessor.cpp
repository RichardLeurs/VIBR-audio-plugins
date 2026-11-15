// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout DY1AudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    const auto flt = [&layout] (const char* id, const char* name, float lo, float hi, float step, float def, const char* unit = "")
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, step), def, AudioParameterFloatAttributes().withLabel (unit)));
    };
    const auto flag = [&layout] (const char* id, const char* name, bool def)
    {
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def, AudioParameterBoolAttributes().withMeta (juce::String (id) == "power" || juce::String (id) == "bypass")));
    };
    // the release knobs are logarithmic: 10 ms .. 800 ms
    const auto rel = [&layout] (const char* id, const char* name, float def)
    {
        NormalisableRange<float> r (10.0f, 800.0f, 1.0f);
        r.setSkewForCentre (90.0f);
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, AudioParameterFloatAttributes().withLabel ("ms")));
    };
    flag ("power", "Power", true);
    flag ("bypass", "Bypass", false);
    flag ("gateOn", "Gate On", true);
    flt ("gateThresh", "Gate Threshold", -80.0f, -10.0f, 1.0f, -50.0f, "dB");
    rel ("gateRelease", "Gate Release", 120.0f);
    flag ("expOn", "Expander On", false);
    flt ("expThresh", "Expander Threshold", -80.0f, -10.0f, 1.0f, -45.0f, "dB");
    flt ("expRatio", "Expander Ratio", 1.0f, 10.0f, 0.1f, 2.0f);
    rel ("expRelease", "Expander Release", 150.0f);
    flag ("transOn", "Transient Shaper On", true);
    flt ("attack", "Attack", -100.0f, 100.0f, 1.0f, 0.0f, "%");
    flt ("sustain", "Sustain", -100.0f, 100.0f, 1.0f, 0.0f, "%");
    flt ("mix", "Mix", 0.0f, 100.0f, 1.0f, 100.0f, "%");
    flt ("level", "Level", -24.0f, 12.0f, 1.0f, 0.0f, "dB");
    return layout;
}

namespace
{
// the factory presets (values are plain host values)
std::vector<vibr::PresetManager::Factory> dy1FactoryPresets()
{
    return {
        { "Init", "Gate and transient shaper at their defaults", {} },
        { "Tight Drums", "Gate the ring, add punch", { { "gateThresh", -42 }, { "gateRelease", 80 }, { "attack", 45 }, { "sustain", -30 } } },
        { "Snare Snap", "Sharp attack, short tail", { { "gateThresh", -46 }, { "gateRelease", 60 }, { "attack", 65 }, { "sustain", -45 } } },
        { "Room Tamer", "Expand the room away", { { "gateOn", 0 }, { "expOn", 1 }, { "expThresh", -38 }, { "expRatio", 3 }, { "expRelease", 220 }, { "sustain", -35 } } },
        { "Sustain Boost", "Longer notes, softer attack", { { "gateOn", 0 }, { "attack", -15 }, { "sustain", 60 } } },
        { "Gentle Gate", "A slow gate for noisy takes", { { "gateThresh", -56 }, { "gateRelease", 300 }, { "transOn", 0 } } },
        { "Soft Pad", "Rounded attacks, an open tail", { { "gateOn", 0 }, { "attack", -60 }, { "sustain", 25 } } },
    };
}
} // namespace

DY1AudioProcessor::DY1AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, &undoManager, "DY1", createLayout()),
      presets (*this, apvts, undoManager, "DY-1 Presets", dy1FactoryPresets(), { "power", "bypass" })
{
    const auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pPower = get ("power"); pGateOn = get ("gateOn"); pGateThresh = get ("gateThresh"); pGateRelease = get ("gateRelease");
    pExpOn = get ("expOn"); pExpThresh = get ("expThresh"); pExpRatio = get ("expRatio"); pExpRelease = get ("expRelease");
    pTransOn = get ("transOn"); pAttack = get ("attack"); pSustain = get ("sustain"); pMix = get ("mix"); pLevel = get ("level");
    midiMap.setDefault ("gateThresh", 11);
}

bool DY1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo() || layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    const auto sc = layouts.getChannelSet (true, 1);                       // the sidechain: off, mono or stereo
    return sc.isDisabled() || sc == juce::AudioChannelSet::mono() || sc == juce::AudioChannelSet::stereo();
}

void DY1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare ((float) sampleRate);
    powerSmooth.reset (sampleRate, 0.02);
    powerSmooth.setCurrentAndTargetValue (pPower->load() > 0.5f ? 1.0f : 0.0f);
    dryBuf.setSize (2, juce::jmax (samplesPerBlock, 8192));
    keyBuf.setSize (2, juce::jmax (samplesPerBlock, 8192));
}

void DY1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    midiMap.process (midi);
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0 || buffer.getNumChannels() < 2 || n > dryBuf.getNumSamples()) return;

    // the external key: the sidechain bus when the host has connected it (a mono key feeds both channels); it drives the gate, expander and transient detectors
    const float* keyL = nullptr; const float* keyR = nullptr;
    if (auto* scBus = getBus (true, 1); scBus != nullptr && scBus->isEnabled() && n <= keyBuf.getNumSamples())
    {
        const auto sc = getBusBuffer (buffer, true, 1);
        if (sc.getNumChannels() > 0)
        {
            for (int c = 0; c < 2; ++c)
            {
                const auto* src = sc.getReadPointer (juce::jmin (c, sc.getNumChannels() - 1));
                auto* k = keyBuf.getWritePointer (c);
                for (int i = 0; i < n; ++i) k[i] = std::abs (src[i]) < 1.0e4f ? src[i] : 0.0f;
            }
            keyL = keyBuf.getReadPointer (0); keyR = keyBuf.getReadPointer (1);
        }
    }
    extKey.store (keyL != nullptr, std::memory_order_relaxed);

    const float from = powerSmooth.getCurrentValue();
    powerSmooth.setTargetValue (pPower->load() > 0.5f ? 1.0f : 0.0f);
    const float to = powerSmooth.skip (n);
    if (from == 0.0f && to == 0.0f) { outMeter.measure (buffer, n); return; }                        // off: the signal passes untouched

    for (int c = 0; c < 2; ++c) dryBuf.copyFrom (c, 0, buffer, c, 0, n);

    dy1::Params p;
    p.gateOn = pGateOn->load() > 0.5f;    p.gateThresh = pGateThresh->load();    p.gateRelease = pGateRelease->load();
    p.expOn = pExpOn->load() > 0.5f;      p.expThresh = pExpThresh->load();      p.expRatio = pExpRatio->load();   p.expRelease = pExpRelease->load();
    p.transOn = pTransOn->load() > 0.5f;  p.attack = pAttack->load();            p.sustain = pSustain->load();
    p.mix = pMix->load() * 0.01f;
    engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n, p, juce::Decibels::decibelsToGain (pLevel->load()), keyL, keyR);

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

juce::AudioProcessorEditor* DY1AudioProcessor::createEditor() { return new DY1AudioProcessorEditor (*this); }

void DY1AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.appendChild (midiMap.toTree(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void DY1AudioProcessor::setStateInformation (const void* data, int size)
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

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DY1AudioProcessor(); }
