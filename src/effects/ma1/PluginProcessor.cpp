// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout MA1AudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    const auto knob = [&] (const char* id, const char* name, float def)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 10.0f, 0.01f), def));
    };
    const auto toggle = [&] (const char* id, const char* name, bool def)
    {
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "inGain", 1 }, "Input", NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f, AudioParameterFloatAttributes().withLabel ("dB")));
    toggle ("gateOn", "Gate On", true);
    knob ("gateThresh", "Gate Threshold", 4.0f);
    knob ("gateRelease", "Gate Release", 3.0f);
    toggle ("boostOn", "Boost On", true);
    knob ("boostDrive", "Boost Drive", 2.0f);
    knob ("boostLevel", "Boost Level", 5.0f);
    knob ("boostTight", "Boost Tight", 5.0f);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "ampType", 1 }, "Amp Type", StringArray { "CRUNCH", "MODERN", "BRUTAL", "ULTRA" }, 1));
    knob ("gain", "Gain", 6.0f);
    knob ("bass", "Bass", 5.0f);
    knob ("middle", "Middle", 5.0f);
    knob ("treble", "Treble", 5.0f);
    knob ("presence", "Presence", 5.0f);
    knob ("resonance", "Resonance", 5.0f);
    knob ("sag", "Sag", 3.0f);
    knob ("master", "Master", 5.0f);
    toggle ("bright", "Bright", false);
    toggle ("deep", "Deep", false);
    toggle ("cabOn", "Cab On", true);
    knob ("irALevel", "IR A Level", 7.0f);
    knob ("irBLevel", "IR B Level", 7.0f);
    knob ("lowCut", "Low Cut", 2.0f);
    knob ("highCut", "High Cut", 7.0f);
    knob ("eqLow", "EQ Low", 5.0f);
    knob ("eqMid", "EQ Mid", 5.0f);
    knob ("eqFreq", "EQ Mid Freq", 5.0f);
    knob ("eqHigh", "EQ High", 5.0f);
    knob ("out", "Output", 5.0f);
    toggle ("limiter", "Limiter", true);
    toggle ("bypass", "Bypass", false);
    return layout;
}

MA1AudioProcessor::MA1AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "HM1", createLayout())
{
    pInGain = p ("inGain"); pGateOn = p ("gateOn"); pGateThresh = p ("gateThresh"); pGateRelease = p ("gateRelease");
    pBoostOn = p ("boostOn"); pBoostDrive = p ("boostDrive"); pBoostLevel = p ("boostLevel"); pBoostTight = p ("boostTight");
    pAmpType = p ("ampType"); pGain = p ("gain"); pBass = p ("bass"); pMiddle = p ("middle"); pTreble = p ("treble");
    pPresence = p ("presence"); pResonance = p ("resonance"); pSag = p ("sag"); pMaster = p ("master"); pBright = p ("bright"); pDeep = p ("deep");
    pCabOn = p ("cabOn"); pIrALevel = p ("irALevel"); pIrBLevel = p ("irBLevel"); pLowCut = p ("lowCut"); pHighCut = p ("highCut");
    pEqLow = p ("eqLow"); pEqMid = p ("eqMid"); pEqFreq = p ("eqFreq"); pEqHigh = p ("eqHigh"); pOut = p ("out"); pLimiter = p ("limiter");
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("bypass"));
    midiMap.setDefault ("gain", 11);
}

bool MA1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) && out == layouts.getMainInputChannelSet();
}

//==============================================================================
void MA1AudioProcessor::loadIrData (int slot, const juce::MemoryBlock& data)
{
    irData[slot] = data;
    conv[slot].loadImpulseResponse (data.getData(), data.getSize(), juce::dsp::Convolution::Stereo::yes,
                                    juce::dsp::Convolution::Trim::yes, 0, juce::dsp::Convolution::Normalise::yes);
    irLoaded[slot] = data.getSize() > 0;
}

bool MA1AudioProcessor::loadImpulseResponse (int slot, const juce::File& file)
{
    juce::MemoryBlock mb;
    if (slot < 0 || slot > 1 || ! file.existsAsFile() || ! file.loadFileAsData (mb) || mb.getSize() == 0) return false;
    loadIrData (slot, mb);
    irName[slot] = file.getFileNameWithoutExtension();
    return true;
}

void MA1AudioProcessor::clearImpulseResponse (int slot)
{
    if (slot < 0 || slot > 1) return;
    irLoaded[slot] = false;
    irData[slot].setSize (0);
    irName[slot] = {};
    conv[slot].reset();
}

//==============================================================================
void MA1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    maxBlock = juce::jmax (1, samplesPerBlock);
    oversampling = std::make_unique<juce::dsp::Oversampling<float>> ((size_t) kMaxChannels, (size_t) kOsFactor,
                                                                      juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    oversampling->initProcessing ((size_t) maxBlock);
    oversampling->reset();
    setLatencySamples ((int) std::ceil (oversampling->getLatencyInSamples()));

    gate.prepare (sampleRate);
    gateGain = 0.0f;
    limiter.reset();
    for (int c = 0; c < kMaxChannels; ++c)
    {
        amp[c].reset();
        for (auto* b : { &bass[c], &middle[c], &treble[c], &presence[c], &resonance[c], &deep[c], &lowCut1[c], &lowCut2[c], &highCut1[c], &highCut2[c],
                         &eqLow[c], &eqMid[c], &eqHigh[c] })
            b->reset();
        factory[c].reset();
    }
    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlock, (juce::uint32) kMaxChannels };
    for (int i = 0; i < 2; ++i)
    {
        conv[i].prepare (spec);
        irBuf[i].setSize (kMaxChannels, maxBlock, false, false, true);
    }
    lastType = -1;

    const auto init = [&] (juce::SmoothedValue<float>& v, float target, double t = 0.03) { v.reset (sampleRate, t); v.setCurrentAndTargetValue (target); };
    init (inSmooth, pInGain->load()); init (gainSmooth, pGain->load()); init (boostSmooth, pBoostDrive->load()); init (masterSmooth, pMaster->load());
    init (outSmooth, pOut->load()); init (sagSmooth, pSag->load()); init (wetSmooth, bypassParam->get() ? 0.0f : 1.0f, 0.01);
}

void MA1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    midiMap.process (midi);
    juce::ScopedNoDenormals noDenormals;
    const int total = buffer.getNumSamples();
    const int channels = juce::jmin (buffer.getNumChannels(), kMaxChannels);
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, total);
    if (total == 0 || channels == 0 || oversampling == nullptr) return;

    inSmooth.setTargetValue (pInGain->load());       gainSmooth.setTargetValue (pGain->load());
    boostSmooth.setTargetValue (pBoostDrive->load()); masterSmooth.setTargetValue (pMaster->load());
    outSmooth.setTargetValue (pOut->load());         sagSmooth.setTargetValue (pSag->load());
    wetSmooth.setTargetValue (bypassParam->get() ? 0.0f : 1.0f);

    const int type = juce::jlimit (0, ma1::kNumTypes - 1, (int) std::lround (pAmpType->load()));
    const auto& t = ma1::kTypes[type];
    const bool gateOn = pGateOn->load() > 0.5f, boostOn = pBoostOn->load() > 0.5f, cabOn = pCabOn->load() > 0.5f, limOn = pLimiter->load() > 0.5f;
    const double osRate = sr * (double) (1 << kOsFactor);
    const float sagAtk = std::exp (-1.0f / (0.005f * (float) osRate)), sagRel = std::exp (-1.0f / (0.120f * (float) osRate));

    if (type != lastType)
    {
        for (auto& a : amp) a.reset();
        lastType = type;
        dipLeft = 1024;
    }

    const int chunk = juce::jmin (maxBlock, 4096);
    for (int start = 0; start < total; start += chunk)
    {
        const int n = juce::jmin (chunk, total - start);
        float* ch[kMaxChannels] = {};
        for (int c = 0; c < channels; ++c) ch[c] = buffer.getWritePointer (c) + start;
        float dryCopy[kMaxChannels][4096];
        for (int c = 0; c < channels; ++c) std::copy (ch[c], ch[c] + n, dryCopy[c]);

        // ---- input gain and noise gate (base rate)
        for (int i = 0; i < n; ++i)
        {
            const float g = ma1::dbToGain (inSmooth.getNextValue());
            float peak = 0.0f;
            for (int c = 0; c < channels; ++c) { ch[c][i] *= g; peak = juce::jmax (peak, std::fabs (ch[c][i])); }
            gateGain = gate.process (peak, ma1::gateThresholdDb (pGateThresh->load()), ma1::gateReleaseMs (pGateRelease->load()));
            if (gateOn) for (int c = 0; c < channels; ++c) ch[c][i] *= gateGain;
        }
        gateOpenLamp.store (gateOn ? gateGain : 0.0f);

        // ---- boost + amp stages (oversampled)
        juce::dsp::AudioBlock<float> block (ch, (size_t) channels, (size_t) n);
        const float g1 = ma1::dbToGain (t.gainLoDb + (t.gainHiDb - t.gainLoDb) * gainSmooth.skip (n) * 0.1f);
        const float drive = 1.0f + boostSmooth.skip (n) * 0.1f * 15.0f;
        const float boostLevel = ma1::dbToGain (ma1::bipolarDb (pBoostLevel->load()));
        const float sag = sagSmooth.skip (n) * 0.1f;
        for (int c = 0; c < channels; ++c)
        {
            amp[c].setBoost (osRate, pBoostTight->load());
            amp[c].setAmp (osRate, t, pBright->load() > 0.5f);
        }
        auto up = oversampling->processSamplesUp (block);
        for (int c = 0; c < channels; ++c)
        {
            auto* x = up.getChannelPointer ((size_t) c);
            for (size_t i = 0; i < up.getNumSamples(); ++i)
            {
                float y = x[i];
                if (boostOn) y = amp[c].processBoost (y, drive, boostLevel);
                x[i] = amp[c].processAmp (y, g1, t, sag, sagAtk, sagRel);
            }
        }
        oversampling->processSamplesDown (block);

        // ---- tone stack, presence, resonance (base rate)
        const float bassK = pBass->load() * 0.1f, midK = pMiddle->load() * 0.1f, trebK = pTreble->load() * 0.1f;
        const float presDb = (pPresence->load() - 5.0f) * 1.6f, resDb = (pResonance->load() - 5.0f) * 1.6f + (pDeep->load() > 0.5f ? 4.0f : 0.0f);
        for (int c = 0; c < channels; ++c)
        {
            bass[c].lowShelf (sr, 120.0f, (bassK - 0.5f) * 24.0f);
            middle[c].peak (sr, 700.0f, 0.8f, (midK - 0.5f) * 20.0f);
            treble[c].highShelf (sr, 3200.0f, (trebK - 0.5f) * 24.0f);
            presence[c].highShelf (sr, 3500.0f, presDb);
            resonance[c].lowShelf (sr, 100.0f, resDb);
            lowCut1[c].highpass (sr, ma1::lowCutHz (pLowCut->load()), 0.7071f);
            lowCut2[c].highpass (sr, ma1::lowCutHz (pLowCut->load()), 0.7071f);
            highCut1[c].lowpass (sr, ma1::highCutHz (pHighCut->load()), 0.7071f);
            highCut2[c].lowpass (sr, ma1::highCutHz (pHighCut->load()), 0.7071f);
            factory[c].set (sr, t.lpHz);
            eqLow[c].lowShelf (sr, 100.0f, ma1::bipolarDb (pEqLow->load()));
            eqMid[c].peak (sr, ma1::eqFreqHz (pEqFreq->load()), 1.2f, ma1::bipolarDb (pEqMid->load()));
            eqHigh[c].highShelf (sr, 6000.0f, ma1::bipolarDb (pEqHigh->load()));
        }
        const float masterGain = ma1::dbToGain (-24.0f + masterSmooth.skip (n) * 3.0f + t.trimDb);        // master 0 .. 10 = -24 .. +6 dB
        for (int c = 0; c < channels; ++c)
            for (int i = 0; i < n; ++i)
                ch[c][i] = resonance[c].process (presence[c].process (treble[c].process (middle[c].process (bass[c].process (ch[c][i]))))) * masterGain;

        // ---- cabinet: impulse responses A + B, or the filter model when none is loaded
        if (cabOn)
        {
            const bool a = irLoaded[0].load(), b = irLoaded[1].load();
            if (a || b)
            {
                const float la = ma1::irLevelGain (pIrALevel->load()), lb = ma1::irLevelGain (pIrBLevel->load());
                for (int s = 0; s < 2; ++s)
                    if (irLoaded[s].load())
                    {
                        for (int c = 0; c < channels; ++c) irBuf[s].copyFrom (c, 0, ch[c], n);
                        juce::dsp::AudioBlock<float> ib (irBuf[s]);
                        auto sub = ib.getSubBlock (0, (size_t) n).getSubsetChannelBlock (0, (size_t) channels);
                        juce::dsp::ProcessContextReplacing<float> ctx (sub);
                        conv[s].process (ctx);
                    }
                for (int c = 0; c < channels; ++c)
                    for (int i = 0; i < n; ++i)
                        ch[c][i] = (a ? irBuf[0].getSample (c, i) * la : 0.0f) + (b ? irBuf[1].getSample (c, i) * lb : 0.0f);
            }
            else
                for (int c = 0; c < channels; ++c)
                    for (int i = 0; i < n; ++i)
                        ch[c][i] = factory[c].process (ch[c][i]);
            for (int c = 0; c < channels; ++c)
                for (int i = 0; i < n; ++i)
                    ch[c][i] = highCut2[c].process (highCut1[c].process (lowCut2[c].process (lowCut1[c].process (ch[c][i]))));
        }

        // ---- post EQ, output level, limiter, bypass
        float limMin = 1.0f;
        for (int i = 0; i < n; ++i)
        {
            const float outGain = ma1::dbToGain ((outSmooth.getNextValue() - 5.0f) * 2.4f), wet = wetSmooth.getNextValue();
            const float fade = dipLeft > 0 ? 1.0f - (float) dipLeft / 1024.0f : 1.0f;
            if (dipLeft > 0) --dipLeft;
            float peak = 0.0f, o[kMaxChannels] = {};
            for (int c = 0; c < channels; ++c)
            {
                o[c] = eqHigh[c].process (eqMid[c].process (eqLow[c].process (ch[c][i]))) * outGain * fade;
                peak = juce::jmax (peak, std::fabs (o[c]));
            }
            const float lg = limOn ? limiter.process (peak, sr, 0.944f) : 1.0f;                 // -0.5 dB ceiling
            limMin = juce::jmin (limMin, lg);
            for (int c = 0; c < channels; ++c)
            {
                const float y = limOn ? juce::jlimit (-1.0f, 1.0f, o[c] * lg) : o[c];
                ch[c][i] = dryCopy[c][i] + (y - dryCopy[c][i]) * wet;
            }
        }
        limiterLamp.store (1.0f - limMin);
    }
}

juce::AudioProcessorEditor* MA1AudioProcessor::createEditor() { return new MA1AudioProcessorEditor (*this); }

void MA1AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    for (int s = 0; s < 2; ++s)
    {
        const juce::String key = s == 0 ? "irA" : "irB";
        state.setProperty (key + "Name", irName[s], nullptr);
        state.setProperty (key + "Data", irData[s].getSize() > 0 ? irData[s].toBase64Encoding() : juce::String(), nullptr);
    }
    state.appendChild (midiMap.toTree(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void MA1AudioProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto state = juce::ValueTree::fromXml (*xml);
    midiMap.fromTree (state.getChildWithName ("MIDIMAP"));
    state.removeChild (state.getChildWithName ("MIDIMAP"), nullptr);
    apvts.replaceState (state);
    for (int s = 0; s < 2; ++s)
    {
        const juce::String key = s == 0 ? "irA" : "irB";
        juce::MemoryBlock mb;
        if (mb.fromBase64Encoding (state.getProperty (key + "Data").toString()) && mb.getSize() > 0)
        {
            loadIrData (s, mb);
            irName[s] = state.getProperty (key + "Name").toString();
        }
        else
            clearImpulseResponse (s);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MA1AudioProcessor(); }
