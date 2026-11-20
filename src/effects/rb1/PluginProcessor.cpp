// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "plugin/ParamLayout.h"
#include "PluginEditor.h"

namespace
{
std::vector<vibr::PresetManager::Factory> rb1FactoryPresets()
{
    return {
        { "Init", "A room", {} },
        { "Small Room", "A short, natural room", { { "mode", 0 }, { "variant", 0 }, { "decay", 35 }, { "size", 45 }, { "preDelay", 8 }, { "mix", 25 } } },
        { "Concert Hall", "A big hall with a long tail", { { "mode", 0 }, { "variant", 1 }, { "decay", 65 }, { "size", 70 }, { "preDelay", 30 }, { "mix", 35 } } },
        { "Studio Plate", "A bright plate", { { "mode", 0 }, { "variant", 2 }, { "decay", 50 }, { "tone", 65 }, { "mix", 30 } } },
        { "Spring Tank", "Dispersive spring", { { "mode", 0 }, { "variant", 3 }, { "decay", 45 }, { "mix", 30 } } },
        { "Swell Pad", "A slow swell into a long tail", { { "mode", 0 }, { "variant", 4 }, { "decay", 70 }, { "size", 60 }, { "mix", 45 } } },
        { "Ducked Hall", "The tail comes up between the notes", { { "mode", 0 }, { "variant", 1 }, { "decay", 60 }, { "duck", 70 }, { "mix", 40 } } },
        { "Shimmer Space", "An octave-up tail", { { "mode", 1 }, { "variant", 0 }, { "decay", 65 }, { "size", 65 }, { "mix", 40 } } },
        { "Infinite", "A tail that never ends", { { "mode", 1 }, { "variant", 1 }, { "decay", 90 }, { "mix", 45 } } },
        { "Frozen Moment", "Freeze the tail and play over it", { { "mode", 1 }, { "variant", 1 }, { "decay", 80 }, { "freeze", 1 }, { "mix", 50 } } },
        { "Angel Shimmer", "Octave-up bloom", { { "mode", 1 }, { "variant", 0 }, { "size", 70 }, { "decay", 75 }, { "texture", 60 }, { "mix", 45 }, { "diffusion", 70 } } },
        { "Endless Hall", "A tail that never ends", { { "mode", 1 }, { "variant", 1 }, { "size", 80 }, { "decay", 90 }, { "diffusion", 80 }, { "mix", 50 } } },
        { "Spectral Cloud", "Smeared, glassy spectrum", { { "mode", 1 }, { "variant", 2 }, { "size", 65 }, { "decay", 70 }, { "motion", 50 }, { "texture", 65 }, { "mix", 55 }, { "diffusion", 70 } } },
        { "Slow Drift", "A chorused, moving space", { { "mode", 1 }, { "variant", 3 }, { "decay", 65 }, { "motion", 60 }, { "texture", 55 }, { "mix", 40 }, { "diffusion", 70 } } },
        { "Frozen Chord", "Freeze what just played", { { "mode", 1 }, { "variant", 1 }, { "freeze", 1 }, { "size", 75 }, { "decay", 85 }, { "mix", 70 }, { "diffusion", 70 } } },
        { "Glass Strings", "Sympathetic strings in C major pentatonic", { { "mode", 2 }, { "variant", 2 }, { "scale", 2 }, { "decay", 60 }, { "size", 60 }, { "blend", 70 }, { "mix", 50 } } },
        { "Metal Drone", "Stiff, bright strings in D dorian", { { "mode", 2 }, { "variant", 1 }, { "root", 2 }, { "scale", 4 }, { "tension", 50 }, { "decay", 70 }, { "mix", 50 } } },
        { "Pentatonic Haze", "A soft major-pentatonic wash", { { "mode", 2 }, { "variant", 2 }, { "scale", 2 }, { "size", 70 }, { "decay", 70 }, { "damping", 35 }, { "mix", 45 }, { "motion", 30 } } },
        { "Wooden Harp", "Warm, short-lived wooden strings", { { "mode", 2 }, { "variant", 0 }, { "scale", 0 }, { "size", 55 }, { "decay", 40 }, { "damping", 60 }, { "tension", 20 }, { "mix", 50 }, { "motion", 30 } } },
        { "Glass Bells", "Bright, long ringing glass", { { "mode", 2 }, { "variant", 2 }, { "scale", 3 }, { "size", 80 }, { "decay", 85 }, { "damping", 15 }, { "motion", 45 }, { "mix", 55 } } },
        { "Dorian Shimmer", "Moving strings in Dorian", { { "mode", 2 }, { "variant", 2 }, { "root", 2 }, { "scale", 4 }, { "size", 75 }, { "decay", 65 }, { "motion", 70 }, { "fine", 55 }, { "mix", 50 } } },
        { "Whole-Tone Dream", "Floating, unresolved colour", { { "mode", 2 }, { "variant", 1 }, { "scale", 5 }, { "size", 65 }, { "decay", 75 }, { "damping", 30 }, { "motion", 55 }, { "mix", 55 } } },
        { "Subtle Body", "Just a little sympathy", { { "mode", 2 }, { "variant", 0 }, { "size", 40 }, { "decay", 35 }, { "damping", 55 }, { "mix", 25 }, { "motion", 30 } } },
    };
}
} // namespace

namespace
{
using vibr::unitAttrs;

int decimalsFor (float step) { return step >= 1.0f ? 0 : step >= 0.1f ? 1 : 2; }

juce::String unitFor (const juce::String& id, float hi)
{
    static const std::pair<const char*, const char*> table[] = { { "preDelay", "ms" }, { "level", "dB" } };
    for (const auto& [k, u] : table) if (id == k) return u;
    return (hi == 100.0f || hi == 75.0f) ? "%" : "";
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout RB1AudioProcessor::createLayout()
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
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode", StringArray { "Classic", "Space", "Resonate" }, 0));
    {
        StringArray names;
        for (int i = 0; i < bigsky::kNumTypes; ++i) names.add (rb1::classicLongName (i));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "variant", 1 }, "Type (Classic: the 12 types, Space: the 5 modes, Resonate: the texture)", names, 0));
    }
    flt ("decay", "Decay", 0.0f, 100.0f, 1.0f, 50.0f);
    flt ("size", "Size", 0.0f, 100.0f, 1.0f, 60.0f);
    flt ("tone", "Tone", 0.0f, 100.0f, 1.0f, 55.0f);
    flt ("motion", "Motion", 0.0f, 100.0f, 1.0f, 35.0f);
    flt ("preDelay", "Pre-delay (ms)", 0.0f, 250.0f, 1.0f, 15.0f, 60.0f);
    flag ("preSync", "Pre-delay Sync", false);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "preDivision", 1 }, "Pre-delay Note Length (with Sync)", vibr::sync::divisionNames(), 5));
    flt ("duck", "Duck", 0.0f, 100.0f, 1.0f, 0.0f);
    flt ("mix", "Mix", 0.0f, 100.0f, 1.0f, 35.0f);
    flt ("level", "Level", -24.0f, 12.0f, 0.1f, 0.0f);
    flag ("freeze", "Freeze", false);
    flt ("param2", "Classic: Param 2", 0.0f, 100.0f, 1.0f, 50.0f);
    flt ("value", "Classic: Value (wet level)", 0.0f, 100.0f, 1.0f, 50.0f);
    flt ("diffusion", "Space: Diffusion", 0.0f, 100.0f, 1.0f, 60.0f);
    flt ("texture", "Space: Texture", 0.0f, 100.0f, 1.0f, 50.0f);
    {
        StringArray notes;
        for (int i = 0; i < 12; ++i) notes.add (sr1::noteName (i));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "root", 1 }, "Resonate: Root", notes, 0));
        StringArray scales;
        for (int i = 0; i < sr1::kNumScales; ++i) scales.add (sr1::scaleName (i));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "scale", 1 }, "Resonate: Scale", scales, 2));
    }
    flt ("damping", "Resonate: Damping", 0.0f, 100.0f, 1.0f, 40.0f);
    flt ("tension", "Resonate: Tension", 0.0f, 100.0f, 1.0f, 25.0f);
    flt ("fine", "Resonate: Fine tune", 0.0f, 100.0f, 1.0f, 50.0f);
    flt ("blend", "Resonate: Blend", 0.0f, 100.0f, 1.0f, 100.0f);
    return layout;
}

RB1AudioProcessor::RB1AudioProcessor()
    : vibr::LabProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false),
                          "RB1", createLayout(), "RB-1 Presets", rb1FactoryPresets(),
                          { "decay", "size", "tone", "motion", "preDelay", "duck", "mix", "level", "freeze", "param2", "value", "diffusion", "texture", "damping", "tension", "blend" })
{
    const auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pPower = get ("power"); pMode = get ("mode"); pVariant = get ("variant"); pDecay = get ("decay"); pSize = get ("size"); pTone = get ("tone"); pMotion = get ("motion"); pPreDelay = get ("preDelay"); pPreSync = get ("preSync"); pPreDivision = get ("preDivision");
    pDuck = get ("duck"); pMix = get ("mix"); pLevel = get ("level"); pFreeze = get ("freeze"); pParam2 = get ("param2"); pValue = get ("value"); pDiffusion = get ("diffusion"); pTexture = get ("texture");
    pRoot = get ("root"); pScale = get ("scale"); pDamping = get ("damping"); pTension = get ("tension"); pFine = get ("fine"); pBlend = get ("blend");

    midiMap.setDefault ("motion", 1);
    midiMap.setDefault ("decay", 11);
    midiMap.setDefault ("freeze", 64);
    midiMap.setSyncAlias ("preDelay", "preSync", "preDivision");          // with sync on, a learned CC moves the note length
}

bool RB1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo() || layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    const auto sc = layouts.getChannelSet (true, 1);                       // the sidechain: off, mono or stereo
    return sc.isDisabled() || sc == juce::AudioChannelSet::mono() || sc == juce::AudioChannelSet::stereo();
}

void RB1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare ((float) sampleRate);
    powerSmooth.reset (sampleRate, 0.02);
    powerSmooth.setCurrentAndTargetValue (pPower->load() > 0.5f ? 1.0f : 0.0f);
    dryBuf.setSize (2, juce::jmax (samplesPerBlock, 8192));
    keyBuf.setSize (2, juce::jmax (samplesPerBlock, 8192));
}

void RB1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    midiMap.process (midi);
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0 || buffer.getNumChannels() < 2 || n > dryBuf.getNumSamples()) return;

    // the external key: the sidechain bus when the host has connected it (a mono key feeds both channels); it drives the DUCK envelope
    const float* keyL = nullptr; const float* keyR = nullptr;
    if (auto* scBus = getBus (true, 1); scBus != nullptr && scBus->isEnabled() && n <= keyBuf.getNumSamples())
    {
        const auto sc = getBusBuffer (buffer, true, 1);
        if (sc.getNumChannels() > 0)
        {
            for (int c = 0; c < 2; ++c)
            {
                const auto* src = sc.getReadPointer (juce::jmin (c, sc.getNumChannels() - 1));
                auto* k = keyBuf.getWritePointer (c);
                for (int i = 0; i < n; ++i) k[i] = std::abs (src[i]) < 1.0e4f ? src[i] : 0.0f;
            }
            keyL = keyBuf.getReadPointer (0); keyR = keyBuf.getReadPointer (1);
        }
    }
    extKey.store (keyL != nullptr, std::memory_order_relaxed);
    for (int c = 0; c < 2; ++c)
    {
        auto* d = buffer.getWritePointer (c);
        for (int i = 0; i < n; ++i) if (! (std::abs (d[i]) < 1.0e4f)) d[i] = 0.0f;
    }
    const float from = powerSmooth.getCurrentValue();
    powerSmooth.setTargetValue (pPower->load() > 0.5f ? 1.0f : 0.0f);
    const float to = powerSmooth.skip (n);
    if (from == 0.0f && to == 0.0f) { outMeter.measure (buffer, n); return; }
    for (int c = 0; c < 2; ++c) dryBuf.copyFrom (c, 0, buffer, c, 0, n);

    rb1::Params p;
    p.mode = juce::jlimit (0, rb1::kNumModes - 1, (int) std::lround (pMode->load()));
    p.variant = juce::jlimit (0, p.mode == 0 ? bigsky::kNumTypes - 1 : p.mode == 1 ? sp1::kNumModes - 1 : sr1::kNumTextures - 1, (int) std::lround (pVariant->load()));
    p.freeze = pFreeze->load() > 0.5f;
    p.decay = pDecay->load() * 0.01f; p.size = pSize->load() * 0.01f; p.tone = pTone->load() * 0.01f; p.motion = pMotion->load() * 0.01f;
    // PRE-DELAY: the knob in ms, or with SYNC one note length at the host tempo (120 bpm without a host), up to 1 s
    p.preDelayMs = pPreSync->load() > 0.5f ? (float) juce::jlimit (0.0, 1000.0, vibr::sync::ms (vibr::sync::hostBpm (*this, 120.0), (int) std::lround (pPreDivision->load()))) : pPreDelay->load();
    p.duck = pDuck->load() * 0.01f; p.mix = pMix->load() * 0.01f;
    p.param2 = pParam2->load() * 0.01f; p.value = pValue->load() * 0.01f; p.diffusion = pDiffusion->load() * 0.01f; p.texture = pTexture->load() * 0.01f;
    p.root = (int) std::lround (pRoot->load()); p.scale = (int) std::lround (pScale->load());
    p.damping = pDamping->load() * 0.01f; p.tension = pTension->load() * 0.01f; p.fine = pFine->load() * 0.01f; p.blend = pBlend->load() * 0.01f;
    engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n, p, juce::Decibels::decibelsToGain (pLevel->load()), keyL, keyR);

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

juce::AudioProcessorEditor* RB1AudioProcessor::createEditor() { return new RB1AudioProcessorEditor (*this); }


juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RB1AudioProcessor(); }
