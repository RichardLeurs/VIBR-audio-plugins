// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
// the factory presets: the defaults of the source plugins and a few musical starting points (values are plain host values)
std::vector<vibr::PresetManager::Factory> dl1FactoryPresets()
{
    return {
        { "Init", "A clean digital delay", {} },
        { "SD-1 default", "The Digital Delay at its default settings",
          { { "mode", 0 }, { "time", 406 }, { "feedback", 30 }, { "high", 8139 }, { "digFactor", 100 }, { "digDirect", 70 }, { "digEffect", 50 }, { "digInput", 50 }, { "modDepth", 0 } } },
        { "MM-1 default", "The Analog Delay at its default settings",
          { { "mode", 1 }, { "variant", 0 }, { "time", 148 }, { "feedback", 40 }, { "high", 20000 }, { "anaBlend", 50 }, { "anaDepth", 20 }, { "modRate", 0.51f } } },
        { "Slapback", "One short repeat, a little modulation", { { "time", 95 }, { "feedback", 12 }, { "high", 6000 }, { "mix", 40 }, { "digDirect", 75 }, { "digEffect", 55 } } },
        { "Dotted Eighth", "Synced to the tempo, ducks under the playing",
          { { "variant", 1 }, { "sync", 1 }, { "division", 7 }, { "feedback", 45 }, { "high", 5000 }, { "low", 120 }, { "mix", 45 } } },
        { "Ping-Pong", "Quarter notes bouncing left and right", { { "sync", 1 }, { "division", 2 }, { "spread", 100 }, { "feedback", 55 }, { "high", 6500 }, { "low", 150 }, { "mix", 40 } } },
        { "Warm Echo", "Analog repeats that get darker", { { "mode", 1 }, { "time", 320 }, { "feedback", 50 }, { "high", 6000 }, { "anaBlend", 45 }, { "anaDepth", 30 }, { "anaGrit", 35 } } },
        { "Analog Chorus", "A short, wide analog chorus", { { "mode", 1 }, { "variant", 1 }, { "time", 250 }, { "spread", 40 }, { "modRate", 0.8f }, { "anaDepth", 55 }, { "mix", 100 } } },
        { "Analog Vibrato", "Pitch wobble, no dry sound", { { "mode", 1 }, { "variant", 2 }, { "time", 250 }, { "modRate", 5.0f }, { "anaDepth", 45 } } },
        { "Space Tape", "Three heads, a little wow and tape saturation",
          { { "mode", 2 }, { "variant", 2 }, { "time", 180 }, { "feedback", 45 }, { "high", 7000 }, { "tapeWow", 35 }, { "tapeSat", 40 }, { "mix", 40 } } },
        { "Worn Tape", "Old tape: dark, wobbly, with drop-outs", { { "mode", 2 }, { "time", 320 }, { "feedback", 50 }, { "tapeWow", 55 }, { "tapeSat", 55 }, { "tapeAge", 70 }, { "mix", 40 } } },
        { "Dual Echo", "Two delays, A and B at three quarters of A", { { "mode", 3 }, { "time", 400 }, { "feedback", 35 }, { "dualTimeB", 0.75f }, { "dualFbB", 35 }, { "mix", 40 } } },
        { "Dual Series", "B repeats what A repeats", { { "mode", 3 }, { "variant", 1 }, { "time", 300 }, { "feedback", 30 }, { "dualTimeB", 0.5f }, { "dualFbB", 40 }, { "mix", 40 } } },
        { "DM-1 default", "The Delay Machine, Classic voicing", { { "mode", 4 }, { "modVoice", 0 }, { "time", 200 }, { "feedback", 40 }, { "modRate", 0.63f }, { "modDepth", 55 }, { "modVariation", 50 }, { "modVolume", 50 } } },
        { "Multi-Head Echo", "Three taps, tape-echo style", { { "mode", 4 }, { "modVoice", 3 }, { "time", 360 }, { "feedback", 45 }, { "modVariation", 50 }, { "mix", 40 } } },
        { "Pan Delay", "Repeats panned left and right", { { "mode", 4 }, { "modVoice", 10 }, { "time", 420 }, { "feedback", 45 }, { "modVariation", 40 }, { "mix", 40 } } },
        { "Endless Loop", "HOLD freezes the repeats", { { "time", 700 }, { "feedback", 60 }, { "high", 9000 }, { "hold", 1 }, { "mix", 60 } } },
    };
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout DL1AudioProcessor::createLayout()
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
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode", StringArray { "Digital", "Analog", "Tape", "Dual", "Modulated" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "variant", 1 }, "Type (Plain / Ducked, Delay / Chorus / Vibrato, 1 / 2 / 3 Heads, Parallel / Series)", StringArray { "Type 1", "Type 2", "Type 3" }, 0));
    flag ("sync", "Sync (time follows the host tempo)", false);
    {
        StringArray names;
        for (int i = 0; i < dl1::kNumDivisions; ++i) names.add (dl1::divisionName (i));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "division", 1 }, "Note Length (with Sync)", names, 2));
    }
    flt ("time", "Time (ms)", 1.0f, 2000.0f, 1.0f, 350.0f, 300.0f);
    flt ("feedback", "Feedback", 0.0f, 100.0f, 1.0f, 40.0f);
    flt ("low", "Low Cut (Hz, 20 = off)", 20.0f, 1000.0f, 1.0f, 20.0f, 150.0f);
    flt ("high", "Hi Cut (Hz)", 1000.0f, 20000.0f, 1.0f, 8000.0f, 6000.0f);
    flt ("modRate", "Mod Rate (Hz)", 0.05f, 10.0f, 0.01f, 1.0f, 1.0f);
    flt ("modDepth", "Mod Depth", 0.0f, 100.0f, 1.0f, 10.0f);
    flt ("spread", "Ping-Pong Spread", 0.0f, 100.0f, 1.0f, 0.0f);
    flag ("hold", "Hold (endless loop)", false);
    flt ("mix", "Mix", 0.0f, 100.0f, 1.0f, 100.0f);
    flt ("level", "Level", -24.0f, 12.0f, 0.1f, 0.0f);
    flt ("digFactor", "Digital: Time Factor", 0.0f, 100.0f, 1.0f, 100.0f);
    flt ("digDirect", "Digital: Direct", 0.0f, 100.0f, 1.0f, 70.0f);
    flt ("digEffect", "Digital: Effect", 0.0f, 100.0f, 1.0f, 50.0f);
    flt ("digInput", "Digital: Input", 0.0f, 100.0f, 1.0f, 50.0f);
    flt ("anaBlend", "Analog: Blend", 0.0f, 100.0f, 1.0f, 50.0f);
    flt ("anaDepth", "Analog: Depth", 0.0f, 100.0f, 1.0f, 20.0f);
    flt ("anaGrit", "Analog: Grit", 0.0f, 100.0f, 1.0f, 0.0f);
    flt ("tapeWow", "Tape: Wow & Flutter", 0.0f, 100.0f, 1.0f, 30.0f);
    flt ("tapeSat", "Tape: Saturation", 0.0f, 100.0f, 1.0f, 30.0f);
    flt ("tapeAge", "Tape: Age", 0.0f, 100.0f, 1.0f, 0.0f);
    flt ("tapeSpacing", "Tape: Head Spacing", 0.0f, 100.0f, 1.0f, 50.0f);
    flt ("dualTimeB", "Dual: Time B (ratio of A)", 0.25f, 4.0f, 0.01f, 0.75f, 1.0f);
    flt ("dualFbB", "Dual: Feedback B", 0.0f, 100.0f, 1.0f, 40.0f);
    flt ("dualLevelA", "Dual: Level A", 0.0f, 100.0f, 1.0f, 80.0f);
    flt ("dualLevelB", "Dual: Level B", 0.0f, 100.0f, 1.0f, 80.0f);
    flt ("dualHighB", "Dual: Hi Cut B (Hz)", 1000.0f, 20000.0f, 1.0f, 8000.0f, 6000.0f);
    {
        StringArray names;
        for (int i = 0; i < dl1::kNumVoices; ++i) names.add (dl1::voiceName (i));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "modVoice", 1 }, "Modulated: Voicing (the 12 DM-1 modes)", names, 0));
    }
    flt ("modVariation", "Modulated: Variation", 0.0f, 100.0f, 1.0f, 50.0f);
    flt ("modVolume", "Modulated: Delay Volume", 0.0f, 100.0f, 1.0f, 50.0f);
    flag ("dualPingA", "Dual: Ping-Pong A", false);
    flag ("dualPingB", "Dual: Ping-Pong B", true);
    return layout;
}

DL1AudioProcessor::DL1AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "DL1", createLayout()),
      presets (*this, apvts, undoManager, "DL-1 Presets", dl1FactoryPresets(), { "power" })
{
    const auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pPower = get ("power"); pMode = get ("mode"); pVariant = get ("variant"); pSync = get ("sync"); pDivision = get ("division"); pTime = get ("time"); pFeedback = get ("feedback");
    pLow = get ("low"); pHigh = get ("high"); pModRate = get ("modRate"); pModDepth = get ("modDepth"); pSpread = get ("spread"); pHold = get ("hold"); pMix = get ("mix"); pLevel = get ("level");
    pDigFactor = get ("digFactor"); pDigDirect = get ("digDirect"); pDigEffect = get ("digEffect"); pDigInput = get ("digInput");
    pAnaBlend = get ("anaBlend"); pAnaDepth = get ("anaDepth"); pAnaGrit = get ("anaGrit");
    pTapeWow = get ("tapeWow"); pTapeSat = get ("tapeSat"); pTapeAge = get ("tapeAge"); pTapeSpacing = get ("tapeSpacing");
    pDualTimeB = get ("dualTimeB"); pDualFbB = get ("dualFbB"); pDualLevelA = get ("dualLevelA"); pDualLevelB = get ("dualLevelB"); pDualHighB = get ("dualHighB");
    pModVoice = get ("modVoice"); pModVariation = get ("modVariation"); pModVolume = get ("modVolume");
    pDualPingA = get ("dualPingA"); pDualPingB = get ("dualPingB");
}

bool DL1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void DL1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare ((float) sampleRate);
    powerSmooth.reset (sampleRate, 0.02);
    powerSmooth.setCurrentAndTargetValue (pPower->load() > 0.5f ? 1.0f : 0.0f);
    dryBuf.setSize (2, juce::jmax (samplesPerBlock, 8192));
}

void DL1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0 || buffer.getNumChannels() < 2 || n > dryBuf.getNumSamples()) return;
    for (int c = 0; c < 2; ++c)                                                    // NaN, inf and absurd values from the host never reach the delay lines
    {
        auto* d = buffer.getWritePointer (c);
        for (int i = 0; i < n; ++i) if (! (std::abs (d[i]) < 1.0e4f)) d[i] = 0.0f;
    }

    const float from = powerSmooth.getCurrentValue();
    powerSmooth.setTargetValue (pPower->load() > 0.5f ? 1.0f : 0.0f);
    const float to = powerSmooth.skip (n);
    if (from == 0.0f && to == 0.0f) return;                                        // off: the signal passes untouched

    for (int c = 0; c < 2; ++c) dryBuf.copyFrom (c, 0, buffer, c, 0, n);

    dl1::Params p;
    p.mode = juce::jlimit (0, dl1::kNumModes - 1, (int) std::lround (pMode->load()));
    p.variant = juce::jlimit (0, dl1::variantCount (p.mode) - 1, (int) std::lround (pVariant->load()));
    p.sync = pSync->load() > 0.5f; p.hold = pHold->load() > 0.5f;
    p.division = (int) std::lround (pDivision->load());
    p.bpm = 120.0f;                                                                // the host tempo (120 when the host gives none)
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) p.bpm = (float) juce::jlimit (20.0, 400.0, *b);
    bpmNow.store (p.bpm);
    p.timeMs = pTime->load(); p.feedback = pFeedback->load() * 0.01f; p.lowHz = pLow->load(); p.highHz = pHigh->load();
    p.modRate = pModRate->load(); p.modDepth = pModDepth->load() * 0.01f; p.spread = pSpread->load() * 0.01f; p.mix = pMix->load() * 0.01f;
    p.digFactor = pDigFactor->load() * 0.01f; p.digDirect = pDigDirect->load() * 0.01f; p.digEffect = pDigEffect->load() * 0.01f; p.digInput = pDigInput->load() * 0.01f;
    p.anaBlend = pAnaBlend->load() * 0.01f; p.anaDepth = pAnaDepth->load() * 0.01f; p.anaGrit = pAnaGrit->load() * 0.01f;
    p.tapeWow = pTapeWow->load() * 0.01f; p.tapeSat = pTapeSat->load() * 0.01f; p.tapeAge = pTapeAge->load() * 0.01f; p.tapeSpacing = pTapeSpacing->load() * 0.01f;
    p.dualRatio = pDualTimeB->load(); p.dualFbB = pDualFbB->load() * 0.01f; p.dualLevelA = pDualLevelA->load() * 0.01f; p.dualLevelB = pDualLevelB->load() * 0.01f;
    p.modVoice = juce::jlimit (0, dl1::kNumVoices - 1, (int) std::lround (pModVoice->load())); p.modVariation = pModVariation->load() * 0.01f; p.modVolume = pModVolume->load() * 0.01f;
    p.dualHighB = pDualHighB->load(); p.dualPingA = pDualPingA->load() > 0.5f; p.dualPingB = pDualPingB->load() > 0.5f;
    engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n, p, juce::Decibels::decibelsToGain (pLevel->load()));

    if (from != 1.0f || to != 1.0f)                                                // power switch: crossfade to the untouched signal
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

juce::AudioProcessorEditor* DL1AudioProcessor::createEditor() { return new DL1AudioProcessorEditor (*this); }

void DL1AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void DL1AudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DL1AudioProcessor(); }
