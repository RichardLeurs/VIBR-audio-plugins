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
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
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

DY1AudioProcessor::DY1AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "DY1", createLayout())
{
    const auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pPower = get ("power"); pGateOn = get ("gateOn"); pGateThresh = get ("gateThresh"); pGateRelease = get ("gateRelease");
    pExpOn = get ("expOn"); pExpThresh = get ("expThresh"); pExpRatio = get ("expRatio"); pExpRelease = get ("expRelease");
    pTransOn = get ("transOn"); pAttack = get ("attack"); pSustain = get ("sustain"); pMix = get ("mix"); pLevel = get ("level");
    midiMap.setDefault ("gateThresh", 11);
}

bool DY1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void DY1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare ((float) sampleRate);
    powerSmooth.reset (sampleRate, 0.02);
    powerSmooth.setCurrentAndTargetValue (pPower->load() > 0.5f ? 1.0f : 0.0f);
    dryBuf.setSize (2, juce::jmax (samplesPerBlock, 8192));
}

void DY1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
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

    dy1::Params p;
    p.gateOn = pGateOn->load() > 0.5f;    p.gateThresh = pGateThresh->load();    p.gateRelease = pGateRelease->load();
    p.expOn = pExpOn->load() > 0.5f;      p.expThresh = pExpThresh->load();      p.expRatio = pExpRatio->load();   p.expRelease = pExpRelease->load();
    p.transOn = pTransOn->load() > 0.5f;  p.attack = pAttack->load();            p.sustain = pSustain->load();
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
