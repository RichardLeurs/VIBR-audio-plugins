// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout PH2AudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    const auto knob = [&] (const char* id, const char* name, float def)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 10.0f, 0.01f), def));
    };
    knob ("res",    "Res",    4.0f);
    knob ("depth",  "Depth",  5.0f);
    knob ("rate",   "Rate",   3.0f);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode",
                                                        StringArray { "I", "II" }, ph2::kModeI));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "bypass", 1 }, "Bypass", false));
    // tempo sync (plugin extension): off = the RATE knob sets the LFO as before; on = one LFO cycle per DIVISION of the host tempo
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "sync", 1 }, "Sync", false));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "division", 1 }, "Division", vibr::sync::divisionNames(), 3));   // 3 = "1/4"
    return layout;
}

PH2AudioProcessor::PH2AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PH2", createLayout())
{
    pRes    = apvts.getRawParameterValue ("res");
    pDepth  = apvts.getRawParameterValue ("depth");
    pRate   = apvts.getRawParameterValue ("rate");
    pMode   = apvts.getRawParameterValue ("mode");
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("bypass"));
    pSync     = apvts.getRawParameterValue ("sync");
    pDivision = apvts.getRawParameterValue ("division");

    midiMap.setDefault ("depth", 1);                  // mod wheel
    midiMap.setDefault ("rate", 11);                  // expression pedal
    midiMap.setSyncAlias ("rate", "sync", "division");
}

bool PH2AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) && out == layouts.getMainInputChannelSet();
}

void PH2AudioProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    for (auto& f : phaser) f.reset();
    phase = 0.0;
    resSmooth.reset (sampleRate, 0.03);    resSmooth.setCurrentAndTargetValue (pRes->load() * 0.1f);
    depthSmooth.reset (sampleRate, 0.03);  depthSmooth.setCurrentAndTargetValue (pDepth->load() * 0.1f);
    rateSmooth.reset (sampleRate, 0.03);   rateSmooth.setCurrentAndTargetValue (pRate->load() * 0.1f);
    modeSmooth.reset (sampleRate, 0.05);   modeSmooth.setCurrentAndTargetValue (pMode->load());
    syncMix.reset (sampleRate, 0.05);     syncMix.setCurrentAndTargetValue (pSync->load() > 0.5f ? 1.0f : 0.0f);
    syncHzSmooth.reset (sampleRate, 0.1); syncHzSmooth.setCurrentAndTargetValue (ph2::syncedHz (vibr::sync::hostBpm (*this, 120.0), (int) pDivision->load()));
    wetSmooth.reset (sampleRate, 0.01);    wetSmooth.setCurrentAndTargetValue (bypassParam->get() ? 0.0f : 1.0f);
}

void PH2AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    midiMap.process (midi);
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int channels = juce::jmin (buffer.getNumChannels(), kMaxChannels);
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, n);
    if (n == 0 || channels == 0) return;

    resSmooth.setTargetValue (pRes->load() * 0.1f);
    depthSmooth.setTargetValue (pDepth->load() * 0.1f);
    rateSmooth.setTargetValue (pRate->load() * 0.1f);
    modeSmooth.setTargetValue (pMode->load());
    wetSmooth.setTargetValue (bypassParam->get() ? 0.0f : 1.0f);
    // tempo sync: target frequency from the host tempo once per block; glides (multiplicative, 100 ms) on tempo / division changes,
    // and SYNC on / off crossfades the knob and synced frequencies over 50 ms so the LFO speed bends instead of jumping
    syncMix.setTargetValue (pSync->load() > 0.5f ? 1.0f : 0.0f);
    syncHzSmooth.setTargetValue (ph2::syncedHz (vibr::sync::hostBpm (*this, 120.0), (int) pDivision->load()));

    for (int i = 0; i < n; ++i)
    {
        const float res = resSmooth.getNextValue(), depth = depthSmooth.getNextValue(), rate = rateSmooth.getNextValue();
        const float mode = modeSmooth.getNextValue(), bypassMix = wetSmooth.getNextValue();

        const float knobHz = ph2::rateHz (rate), syncHz = syncHzSmooth.getNextValue(), mix = syncMix.getNextValue();
        const float lfoHz = mix <= 0.0f ? knobHz : knobHz + (syncHz - knobHz) * mix;           // sync off: exactly the knob frequency, as before
        phase += (double) lfoHz / sr;
        if (phase >= 1.0) phase -= 1.0;
        const float ph = (float) phase;
        // right channel's sweep is 90 degrees behind for width
        const float lfo[2] = { std::sin (juce::MathConstants<float>::twoPi * ph), std::sin (juce::MathConstants<float>::twoPi * (ph + 0.25f)) };
        const float octaves = 0.5f + 2.2f * depth;
        const float fb = ph2::feedbackAmount (res, mode > 0.5f ? ph2::kModeII : ph2::kModeI);
        const int stages = mode > 0.5f ? 8 : 4;

        for (int c = 0; c < channels; ++c)
        {
            auto* x = buffer.getWritePointer (c);
            const float in = x[i];
            const float wet = phaser[c].process (in, lfo[c], 650.0f, octaves, fb, stages, sr);
            x[i] = in + (0.5f * (in + wet) - in) * bypassMix;
        }
    }
}

juce::AudioProcessorEditor* PH2AudioProcessor::createEditor() { return new PH2AudioProcessorEditor (*this); }

void PH2AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.appendChild (midiMap.toTree(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void PH2AudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto st = juce::ValueTree::fromXml (*xml);
            midiMap.fromTree (st.getChildWithName ("MIDIMAP"));
            st.removeChild (st.getChildWithName ("MIDIMAP"), nullptr);
            apvts.replaceState (st);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PH2AudioProcessor(); }
