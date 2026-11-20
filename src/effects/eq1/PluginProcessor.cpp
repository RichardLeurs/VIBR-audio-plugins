// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout EQ1AudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    const auto flag = [&layout] (const String& id, const String& name, bool def)
    {
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def, AudioParameterBoolAttributes().withMeta (id == "power" || id == "bypass")));
    };
    const auto flt = [&layout] (const String& id, const String& name, NormalisableRange<float> r, float def, const char* unit)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, AudioParameterFloatAttributes().withLabel (unit)));
    };
    flag ("power", "Power", true);
    flag ("bypass", "Bypass", false);
    flt ("outGain", "Output Gain", NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, "dB");

    // band 1 low shelf, 2 .. 7 bells, 8 high shelf (the same parameters as the MS-1)
    static const float defFreq[ms1::kEqBands] = { 80.0f, 150.0f, 400.0f, 1000.0f, 2500.0f, 5000.0f, 9000.0f, 12000.0f };
    for (int n = 1; n <= ms1::kEqBands; ++n)
    {
        const String id = "eq" + String (n), nm = "EQ " + String (n) + " ";
        const bool shelf = n == 1 || n == ms1::kEqBands;
        flag (id + "On", nm + "On", true);
        NormalisableRange<float> fr (20.0f, 20000.0f, 1.0f);  fr.setSkewForCentre (1000.0f);
        flt (id + "Freq", nm + "Frequency", fr, defFreq[n - 1], "Hz");
        flt (id + "Gain", nm + "Gain", NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f, "dB");
        NormalisableRange<float> qr (0.2f, 10.0f, 0.01f);  qr.setSkewForCentre (1.0f);
        flt (id + "Q", nm + "Q", qr, shelf ? 0.71f : 1.0f, "");
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id + "Mode", 1 }, nm + "Mode", StringArray { "Stereo", "Left", "Right", "Mid", "Side" }, 0));
        NormalisableRange<float> ar (1.0f, 100.0f, 0.1f), rr (10.0f, 1000.0f, 1.0f), dr (1.0f, 10.0f, 0.1f);
        ar.setSkewForCentre (10.0f);  rr.setSkewForCentre (100.0f);  dr.setSkewForCentre (3.0f);
        flag (id + "Dyn", nm + "Dynamic", false);
        flt (id + "DynThr", nm + "Dynamic Threshold", NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -30.0f, "dB");
        flt (id + "DynRatio", nm + "Dynamic Ratio", dr, 2.0f, "");
        flt (id + "DynAtk", nm + "Dynamic Attack", ar, 10.0f, "ms");
        flt (id + "DynRel", nm + "Dynamic Release", rr, 100.0f, "ms");
        flag (id + "DynBelow", nm + "Dynamic Below Threshold", false);
    }
    return layout;
}

namespace
{
std::vector<vibr::PresetManager::Factory> eq1FactoryPresets()
{
    return {
        { "Init", "Flat: every band at 0 dB", {} },
        { "Smile", "Bass and treble lifted, the mids a little back", { { "eq1Gain", 4.0f }, { "eq1Freq", 90.0f }, { "eq4Gain", -2.5f }, { "eq4Freq", 900.0f }, { "eq4Q", 0.7f }, { "eq8Gain", 4.0f }, { "eq8Freq", 10000.0f } } },
        { "Cut Mud", "A narrow dip around 300 Hz and the rumble below 40 Hz tamed", { { "eq1Gain", -6.0f }, { "eq1Freq", 35.0f }, { "eq3Gain", -3.5f }, { "eq3Freq", 300.0f }, { "eq3Q", 1.4f } } },
        { "Vocal Presence", "Body at 200 Hz, presence at 3 kHz, air above 10 kHz", { { "eq2Gain", 2.0f }, { "eq2Freq", 200.0f }, { "eq5Gain", 3.0f }, { "eq5Freq", 3000.0f }, { "eq5Q", 0.9f }, { "eq8Gain", 2.5f }, { "eq8Freq", 11000.0f } } },
        { "Warm", "Low shelf up, the top rolled off a little", { { "eq1Gain", 3.0f }, { "eq1Freq", 120.0f }, { "eq8Gain", -3.0f }, { "eq8Freq", 9000.0f } } },
        { "Air", "A gentle high shelf from 8 kHz", { { "eq8Gain", 5.0f }, { "eq8Freq", 8000.0f }, { "eq8Q", 0.6f } } },
        { "De-Ess", "A dynamic cut at 7 kHz: it only works when the band is loud", { { "eq7Gain", -8.0f }, { "eq7Freq", 7000.0f }, { "eq7Q", 2.0f }, { "eq7Dyn", 1 }, { "eq7DynThr", -34.0f }, { "eq7DynRatio", 3.0f }, { "eq7DynAtk", 3.0f }, { "eq7DynRel", 80.0f } } },
        { "Mid / Side Width", "Highs added to the sides only, the centre untouched", { { "eq7Gain", 3.0f }, { "eq7Freq", 6000.0f }, { "eq7Q", 0.7f }, { "eq7Mode", 4 } } },
    };
}
}

EQ1AudioProcessor::EQ1AudioProcessor()
    : vibr::LabProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true),
                          "EQ1", createLayout(), "EQ-1 Presets", eq1FactoryPresets(), { "outGain" })
{
    const auto get = [this] (const juce::String& id) { return apvts.getRawParameterValue (id); };
    pPower = get ("power");  pOutGain = get ("outGain");
    for (int k = 0; k < ms1::kEqBands; ++k)
    {
        const juce::String id = "eq" + juce::String (k + 1);
        auto& b = pBand[(size_t) k];
        b.on = get (id + "On");  b.freq = get (id + "Freq");  b.gain = get (id + "Gain");  b.q = get (id + "Q");  b.mode = get (id + "Mode");
        b.dyn = get (id + "Dyn");  b.thr = get (id + "DynThr");  b.ratio = get (id + "DynRatio");  b.atk = get (id + "DynAtk");  b.rel = get (id + "DynRel");  b.below = get (id + "DynBelow");
    }
    for (auto& s : spec) s.store (0.0f);
}

bool EQ1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void EQ1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sampleRateNow = sampleRate;
    eq.prepare ((float) sampleRate);
    powerSmooth.reset (sampleRate, 0.02);
    powerSmooth.setCurrentAndTargetValue (pPower->load() > 0.5f ? 1.0f : 0.0f);
    gainSmooth.reset (sampleRate, 0.03);
    gainSmooth.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOutGain->load()));
    dryBuf.setSize (2, juce::jmax (samplesPerBlock, 8192));
}

void EQ1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
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

    std::array<ms1::EqBand, ms1::kEqBands> bands;
    for (int k = 0; k < ms1::kEqBands; ++k)
    {
        const auto& p = pBand[(size_t) k];
        auto& b = bands[(size_t) k];
        b.freq = p.freq->load();  b.gainDb = p.gain->load();  b.q = p.q->load();  b.mode = juce::jlimit (0, 4, (int) std::lround (p.mode->load()));  b.on = p.on->load() > 0.5f;
        b.dyn = p.dyn->load() > 0.5f;  b.dynBelow = p.below->load() > 0.5f;
        b.dynThresholdDb = p.thr->load();  b.dynRatio = p.ratio->load();  b.dynAttackMs = p.atk->load();  b.dynReleaseMs = p.rel->load();
    }
    auto* l = buffer.getWritePointer (0);
    auto* r = buffer.getWritePointer (1);
    eq.process (l, r, n, bands.data());

    gainSmooth.setTargetValue (juce::Decibels::decibelsToGain (pOutGain->load()));
    for (int i = 0; i < n; ++i) { const float g = gainSmooth.getNextValue(); l[i] *= g; r[i] *= g; }

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

    int pos = specPos.load (std::memory_order_relaxed);                // the spectrum display reads the mono output
    for (int i = 0; i < n; ++i) { spec[pos].store (0.5f * (l[i] + r[i]), std::memory_order_relaxed); pos = (pos + 1) % kSpec; }
    specPos.store (pos, std::memory_order_release);
    outMeter.measure (buffer, n);
}

juce::AudioProcessorEditor* EQ1AudioProcessor::createEditor() { return new EQ1AudioProcessorEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new EQ1AudioProcessor(); }
