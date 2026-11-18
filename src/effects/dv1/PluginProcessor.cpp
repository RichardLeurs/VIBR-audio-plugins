// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "plugin/ParamLayout.h"
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
        { "Warm Tube", "Even-order glow, nothing else", { { "mode", 6 }, { "autoLevel", 0 }, { "tapeOn", 0 }, { "tubeDrive", 55 }, { "tubeBias", 45 } } },
        { "Tape Glue", "Soft compression and warmth", { { "mode", 6 }, { "autoLevel", 0 }, { "tubeOn", 0 }, { "tapeDrive", 55 }, { "warmth", 65 } } },
        { "Crunch Boost", "A hot boost into the tube", { { "mode", 6 }, { "autoLevel", 0 }, { "boostOn", 1 }, { "boost", 14 }, { "boostTone", 60 }, { "tubeDrive", 60 }, { "tapeOn", 0 } } },
        { "Lo-Fi Radio", "Band-limited, dusty and warm", { { "mode", 6 }, { "autoLevel", 0 }, { "crushOn", 1 }, { "bits", 10 }, { "rate", 8000 }, { "crushMix", 70 }, { "tubeDrive", 25 }, { "tapeDrive", 50 }, { "warmth", 70 } } },
        { "8-Bit Grit", "The crusher on its own", { { "mode", 6 }, { "autoLevel", 0 }, { "tubeOn", 0 }, { "tapeOn", 0 }, { "crushOn", 1 }, { "bits", 6 }, { "rate", 12000 }, { "crushMix", 100 } } },
        { "Crushed Tape", "A little crush on a warm tape", { { "mode", 6 }, { "autoLevel", 0 }, { "crushOn", 1 }, { "bits", 12 }, { "rate", 20000 }, { "crushMix", 60 }, { "tapeDrive", 60 }, { "warmth", 75 }, { "mix", 80 } } },
        { "Full Chain", "All four stages at work", { { "mode", 6 }, { "autoLevel", 0 }, { "boostOn", 1 }, { "boost", 8 }, { "tubeDrive", 50 }, { "tapeDrive", 50 }, { "crushOn", 1 }, { "bits", 12 }, { "rate", 24000 }, { "crushMix", 40 } } },
        { "Harmonic Warm Tube", "Even harmonics, soft and thick", { { "mode", 7 }, { "autoLevel", 0 }, { "character", 0 }, { "drive", 45 }, { "oddeven", 25 }, { "tone", 70 }, { "mix", 100 } } },
        { "Harmonic Tape Glue", "Gentle tape colour", { { "mode", 7 }, { "autoLevel", 0 }, { "character", 1 }, { "drive", 35 }, { "oddeven", 40 }, { "tilt", 40 }, { "tone", 65 }, { "mix", 100 } } },
        { "Odd Grit", "Odd harmonics, edgy", { { "mode", 7 }, { "autoLevel", 0 }, { "character", 2 }, { "drive", 65 }, { "oddeven", 85 }, { "tone", 75 }, { "mix", 100 } } },
        { "Fold Fire", "Wave folder, bright", { { "mode", 7 }, { "autoLevel", 0 }, { "character", 3 }, { "drive", 70 }, { "oddeven", 60 }, { "tilt", 65 }, { "mix", 80 }, { "tone", 80 } } },
        { "Octave Shine", "Added octave above", { { "mode", 7 }, { "autoLevel", 0 }, { "character", 0 }, { "drive", 40 }, { "octave", 45 }, { "focus", 55 }, { "mix", 70 }, { "tone", 80 } } },
        { "Sub Weight", "Sub octave for the low end", { { "mode", 7 }, { "autoLevel", 0 }, { "character", 1 }, { "drive", 40 }, { "sub", 55 }, { "focus", 25 }, { "tilt", 30 }, { "mix", 70 }, { "tone", 80 } } },
        { "Air Exciter", "Drive on the highs only", { { "mode", 7 }, { "autoLevel", 0 }, { "character", 2 }, { "drive", 55 }, { "focus", 80 }, { "tilt", 75 }, { "tone", 85 }, { "mix", 40 } } },
    };
}
} // namespace

namespace
{
using vibr::unitAttrs;

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
    const auto flag = [&layout] (const char* id, const char* name, bool def) { layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def, AudioParameterBoolAttributes().withMeta (juce::String (id) == "power" || juce::String (id) == "bypass"))); };
    flag ("power", "Power", true);
    flag ("bypass", "Bypass", false);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode", StringArray { "Overdrive", "Distortion", "Tube", "Tape", "Fold", "Crush", "Chain", "Harmonics" }, 0));
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
    // CHAIN (the SA-1: boost, tube, tape and crush in one chain; the BOOST switch and gain are the shared ones); added after the first release, version hint 2
    const auto flt2 = [&layout] (const char* id, const char* name, float def) { layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 2 }, name, NormalisableRange<float> (0.0f, 100.0f, 1.0f), def, unitAttrs (unitFor (id, 100.0f), 0))); };
    const auto flag2 = [&layout] (const char* id, const char* name, bool def) { layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 2 }, name, def)); };
    flt2 ("boostTone", "Chain: Boost Tone", 50.0f);
    flag2 ("tubeOn", "Chain: Tube On", true);
    flt2 ("tubeDrive", "Chain: Tube Drive", 40.0f);
    flag2 ("tapeOn", "Chain: Tape On", true);
    flt2 ("tapeDrive", "Chain: Tape Drive", 40.0f);
    flag2 ("crushOn", "Chain: Crush On", false);
    // HARMONICS (the HM-2; DRIVE and TONE are the shared knobs), version hint 3
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "character", 3 }, "Harmonics: Character", StringArray { "Tube", "Tape", "Transistor", "Fold" }, 0));
    const auto flt3 = [&layout] (const char* id, const char* name, float def) { layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 3 }, name, NormalisableRange<float> (0.0f, 100.0f, 1.0f), def, unitAttrs ("%", 0))); };
    flt3 ("oddeven", "Harmonics: Odd / Even", 50.0f);
    flt3 ("octave", "Harmonics: Octave Up", 0.0f);
    flt3 ("sub", "Harmonics: Sub Octave", 0.0f);
    flt3 ("focus", "Harmonics: Focus", 40.0f);
    flt3 ("tilt", "Harmonics: Tilt", 50.0f);
    return layout;
}

DV1AudioProcessor::DV1AudioProcessor()
    : vibr::LabProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true),
                          "DV1", createLayout(), "DV-1 Presets", dv1FactoryPresets(),
                          { "drive", "tone", "level", "mix", "boost", "boostOn", "odParam", "odLow", "odMid", "odHigh", "gateThresh", "tight", "tubeBias", "warmth", "fold", "bits", "boostTone", "tubeDrive", "tapeDrive", "oddeven", "octave", "sub", "focus", "tilt" })
{
    const auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pPower = get ("power"); pMode = get ("mode"); pVariant = get ("variant"); pOdType = get ("odType"); pDrive = get ("drive"); pTone = get ("tone"); pBoostOn = get ("boostOn");
    pBoost = get ("boost"); pAutoLevel = get ("autoLevel"); pMix = get ("mix"); pLevel = get ("level"); pOdParam = get ("odParam"); pOdLow = get ("odLow"); pOdMid = get ("odMid");
    pOdHigh = get ("odHigh"); pGateThresh = get ("gateThresh"); pGateDecay = get ("gateDecay"); pTight = get ("tight"); pTubeBias = get ("tubeBias"); pWarmth = get ("warmth");
    pFold = get ("fold"); pSymmetry = get ("symmetry"); pBits = get ("bits"); pRate = get ("rate"); pCrushMix = get ("crushMix");
    pBoostTone = get ("boostTone"); pTubeOn = get ("tubeOn"); pTubeDrive = get ("tubeDrive"); pTapeOn = get ("tapeOn"); pTapeDrive = get ("tapeDrive"); pCrushOn = get ("crushOn");
    pCharacter = get ("character"); pOddEven = get ("oddeven"); pOctave = get ("octave"); pSub = get ("sub"); pFocus = get ("focus"); pTilt = get ("tilt");
    midiMap.setDefault ("drive", 11);
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
    p.boostTone = pBoostTone->load() * 0.01f; p.tubeOn = pTubeOn->load() > 0.5f; p.tubeDrive = pTubeDrive->load() * 0.01f; p.tapeOn = pTapeOn->load() > 0.5f;
    p.tapeDrive = pTapeDrive->load() * 0.01f; p.crushOn = pCrushOn->load() > 0.5f;
    p.character = juce::jlimit (0, 3, (int) std::lround (pCharacter->load())); p.oddEven = pOddEven->load() * 0.01f; p.octave = pOctave->load() * 0.01f;
    p.sub = pSub->load() * 0.01f; p.focus = pFocus->load() * 0.01f; p.tilt = pTilt->load() * 0.01f;
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

void DV1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    midiMap.process (midi);
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0 || buffer.getNumChannels() < 2 || n > dryBuf.getNumSamples()) { outMeter.measure (buffer, n); return; }
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
    if (from == 0.0f && to == 0.0f) { for (int c = 0; c < 2; ++c) buffer.copyFrom (c, 0, dryBuf, c, 0, n); outMeter.measure (buffer, n); return; }

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
    outMeter.measure (buffer, n);
}

juce::AudioProcessorEditor* DV1AudioProcessor::createEditor() { return new DV1AudioProcessorEditor (*this); }


juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DV1AudioProcessor(); }
