// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr float kTwoPi = juce::MathConstants<float>::twoPi;
float knobRange (float v) { return juce::jlimit (0.0f, 1.0f, v / 10.0f); }
} // namespace

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout CE1AudioProcessor::createLayout()
{
    using FP = juce::AudioParameterFloat;
    using NR = juce::NormalisableRange<float>;
    juce::AudioProcessorValueTreeState::ParameterLayout l;

    auto knob = [&] (const char* id, const char* name, float def)
    {
        l.add (std::make_unique<FP> (juce::ParameterID { id, 1 }, name, NR (0.0f, 10.0f, 0.01f), def));
    };
    knob ("level",           "Level Control",    6.0f);
    knob ("chorusIntensity", "Chorus Intensity", 5.0f);
    knob ("vibratoDepth",    "Vibrato Depth",    5.0f);
    knob ("vibratoRate",     "Vibrato Rate",     5.0f);   // 0..10 -> 0.4 .. 10 Hz, exponential (ce1::vibratoRateHz); ignored while sync is on
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "vibrato", 1 },   "Effect (off = Chorus, on = Vibrato)", false));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "inputHigh", 1 }, "Input (off = Low, on = High)", true));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "power", 1 },     "Power", true, juce::AudioParameterBoolAttributes().withMeta (true)));
    l.add (vibr::makeBypassParam());
    // tempo sync of the vibrato rate (the chorus rate is fixed): on = one LFO cycle per division of the host tempo, clamped to 0.4 .. 10 Hz
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "sync", 1 }, "Sync", false));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "division", 1 }, "Division", vibr::sync::divisionNames(),
                                                         vibr::sync::divisionNames().indexOf ("1/4")));
    return l;
}

// The factory presets of the preset manager (plain values; ids not listed keep their default). Bypass / power are never touched.
// vibrato: off = chorus, on = vibrato; inputHigh: on = High input.
std::vector<vibr::PedalPresets::Factory> CE1AudioProcessor::factoryPresets()
{
    return {
        { "Init", "All controls at their defaults", {} },
        { "Classic Chorus", "The familiar shimmering chorus",
          { { "vibrato", 0 }, { "chorusIntensity", 6.0f }, { "level", 6.0f } } },
        { "Light Shimmer", "Subtle widening, barely there",
          { { "vibrato", 0 }, { "chorusIntensity", 3.0f }, { "level", 6.0f } } },
        { "Deep Chorus", "Thick, swirling chorus",
          { { "vibrato", 0 }, { "chorusIntensity", 9.0f }, { "level", 5.5f } } },
        { "Slow Vibrato", "Gentle pitch wave, vintage amp feel",
          { { "vibrato", 1 }, { "vibratoDepth", 4.0f }, { "vibratoRate", 3.0f }, { "level", 6.0f } } },
        { "Fast Vibrato", "Quick pulsing pitch wobble",
          { { "vibrato", 1 }, { "vibratoDepth", 5.5f }, { "vibratoRate", 7.0f }, { "level", 6.0f } } },
        { "Seasick", "Deep, slow pitch swells",
          { { "vibrato", 1 }, { "vibratoDepth", 9.0f }, { "vibratoRate", 1.5f }, { "level", 5.5f } } },
        { "Synced Vibrato", "Pitch wave locked to the host tempo",
          { { "vibrato", 1 }, { "vibratoDepth", 5.0f }, { "sync", 1 }, { "division", 3 }, { "level", 6.0f } } },
    };
}

CE1AudioProcessor::CE1AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "STATE", createLayout())
{
    pLevel     = apvts.getRawParameterValue ("level");
    pChorus    = apvts.getRawParameterValue ("chorusIntensity");
    pDepth     = apvts.getRawParameterValue ("vibratoDepth");
    pRate      = apvts.getRawParameterValue ("vibratoRate");
    pVibrato   = apvts.getRawParameterValue ("vibrato");
    pInputHigh = apvts.getRawParameterValue ("inputHigh");
    pPower     = apvts.getRawParameterValue ("power");
    pSync      = apvts.getRawParameterValue ("sync");
    pDivision  = apvts.getRawParameterValue ("division");

    midiMap.setDefault ("vibratoDepth", 1);           // mod wheel
    midiMap.setDefault ("vibratoRate", 11);           // expression pedal
    midiMap.setSyncAlias ("vibratoRate", "sync", "division");
}

bool CE1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) && in == out;
}

//==============================================================================
void CE1AudioProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    for (auto& c : channels)
    {
        c.delay.prepare (sr);
        c.reset();
    }

    auto init = [this] (juce::SmoothedValue<float>& s, double seconds, float v) { s.reset (sr, seconds); s.setCurrentAndTargetValue (v); };
    init (sDry, 0.03, 0.62f);       init (sWet, 0.03, 0.4f);
    init (sCentreMs, 0.08, 6.5f);   init (sDepthMs, 0.05, 1.5f);
    init (sRateHz, 0.08, 0.85f);    init (sInGain, 0.03, 1.0f);
    init (sOutGain, 0.03, 0.8f);    init (sPower, 0.02, *pPower > 0.5f ? 1.0f : 0.0f);

    lpCoef = 1.0f - std::exp (-kTwoPi * 7000.0f / (float) sr);   // BBD-style darkening above ~7 kHz
    phase = 0.0f;
}

//==============================================================================
void CE1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    midiMap.process (midi);
    juce::ScopedNoDenormals noDenormals;
    const int numCh = juce::jmin (2, buffer.getNumChannels());
    const int numSamples = buffer.getNumSamples();
    for (int c = numCh; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, numSamples);

    const bool vibrato = *pVibrato > 0.5f;
    const float chorusI = knobRange (*pChorus);
    const float depthK = knobRange (*pDepth);
    const float rateK = knobRange (*pRate);
    const float level = knobRange (*pLevel);
    const double bpm = vibr::sync::hostBpm (*this, 120.0);          // host tempo, once per block (120 without a host)
    const bool synced = *pSync > 0.5f;

    if (! vibrato)
    {
        // Chorus: fixed slow rate, depth and wet amount follow Chorus Intensity (Vibrato knobs are not used)
        sCentreMs.setTargetValue (6.5f);
        sDepthMs.setTargetValue (0.5f + 2.8f * chorusI);
        sRateHz.setTargetValue (0.85f);
        sDry.setTargetValue (0.62f);
        sWet.setTargetValue (0.20f + 0.50f * chorusI);
    }
    else
    {
        // Vibrato: 100% wet pitch modulation; Depth up to 3 ms, Rate 0.4 .. 10 Hz (Chorus Intensity is not used).
        // SYNC: one LFO cycle per division of the host tempo, clamped to the same range; sRateHz glides (80 ms) so tempo changes do not click
        sCentreMs.setTargetValue (4.0f);
        sDepthMs.setTargetValue (3.0f * depthK);
        sRateHz.setTargetValue (synced ? juce::jlimit (ce1::kVibratoMinHz, ce1::kVibratoMaxHz, (float) vibr::sync::hz (bpm, (int) pDivision->load()))
                                     : ce1::vibratoRateHz (rateK));
        sDry.setTargetValue (0.0f);
        sWet.setTargetValue (1.0f);
    }
    sInGain.setTargetValue (*pInputHigh > 0.5f ? 1.0f : 0.35f);   // LOW pads the input
    sOutGain.setTargetValue (level * level * 2.2f);
    sPower.setTargetValue (*pPower > 0.5f ? 1.0f : 0.0f);

    float peak = 0.0f;
    auto* data = buffer.getArrayOfWritePointers();
    for (int n = 0; n < numSamples; ++n)
    {
        const float rate = sRateHz.getNextValue();
        phase += kTwoPi * rate / (float) sr;
        if (phase > kTwoPi) phase -= kTwoPi;

        const float centre = sCentreMs.getNextValue(), depth = sDepthMs.getNextValue();
        const float dry = sDry.getNextValue(), wet = sWet.getNextValue();
        const float gIn = sInGain.getNextValue(), gOut = sOutGain.getNextValue(), gPower = sPower.getNextValue();

        for (int c = 0; c < numCh; ++c)
        {
            auto& ch = channels[c];
            const float raw = std::isfinite (data[c][n]) ? data[c][n] : 0.0f;
            const float in = raw * gIn;
            peak = juce::jmax (peak, std::abs (in));

            const float x = std::tanh (in * 1.3f) / 1.3f;                 // input stage
            ch.delay.write (x);

            const float lfo = std::sin (phase + (float) c * 0.5f * juce::MathConstants<float>::pi);   // right channel 90 degrees later
            const float delaySamples = juce::jmax (2.0f, (centre + depth * lfo) * 0.001f * (float) sr);
            float d = ch.delay.read (delaySamples);
            ch.lp1 += lpCoef * (d - ch.lp1);
            ch.lp2 += lpCoef * (ch.lp1 - ch.lp2);
            d = ch.lp2;

            const float y = (dry * x + wet * d) * gOut;
            data[c][n] = raw + (y - raw) * gPower;                         // power = smooth bypass
        }
    }

    if (peak > inputPeak.load()) inputPeak.store (peak);
}

//==============================================================================
juce::AudioProcessorEditor* CE1AudioProcessor::createEditor()
{
    return new CE1AudioProcessorEditor (*this);
}

void CE1AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.appendChild (midiMap.toTree(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void CE1AudioProcessor::setStateInformation (const void* data, int size)
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

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CE1AudioProcessor();
}
