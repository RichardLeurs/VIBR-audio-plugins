// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout IR2AudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    const auto knob = [&] (const char* id, const char* name, float def)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 10.0f, 0.01f), def));
    };
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "type", 1 }, "Amp Type", StringArray { "CLEAN","TWN","TWEED","DIAMOND","CRUNCH","BRIT","BROWN","HI-GAIN","SLDN","MODDED","RFIER" }, 5));
    knob ("gain",     "Gain",     5.0f);
    knob ("level",    "Level",    5.0f);
    knob ("ambience", "Ambience", 2.0f);
    knob ("bass",     "Bass",     5.0f);
    knob ("middle",   "Middle",   5.0f);
    knob ("treble",   "Treble",   5.0f);
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "bypass", 1 }, "Bypass", false));
    return layout;
}

IR2AudioProcessor::IR2AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "IR2", createLayout())
{
    pType = apvts.getRawParameterValue ("type");
    pGain = apvts.getRawParameterValue ("gain");
    pLevel = apvts.getRawParameterValue ("level");
    pAmbience = apvts.getRawParameterValue ("ambience");
    pBass = apvts.getRawParameterValue ("bass");
    pMiddle = apvts.getRawParameterValue ("middle");
    pTreble = apvts.getRawParameterValue ("treble");
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("bypass"));
}

bool IR2AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) && out == layouts.getMainInputChannelSet();
}

void IR2AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    maxBlock = juce::jmax (1, samplesPerBlock);
    oversampling = std::make_unique<juce::dsp::Oversampling<float>> ((size_t) kMaxChannels, (size_t) kOsFactor,
                                                                      juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    oversampling->initProcessing ((size_t) maxBlock);
    oversampling->reset();
    setLatencySamples ((int) std::ceil (oversampling->getLatencyInSamples()));
    for (auto& p : pre) p.reset();
    for (auto& p : post) p.reset();
    room.prepare (sampleRate);
    lastType = -1;
    const auto init = [&] (juce::SmoothedValue<float>& v, float target, double t = 0.03) { v.reset (sampleRate, t); v.setCurrentAndTargetValue (target); };
    init (gainSmooth, pGain->load()); init (levelSmooth, pLevel->load()); init (ambSmooth, pAmbience->load());
    init (bassSmooth, pBass->load()); init (midSmooth, pMiddle->load()); init (trebSmooth, pTreble->load());
    init (wetSmooth, bypassParam->get() ? 0.0f : 1.0f, 0.01);
}

void IR2AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int total = buffer.getNumSamples();
    const int channels = juce::jmin (buffer.getNumChannels(), kMaxChannels);
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, total);
    if (total == 0 || channels == 0 || oversampling == nullptr) return;

    gainSmooth.setTargetValue (pGain->load());   levelSmooth.setTargetValue (pLevel->load());
    ambSmooth.setTargetValue (pAmbience->load()); bassSmooth.setTargetValue (pBass->load());
    midSmooth.setTargetValue (pMiddle->load());  trebSmooth.setTargetValue (pTreble->load());
    wetSmooth.setTargetValue (bypassParam->get() ? 0.0f : 1.0f);

    const int type = juce::jlimit (0, ir2::kNumTypes - 1, (int) std::lround (pType->load()));
    const auto& t = ir2::kTypes[type];
    if (type != lastType)
    {
        for (auto& p : pre) { p.reset(); p.setType (sr * 2.0, t); }
        for (auto& p : post) p.reset();
        lastType = type;
        dipLeft = 1024;
    }

    const int chunk = juce::jmin (maxBlock, 4096);
    for (int start = 0; start < total; start += chunk)
    {
        const int n = juce::jmin (chunk, total - start);
        float* ch[kMaxChannels] = {};
        for (int c = 0; c < channels; ++c) ch[c] = buffer.getWritePointer (c) + start;
        juce::dsp::AudioBlock<float> block (ch, (size_t) channels, (size_t) n);

        // dry copy for the bypass fade
        float dryCopy[kMaxChannels][4096];
        constexpr bool keepDry = true;
        for (int c = 0; c < channels; ++c) std::copy (ch[c], ch[c] + n, dryCopy[c]);

        const float g01 = gainSmooth.skip (n) * 0.1f;
        const float g1 = ir2::dbToGain (t.gainLoDb + (t.gainHiDb - t.gainLoDb) * g01);
        auto up = oversampling->processSamplesUp (block);
        for (int c = 0; c < channels; ++c)
        {
            auto* x = up.getChannelPointer ((size_t) c);
            for (size_t i = 0; i < up.getNumSamples(); ++i) x[i] = pre[c].process (x[i], g1, t);
        }
        oversampling->processSamplesDown (block);

        const float bass = bassSmooth.skip (n) * 0.1f, mid = midSmooth.skip (n) * 0.1f, treb = trebSmooth.skip (n) * 0.1f;
        for (int c = 0; c < channels; ++c) post[c].set (sr, bass, mid, treb, t);

        for (int i = 0; i < n; ++i)
        {
            const float lv = levelSmooth.getNextValue(), amb = ambSmooth.getNextValue() * 0.1f, wet = wetSmooth.getNextValue();
            const float outGain = lv <= 0.01f ? 0.0f : ir2::dbToGain ((lv - 5.0f) * 3.6f + t.trimDb);
            const float fade = dipLeft > 0 ? 1.0f - (float) dipLeft / 1024.0f : 1.0f;
            if (dipLeft > 0) --dipLeft;
            float o[kMaxChannels] = {};
            for (int c = 0; c < channels; ++c) o[c] = post[c].process (ch[c][i]) * outGain * fade;
            for (int c = 0; c < channels; ++c)
            {
                const float r = room.process (c, channels == 1 ? o[0] : 0.5f * (o[0] + o[channels - 1]));
                const float y = std::tanh (o[c] + 0.45f * amb * r * 3.0f);
                ch[c][i] = keepDry ? dryCopy[c][i] + (y - dryCopy[c][i]) * wet : y;
            }
        }
    }
}

juce::AudioProcessorEditor* IR2AudioProcessor::createEditor() { return new IR2AudioProcessorEditor (*this); }

void IR2AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void IR2AudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new IR2AudioProcessor(); }
