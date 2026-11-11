// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout GE7AudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    const auto fader = [&] (const char* id, const char* name)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (-15.0f, 15.0f, 0.1f), 0.0f));
    };
    fader ("f100", "100 Hz");
    fader ("f200", "200 Hz");
    fader ("f400", "400 Hz");
    fader ("f800", "800 Hz");
    fader ("f1k6", "1.6 kHz");
    fader ("f3k2", "3.2 kHz");
    fader ("f6k4", "6.4 kHz");
    fader ("level", "Level");
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "bypass", 1 }, "Bypass", false));
    return layout;
}

GE7AudioProcessor::GE7AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "GE7", createLayout())
{
    pBand[0] = apvts.getRawParameterValue ("f100");
    pBand[1] = apvts.getRawParameterValue ("f200");
    pBand[2] = apvts.getRawParameterValue ("f400");
    pBand[3] = apvts.getRawParameterValue ("f800");
    pBand[4] = apvts.getRawParameterValue ("f1k6");
    pBand[5] = apvts.getRawParameterValue ("f3k2");
    pBand[6] = apvts.getRawParameterValue ("f6k4");
    pLevel = apvts.getRawParameterValue ("level");
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("bypass"));
}

bool GE7AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) && out == layouts.getMainInputChannelSet();
}

void GE7AudioProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    for (auto& ch : peaks) for (auto& p : ch) p.reset();
    for (int b = 0; b < ge7::kBands; ++b) { bandSmooth[b].reset (sampleRate, 0.03); bandSmooth[b].setCurrentAndTargetValue (pBand[b]->load()); }
    levelSmooth.reset (sampleRate, 0.03); levelSmooth.setCurrentAndTargetValue (pLevel->load());
    wetSmooth.reset (sampleRate, 0.01);   wetSmooth.setCurrentAndTargetValue (bypassParam->get() ? 0.0f : 1.0f);
}

void GE7AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int channels = juce::jmin (buffer.getNumChannels(), kMaxChannels);
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, n);
    if (n == 0 || channels == 0) return;

    for (int b = 0; b < ge7::kBands; ++b) bandSmooth[b].setTargetValue (pBand[b]->load());
    levelSmooth.setTargetValue (pLevel->load());
    wetSmooth.setTargetValue (bypassParam->get() ? 0.0f : 1.0f);

    // coefficients are updated in sub-blocks of 32 samples from the smoothed fader values
    constexpr int kSub = 32;
    for (int start = 0; start < n; start += kSub)
    {
        const int len = juce::jmin (kSub, n - start);
        for (int b = 0; b < ge7::kBands; ++b)
        {
            const float db = bandSmooth[b].skip (len);
            for (int c = 0; c < channels; ++c) peaks[c][b].set (sr, ge7::kFreq[b], db);
        }
        for (int i = start; i < start + len; ++i)
        {
            const float lv = ge7::dbToGain (levelSmooth.getNextValue()), mix = wetSmooth.getNextValue();
            for (int c = 0; c < channels; ++c)
            {
                auto* x = buffer.getWritePointer (c);
                const float in = x[i];
                float y = in;
                for (int b = 0; b < ge7::kBands; ++b) y = peaks[c][b].process (y);
                y *= lv;
                x[i] = in + (y - in) * mix;
            }
        }
    }
}

juce::AudioProcessorEditor* GE7AudioProcessor::createEditor() { return new GE7AudioProcessorEditor (*this); }

void GE7AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void GE7AudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GE7AudioProcessor(); }
