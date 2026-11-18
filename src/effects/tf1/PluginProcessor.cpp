// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "plugin/ParamLayout.h"
#include "PluginEditor.h"

namespace
{
using vibr::unitAttrs;
const char* const kStepIds[tf1::kSteps] = { "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11", "s12", "s13", "s14", "s15", "s16" };
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout TF1AudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    const auto pct = [&layout] (const char* id, const char* name, float def) { vibr::addPercent (layout, id, name, def); };
    vibr::addPower (layout);
    StringArray ops; for (int o = 0; o < tf1::kOps; ++o) ops.add (tf1::opName (o));
    for (int s = 0; s < tf1::kSteps; ++s)
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { kStepIds[s], 1 }, "Step " + String (s + 1), ops, 0));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "length", 1 }, "Pattern Length", 1, tf1::kSteps, tf1::kSteps));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "division", 1 }, "Step Division", StringArray { "1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T" }, 2));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "hold", 1 }, "Hold (stop recording the buffer)", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "tempo", 1 }, "Tempo (without a host tempo)", NormalisableRange<float> (40.0f, 240.0f, 1.0f), 120.0f, unitAttrs ("bpm", 0)));
    pct ("chance", "Chance", 0.0f);
    pct ("loop", "Loop", 50.0f);
    pct ("scramble", "Scramble", 50.0f);
    pct ("pitch", "Pitch", 50.0f);
    pct ("stretch", "Stretch", 50.0f);
    pct ("crush", "Crush", 0.0f);
    pct ("mix", "Mix", 100.0f);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "level", 1 }, "Level", NormalisableRange<float> (-24.0f, 12.0f, 1.0f), 0.0f, unitAttrs ("dB", 0)));
    // the Time Lab: the modes SCRUB (the RV-1) and GRAINS (the GR-1); added after the first release, version hint 2
    const auto pct2 = [&layout] (const char* id, const char* name, float def) { layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 2 }, name, NormalisableRange<float> (0.0f, 100.0f, 1.0f), def, unitAttrs ("%", 0))); };
    const auto flag2 = [&layout] (const char* id, const char* name) { layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 2 }, name, false)); };
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 2 }, "Mode", StringArray { "Steps", "Scrub", "Grains" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "scrubMode", 2 }, "Scrub: Mode", StringArray { "Reverse", "Scrub", "Stutter", "Glitch", "Repeat" }, 0));
    flag2 ("scrubFreeze", "Scrub: Freeze");
    flag2 ("scrubSync", "Scrub: Sync (Length follows the host tempo)");
    pct2 ("scrubLength", "Scrub: Length", 50.0f);
    pct2 ("scrubSpeed", "Scrub: Speed", 50.0f);
    pct2 ("scrubSmooth", "Scrub: Smooth", 40.0f);
    pct2 ("scrubRandom", "Scrub: Random", 30.0f);
    pct2 ("scrubRepeats", "Scrub: Repeats", 50.0f);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "grainShape", 2 }, "Grains: Envelope", StringArray { "Soft", "Sharp", "Percussive", "Swell" }, 0));
    flag2 ("grainFreeze", "Grains: Freeze");
    flag2 ("grainSync", "Grains: Sync (Density follows the host tempo)");
    pct2 ("grainSize", "Grains: Size", 50.0f);
    pct2 ("grainDensity", "Grains: Density", 55.0f);
    pct2 ("grainPosition", "Grains: Position", 20.0f);
    pct2 ("grainSpray", "Grains: Spray", 30.0f);
    pct2 ("grainPitch", "Grains: Pitch", 50.0f);
    pct2 ("grainRandom", "Grains: Random", 0.0f);
    pct2 ("grainReverse", "Grains: Reverse", 0.0f);
    pct2 ("grainSpread", "Grains: Spread", 60.0f);
    return layout;
}

namespace
{
// the factory presets (values are plain host values; the steps s1 .. s16 hold an operation: 0 PLAY, 1 REPEAT, 2 REVERSE, 3 SCRAMBLE, 4 PITCH, 5 STRETCH, 6 MUTE)
std::vector<vibr::PresetManager::Factory> tf1FactoryPresets()
{
    return {
        { "Init", "Every step plays: the dry sound", {  } },
        { "Stutter Gate", "A short repeat on every fourth step", { { "s2", 1 }, { "s6", 1 }, { "s10", 1 }, { "s14", 1 }, { "loop", 35 } } },
        { "Reverse Fills", "The last step of each beat plays backwards", { { "s4", 2 }, { "s8", 2 }, { "s12", 2 }, { "s16", 2 } } },
        { "Scramble Bar", "Slices of the bar before, in a new order", { { "s2", 3 }, { "s4", 3 }, { "s6", 3 }, { "s8", 3 }, { "s10", 3 }, { "s12", 3 }, { "s14", 3 }, { "s16", 3 }, { "scramble", 80 } } },
        { "Pitch Jumps", "Random jumps up and down", { { "s3", 4 }, { "s7", 4 }, { "s11", 4 }, { "s15", 4 }, { "pitch", 80 }, { "division", 1 } } },
        { "Tape Slush", "Stretched fragments, slow and smeared", { { "s2", 5 }, { "s4", 5 }, { "s6", 5 }, { "s8", 5 }, { "s10", 5 }, { "s12", 5 }, { "s14", 5 }, { "s16", 5 }, { "stretch", 100 }, { "division", 1 } } },
        { "Half-Time Chop", "Repeat, play, reverse, play on eighths", { { "s1", 1 }, { "s3", 2 }, { "s5", 1 }, { "s7", 2 }, { "s9", 1 }, { "s11", 2 }, { "s13", 1 }, { "s15", 2 }, { "division", 1 }, { "length", 16 } } },
        { "Glitch Storm", "Chance and crush decide", { { "chance", 60 }, { "crush", 35 }, { "mix", 100 } } },
        { "Bit Rubble", "Repeats and gaps, crushed hard", { { "s2", 1 }, { "s3", 6 }, { "s6", 1 }, { "s7", 6 }, { "s10", 1 }, { "s11", 6 }, { "s14", 1 }, { "s15", 6 }, { "crush", 75 }, { "loop", 20 } } },
        { "Random Beat", "A pattern that never repeats exactly", { { "s2", 1 }, { "s5", 2 }, { "s9", 3 }, { "s12", 4 }, { "chance", 45 }, { "pitch", 50 } } },
        { "Reverse Echo", "Short reversed slices", { { "mode", 1 }, { "scrubMode", 0 }, { "scrubLength", 45 }, { "scrubSpeed", 50 }, { "scrubSmooth", 50 }, { "scrubRepeats", 35 }, { "mix", 60 } } },
        { "Half-Speed Reverse", "Slow, dark reversed swells", { { "mode", 1 }, { "scrubMode", 0 }, { "scrubLength", 70 }, { "scrubSpeed", 25 }, { "scrubSmooth", 60 }, { "mix", 70 } } },
        { "Tape Scrub", "The read position wanders", { { "mode", 1 }, { "scrubMode", 1 }, { "scrubLength", 55 }, { "scrubRandom", 45 }, { "scrubRepeats", 40 }, { "mix", 70 } } },
        { "Scrub Stutter", "A short repeat", { { "mode", 1 }, { "scrubMode", 2 }, { "scrubLength", 20 }, { "scrubRandom", 10 }, { "scrubRepeats", 60 }, { "mix", 80 } } },
        { "Glitch Machine", "Chopped, random slices", { { "mode", 1 }, { "scrubMode", 3 }, { "scrubLength", 30 }, { "scrubRandom", 70 }, { "scrubRepeats", 60 }, { "mix", 85 } } },
        { "Beat Repeat", "A repeat locked to the tempo", { { "mode", 1 }, { "scrubMode", 4 }, { "scrubSync", 1 }, { "scrubLength", 50 }, { "scrubRepeats", 50 }, { "mix", 90 } } },
        { "Frozen Slice", "Freeze what just played", { { "mode", 1 }, { "scrubMode", 0 }, { "scrubFreeze", 1 }, { "scrubLength", 60 }, { "mix", 100 } } },
        { "Soft Cloud", "Long, smooth grains", { { "mode", 2 }, { "grainShape", 0 }, { "grainSize", 70 }, { "grainDensity", 60 }, { "grainPosition", 30 }, { "grainSpray", 40 }, { "grainSpread", 80 }, { "mix", 70 } } },
        { "Glass Dust", "Tiny bright grains, an octave up", { { "mode", 2 }, { "grainShape", 1 }, { "grainSize", 15 }, { "grainDensity", 80 }, { "grainPitch", 100 }, { "grainRandom", 25 }, { "grainSpray", 50 }, { "mix", 65 } } },
        { "Reverse Swell", "Grains played backwards", { { "mode", 2 }, { "grainShape", 3 }, { "grainSize", 65 }, { "grainDensity", 55 }, { "grainPosition", 45 }, { "grainReverse", 80 }, { "mix", 75 } } },
        { "Rain Drops", "Sparse percussive ticks", { { "mode", 2 }, { "grainShape", 2 }, { "grainSize", 25 }, { "grainDensity", 30 }, { "grainSpray", 60 }, { "grainRandom", 40 }, { "grainSpread", 90 }, { "mix", 60 } } },
        { "Deep Drift", "An octave down, wide and slow", { { "mode", 2 }, { "grainShape", 0 }, { "grainSize", 80 }, { "grainDensity", 45 }, { "grainPosition", 55 }, { "grainPitch", 0 }, { "grainSpread", 100 }, { "mix", 70 } } },
        { "Tempo Pulse", "Grains locked to the tempo", { { "mode", 2 }, { "grainShape", 1 }, { "grainSync", 1 }, { "grainSize", 40 }, { "grainDensity", 50 }, { "grainSpray", 10 }, { "mix", 80 } } },
        { "Frozen Moment", "Freeze what just played", { { "mode", 2 }, { "grainShape", 0 }, { "grainFreeze", 1 }, { "grainSize", 60 }, { "grainDensity", 70 }, { "grainSpray", 45 }, { "grainSpread", 85 }, { "mix", 100 } } },
    };
}
} // namespace

TF1AudioProcessor::TF1AudioProcessor()
    : vibr::LabProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true),
                          "TF1", createLayout(), "TF-1 Presets", tf1FactoryPresets(),
                          { "chance", "loop", "scramble", "pitch", "stretch", "crush", "tempo", "mix", "level", "hold", "scrubLength", "scrubSpeed", "scrubSmooth", "scrubRandom", "scrubRepeats", "grainSize", "grainDensity", "grainPosition", "grainSpray", "grainPitch", "grainRandom", "grainReverse", "grainSpread" })
{
    pPower = apvts.getRawParameterValue ("power");
    for (int s = 0; s < tf1::kSteps; ++s) pStep[s] = apvts.getRawParameterValue (kStepIds[s]);
    pLength = apvts.getRawParameterValue ("length");
    pDivision = apvts.getRawParameterValue ("division");
    pHold = apvts.getRawParameterValue ("hold");
    pTempo = apvts.getRawParameterValue ("tempo");
    pChance = apvts.getRawParameterValue ("chance");
    pLoop = apvts.getRawParameterValue ("loop");
    pScramble = apvts.getRawParameterValue ("scramble");
    pPitch = apvts.getRawParameterValue ("pitch");
    pStretch = apvts.getRawParameterValue ("stretch");
    pCrush = apvts.getRawParameterValue ("crush");
    pMix = apvts.getRawParameterValue ("mix");
    pLevel = apvts.getRawParameterValue ("level");

    pMode = apvts.getRawParameterValue ("mode"); pScrubMode = apvts.getRawParameterValue ("scrubMode"); pScrubFreeze = apvts.getRawParameterValue ("scrubFreeze"); pScrubSync = apvts.getRawParameterValue ("scrubSync");
    pScrubLength = apvts.getRawParameterValue ("scrubLength"); pScrubSpeed = apvts.getRawParameterValue ("scrubSpeed"); pScrubSmooth = apvts.getRawParameterValue ("scrubSmooth");
    pScrubRandom = apvts.getRawParameterValue ("scrubRandom"); pScrubRepeats = apvts.getRawParameterValue ("scrubRepeats");
    pGrainShape = apvts.getRawParameterValue ("grainShape"); pGrainFreeze = apvts.getRawParameterValue ("grainFreeze"); pGrainSync = apvts.getRawParameterValue ("grainSync");
    pGrainSize = apvts.getRawParameterValue ("grainSize"); pGrainDensity = apvts.getRawParameterValue ("grainDensity"); pGrainPosition = apvts.getRawParameterValue ("grainPosition");
    pGrainSpray = apvts.getRawParameterValue ("grainSpray"); pGrainPitch = apvts.getRawParameterValue ("grainPitch"); pGrainRandom = apvts.getRawParameterValue ("grainRandom");
    pGrainReverse = apvts.getRawParameterValue ("grainReverse"); pGrainSpread = apvts.getRawParameterValue ("grainSpread");

    midiMap.setDefault ("chance", 1);
    midiMap.setDefault ("loop", 11);
    midiMap.setDefault ("hold", 64);
}

bool TF1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void TF1AudioProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare ((float) sampleRate);                               // no latency: the dry sound passes at once
}

void TF1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    midiMap.process (midi);
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0 || buffer.getNumChannels() < 2) return;

    tf1::Params p;
    for (int s = 0; s < tf1::kSteps; ++s) p.ops[s] = juce::jlimit (0, tf1::kOps - 1, (int) std::lround (pStep[s]->load()));
    p.length = juce::jlimit (1, tf1::kSteps, (int) std::lround (pLength->load()));
    p.division = juce::jlimit (0, 5, (int) std::lround (pDivision->load()));
    p.hold = pHold->load() > 0.5f;
    p.chance = pChance->load() * 0.01f;
    p.loop = pLoop->load() * 0.01f;
    p.scramble = pScramble->load() * 0.01f;
    p.pitch = pPitch->load() * 0.01f;
    p.stretch = pStretch->load() * 0.01f;
    p.crush = pCrush->load() * 0.01f;
    p.mode = juce::jlimit (0, 2, (int) std::lround (pMode->load()));
    p.scrub.mode = juce::jlimit (0, rv1::kNumModes - 1, (int) std::lround (pScrubMode->load())); p.scrub.freeze = pScrubFreeze->load() > 0.5f; p.scrub.sync = pScrubSync->load() > 0.5f;
    p.scrub.length = pScrubLength->load() * 0.01f; p.scrub.speed = pScrubSpeed->load() * 0.01f; p.scrub.smooth = pScrubSmooth->load() * 0.01f;
    p.scrub.random = pScrubRandom->load() * 0.01f; p.scrub.repeats = pScrubRepeats->load() * 0.01f;
    p.grain.shape = juce::jlimit (0, gr1::kNumShapes - 1, (int) std::lround (pGrainShape->load())); p.grain.freeze = pGrainFreeze->load() > 0.5f; p.grain.sync = pGrainSync->load() > 0.5f;
    p.grain.size = pGrainSize->load() * 0.01f; p.grain.density = pGrainDensity->load() * 0.01f; p.grain.position = pGrainPosition->load() * 0.01f; p.grain.spray = pGrainSpray->load() * 0.01f;
    p.grain.pitch = pGrainPitch->load() * 0.01f; p.grain.random = pGrainRandom->load() * 0.01f; p.grain.reverse = pGrainReverse->load() * 0.01f; p.grain.spread = pGrainSpread->load() * 0.01f;

    tf1::Host host;                                                    // the host tempo and position; the TEMPO knob when the host gives no tempo
    host.bpm = pTempo->load();
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) host.bpm = *b;
            if (auto q = pos->getPpqPosition()) { host.ppq = *q; host.playing = pos->getIsPlaying(); }
        }
    engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n, p, pMix->load() * 0.01f, juce::Decibels::decibelsToGain (pLevel->load()), pPower->load() > 0.5f, host);
    outMeter.measure (buffer, n);
}

juce::AudioProcessorEditor* TF1AudioProcessor::createEditor() { return new TF1AudioProcessorEditor (*this); }


juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TF1AudioProcessor(); }
