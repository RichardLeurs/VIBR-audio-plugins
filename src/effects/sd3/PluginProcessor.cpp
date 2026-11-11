// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr float kTwoPi = juce::MathConstants<float>::twoPi;
float knob01 (float v) { return juce::jlimit (0.0f, 1.0f, v / 10.0f); }
float attenuatorGain (float pos) { return juce::Decibels::decibelsToGain (pos < 0.5f ? -20.0f : (pos < 1.5f ? -10.0f : 0.0f)); }
} // namespace

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout SDD3000AudioProcessor::createLayout()
{
    using FP = juce::AudioParameterFloat;
    using IP = juce::AudioParameterInt;
    using BP = juce::AudioParameterBool;
    using NR = juce::NormalisableRange<float>;
    juce::AudioProcessorValueTreeState::ParameterLayout l;

    auto knob = [&] (const char* id, const char* name, float def)
    {
        l.add (std::make_unique<FP> (juce::ParameterID { id, 1 }, name, NR (0.0f, 10.0f, 0.01f), def));
    };
    knob ("level",     "Input Level",    5.0f);
    knob ("feedback",  "Feedback",       3.0f);
    knob ("intensity", "Mod Intensity",  0.0f);
    knob ("balance",   "Level Balance",  5.0f);    // 0 = Direct, 5 = both, 10 = Effect

    l.add (std::make_unique<IP> (juce::ParameterID { "attIn", 1 },    "Input Attenuator (0 = -20 dB, 1 = -10 dB, 2 = 0 dB)",  0, 2, 2));
    l.add (std::make_unique<IP> (juce::ParameterID { "attOut", 1 },   "Output Attenuator (0 = -20 dB, 1 = -10 dB, 2 = 0 dB)", 0, 2, 2));
    l.add (std::make_unique<IP> (juce::ParameterID { "delayMs", 1 },  "Delay Time (ms)", 1, 1999, 199));
    l.add (std::make_unique<IP> (juce::ParameterID { "program", 1 },  "Program", 1, kNumPrograms, 1));
    l.add (std::make_unique<IP> (juce::ParameterID { "filter", 1 },   "Filter (0 Off, 1 Low, 2 High, 3 Both)", 0, 3, 0));
    l.add (std::make_unique<IP> (juce::ParameterID { "waveform", 1 }, "Waveform (sine, triangle, ramp up, ramp down, square)", 0, 4, 0));
    l.add (std::make_unique<BP> (juce::ParameterID { "invFeedback", 1 }, "Inv (feedback)", false));
    l.add (std::make_unique<BP> (juce::ParameterID { "hold", 1 },     "Hold",   false));
    l.add (std::make_unique<BP> (juce::ParameterID { "bypass", 1 },   "Bypass", false));
    l.add (std::make_unique<BP> (juce::ParameterID { "invOut", 1 },   "Inv (output)", false));
    l.add (std::make_unique<BP> (juce::ParameterID { "power", 1 },    "Power",  true));
    return l;
}

SDD3000AudioProcessor::SDD3000AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    pLevel     = apvts.getRawParameterValue ("level");
    pAttIn     = apvts.getRawParameterValue ("attIn");
    pAttOut    = apvts.getRawParameterValue ("attOut");
    pDelayMs   = apvts.getRawParameterValue ("delayMs");
    pProgram   = apvts.getRawParameterValue ("program");
    pFeedback  = apvts.getRawParameterValue ("feedback");
    pInvFb     = apvts.getRawParameterValue ("invFeedback");
    pHold      = apvts.getRawParameterValue ("hold");
    pFilter    = apvts.getRawParameterValue ("filter");
    pWaveform  = apvts.getRawParameterValue ("waveform");
    pIntensity = apvts.getRawParameterValue ("intensity");
    pBalance   = apvts.getRawParameterValue ("balance");
    pBypass    = apvts.getRawParameterValue ("bypass");
    pInvOut    = apvts.getRawParameterValue ("invOut");
    pPower     = apvts.getRawParameterValue ("power");

    programTimes = { 199, 100, 250, 375, 500, 750, 1000, 1500, 1999 };
}

bool SDD3000AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) && in == out;
}

//==============================================================================
void SDD3000AudioProcessor::setIntParam (const char* id, int value)
{
    if (auto* prm = apvts.getParameter (id))
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) value));
}

void SDD3000AudioProcessor::selectNextProgram()
{
    const int next = getProgramNumber() % kNumPrograms + 1;
    setIntParam ("program", next);
    setIntParam ("delayMs", programTimes[(size_t) (next - 1)]);
}

void SDD3000AudioProcessor::writeProgram()
{
    programTimes[(size_t) (getProgramNumber() - 1)] = (int) std::round (pDelayMs->load());
}

//==============================================================================
void SDD3000AudioProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    channels[0].delay.prepare (sr);
    channels[1].delay.prepare (sr);
    for (auto& c : channels) c.reset();

    auto init = [this] (juce::SmoothedValue<float>& s, double seconds, float v) { s.reset (sr, seconds); s.setCurrentAndTargetValue (v); };
    init (sDelayMs, 0.06, 199.0f);  init (sFeedback, 0.03, 0.0f);   init (sHoldIn, 0.03, 1.0f);
    init (sInGain, 0.03, 1.0f);     init (sOutGain, 0.03, 1.0f);    init (sDry, 0.03, 1.0f);
    init (sWet, 0.03, 1.0f);        init (sActive, 0.02, 1.0f);     init (sModDepth, 0.05, 0.0f);
    init (sLpHz, 0.03, 20000.0f);   init (sHpHz, 0.03, 5.0f);
    phase = 0.0f;
}

//==============================================================================
void SDD3000AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = juce::jmin (2, buffer.getNumChannels());
    const int numSamples = buffer.getNumSamples();
    for (int c = numCh; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, numSamples);

    const bool hold = *pHold > 0.5f;
    const int filter = juce::jlimit (0, 3, (int) std::round (*pFilter));
    const int shape = juce::jlimit (0, 4, (int) std::round (*pWaveform));
    const float balance = knob01 (*pBalance) * 2.0f;          // 0..2, 1 = centre

    sDelayMs.setTargetValue (juce::jlimit (1.0f, 1999.0f, pDelayMs->load()));
    const float fb = knob01 (*pFeedback) * 1.02f * (*pInvFb > 0.5f ? -1.0f : 1.0f);   // near the end it self-oscillates (bounded by tanh)
    sFeedback.setTargetValue (hold ? 0.9995f : fb);           // HOLD freezes the loop and stops new input entering it
    sHoldIn.setTargetValue (hold ? 0.0f : 1.0f);
    sInGain.setTargetValue (*pLevel / 5.0f * attenuatorGain (*pAttIn));
    sOutGain.setTargetValue (attenuatorGain (*pAttOut));
    sDry.setTargetValue (juce::jmin (1.0f, 2.0f - balance));  // Level Balance: DIRECT <- centre -> EFFECT
    sWet.setTargetValue (juce::jmin (1.0f, balance) * (*pInvOut > 0.5f ? -1.0f : 1.0f));
    sActive.setTargetValue ((*pPower > 0.5f && *pBypass < 0.5f) ? 1.0f : 0.0f);   // power off / bypass = input passes untouched
    sModDepth.setTargetValue (knob01 (*pIntensity));
    sLpHz.setTargetValue ((filter == 2 || filter == 3) ? 2500.0f : 20000.0f);      // HIGH / BOTH: roll off highs in the loop
    sHpHz.setTargetValue ((filter == 1 || filter == 3) ? 250.0f : 5.0f);           // LOW / BOTH: roll off lows in the loop

    constexpr float kModRateHz = 0.8f;   // the panel has no rate control, so the rate is fixed
    float peak = 0.0f;
    auto* data = buffer.getArrayOfWritePointers();
    for (int n = 0; n < numSamples; ++n)
    {
        const float delayMs = sDelayMs.getNextValue(), g = sFeedback.getNextValue(), holdIn = sHoldIn.getNextValue();
        const float gIn = sInGain.getNextValue(), gOut = sOutGain.getNextValue(), dry = sDry.getNextValue(), wet = sWet.getNextValue();
        const float active = sActive.getNextValue(), modI = sModDepth.getNextValue();
        const float lpCoef = 1.0f - std::exp (-kTwoPi * juce::jmin (sLpHz.getNextValue(), 0.45f * (float) sr) / (float) sr);
        const float hpCoef = 1.0f - std::exp (-kTwoPi * sHpHz.getNextValue() / (float) sr);

        phase += kTwoPi * kModRateHz / (float) sr;
        if (phase > kTwoPi) phase -= kTwoPi;
        const float depthMs = modI * modI * juce::jmin (8.0f, 0.25f * delayMs);

        for (int c = 0; c < numCh; ++c)
        {
            auto& ch = channels[c];
            const float raw = std::isfinite (data[c][n]) ? data[c][n] : 0.0f;
            const float inLevel = raw * (*pLevel / 5.0f);               // headroom meter reads after the Level knob
            peak = juce::jmax (peak, std::abs (inLevel));
            const float x = std::tanh (raw * gIn);                      // converters clip softly past full scale

            const float lfo = sdd3000::lfoShape (std::fmod (phase + (float) c * 0.25f * kTwoPi, kTwoPi), shape);
            const float d = juce::jlimit (2.0f, ch.delay.getMaxDelay(), (delayMs + depthMs * lfo) * 0.001f * (float) sr);
            const float echo = ch.delay.read (d);

            ch.lp1 += lpCoef * (echo - ch.lp1);                         // regeneration filters
            ch.lp2 += lpCoef * (ch.lp1 - ch.lp2);
            ch.hpLow += hpCoef * (ch.lp2 - ch.hpLow);
            const float filtered = ch.lp2 - ch.hpLow;
            const float regen = std::tanh (filtered * g);

            ch.delay.write (x * holdIn + regen);

            const float y = (x * dry + echo * wet) * gOut;
            data[c][n] = raw + (y - raw) * active;
        }
    }

    const float decay = std::pow (0.88f, (float) numSamples / 512.0f);
    meter = juce::jmax (peak, meter * decay);
    meterLevel.store (meter);
}

//==============================================================================
juce::AudioProcessorEditor* SDD3000AudioProcessor::createEditor()
{
    return new SDD3000AudioProcessorEditor (*this);
}

void SDD3000AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::StringArray times;
    for (auto t : programTimes) times.add (juce::String (t));
    apvts.state.setProperty ("programTimes", times.joinIntoString (","), nullptr);

    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void SDD3000AudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            const auto times = juce::StringArray::fromTokens (apvts.state.getProperty ("programTimes").toString(), ",", "");
            for (int i = 0; i < juce::jmin (kNumPrograms, times.size()); ++i)
                programTimes[(size_t) i] = juce::jlimit (1, 1999, times[i].getIntValue());
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SDD3000AudioProcessor();
}
