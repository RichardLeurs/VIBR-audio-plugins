// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout BF3AudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    const auto knob = [&] (const char* id, const char* name, float def)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 10.0f, 0.01f), def));
    };
    knob ("manual", "Manual", 5.0f);
    knob ("res",    "Res",    4.0f);
    knob ("depth",  "Depth",  5.0f);
    knob ("rate",   "Rate",   3.0f);           // 0..10 -> 0.05 .. 8 Hz, exponential (bf3::rateHz); ignored while sync is on
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode",
                                                        StringArray { "Gate/Pan", "Standard", "Ultra" }, bf3::kModeStandard));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "bypass", 1 }, "Bypass", false));
    // tempo sync: on = one LFO cycle per division of the host tempo (clamped to 0.05 .. 8 Hz) instead of the RATE knob
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "sync", 1 }, "Sync", false));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "division", 1 }, "Division", vibr::sync::divisionNames(),
                                                        vibr::sync::divisionNames().indexOf ("1/4")));
    return layout;
}

BF3AudioProcessor::BF3AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BF3", createLayout())
{
    pManual = apvts.getRawParameterValue ("manual");
    pRes    = apvts.getRawParameterValue ("res");
    pDepth  = apvts.getRawParameterValue ("depth");
    pRate   = apvts.getRawParameterValue ("rate");
    pMode   = apvts.getRawParameterValue ("mode");
    pSync   = apvts.getRawParameterValue ("sync");
    pDivision = apvts.getRawParameterValue ("division");
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("bypass"));

    midiMap.setDefault ("depth", 1);                  // mod wheel
    midiMap.setDefault ("rate", 11);                  // expression pedal
    midiMap.setSyncAlias ("rate", "sync", "division");
}

float BF3AudioProcessor::rateTarget01 (double tempo) const
{
    if (pSync->load() < 0.5f) return pRate->load() * 0.1f;               // sync off: exactly the knob, as before
    return bf3::rate01ForHz ((float) vibr::sync::hz (tempo, (int) pDivision->load()));
}

bool BF3AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) && out == layouts.getMainInputChannelSet();
}

void BF3AudioProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    for (auto& f : flanger) f.prepare (sampleRate);
    phase = 0.0;
    manualSmooth.reset (sampleRate, 0.03); manualSmooth.setCurrentAndTargetValue (pManual->load() * 0.1f);
    resSmooth.reset (sampleRate, 0.03);    resSmooth.setCurrentAndTargetValue (pRes->load() * 0.1f);
    depthSmooth.reset (sampleRate, 0.03);  depthSmooth.setCurrentAndTargetValue (pDepth->load() * 0.1f);
    rateSmooth.reset (sampleRate, 0.03);   rateSmooth.setCurrentAndTargetValue (rateTarget01 (bpm));
    modeSmooth.reset (sampleRate, 0.05);   modeSmooth.setCurrentAndTargetValue (pMode->load());
    wetSmooth.reset (sampleRate, 0.01);    wetSmooth.setCurrentAndTargetValue (bypassParam->get() ? 0.0f : 1.0f);
}

void BF3AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    midiMap.process (midi);
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int channels = juce::jmin (buffer.getNumChannels(), kMaxChannels);
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, n);
    if (n == 0 || channels == 0) return;

    manualSmooth.setTargetValue (pManual->load() * 0.1f);
    resSmooth.setTargetValue (pRes->load() * 0.1f);
    depthSmooth.setTargetValue (pDepth->load() * 0.1f);
    bpm = vibr::sync::hostBpm (*this, 120.0);
    rateSmooth.setTargetValue (rateTarget01 (bpm));                     // smoothed (30 ms), so tempo changes and SYNC on / off glide
    modeSmooth.setTargetValue (pMode->load());
    wetSmooth.setTargetValue (bypassParam->get() ? 0.0f : 1.0f);

    const float sampleRateF = (float) sr;
    for (int i = 0; i < n; ++i)
    {
        const float manual = manualSmooth.getNextValue(), res = resSmooth.getNextValue(), depth = depthSmooth.getNextValue();
        const float rate = rateSmooth.getNextValue(), mode = modeSmooth.getNextValue(), bypassMix = wetSmooth.getNextValue();

        // mode as a continuous value (0 gate/pan, 1 standard, 2 ultra) so switching it never clicks
        const float ultra = juce::jlimit (0.0f, 1.0f, mode - 1.0f);                    // 0 .. 1 from standard to ultra
        const float pan = juce::jlimit (0.0f, 1.0f, 1.0f - mode);                       // 1 in gate / pan, 0 otherwise
        const float oct = depth * (2.0f + ultra);
        const float fb = res * (0.85f + 0.12f * ultra);
        const float centre = bf3::centreMs (manual) * 0.001f * sampleRateF;             // samples

        phase += (double) bf3::rateHz (rate) / sr;
        if (phase >= 1.0) phase -= 1.0;
        const float ph = (float) phase;

        // the right channel's sweep is 90 degrees behind (not in gate / pan, where both sweep together)
        const float lfo[2] = { bf3::triangle (ph), bf3::triangle (ph + 0.25f * (1.0f - pan)) };
        // pan: the flange moves between the two outputs (slower than the sweep, a sine over two LFO periods)
        const float panPos = 0.5f + 0.5f * std::sin (juce::MathConstants<float>::twoPi * ph * 0.5f);
        const float panGain[2] = { std::cos (0.5f * juce::MathConstants<float>::pi * panPos), std::sin (0.5f * juce::MathConstants<float>::pi * panPos) };

        for (int c = 0; c < channels; ++c)
        {
            auto* x = buffer.getWritePointer (c);
            const float in = x[i];
            const float delay = juce::jlimit (2.0f, 0.025f * sampleRateF, centre * std::exp2 (oct * lfo[c]));
            const float wet = flanger[c].process (in, delay, fb);
            const float wetGain = 0.7f + pan * (1.4f * panGain[c] - 0.7f);            // plain 0.7, or panned (equal-power, 0 dB centre)
            const float effect = 0.7f * in + wetGain * wet;
            x[i] = in + (effect - in) * bypassMix;
        }
    }
}

juce::AudioProcessorEditor* BF3AudioProcessor::createEditor() { return new BF3AudioProcessorEditor (*this); }

void BF3AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.appendChild (midiMap.toTree(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void BF3AudioProcessor::setStateInformation (const void* data, int size)
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

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BF3AudioProcessor(); }
