// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout SA1AudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    const auto flt = [&layout] (const char* id, const char* name, float lo, float hi, float step, float def, const char* unit = "")
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, step), def, AudioParameterFloatAttributes().withLabel (unit)));
    };
    const auto flag = [&layout] (const char* id, const char* name, bool def)
    {
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def, AudioParameterBoolAttributes().withMeta (juce::String (id) == "power" || juce::String (id) == "bypass")));
    };
    flag ("power", "Power", true);
    flag ("bypass", "Bypass", false);
    flag ("boostOn", "Boost On", false);
    flt ("boostGain", "Boost Gain", 0.0f, 24.0f, 0.5f, 10.0f, "dB");
    flt ("boostTone", "Boost Tone", 0.0f, 100.0f, 1.0f, 50.0f, "%");
    flag ("tubeOn", "Tube On", true);
    flt ("tubeDrive", "Tube Drive", 0.0f, 100.0f, 1.0f, 40.0f, "%");
    flt ("tubeBias", "Tube Bias", 0.0f, 100.0f, 1.0f, 40.0f, "%");
    flag ("tapeOn", "Tape On", true);
    flt ("tapeDrive", "Tape Drive", 0.0f, 100.0f, 1.0f, 40.0f, "%");
    flt ("tapeWarmth", "Tape Warmth", 0.0f, 100.0f, 1.0f, 50.0f, "%");
    flag ("crushOn", "Crush On", false);
    flt ("bits", "Crush Bits", 1.0f, 16.0f, 1.0f, 8.0f);
    {
        NormalisableRange<float> r (500.0f, 48000.0f, 1.0f);
        r.setSkewForCentre (6000.0f);
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "rate", 1 }, "Crush Rate", r, 12000.0f, AudioParameterFloatAttributes().withLabel ("Hz")));
    }
    flt ("crushMix", "Crush Mix", 0.0f, 100.0f, 1.0f, 100.0f, "%");
    flt ("mix", "Mix", 0.0f, 100.0f, 1.0f, 100.0f, "%");
    flt ("level", "Level", -24.0f, 12.0f, 1.0f, 0.0f, "dB");
    return layout;
}

namespace
{
// the factory presets (values are plain host values; the rest stays at the defaults: BOOST and CRUSH off, TUBE and TAPE on)
std::vector<vibr::PresetManager::Factory> sa1FactoryPresets()
{
    return {
        { "Init", "SA-1 at its defaults", {} },
        { "Warm Tube", "Even-order glow, nothing else", { { "tapeOn", 0 }, { "tubeDrive", 55 }, { "tubeBias", 45 } } },
        { "Tape Glue", "Soft compression and warmth", { { "tubeOn", 0 }, { "tapeDrive", 55 }, { "tapeWarmth", 65 } } },
        { "Crunch Boost", "A hot boost into the tube", { { "boostOn", 1 }, { "boostGain", 14 }, { "boostTone", 60 }, { "tubeDrive", 60 }, { "tapeOn", 0 } } },
        { "Lo-Fi Radio", "Band-limited, dusty and warm", { { "crushOn", 1 }, { "bits", 10 }, { "rate", 8000 }, { "crushMix", 70 }, { "tubeDrive", 25 }, { "tapeDrive", 50 }, { "tapeWarmth", 70 } } },
        { "8-Bit Grit", "The crusher on its own", { { "tubeOn", 0 }, { "tapeOn", 0 }, { "crushOn", 1 }, { "bits", 6 }, { "rate", 12000 }, { "crushMix", 100 } } },
        { "Crushed Tape", "A little crush on a warm tape", { { "crushOn", 1 }, { "bits", 12 }, { "rate", 20000 }, { "crushMix", 60 }, { "tapeDrive", 60 }, { "tapeWarmth", 75 }, { "mix", 80 } } },
        { "Full Chain", "All four stages at work", { { "boostOn", 1 }, { "boostGain", 8 }, { "tubeDrive", 50 }, { "tapeDrive", 50 }, { "crushOn", 1 }, { "bits", 12 }, { "rate", 24000 }, { "crushMix", 40 } } },
    };
}
} // namespace

SA1AudioProcessor::SA1AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "SA1", createLayout()),
      presets (*this, apvts, undoManager, "SA-1 Presets", sa1FactoryPresets(), { "power", "bypass" })
{
    const auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pPower = get ("power"); pBoostOn = get ("boostOn"); pBoostGain = get ("boostGain"); pBoostTone = get ("boostTone");
    pTubeOn = get ("tubeOn"); pTubeDrive = get ("tubeDrive"); pTubeBias = get ("tubeBias"); pTapeOn = get ("tapeOn"); pTapeDrive = get ("tapeDrive");
    pTapeWarmth = get ("tapeWarmth"); pCrushOn = get ("crushOn"); pBits = get ("bits"); pRate = get ("rate"); pCrushMix = get ("crushMix");
    pMix = get ("mix"); pLevel = get ("level");
    midiMap.setDefault ("tubeDrive", 11);
}

bool SA1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void SA1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare ((float) sampleRate);
    powerSmooth.reset (sampleRate, 0.02);
    powerSmooth.setCurrentAndTargetValue (pPower->load() > 0.5f ? 1.0f : 0.0f);
    dryBuf.setSize (2, juce::jmax (samplesPerBlock, 8192));
}

void SA1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    midiMap.process (midi);
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0 || buffer.getNumChannels() < 2 || n > dryBuf.getNumSamples()) return;

    const float from = powerSmooth.getCurrentValue();
    powerSmooth.setTargetValue (pPower->load() > 0.5f ? 1.0f : 0.0f);
    const float to = powerSmooth.skip (n);
    if (from == 0.0f && to == 0.0f) { outMeter.measure (buffer, n); return; }                          // off: the signal passes untouched

    for (int c = 0; c < 2; ++c) dryBuf.copyFrom (c, 0, buffer, c, 0, n);

    sa1::Params p;
    p.boostOn = pBoostOn->load() > 0.5f;  p.boostGain = pBoostGain->load();   p.boostTone = pBoostTone->load() * 0.01f;
    p.tubeOn = pTubeOn->load() > 0.5f;    p.tubeDrive = pTubeDrive->load() * 0.01f;   p.tubeBias = pTubeBias->load() * 0.01f;
    p.tapeOn = pTapeOn->load() > 0.5f;    p.tapeDrive = pTapeDrive->load() * 0.01f;   p.tapeWarmth = pTapeWarmth->load() * 0.01f;
    p.crushOn = pCrushOn->load() > 0.5f;  p.bits = pBits->load();             p.rate = pRate->load();   p.crushMix = pCrushMix->load() * 0.01f;
    p.mix = pMix->load() * 0.01f;
    engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n, p, juce::Decibels::decibelsToGain (pLevel->load()));

    if (from != 1.0f || to != 1.0f)                                  // power switch: crossfade to the untouched signal
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

juce::AudioProcessorEditor* SA1AudioProcessor::createEditor() { return new SA1AudioProcessorEditor (*this); }

void SA1AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.appendChild (midiMap.toTree(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void SA1AudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto st = juce::ValueTree::fromXml (*xml);
            midiMap.fromTree (st.getChildWithName ("MIDIMAP"));
            st.removeChild (st.getChildWithName ("MIDIMAP"), nullptr);
            vibr::BypassLink::normalise (st);
            apvts.replaceState (st);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SA1AudioProcessor(); }
