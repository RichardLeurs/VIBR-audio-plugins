// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout CS3AudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    const auto knob = [&] (const char* id, const char* name, float def)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 10.0f, 0.01f), def));
    };
    knob ("level",   "Level",   6.0f);
    knob ("tone",    "Tone",    5.0f);
    knob ("attack",  "Attack",  3.0f);
    knob ("sustain", "Sustain", 5.0f);
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "bypass", 1 }, "Bypass", false));
    return layout;
}

CS3AudioProcessor::CS3AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "CS3", createLayout())
{
    pLevel   = apvts.getRawParameterValue ("level");
    pTone    = apvts.getRawParameterValue ("tone");
    pAttack  = apvts.getRawParameterValue ("attack");
    pSustain = apvts.getRawParameterValue ("sustain");
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("bypass"));
}

bool CS3AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) && out == layouts.getMainInputChannelSet();
}

void CS3AudioProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    for (auto& f : low) f.reset();
    for (auto& f : high) f.reset();
    envDb = -120.0f;
    levelSmooth.reset (sampleRate, 0.03);   levelSmooth.setCurrentAndTargetValue (pLevel->load());
    toneSmooth.reset (sampleRate, 0.03);    toneSmooth.setCurrentAndTargetValue (pTone->load());
    attackSmooth.reset (sampleRate, 0.03);  attackSmooth.setCurrentAndTargetValue (pAttack->load());
    sustainSmooth.reset (sampleRate, 0.03); sustainSmooth.setCurrentAndTargetValue (pSustain->load());
    wetSmooth.reset (sampleRate, 0.01);     wetSmooth.setCurrentAndTargetValue (bypassParam->get() ? 0.0f : 1.0f);
}

void CS3AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int channels = juce::jmin (buffer.getNumChannels(), kMaxChannels);
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, n);
    if (n == 0 || channels == 0) return;

    levelSmooth.setTargetValue (pLevel->load());
    toneSmooth.setTargetValue (pTone->load());
    attackSmooth.setTargetValue (pAttack->load());
    sustainSmooth.setTargetValue (pSustain->load());
    wetSmooth.setTargetValue (bypassParam->get() ? 0.0f : 1.0f);

    // tone filters: updated once per block from the smoothed knob (no allocation)
    toneSmooth.skip (n);
    const float tone = toneSmooth.getCurrentValue();
    const float tiltDb = (tone - 5.0f) * 1.6f;
    for (int c = 0; c < channels; ++c) { low[c].set (sr, 350.0f, -tiltDb, false); high[c].set (sr, 2500.0f, tiltDb, true); }

    float* data[kMaxChannels] = {};
    for (int c = 0; c < channels; ++c) data[c] = buffer.getWritePointer (c);

    float reduction = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float lv = levelSmooth.getNextValue(), atk = attackSmooth.getNextValue() * 0.1f, sus = sustainSmooth.getNextValue() * 0.1f;
        const float bypassMix = wetSmooth.getNextValue();

        float peak = 0.0f;
        for (int c = 0; c < channels; ++c) peak = juce::jmax (peak, std::fabs (data[c][i]));
        const float inDb = 20.0f * std::log10 (peak + 1.0e-6f);

        // envelope follower in dB: attack when rising, release when falling
        const float aCoef = std::exp (-1.0f / (0.001f * cs3::attackMs (atk) * (float) sr));
        const float rCoef = std::exp (-1.0f / (0.001f * cs3::releaseMs (sus) * (float) sr));
        const float coef = inDb > envDb ? aCoef : rCoef;
        envDb = coef * envDb + (1.0f - coef) * inDb;

        const float thr = cs3::thresholdDb (sus), ratio = cs3::ratioFor (sus);
        const float gr = cs3::gainReductionDb (envDb, thr, ratio);
        reduction = gr;
        const float outGain = cs3::dbToGain (gr + cs3::makeupDb (thr, ratio)) * (lv <= 0.01f ? 0.0f : cs3::dbToGain ((lv - 6.0f) * 4.0f));

        for (int c = 0; c < channels; ++c)
        {
            const float in = data[c][i];
            float y = high[c].process (low[c].process (in * outGain));
            y = std::tanh (y * 0.9f) / 0.9f;                          // soft ceiling
            data[c][i] = in + (y - in) * bypassMix;
        }
    }
    lastReduction = reduction;
}

juce::AudioProcessorEditor* CS3AudioProcessor::createEditor() { return new CS3AudioProcessorEditor (*this); }

void CS3AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void CS3AudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new CS3AudioProcessor(); }
