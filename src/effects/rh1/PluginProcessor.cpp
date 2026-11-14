// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
juce::String stepId (int i) { return "step" + juce::String (i + 1); }

// a lane as the preset values: step1 .. step16 from a pattern string
std::vector<std::pair<juce::String, float>> lane (int pattern, std::vector<std::pair<juce::String, float>> more = {})
{
    for (int s = 0; s < rh1::kSteps; ++s) more.push_back ({ stepId (s), rh1::patternString (pattern)[s] == 'x' ? 100.0f : 0.0f });
    return more;
}

std::vector<vibr::PresetManager::Factory> rh1FactoryPresets()
{
    return {
        { "Init", "A low-pass filter moved by a slow LFO", {} },
        { "Auto-Wah", "The playing opens the filter", { { "mode", 0 }, { "variant", 1 }, { "source", 1 }, { "sens", 55 }, { "decay", 35 }, { "depth", 75 }, { "cutoff", 25 }, { "res", 70 } } },
        { "Vowel Sweep", "A slow A-E-I-O-U sweep", { { "mode", 0 }, { "variant", 4 }, { "source", 0 }, { "rate", 0.4f }, { "depth", 80 }, { "cutoff", 30 }, { "res", 60 } } },
        { "Stepped Filter", "A sequenced low-pass on 16ths", { { "mode", 0 }, { "variant", 0 }, { "source", 2 }, { "sync", 1 }, { "division", 4 }, { "glide", 25 }, { "depth", 70 }, { "res", 65 },
                                                              { "step1", 100 }, { "step2", 30 }, { "step3", 70 }, { "step4", 20 }, { "step5", 90 }, { "step6", 40 }, { "step7", 60 }, { "step8", 10 },
                                                              { "step9", 100 }, { "step10", 30 }, { "step11", 70 }, { "step12", 20 }, { "step13", 90 }, { "step14", 50 }, { "step15", 80 }, { "step16", 10 } } },
        { "Vintage Tremolo", "Smooth amplitude tremolo", { { "mode", 1 }, { "variant", 0 }, { "rate", 5.0f }, { "depth", 60 }, { "shape", 0 } } },
        { "Harmonic Tremolo", "Lows and highs in opposite phase", { { "mode", 1 }, { "variant", 1 }, { "rate", 4.0f }, { "depth", 70 } } },
        { "Auto-Pan", "Left and right in opposite phase, synced to the beat", { { "mode", 1 }, { "variant", 2 }, { "sync", 1 }, { "division", 2 }, { "depth", 100 } } },
        { "Eighth Gate", "A slicer on eighth notes", lane (1, { { "mode", 2 }, { "variant", 0 }, { "slDuty", 6 } }) },
        { "Syncopated Chop", "A syncopated slicer pattern", lane (6, { { "mode", 2 }, { "variant", 0 }, { "slDuty", 7 }, { "slAttack", 2 } }) },
        { "Ping-Pong Slice", "Pattern left, gaps right", lane (3, { { "mode", 2 }, { "variant", 1 }, { "slDuty", 8 } }) },
        { "Shuffle Filter Slice", "The SFX slicer with swing", lane (10, { { "mode", 2 }, { "variant", 3 }, { "swing", 40 } }) },
    };
}
} // namespace

namespace
{
// host label and readable text for a float parameter (the stored value is the shown value, the host shows the label next to it)
juce::AudioParameterFloatAttributes unitAttrs (const juce::String& unit, int decimals)
{
    return juce::AudioParameterFloatAttributes()
        .withLabel (unit)
        .withStringFromValueFunction ([decimals] (float v, int)
        {
            return juce::String (v, decimals);                       // the number only: the host adds the label (unit) itself
        })
        .withValueFromStringFunction ([] (const juce::String& s)
        {
            return s.retainCharacters ("0123456789.-+").getFloatValue();
        });
}

int decimalsFor (float step) { return step >= 1.0f ? 0 : step >= 0.1f ? 1 : 2; }

juce::String unitFor (const juce::String& id, float hi)
{
    static const std::pair<const char*, const char*> table[] = { { "tempo", "bpm" }, { "rate", "Hz" }, { "level", "dB" } };
    for (const auto& [k, u] : table) if (id == k) return u;
    return (hi == 100.0f || hi == 75.0f) ? "%" : "";
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout RH1AudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    const auto flt = [&layout] (const char* id, const char* name, float lo, float hi, float step, float def, float centre = 0.0f)
    {
        NormalisableRange<float> r (lo, hi, step);
        if (centre > 0.0f) r.setSkewForCentre (centre);
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, unitAttrs (unitFor (id, hi), decimalsFor (step))));
    };
    const auto flag = [&layout] (const char* id, const char* name, bool def) { layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };
    flag ("power", "Power", true);
    flag ("bypass", "Bypass", false);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Destination", StringArray { "Filter", "Tremolo", "Slicer" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "variant", 1 }, "Type (filter type, tremolo type, slicer mode)", StringArray { "Type 1", "Type 2", "Type 3", "Type 4", "Type 5" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "source", 1 }, "Source", StringArray { "LFO", "Envelope", "Steps" }, 0));
    flag ("sync", "Sync (the rate follows the tempo)", false);
    {
        StringArray names;
        for (int i = 0; i < rh1::kNumDivisions; ++i) names.add (rh1::divisionName (i));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "division", 1 }, "Note Length (with Sync)", names, 4));
    }
    flag ("followHost", "Follow Host Tempo", true);
    flt ("tempo", "Tempo (bpm, without a host tempo)", 30.0f, 300.0f, 0.1f, 120.0f, 120.0f);
    flt ("rate", "Rate (Hz)", 0.05f, 20.0f, 0.01f, 3.0f, 3.0f);
    flt ("shape", "Shape", 0.0f, 100.0f, 1.0f, 0.0f);
    flt ("depth", "Depth", 0.0f, 100.0f, 1.0f, 60.0f);
    flt ("sens", "Sensitivity (Envelope)", 0.0f, 100.0f, 1.0f, 35.0f);
    flt ("decay", "Decay (Envelope)", 0.0f, 100.0f, 1.0f, 40.0f);
    flt ("swing", "Swing", 0.0f, 75.0f, 1.0f, 0.0f);
    flt ("glide", "Glide (Steps)", 0.0f, 100.0f, 1.0f, 0.0f);
    for (int s = 0; s < rh1::kSteps; ++s) flt (stepId (s).toRawUTF8(), ("Step " + String (s + 1)).toRawUTF8(), 0.0f, 100.0f, 1.0f, rh1::patternString (0)[s] == 'x' ? 100.0f : 0.0f);
    flt ("cutoff", "Filter: Cutoff", 0.0f, 100.0f, 1.0f, 40.0f);
    flt ("res", "Filter: Resonance", 0.0f, 100.0f, 1.0f, 55.0f);
    flt ("tremStereo", "Tremolo: Stereo", 0.0f, 100.0f, 1.0f, 0.0f);
    flt ("tremBias", "Tremolo: Bias", 0.0f, 100.0f, 1.0f, 0.0f);
    flt ("slAttack", "Slicer: Attack", 0.0f, 10.0f, 0.1f, 3.0f);
    flt ("slDuty", "Slicer: Duty", 0.0f, 10.0f, 0.1f, 10.0f);
    flt ("slBalance", "Slicer: Balance", 0.0f, 10.0f, 0.1f, 7.0f);
    flt ("mix", "Mix", 0.0f, 100.0f, 1.0f, 100.0f);
    flt ("level", "Level", -24.0f, 12.0f, 0.1f, 0.0f);
    return layout;
}

RH1AudioProcessor::RH1AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "RH1", createLayout()),
      presets (*this, apvts, undoManager, "RH-1 Presets", rh1FactoryPresets(), { "power", "bypass" })
{
    const auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pPower = get ("power"); pMode = get ("mode"); pVariant = get ("variant"); pSource = get ("source"); pSync = get ("sync"); pDivision = get ("division"); pFollow = get ("followHost"); pTempo = get ("tempo");
    pRate = get ("rate"); pShape = get ("shape"); pDepth = get ("depth"); pSens = get ("sens"); pDecay = get ("decay"); pSwing = get ("swing"); pGlide = get ("glide");
    for (int s = 0; s < rh1::kSteps; ++s) pSteps[s] = apvts.getRawParameterValue (stepId (s));
    pCutoff = get ("cutoff"); pRes = get ("res"); pTremStereo = get ("tremStereo"); pTremBias = get ("tremBias"); pSlAttack = get ("slAttack"); pSlDuty = get ("slDuty"); pSlBalance = get ("slBalance");
    pMix = get ("mix"); pLevel = get ("level");
    midiMap.setDefault ("depth", 1);
    midiMap.setDefault ("rate", 11);
    midiMap.setSyncAlias ("rate", "sync", "division");
}

bool RH1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void RH1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare ((float) sampleRate);
    powerSmooth.reset (sampleRate, 0.02);
    powerSmooth.setCurrentAndTargetValue (pPower->load() > 0.5f ? 1.0f : 0.0f);
    dryBuf.setSize (2, juce::jmax (samplesPerBlock, 8192));
}

void RH1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    midiMap.process (midi);
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0 || buffer.getNumChannels() < 2 || n > dryBuf.getNumSamples()) return;
    for (int c = 0; c < 2; ++c)
    {
        auto* d = buffer.getWritePointer (c);
        for (int i = 0; i < n; ++i) if (! (std::abs (d[i]) < 1.0e4f)) d[i] = 0.0f;
    }
    const float from = powerSmooth.getCurrentValue();
    powerSmooth.setTargetValue (pPower->load() > 0.5f ? 1.0f : 0.0f);
    const float to = powerSmooth.skip (n);
    if (from == 0.0f && to == 0.0f) return;
    for (int c = 0; c < 2; ++c) dryBuf.copyFrom (c, 0, buffer, c, 0, n);

    rh1::Params p;
    p.mode = juce::jlimit (0, rh1::kNumModes - 1, (int) std::lround (pMode->load()));
    p.variant = juce::jlimit (0, rh1::variantCount (p.mode) - 1, (int) std::lround (pVariant->load()));
    p.source = juce::jlimit (0, 2, (int) std::lround (pSource->load()));
    p.sync = pSync->load() > 0.5f; p.division = juce::jlimit (0, rh1::kNumDivisions - 1, (int) std::lround (pDivision->load()));
    p.bpm = juce::jlimit (30.0f, 300.0f, pTempo->load());
    if (pFollow->load() > 0.5f)
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
            {
                if (auto b = pos->getBpm()) p.bpm = (float) juce::jlimit (30.0, 300.0, *b);
                if (pos->getIsPlaying()) if (auto ppq = pos->getPpqPosition()) { p.hostPlaying = true; p.hostBeats = *ppq; }
            }
    bpmNow.store (p.bpm);
    p.rate = pRate->load(); p.shape = pShape->load() * 0.01f; p.depth = pDepth->load() * 0.01f; p.sens = pSens->load() * 0.01f; p.decay = pDecay->load() * 0.01f;
    p.swing = pSwing->load() * 0.01f; p.glide = pGlide->load() * 0.01f;
    for (int s = 0; s < rh1::kSteps; ++s) p.steps[s] = pSteps[s]->load() * 0.01f;
    p.cutoff = pCutoff->load() * 0.01f; p.res = pRes->load() * 0.01f; p.tremStereo = pTremStereo->load() * 0.01f; p.tremBias = pTremBias->load() * 0.01f;
    p.slAttack = pSlAttack->load() * 0.1f; p.slDuty = pSlDuty->load() * 0.1f; p.slBalance = pSlBalance->load() * 0.1f; p.mix = pMix->load() * 0.01f;
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

juce::AudioProcessorEditor* RH1AudioProcessor::createEditor() { return new RH1AudioProcessorEditor (*this); }

void RH1AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.appendChild (midiMap.toTree(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void RH1AudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) {
            auto st = juce::ValueTree::fromXml (*xml);
            midiMap.fromTree (st.getChildWithName ("MIDIMAP"));
            st.removeChild (st.getChildWithName ("MIDIMAP"), nullptr);
            vibr::BypassLink::normalise (st);
            apvts.replaceState (st);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RH1AudioProcessor(); }
