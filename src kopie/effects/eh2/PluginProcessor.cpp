// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout EH2AudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    const auto knob = [&] (const char* id, const char* name, float def)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 10.0f, 0.01f), def));
    };
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Mix", NormalisableRange<float> (-10.0f, 10.0f, 0.01f), 4.0f));
    knob ("freq", "Freq", 5.0f);
    knob ("sens", "Sens", 5.0f);
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "bypass", 1 }, "Bypass", false));
    return layout;
}

EH2AudioProcessor::EH2AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "EH2", createLayout())
{
    pMix  = apvts.getRawParameterValue ("mix");
    pFreq = apvts.getRawParameterValue ("freq");
    pSens = apvts.getRawParameterValue ("sens");
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("bypass"));
}

bool EH2AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) && out == layouts.getMainInputChannelSet();
}

void EH2AudioProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    for (auto& f : hp) f.reset();
    for (auto& f : hp2) f.reset();
    envDb = -120.0f; open = 0.0f;
    mixSmooth.reset (sampleRate, 0.03);  mixSmooth.setCurrentAndTargetValue (pMix->load());
    freqSmooth.reset (sampleRate, 0.03); freqSmooth.setCurrentAndTargetValue (pFreq->load());
    wetSmooth.reset (sampleRate, 0.01);  wetSmooth.setCurrentAndTargetValue (bypassParam->get() ? 0.0f : 1.0f);
}

void EH2AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int channels = juce::jmin (buffer.getNumChannels(), kMaxChannels);
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, n);
    if (n == 0 || channels == 0) return;

    mixSmooth.setTargetValue (pMix->load());
    freqSmooth.setTargetValue (pFreq->load());
    wetSmooth.setTargetValue (bypassParam->get() ? 0.0f : 1.0f);
    const float thr = eh2::thresholdDb (pSens->load() * 0.1f);

    const float aCoef = std::exp (-1.0f / (0.002f * (float) sr)), rCoef = std::exp (-1.0f / (0.080f * (float) sr));
    const float gCoef = std::exp (-1.0f / (0.010f * (float) sr));            // gate smoothing, 10 ms

    constexpr int kSub = 32;
    for (int start = 0; start < n; start += kSub)
    {
        const int len = juce::jmin (kSub, n - start);
        const float fHz = eh2::freqHz (freqSmooth.skip (len) * 0.1f);
        for (int c = 0; c < channels; ++c) { hp[c].set (sr, fHz); hp2[c].set (sr, fHz); }

        for (int i = start; i < start + len; ++i)
        {
            const float mix = mixSmooth.getNextValue() * 0.1f, bypassMix = wetSmooth.getNextValue();
            float peak = 0.0f;
            for (int c = 0; c < channels; ++c) peak = juce::jmax (peak, std::fabs (buffer.getReadPointer (c)[i]));
            const float inDb = 20.0f * std::log10 (peak + 1.0e-6f);
            const float coef = inDb > envDb ? aCoef : rCoef;
            envDb = coef * envDb + (1.0f - coef) * inDb;
            const float t = juce::jlimit (0.0f, 1.0f, (envDb - thr) / 15.0f + 0.2f);          // opens over 15 dB above the threshold
            open = gCoef * open + (1.0f - gCoef) * t;

            for (int c = 0; c < channels; ++c)
            {
                auto* x = buffer.getWritePointer (c);
                const float in = x[i];
                const float treble = hp2[c].process (hp[c].process (in)) * 2.5f;
                const float bright = std::tanh (treble * 2.0f) * 0.5f + treble * 0.5f;            // a little soft clipping adds harmonics
                const float y = in + 2.0f * mix * open * bright;
                x[i] = in + (y - in) * bypassMix;
            }
        }
    }
    openMeter.store (open);
}

juce::AudioProcessorEditor* EH2AudioProcessor::createEditor() { return new EH2AudioProcessorEditor (*this); }

void EH2AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void EH2AudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new EH2AudioProcessor(); }
