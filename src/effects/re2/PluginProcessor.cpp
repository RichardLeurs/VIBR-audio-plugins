// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace re2;

namespace
{
constexpr float kTwoPi = juce::MathConstants<float>::twoPi;

float knobRange (float v) { return juce::jlimit (0.0f, 1.0f, v / 10.0f); }
} // namespace

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout RE2AudioProcessor::createLayout()
{
    using FP = juce::AudioParameterFloat;
    using NR = juce::NormalisableRange<float>;
    juce::AudioProcessorValueTreeState::ParameterLayout l;

    auto knob = [&] (const char* id, const char* name, float def)
    {
        l.add (std::make_unique<FP> (juce::ParameterID { id, 1 }, name, NR (0.0f, 10.0f, 0.01f), def));
    };

    l.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "mode", 1 }, "Mode Selector", 1, 12, 4));
    knob ("repeatRate", "Repeat Rate", 5.0f);
    knob ("intensity",  "Intensity",   4.0f);
    knob ("echoVol",    "Echo Volume", 5.0f);
    knob ("reverbVol",  "Reverb Volume", 3.0f);
    knob ("bass",       "Bass",        5.0f);
    knob ("treble",     "Treble",      5.0f);
    knob ("saturation", "Saturation",  3.0f);
    knob ("wowFlutter", "Wow & Flutter", 3.0f);
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "tapeAged", 1 }, "Tape Aged (off = New)", false));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "inputLine", 1 }, "Input Line (off = Guitar)", false));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "power", 1 }, "Power", true));
    l.add (vibr::makeBypassParam());
    return l;
}

RE2AudioProcessor::RE2AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    pMode       = apvts.getRawParameterValue ("mode");
    pRate       = apvts.getRawParameterValue ("repeatRate");
    pIntensity  = apvts.getRawParameterValue ("intensity");
    pEchoVol    = apvts.getRawParameterValue ("echoVol");
    pRevVol     = apvts.getRawParameterValue ("reverbVol");
    pBass       = apvts.getRawParameterValue ("bass");
    pTreble     = apvts.getRawParameterValue ("treble");
    pSat        = apvts.getRawParameterValue ("saturation");
    pWowFlutter = apvts.getRawParameterValue ("wowFlutter");
    pTapeAged   = apvts.getRawParameterValue ("tapeAged");
    pInputLine  = apvts.getRawParameterValue ("inputLine");
    pPower     = apvts.getRawParameterValue ("power");
}

bool RE2AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) && in == out;
}

//==============================================================================
void RE2AudioProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    channels[0].prepare (sr, false);
    channels[1].prepare (sr, true);

    inGain.reset (sr, 0.03);   inGain.setCurrentAndTargetValue (1.0f);
    echoGain.reset (sr, 0.03); echoGain.setCurrentAndTargetValue (0.0f);
    revGain.reset (sr, 0.03);  revGain.setCurrentAndTargetValue (0.0f);
    fbGain.reset (sr, 0.03);   fbGain.setCurrentAndTargetValue (0.0f);
    powerMix.reset (sr, 0.02); powerMix.setCurrentAndTargetValue (1.0f);

    // ~250 ms time constant: turning Repeat Rate bends pitch like the real tape motor
    tapeUnitCoef = 1.0f - std::exp (-1.0f / (0.25f * (float) sr));
    tapeUnit = 0.18f * std::pow (0.045f / 0.18f, knobRange (*pRate));

    lastBass = lastTreble = lastAge = -1.0f;
    updateFilters (*pBass, *pTreble, ageValue());
    for (auto& c : channels)
        c.reset();
}

void RE2AudioProcessor::updateFilters (float bass, float treble, float age)
{
    using Coefs = juce::dsp::IIR::Coefficients<float>;
    const auto db = [] (float knob) { return juce::Decibels::decibelsToGain ((knob - 5.0f) * 2.4f); }; // +-12 dB

    // Tape response: high-pass ~90 Hz, low-pass falls from 8 kHz to 3.2 kHz with age
    const float lpFreq = juce::jmap (knobRange (age), 8000.0f, 3200.0f);
    using Arr = juce::dsp::IIR::ArrayCoefficients<float>;
    // First call (prepareToPlay) allocates; later calls overwrite in place, so the audio thread never allocates.
    auto set = [] (juce::dsp::IIR::Filter<float>& f, const std::array<float, 6>& a)
    {
        if (f.coefficients == nullptr) f.coefficients = new Coefs (a);
        else                           *f.coefficients = a;
    };
    for (auto& c : channels)
    {
        set (c.bass,   Arr::makeLowShelf  (sr, 120.0f, 0.7f, db (bass)));
        set (c.treble, Arr::makeHighShelf (sr, 3500.0f, 0.7f, db (treble)));
        set (c.tapeHP, Arr::makeHighPass  (sr, 90.0f, 0.707f));
        set (c.tapeLP, Arr::makeLowPass   (sr, lpFreq, 0.707f));
        set (c.revHP,  Arr::makeHighPass  (sr, 180.0f, 0.707f));
        set (c.revLP,  Arr::makeLowPass   (sr, 4200.0f, 0.707f));
    }
    lastBass = bass; lastTreble = treble; lastAge = age;
}

//==============================================================================
void RE2AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = juce::jmin (2, buffer.getNumChannels());
    const int numSamples = buffer.getNumSamples();

    for (int c = numCh; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, numSamples);

    if (*pBass != lastBass || *pTreble != lastTreble || ageValue() != lastAge)
        updateFilters (*pBass, *pTreble, ageValue());

    const auto& mode = kModes[juce::jlimit (1, 12, (int) std::round (*pMode)) - 1];
    const float rate = knobRange (*pRate);
    const float age = knobRange (ageValue());
    const float wf = knobRange (*pWowFlutter);

    const float tapeUnitTarget = 0.18f * std::pow (0.045f / 0.18f, rate); // 180 ms .. 45 ms per head
    const int numHeads = (mode.headMask & 1) + ((mode.headMask >> 1) & 1) + ((mode.headMask >> 2) & 1);
    const float headWeight = numHeads <= 1 ? 1.0f : (numHeads == 2 ? 0.75f : 0.6f);

    inGain.setTargetValue (std::pow (knobRange (inputValue()), 2.0f) * 3.0f);
    echoGain.setTargetValue (std::pow (knobRange (*pEchoVol), 2.0f) * 1.6f * headWeight);
    revGain.setTargetValue (mode.reverb ? std::pow (knobRange (*pRevVol), 2.0f) * 2.2f : 0.0f);
    fbGain.setTargetValue (numHeads > 0 ? knobRange (*pIntensity) * 1.08f / (float) numHeads : 0.0f); // in-phase taps add, so divide: >1 only near max
    powerMix.setTargetValue (*pPower > 0.5f ? 1.0f : 0.0f);   // power switch = bypass (smoothed)

    const float wowDepth   = (wf + 0.25f * age) * 0.0030f;
    const float flutDepth  = (wf + 0.50f * age) * 0.00035f;
    const float hissAmount = age * age * 0.0012f;
    const float sat = knobRange (*pSat);
    const float satDrive   = 1.0f + age * 1.0f + sat * 3.0f;   // tape saturation in the repeat loop
    const float preDrive   = 1.2f + sat * 2.5f;                // preamp drive (unity gain for small signals)
    const float revSendTarget = mode.reverb ? 1.0f : 0.0f;
    const float modeCoef = 1.0f - std::exp (-1.0f / (0.005f * (float) sr));

    const float maxUnitStep = 0.6f / (3.0f * (float) sr);
    float peakIn = 0.0f, peakOut = 0.0f;
    auto* data = buffer.getArrayOfWritePointers();

    for (int n = 0; n < numSamples; ++n)
    {
        // one-pole glide, slew-limited so head 3's delay grows slower than real time (no reverse playback)
        tapeUnit += juce::jlimit (-maxUnitStep, maxUnitStep, (tapeUnitTarget - tapeUnit) * tapeUnitCoef);

        wowPhase  += kTwoPi * 0.62f / (float) sr;
        wowPhase2 += kTwoPi * 0.62f * 2.7f / (float) sr;
        flutPhase += kTwoPi * 8.4f  / (float) sr;
        if (wowPhase > kTwoPi)  wowPhase  -= kTwoPi;
        if (wowPhase2 > kTwoPi) wowPhase2 -= kTwoPi;
        if (flutPhase > kTwoPi) flutPhase -= kTwoPi;
        noiseLP += 0.0008f * (rng.nextFloat() * 2.0f - 1.0f - noiseLP);

        const float mod = wowDepth * (std::sin (wowPhase) + 0.35f * std::sin (wowPhase2 + 1.1f))
                        + flutDepth * (std::sin (flutPhase) + 18.0f * noiseLP);
        const float unitSamples = tapeUnit * (1.0f + mod) * (float) sr;

        const float gIn = inGain.getNextValue();
        const float gEcho = echoGain.getNextValue();
        const float gRev = revGain.getNextValue();
        const float gFb = fbGain.getNextValue();
        const float gPower = powerMix.getNextValue();
        const float dryComp = 1.0f / juce::jmax (gIn, 0.05f);   // undoes the input gain (0.75 instrument / 0.37 line) on the dry path
        for (int h = 0; h < 3; ++h)
            headGain[h] += (((mode.headMask >> h) & 1) - headGain[h]) * modeCoef;
        revSend += (revSendTarget - revSend) * modeCoef;

        for (int c = 0; c < numCh; ++c)
        {
            auto& ch = channels[c];
            const float raw = std::isfinite (data[c][n]) ? data[c][n] : 0.0f;
            peakIn = juce::jmax (peakIn, std::abs (raw));

            // instrument preamp
            const float x = std::tanh (raw * gIn * preDrive) / preDrive;

            // playback heads
            float tap = 0.0f;
            for (int h = 0; h < 3; ++h)
                if (headGain[h] > 1.0e-4f)
                    tap += headGain[h] * ch.tape.read (juce::jmax (2.0f, unitSamples * (float) (h + 1)));

            tap = ch.tapeLP.processSample (ch.tapeHP.processSample (tap));
            tap = softSat (tap * satDrive) / satDrive;

            // record head: dry + repeat feedback + hiss
            const float hiss = hissAmount * (rng.nextFloat() * 2.0f - 1.0f);
            ch.tape.write ((x + tap * gFb + hiss) * gPower);

            // spring reverb, fed from the preamp
            const float rIn = ch.revLP.processSample (ch.revHP.processSample (x * revSend));
            const float rev = 0.5f * (ch.springA.process (rIn) + ch.springB.process (rIn));

            // wet path through the tone stack
            float wet = tap * gEcho + rev * gRev;
            wet = ch.treble.processSample (ch.bass.processSample (wet));

            const float out = x * dryComp + wet;       // dry path at unity for small signals, so ON matches bypass level
            const float y = raw + (out - raw) * gPower;   // power switch = bypass (smoothed)
            data[c][n] = y;
            peakOut = juce::jmax (peakOut, std::abs (y));
        }
    }

    // metering
    if (peakIn > inputPeak.load()) inputPeak.store (peakIn);   // editor clears it with exchange(0)
    const float decay = std::pow (0.9f, (float) numSamples / 512.0f);
    meterOut = juce::jmax (peakOut, meterOut * decay);
    outputLevel.store (meterOut);
}

//==============================================================================
juce::AudioProcessorEditor* RE2AudioProcessor::createEditor()
{
    return new RE2AudioProcessorEditor (*this);
}

void RE2AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void RE2AudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RE2AudioProcessor();
}
