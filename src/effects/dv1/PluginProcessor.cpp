// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
std::vector<vibr::PresetManager::Factory> dv1FactoryPresets()
{
    return {
        { "Init", "The OD-2 overdrive type at its defaults", {} },
        { "Blues Break-up", "A light, touch-sensitive overdrive", { { "mode", 0 }, { "odType", 1 }, { "drive", 35 }, { "odParam", 4 }, { "gateThresh", 0 } } },
        { "Scream Lead", "A mid-pushed lead overdrive", { { "mode", 0 }, { "odType", 2 }, { "drive", 60 }, { "odMid", 7 }, { "boostOn", 1 }, { "boost", 6 } } },
        { "Brown Rhythm", "A tight, high-gain rhythm sound", { { "mode", 0 }, { "odType", 9 }, { "drive", 55 }, { "gateThresh", 3 }, { "gateDecay", 2 } } },
        { "Stompbox Distortion", "The DS-1 as it is", { { "mode", 1 }, { "variant", 0 }, { "drive", 55 }, { "tone", 50 } } },
        { "Modern Tight", "Distortion with a tight low end", { { "mode", 1 }, { "variant", 1 }, { "drive", 60 }, { "tight", 60 } } },
        { "Tube Glow", "Warm even harmonics", { { "mode", 2 }, { "variant", 0 }, { "drive", 45 }, { "tubeBias", 45 } } },
        { "Push-Pull Crunch", "Symmetric tube saturation", { { "mode", 2 }, { "variant", 1 }, { "drive", 60 } } },
        { "Tape Warmth", "Soft saturation with head bump", { { "mode", 3 }, { "variant", 0 }, { "drive", 40 }, { "warmth", 60 } } },
        { "Worn Tape", "Dark, aged tape", { { "mode", 3 }, { "variant", 1 }, { "drive", 50 }, { "warmth", 80 } } },
        { "Fold Over", "Foldback distortion", { { "mode", 4 }, { "variant", 1 }, { "drive", 55 }, { "fold", 65 } } },
        { "Lo-Fi Crush", "8 bits at 12 kHz", { { "mode", 5 }, { "variant", 0 }, { "bits", 8 }, { "rate", 12000 }, { "crushMix", 100 } } },
        { "Smooth Crush", "A dithered, softened crush", { { "mode", 5 }, { "variant", 1 }, { "bits", 6 }, { "rate", 16000 }, { "crushMix", 70 } } },
    };
}
} // namespace

namespace
{
// host label and readable text for a float parameter (the stored value is the shown value; Hz above 1000 reads as kHz)
juce::AudioParameterFloatAttributes unitAttrs (const juce::String& unit, int decimals)
{
    return juce::AudioParameterFloatAttributes()
        .withLabel (unit)
        .withStringFromValueFunction ([unit, decimals] (float v, int)
        {
            if (unit == "Hz" && std::abs (v) >= 1000.0f) return juce::String (v / 1000.0f, 2) + " kHz";
            return juce::String (v, decimals) + (unit.isEmpty() ? juce::String() : " " + unit);
        })
        .withValueFromStringFunction ([unit] (const juce::String& s)
        {
            const float v = s.retainCharacters ("0123456789.-+").getFloatValue();
            return (unit == "Hz" && s.containsIgnoreCase ("k")) ? v * 1000.0f : v;
        });
}

int decimalsFor (float step) { return step >= 1.0f ? 0 : step >= 0.1f ? 1 : 2; }

juce::String unitFor (const juce::String& id, float hi)
{
    static const std::pair<const char*, const char*> table[] = { { "boost", "dB" }, { "level", "dB" }, { "rate", "Hz" }, { "bits", "bit" } };
    for (const auto& [k, u] : table) if (id == k) return u;
    return (hi == 100.0f || hi == 75.0f) ? "%" : "";
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout DV1AudioProcessor::createLayout()
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
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode", StringArray { "Overdrive", "Distortion", "Tube", "Tape", "Fold", "Crush" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "variant", 1 }, "Type (Classic / Modern, Single / Push-Pull, New / Aged, Transistor / Foldback, Hard / Smooth)", StringArray { "Type 1", "Type 2" }, 0));
    {
        StringArray names;
        for (int i = 0; i < dv1::kNumOdTypes; ++i) names.add (dv1::odTypeName (i));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "odType", 1 }, "Overdrive: Type (the 12 OD-2 types)", names, 0));
    }
    flt ("drive", "Drive", 0.0f, 100.0f, 1.0f, 40.0f);
    flt ("tone", "Tone", 0.0f, 100.0f, 1.0f, 50.0f);
    flag ("boostOn", "Boost On", false);
    flt ("boost", "Boost (dB)", 0.0f, 24.0f, 0.1f, 10.0f);
    flag ("autoLevel", "Auto Level", true);
    flt ("mix", "Mix", 0.0f, 100.0f, 1.0f, 100.0f);
    flt ("level", "Level", -24.0f, 12.0f, 0.1f, 0.0f);
    flt ("odParam", "Overdrive: Param", 0.0f, 10.0f, 0.1f, 5.0f);
    flt ("odLow", "Overdrive: Low", 0.0f, 10.0f, 0.1f, 5.0f);
    flt ("odMid", "Overdrive: Middle", 0.0f, 10.0f, 0.1f, 5.0f);
    flt ("odHigh", "Overdrive: High", 0.0f, 10.0f, 0.1f, 5.0f);
    flt ("gateThresh", "Overdrive: Gate Threshold", 0.0f, 10.0f, 0.1f, 0.0f);
    flt ("gateDecay", "Overdrive: Gate Decay", 0.0f, 10.0f, 0.1f, 3.0f);
    flt ("tight", "Distortion: Tight", 0.0f, 100.0f, 1.0f, 0.0f);
    flt ("tubeBias", "Tube: Bias", 0.0f, 100.0f, 1.0f, 40.0f);
    flt ("warmth", "Tape: Warmth", 0.0f, 100.0f, 1.0f, 50.0f);
    flt ("fold", "Fold: Amount", 0.0f, 100.0f, 1.0f, 50.0f);
    flt ("symmetry", "Fold: Symmetry", 0.0f, 100.0f, 1.0f, 100.0f);
    flt ("bits", "Crush: Bits", 1.0f, 16.0f, 1.0f, 8.0f);
    flt ("rate", "Crush: Rate (Hz)", 500.0f, 48000.0f, 1.0f, 12000.0f, 8000.0f);
    flt ("crushMix", "Crush: Mix", 0.0f, 100.0f, 1.0f, 100.0f);
    return layout;
}

DV1AudioProcessor::DV1AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "DV1", createLayout()),
      presets (*this, apvts, undoManager, "DV-1 Presets", dv1FactoryPresets(), { "power", "bypass" })
{
    const auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pPower = get ("power"); pMode = get ("mode"); pVariant = get ("variant"); pOdType = get ("odType"); pDrive = get ("drive"); pTone = get ("tone"); pBoostOn = get ("boostOn");
    pBoost = get ("boost"); pAutoLevel = get ("autoLevel"); pMix = get ("mix"); pLevel = get ("level"); pOdParam = get ("odParam"); pOdLow = get ("odLow"); pOdMid = get ("odMid");
    pOdHigh = get ("odHigh"); pGateThresh = get ("gateThresh"); pGateDecay = get ("gateDecay"); pTight = get ("tight"); pTubeBias = get ("tubeBias"); pWarmth = get ("warmth");
    pFold = get ("fold"); pSymmetry = get ("symmetry"); pBits = get ("bits"); pRate = get ("rate"); pCrushMix = get ("crushMix");
}

dv1::Params DV1AudioProcessor::readParams() const
{
    dv1::Params p;
    p.mode = juce::jlimit (0, dv1::kNumModes - 1, (int) std::lround (pMode->load()));
    p.variant = juce::jlimit (0, 1, (int) std::lround (pVariant->load()));
    p.odType = juce::jlimit (0, dv1::kNumOdTypes - 1, (int) std::lround (pOdType->load()));
    p.drive = pDrive->load() * 0.01f; p.tone = pTone->load() * 0.01f; p.boostOn = pBoostOn->load() > 0.5f; p.boostDb = pBoost->load(); p.autoLevel = pAutoLevel->load() > 0.5f;
    p.mix = pMix->load() * 0.01f; p.odParam = pOdParam->load(); p.odLow = pOdLow->load(); p.odMid = pOdMid->load(); p.odHigh = pOdHigh->load();
    p.gateThresh = pGateThresh->load(); p.gateDecay = pGateDecay->load(); p.tight = pTight->load() * 0.01f; p.tubeBias = pTubeBias->load() * 0.01f; p.warmth = pWarmth->load() * 0.01f;
    p.fold = pFold->load() * 0.01f; p.symmetry = pSymmetry->load() * 0.01f; p.bits = pBits->load(); p.rate = pRate->load(); p.crushMix = pCrushMix->load() * 0.01f;
    return p;
}

bool DV1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void DV1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare ((float) sampleRate);
    setLatencySamples (engine.latency());
    powerSmooth.reset (sampleRate, 0.02);
    powerSmooth.setCurrentAndTargetValue (pPower->load() > 0.5f ? 1.0f : 0.0f);
    dryBuf.setSize (2, juce::jmax (samplesPerBlock, 8192));
    for (auto& d : powerDelay) d.assign (8192, 0.0f);
    delayPos = 0;
}

void DV1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0 || buffer.getNumChannels() < 2 || n > dryBuf.getNumSamples()) return;
    for (int c = 0; c < 2; ++c)
    {
        auto* d = buffer.getWritePointer (c);
        for (int i = 0; i < n; ++i) if (! (std::abs (d[i]) < 1.0e4f)) d[i] = 0.0f;
    }

    const int lat = engine.latency();                                              // the power switch crosses to the input delayed by the latency
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

    engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n, readParams(), juce::Decibels::decibelsToGain (pLevel->load()));

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

juce::AudioProcessorEditor* DV1AudioProcessor::createEditor() { return new DV1AudioProcessorEditor (*this); }

void DV1AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void DV1AudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DV1AudioProcessor(); }
