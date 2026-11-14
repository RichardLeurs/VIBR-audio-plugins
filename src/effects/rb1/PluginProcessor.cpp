// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
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
        { "Glass Strings", "Sympathetic strings in C major pentatonic", { { "mode", 2 }, { "variant", 2 }, { "scale", 2 }, { "decay", 60 }, { "size", 60 }, { "blend", 70 }, { "mix", 50 } } },
        { "Metal Drone", "Stiff, bright strings in D dorian", { { "mode", 2 }, { "variant", 1 }, { "root", 2 }, { "scale", 4 }, { "tension", 50 }, { "decay", 70 }, { "mix", 50 } } },
    };
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
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def));
    };
    const auto flag = [&layout] (const char* id, const char* name, bool def) { layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };
    flag ("power", "Power", true);
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
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "RB1", createLayout()),
      presets (*this, apvts, undoManager, "RB-1 Presets", rb1FactoryPresets(), { "power" })
{
    const auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pPower = get ("power"); pMode = get ("mode"); pVariant = get ("variant"); pDecay = get ("decay"); pSize = get ("size"); pTone = get ("tone"); pMotion = get ("motion"); pPreDelay = get ("preDelay");
    pDuck = get ("duck"); pMix = get ("mix"); pLevel = get ("level"); pFreeze = get ("freeze"); pParam2 = get ("param2"); pValue = get ("value"); pDiffusion = get ("diffusion"); pTexture = get ("texture");
    pRoot = get ("root"); pScale = get ("scale"); pDamping = get ("damping"); pTension = get ("tension"); pFine = get ("fine"); pBlend = get ("blend");
}

bool RB1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void RB1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare ((float) sampleRate);
    powerSmooth.reset (sampleRate, 0.02);
    powerSmooth.setCurrentAndTargetValue (pPower->load() > 0.5f ? 1.0f : 0.0f);
    dryBuf.setSize (2, juce::jmax (samplesPerBlock, 8192));
}

void RB1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
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

    rb1::Params p;
    p.mode = juce::jlimit (0, rb1::kNumModes - 1, (int) std::lround (pMode->load()));
    p.variant = juce::jlimit (0, p.mode == 0 ? bigsky::kNumTypes - 1 : p.mode == 1 ? sp1::kNumModes - 1 : sr1::kNumTextures - 1, (int) std::lround (pVariant->load()));
    p.freeze = pFreeze->load() > 0.5f;
    p.decay = pDecay->load() * 0.01f; p.size = pSize->load() * 0.01f; p.tone = pTone->load() * 0.01f; p.motion = pMotion->load() * 0.01f;
    p.preDelayMs = pPreDelay->load(); p.duck = pDuck->load() * 0.01f; p.mix = pMix->load() * 0.01f;
    p.param2 = pParam2->load() * 0.01f; p.value = pValue->load() * 0.01f; p.diffusion = pDiffusion->load() * 0.01f; p.texture = pTexture->load() * 0.01f;
    p.root = (int) std::lround (pRoot->load()); p.scale = (int) std::lround (pScale->load());
    p.damping = pDamping->load() * 0.01f; p.tension = pTension->load() * 0.01f; p.fine = pFine->load() * 0.01f; p.blend = pBlend->load() * 0.01f;
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

juce::AudioProcessorEditor* RB1AudioProcessor::createEditor() { return new RB1AudioProcessorEditor (*this); }

void RB1AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void RB1AudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RB1AudioProcessor(); }
