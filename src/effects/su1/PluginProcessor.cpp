// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout SU1AudioProcessor::createLayout()
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
    flt ("gain", "Gain", -60.0f, 12.0f, 0.5f, 0.0f, "dB");
    flt ("pan", "Pan", -100.0f, 100.0f, 1.0f, 0.0f);
    {
        NormalisableRange<float> r (0.1f, 10.0f, 0.01f);
        r.setSkewForCentre (1.0f);
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "autoRate", 1 }, "Auto-Pan Rate", r, 1.0f, AudioParameterFloatAttributes().withLabel ("Hz")));
    }
    flt ("autoDepth", "Auto-Pan Depth", 0.0f, 100.0f, 1.0f, 0.0f, "%");
    flt ("width", "Width", 0.0f, 200.0f, 1.0f, 100.0f, "%");
    flt ("spread", "Spread", 0.0f, 100.0f, 1.0f, 0.0f, "%");
    flt ("bassMono", "Bass Mono (Hz, 0 = off)", 0.0f, 300.0f, 1.0f, 0.0f, "Hz");
    flag ("invL", "Invert Left", false);
    flag ("invR", "Invert Right", false);
    flag ("swap", "Swap Left / Right", false);
    flag ("mono", "Mono", false);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "solo", 1 }, "Listen", StringArray { "Stereo", "Mid", "Side" }, 0));
    return layout;
}

namespace
{
std::vector<vibr::PresetManager::Factory> su1FactoryPresets()
{
    return {
        { "Init", "Everything neutral: the signal passes unchanged", {} },
        { "Wide Stereo", "A wider image with a little spread", { { "width", 150 }, { "spread", 30 } } },
        { "Mono Bass", "Highs stay wide, the lows below 120 Hz are mono", { { "width", 130 }, { "bassMono", 120 } } },
        { "Narrow", "A narrow image, close to the centre", { { "width", 40 } } },
        { "Auto Pan", "The signal sweeps from left to right", { { "autoRate", 0.5f }, { "autoDepth", 100 } } },
        { "Swap Channels", "Left and right exchanged", { { "swap", 1 } } },
        { "Mid Only", "Listen to the centre of the image", { { "solo", 1 } } },
        { "Side Only", "Listen to what is left and right of the centre", { { "solo", 2 } } },
    };
}
}

SU1AudioProcessor::SU1AudioProcessor()
    : vibr::LabProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true),
                          "SU1", createLayout(), "SU-1 Presets", su1FactoryPresets(),
                          { "gain", "pan", "autoRate", "autoDepth", "width", "spread", "bassMono" })
{
    const auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pPower = get ("power"); pGain = get ("gain"); pPan = get ("pan"); pAutoRate = get ("autoRate"); pAutoDepth = get ("autoDepth");
    pWidth = get ("width"); pSpread = get ("spread"); pBassMono = get ("bassMono"); pInvL = get ("invL"); pInvR = get ("invR");
    pSwap = get ("swap"); pMono = get ("mono"); pSolo = get ("solo");
}

bool SU1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void SU1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare ((float) sampleRate);
    powerSmooth.reset (sampleRate, 0.02);
    powerSmooth.setCurrentAndTargetValue (pPower->load() > 0.5f ? 1.0f : 0.0f);
    dryBuf.setSize (2, juce::jmax (samplesPerBlock, 8192));
}

void SU1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
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

    su1::Params p;
    p.gainDb = pGain->load();  p.pan = pPan->load() * 0.01f;  p.autoRate = pAutoRate->load();  p.autoDepth = pAutoDepth->load() * 0.01f;
    p.width = pWidth->load() * 0.01f;  p.spread = pSpread->load() * 0.01f;  p.bassMono = pBassMono->load();
    p.invL = pInvL->load() > 0.5f;  p.invR = pInvR->load() > 0.5f;  p.swap = pSwap->load() > 0.5f;  p.mono = pMono->load() > 0.5f;
    p.solo = juce::jlimit (0, 2, (int) std::lround (pSolo->load()));
    engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n, p);

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

juce::AudioProcessorEditor* SU1AudioProcessor::createEditor() { return new SU1AudioProcessorEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SU1AudioProcessor(); }
