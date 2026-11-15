// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout SL2AudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    const auto knob = [&] (const char* id, const char* name, float def)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 10.0f, 0.01f), def));
    };
    knob ("balance", "Balance", 7.0f);
    knob ("tempo",   "Tempo",   3.5f);       // 110 bpm
    knob ("attack",  "Attack",  2.0f);
    knob ("duty",    "Duty",    6.0f);
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "variation", 1 }, "Variation", 1, sl2::kNumPatterns, 2));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode",
                                                        StringArray { "Single", "Dual", "Tremolo", "SFX", "Harmonic" }, sl2::Single));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "hostSync", 1 }, "Follow Host Tempo (off = Tempo knob)", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "bypass", 1 }, "Bypass", false));
    return layout;
}

SL2AudioProcessor::SL2AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SL2", createLayout())
{
    pBalance   = apvts.getRawParameterValue ("balance");
    pTempo     = apvts.getRawParameterValue ("tempo");
    pAttack    = apvts.getRawParameterValue ("attack");
    pDuty      = apvts.getRawParameterValue ("duty");
    pVariation = apvts.getRawParameterValue ("variation");
    pMode      = apvts.getRawParameterValue ("mode");
    pHostSync  = apvts.getRawParameterValue ("hostSync");
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("bypass"));
}

bool SL2AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) && out == layouts.getMainInputChannelSet();
}

void SL2AudioProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    for (auto& s : svf) s.reset();
    for (auto& l : lowBand) l = 0.0f;
    envA = envB = 0.0f; envTa = envTb = 1.0f;
    stepPos = 0.0;
    balanceSmooth.reset (sampleRate, 0.02); balanceSmooth.setCurrentAndTargetValue (pBalance->load() * 0.1f);
    wetSmooth.reset (sampleRate, 0.01);     wetSmooth.setCurrentAndTargetValue (bypassParam->get() ? 0.0f : 1.0f);
}

void SL2AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int channels = juce::jmin (buffer.getNumChannels(), kMaxChannels);
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, n);
    if (n == 0 || channels == 0) return;

    balanceSmooth.setTargetValue (pBalance->load() * 0.1f);
    wetSmooth.setTargetValue (bypassParam->get() ? 0.0f : 1.0f);

    // tempo: the host's (and its beat position while it plays) or the TEMPO knob
    double bpm = (double) sl2::bpmFromKnob (pTempo->load() * 0.1f);
    bool hostLocked = false;
    double hostStep = 0.0;
    if (pHostSync->load() > 0.5f)
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
            {
                if (auto hb = pos->getBpm()) bpm = juce::jlimit (30.0, 300.0, *hb);
                if (pos->getIsPlaying())
                    if (auto ppq = pos->getPpqPosition()) { hostLocked = true; hostStep = *ppq * 4.0; }
            }
    const double stepInc = bpm / 60.0 * 4.0 / sr;
    if (hostLocked) stepPos = hostStep;

    const int pattern = juce::jlimit (0, sl2::kNumPatterns - 1, (int) std::lround (pVariation->load()) - 1);
    const int mode = juce::jlimit (0, sl2::kNumModes - 1, (int) std::lround (pMode->load()));
    const float sampleRateF = (float) sr;
    const float duty = sl2::dutyFromKnob (pDuty->load() * 0.1f);
    const float atkCoef = sl2::coefMs (sl2::attackMs (pAttack->load() * 0.1f), sampleRateF);
    const float relCoef = sl2::coefMs (4.0f, sampleRateF);
    const float swellCoef = sl2::coefMs (0.5f * (float) (60000.0 / bpm / 4.0), sampleRateF);     // tremolo / harmonic: smooth over half a step
    const float xoverCoef = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * 700.0f / sampleRateF);

    for (int i = 0; i < n; ++i)
    {
        const float bal = balanceSmooth.getNextValue(), bypassMix = wetSmooth.getNextValue();
        const float dryGain = juce::jmin (1.0f, 2.0f * (1.0f - bal)), wetGain = juce::jmin (1.0f, 2.0f * bal);

        double pos = hostLocked ? stepPos + (double) i * stepInc : stepPos;
        const int stepIndex = (int) std::floor (pos);
        const float u = (float) (pos - std::floor (pos));
        const bool on = sl2::stepOn (pattern, ((stepIndex % sl2::kSteps) + sl2::kSteps) % sl2::kSteps);
        if (! hostLocked) stepPos += stepInc;

        // slice envelopes: gate of the pattern (A) and of its gaps (B), plus smooth versions for tremolo / harmonic
        const float gateA = (on && u < duty) ? 1.0f : 0.0f, gateB = (! on && u < duty) ? 1.0f : 0.0f;
        envA += (gateA - envA) * (gateA > envA ? atkCoef : relCoef);
        envB += (gateB - envB) * (gateB > envB ? atkCoef : relCoef);
        envTa += ((on ? 1.0f : 0.15f) - envTa) * swellCoef;
        envTb += ((on ? 0.15f : 1.0f) - envTb) * swellCoef;

        for (int c = 0; c < channels; ++c)
        {
            auto* x = buffer.getWritePointer (c);
            const float in = x[i];
            float wet = 0.0f;
            switch (mode)
            {
                case sl2::Single:  wet = in * envA; break;
                case sl2::Dual:    wet = in * (c == 0 ? envA : envB); break;
                case sl2::Tremolo: wet = in * envTa; break;
                case sl2::Sfx:     wet = svf[c].process (in, 250.0f + 5500.0f * envA, sampleRateF) * (0.15f + 0.85f * envA) * 1.6f; break;
                default:
                {
                    lowBand[c] += xoverCoef * (in - lowBand[c]);
                    wet = lowBand[c] * envTa + (in - lowBand[c]) * envTb;
                    break;
                }
            }
            const float effect = in * dryGain + wet * wetGain;
            x[i] = in + (effect - in) * bypassMix;
        }
    }
    if (! hostLocked) stepPos = std::fmod (stepPos, (double) sl2::kSteps * 1024.0);
}

juce::AudioProcessorEditor* SL2AudioProcessor::createEditor() { return new SL2AudioProcessorEditor (*this); }

void SL2AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void SL2AudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SL2AudioProcessor(); }
