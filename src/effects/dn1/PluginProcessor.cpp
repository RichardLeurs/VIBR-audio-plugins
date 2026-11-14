// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
std::vector<vibr::PresetManager::Factory> dn1FactoryPresets()
{
    return {
        { "Init", "The shaper with the gate and the transient shaper on, nothing moved", {} },
        { "Tight Drums", "Gate and transient shaper: more attack, less ring", { { "mode", 0 }, { "gateOn", 1 }, { "gateThresh", -42 }, { "gateRelease", 90 }, { "transOn", 1 }, { "attack", 45 }, { "sustain", -35 } } },
        { "Noise Gate", "A gate for a noisy amp", { { "mode", 0 }, { "gateOn", 1 }, { "gateThresh", -55 }, { "gateRelease", 180 }, { "transOn", 0 } } },
        { "Guitar Sustain", "The CS-3 compression sustainer", { { "mode", 1 }, { "variant", 0 }, { "compSustain", 65 }, { "compAttack", 45 }, { "tone", 55 } } },
        { "Glue", "A gentle VCA compressor for a bus", { { "mode", 1 }, { "variant", 1 }, { "compThresh", -16 }, { "compRatio", 2.5f }, { "compAttackMs", 20 }, { "compRelease", 250 }, { "compMakeup", 3 } } },
        { "Vocal Opto", "Slow, smooth, program-dependent", { { "mode", 1 }, { "variant", 2 }, { "compThresh", -22 }, { "compRatio", 4 }, { "compAttackMs", 10 }, { "compRelease", 300 }, { "compMakeup", 5 } } },
        { "Parallel Smash", "A hard VCA compressor mixed in with the dry sound", { { "mode", 1 }, { "variant", 1 }, { "compThresh", -35 }, { "compRatio", 10 }, { "compAttackMs", 2 }, { "compRelease", 120 }, { "compMakeup", 12 }, { "compMix", 40 } } },
        { "Peak Limiter", "The LM-2 limiter", { { "mode", 2 }, { "variant", 0 }, { "limThresh", -10 }, { "limRelease", 150 }, { "limCeiling", 0 } } },
        { "Safe Ceiling", "A true-peak limiter at -1 dBTP", { { "mode", 2 }, { "variant", 1 }, { "limThresh", -6 }, { "limRelease", 120 }, { "limCeiling", -1 } } },
    };
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout DN1AudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    const auto flt = [&layout] (const char* id, const char* name, float lo, float hi, float step, float def, float centre = 0.0f)
    {
        NormalisableRange<float> r (lo, hi, step);
        if (centre > 0.0f) r.setSkewForCentre (centre);
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def));
    };
    const auto flag = [&layout] (const char* id, const char* name, bool def) { layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };
    flag ("power", "Power", true);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode", StringArray { "Shaper", "Compressor", "Limiter" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "variant", 1 }, "Type (Sustain / VCA / Opto, Peak / True Peak)", StringArray { "Type 1", "Type 2", "Type 3" }, 0));
    flag ("gateOn", "Gate On", true);
    flt ("gateThresh", "Gate Threshold (dB)", -80.0f, -10.0f, 1.0f, -50.0f);
    flt ("gateRelease", "Gate Release (ms)", 10.0f, 800.0f, 1.0f, 120.0f, 150.0f);
    flag ("expOn", "Expander On", false);
    flt ("expThresh", "Expander Threshold (dB)", -80.0f, -10.0f, 1.0f, -45.0f);
    flt ("expRatio", "Expander Ratio", 1.0f, 10.0f, 0.1f, 2.0f);
    flt ("expRelease", "Expander Release (ms)", 10.0f, 800.0f, 1.0f, 150.0f, 150.0f);
    flag ("transOn", "Transient Shaper On", true);
    flt ("attack", "Transient Attack", -100.0f, 100.0f, 1.0f, 0.0f);
    flt ("sustain", "Transient Sustain", -100.0f, 100.0f, 1.0f, 0.0f);
    flt ("compSustain", "Sustain (Sustain type)", 0.0f, 100.0f, 1.0f, 50.0f);
    flt ("compAttack", "Attack (Sustain type)", 0.0f, 100.0f, 1.0f, 40.0f);
    flt ("compThresh", "Compressor Threshold (dB)", -60.0f, 0.0f, 0.1f, -20.0f);
    flt ("compRatio", "Compressor Ratio", 1.0f, 20.0f, 0.1f, 4.0f, 4.0f);
    flt ("compAttackMs", "Compressor Attack (ms)", 0.1f, 100.0f, 0.1f, 10.0f, 10.0f);
    flt ("compRelease", "Compressor Release (ms)", 20.0f, 2000.0f, 1.0f, 200.0f, 200.0f);
    flt ("compMakeup", "Compressor Make-up (dB)", 0.0f, 24.0f, 0.1f, 0.0f);
    flt ("compMix", "Compressor Mix", 0.0f, 100.0f, 1.0f, 100.0f);
    flt ("tone", "Tone", 0.0f, 100.0f, 1.0f, 50.0f);
    flt ("limThresh", "Limiter Threshold (dB)", -30.0f, 0.0f, 0.1f, -6.0f);
    flt ("limRelease", "Limiter Release (ms)", 10.0f, 1000.0f, 1.0f, 100.0f, 120.0f);
    flt ("limCeiling", "Limiter Ceiling (dBTP)", -3.0f, 0.0f, 0.1f, -1.0f);
    flt ("scHp", "Sidechain High-pass (Hz, 20 = off)", 20.0f, 300.0f, 1.0f, 20.0f, 90.0f);
    flt ("link", "Stereo Link", 0.0f, 100.0f, 1.0f, 100.0f);
    flt ("mix", "Mix", 0.0f, 100.0f, 1.0f, 100.0f);
    flt ("level", "Level", -24.0f, 12.0f, 0.1f, 0.0f);
    flag ("autoLevel", "Auto Level", false);
    return layout;
}

DN1AudioProcessor::DN1AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "DN1", createLayout()),
      presets (*this, apvts, undoManager, "DN-1 Presets", dn1FactoryPresets(), { "power" })
{
    const auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pPower = get ("power"); pMode = get ("mode"); pVariant = get ("variant"); pGateOn = get ("gateOn"); pGateThresh = get ("gateThresh"); pGateRelease = get ("gateRelease");
    pExpOn = get ("expOn"); pExpThresh = get ("expThresh"); pExpRatio = get ("expRatio"); pExpRelease = get ("expRelease"); pTransOn = get ("transOn"); pAttack = get ("attack"); pSustain = get ("sustain");
    pCompSustain = get ("compSustain"); pCompAttack = get ("compAttack"); pCompThresh = get ("compThresh"); pCompRatio = get ("compRatio"); pCompAttackMs = get ("compAttackMs");
    pCompRelease = get ("compRelease"); pCompMakeup = get ("compMakeup"); pCompMix = get ("compMix"); pTone = get ("tone");
    pLimThresh = get ("limThresh"); pLimRelease = get ("limRelease"); pLimCeiling = get ("limCeiling"); pScHp = get ("scHp"); pLink = get ("link"); pMix = get ("mix"); pLevel = get ("level"); pAutoLevel = get ("autoLevel");
}

bool DN1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void DN1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare ((float) sampleRate);
    setLatencySamples (engine.latency());
    powerSmooth.reset (sampleRate, 0.02);
    powerSmooth.setCurrentAndTargetValue (pPower->load() > 0.5f ? 1.0f : 0.0f);
    dryBuf.setSize (2, juce::jmax (samplesPerBlock, 8192));
    for (auto& d : powerDelay) d.assign (8192, 0.0f);
    delayPos = 0;
}

void DN1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0 || buffer.getNumChannels() < 2 || n > dryBuf.getNumSamples()) return;
    for (int c = 0; c < 2; ++c)
    {
        auto* d = buffer.getWritePointer (c);
        for (int i = 0; i < n; ++i) if (! (std::abs (d[i]) < 1.0e4f)) d[i] = 0.0f;
    }

    // the untouched signal, delayed by the latency, is what the power switch crossfades to (so switching it off does not jump in time)
    const int lat = engine.latency();
    for (int c = 0; c < 2; ++c)
    {
        const auto* s = buffer.getReadPointer (c);
        for (int i = 0; i < n; ++i)
        {
            powerDelay[c][(size_t) ((delayPos + i) & 8191)] = s[i];
            dryBuf.setSample (c, i, powerDelay[c][(size_t) ((delayPos + i - lat) & 8191)]);
        }
    }
    delayPos = (delayPos + n) & 8191;

    const float from = powerSmooth.getCurrentValue();
    powerSmooth.setTargetValue (pPower->load() > 0.5f ? 1.0f : 0.0f);
    const float to = powerSmooth.skip (n);
    if (from == 0.0f && to == 0.0f) { for (int c = 0; c < 2; ++c) buffer.copyFrom (c, 0, dryBuf, c, 0, n); return; }

    dn1::Params p;
    p.mode = juce::jlimit (0, dn1::kNumModes - 1, (int) std::lround (pMode->load()));
    p.variant = juce::jlimit (0, juce::jmax (0, dn1::variantCount (p.mode) - 1), (int) std::lround (pVariant->load()));
    p.shaper.gateOn = pGateOn->load() > 0.5f; p.shaper.expOn = pExpOn->load() > 0.5f; p.shaper.transOn = pTransOn->load() > 0.5f;
    p.shaper.gateThresh = pGateThresh->load(); p.shaper.gateRelease = pGateRelease->load(); p.shaper.expThresh = pExpThresh->load(); p.shaper.expRatio = pExpRatio->load();
    p.shaper.expRelease = pExpRelease->load(); p.shaper.attack = pAttack->load(); p.shaper.sustain = pSustain->load();
    p.sustain = pCompSustain->load() * 0.01f; p.attack01 = pCompAttack->load() * 0.01f; p.compThresh = pCompThresh->load(); p.compRatio = pCompRatio->load();
    p.compAttackMs = pCompAttackMs->load(); p.compReleaseMs = pCompRelease->load(); p.compMakeup = pCompMakeup->load(); p.compMix = pCompMix->load() * 0.01f; p.tone = pTone->load() * 0.01f;
    p.limThresh = pLimThresh->load(); p.limRelease = pLimRelease->load(); p.limCeiling = pLimCeiling->load();
    p.scHpHz = pScHp->load(); p.link = pLink->load() * 0.01f; p.mix = pMix->load() * 0.01f; p.autoLevel = pAutoLevel->load() > 0.5f;
    engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n, p, juce::Decibels::decibelsToGain (pLevel->load()));

    if (from != 1.0f || to != 1.0f)
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

juce::AudioProcessorEditor* DN1AudioProcessor::createEditor() { return new DN1AudioProcessorEditor (*this); }

void DN1AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void DN1AudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DN1AudioProcessor(); }
