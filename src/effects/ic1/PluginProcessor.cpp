// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "plugin/ParamLayout.h"
#include "PluginEditor.h"

namespace
{
using vibr::unitAttrs;
const char* const kLevelIds[ic1::kNodes] = { "f1", "f2", "f3", "f4" };
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout IC1AudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    const auto pct = [&layout] (const char* id, const char* name, float def) { vibr::addPercent (layout, id, name, def); };
    const auto skewed = [] (float lo, float hi, float step, float centre) { return vibr::skewedRange (lo, hi, step, centre); };
    vibr::addPower (layout);
    StringArray routes, injects, filters;
    for (int i = 0; i < ic1::kRoutes; ++i) routes.add (ic1::routeName (i));
    for (int i = 0; i < 3; ++i) { injects.add (ic1::injectName (i)); filters.add (ic1::filterName (i)); }
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "route", 1 }, "Routing", routes, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "inject", 1 }, "Input Paths", injects, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "filter", 1 }, "Filter Type", filters, 0));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "time", 1 }, "Time", skewed (5.0f, 1500.0f, 1.0f, 150.0f), 300.0f, unitAttrs ("ms", 0)));
    pct ("spread", "Spread", 50.0f);
    for (int i = 0; i < ic1::kNodes; ++i) pct (kLevelIds[i], ("Feedback " + String (i + 1)).toRawUTF8(), 70.0f);
    pct ("cross", "Cross-Feedback", 0.0f);
    pct ("regen", "Regeneration", 85.0f);
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "selfosc", 1 }, "Self-Oscillation (regeneration above 100 %)", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "hold", 1 }, "Hold (endless sustain)", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "cutoff", 1 }, "Cutoff", skewed (40.0f, 16000.0f, 1.0f, 1500.0f), 4000.0f, unitAttrs ("Hz", 0)));
    pct ("res", "Resonance", 20.0f);
    pct ("tilt", "Tilt", 0.0f);
    pct ("phase", "Phase", 0.0f);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "rate", 1 }, "Mod Rate", skewed (0.02f, 8.0f, 0.01f, 0.5f), 0.3f, unitAttrs ("Hz", 2)));
    pct ("depth", "Mod Depth", 0.0f);
    pct ("drive", "Drive", 0.0f);
    pct ("mix", "Mix", 50.0f);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "level", 1 }, "Level", NormalisableRange<float> (-24.0f, 12.0f, 1.0f), 0.0f, unitAttrs ("dB", 0)));
    return layout;
}

namespace
{
// the factory presets (values are plain host values; route 0 PARALLEL .. 5 STAR, inject 0 PARALLEL / 1 SERIES / 2 SPLIT, filter 0 LP / 1 BP / 2 HP)
std::vector<vibr::PresetManager::Factory> ic1FactoryPresets()
{
    return {
        { "Init", "Four parallel echoes with a dark loop", {  } },
        { "Dub Echo", "Warm, filtered repeats that fade slowly", { { "time", 420 }, { "f1", 62 }, { "f2", 62 }, { "f3", 62 }, { "f4", 62 }, { "regen", 82 }, { "cutoff", 2200 }, { "res", 12 }, { "drive", 12 }, { "mix", 45 } } },
        { "Ping-Pong Ring", "Left and right pass the sound back and forth", { { "route", 2 }, { "inject", 2 }, { "time", 320 }, { "spread", 0 }, { "cross", 0 }, { "regen", 88 }, { "cutoff", 5000 }, { "mix", 50 } } },
        { "Series Cascade", "A ring of four delays, one feeding the next", { { "route", 1 }, { "inject", 1 }, { "time", 130 }, { "spread", 70 }, { "regen", 92 }, { "res", 40 }, { "cutoff", 3200 }, { "mix", 55 } } },
        { "Resonant Mesh", "Every node feeds every node, through a band-pass", { { "route", 4 }, { "filter", 1 }, { "time", 90 }, { "spread", 80 }, { "cutoff", 1400 }, { "res", 70 }, { "regen", 90 }, { "mix", 55 } } },
        { "Star Bloom", "One hub node scatters the sound to the others", { { "route", 5 }, { "time", 240 }, { "spread", 60 }, { "phase", 55 }, { "rate", 0.2 }, { "depth", 30 }, { "regen", 86 }, { "mix", 50 } } },
        { "Phase Garden", "All-pass stages and a slow modulation inside the loop", { { "route", 3 }, { "time", 200 }, { "phase", 85 }, { "rate", 0.15 }, { "depth", 55 }, { "tilt", 40 }, { "regen", 88 }, { "cutoff", 3000 }, { "mix", 55 } } },
        { "Self Oscillator", "Press EXCITE: the circuit sings by itself", { { "route", 4 }, { "selfosc", 1 }, { "regen", 100 }, { "drive", 30 }, { "res", 60 }, { "cutoff", 1800 }, { "time", 60 }, { "mix", 60 } } },
        { "Endless Hold", "HOLD keeps what is in the loop for ever", { { "hold", 1 }, { "time", 500 }, { "spread", 60 }, { "cutoff", 6000 }, { "mix", 60 } } },
        { "Metal Tubes", "Very short delays turn the loop into a resonating tube", { { "route", 4 }, { "filter", 1 }, { "time", 9 }, { "spread", 100 }, { "regen", 96 }, { "res", 80 }, { "cutoff", 900 }, { "drive", 15 }, { "mix", 65 } } },
    };
}
} // namespace

IC1AudioProcessor::IC1AudioProcessor()
    : vibr::LabProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true),
                          "IC1", createLayout(), "IC-1 Presets", ic1FactoryPresets(),
                          { "time", "spread", "f1", "f2", "f3", "f4", "cross", "regen", "cutoff", "res", "phase", "depth", "drive", "mix", "level", "hold" })
{
    pPower = apvts.getRawParameterValue ("power");
    pRoute = apvts.getRawParameterValue ("route");
    pInject = apvts.getRawParameterValue ("inject");
    pFilter = apvts.getRawParameterValue ("filter");
    pTime = apvts.getRawParameterValue ("time");
    pSpread = apvts.getRawParameterValue ("spread");
    for (int i = 0; i < ic1::kNodes; ++i) pF[i] = apvts.getRawParameterValue (kLevelIds[i]);
    pCross = apvts.getRawParameterValue ("cross");
    pRegen = apvts.getRawParameterValue ("regen");
    pSelfOsc = apvts.getRawParameterValue ("selfosc");
    pHold = apvts.getRawParameterValue ("hold");
    pCutoff = apvts.getRawParameterValue ("cutoff");
    pRes = apvts.getRawParameterValue ("res");
    pTilt = apvts.getRawParameterValue ("tilt");
    pPhase = apvts.getRawParameterValue ("phase");
    pRate = apvts.getRawParameterValue ("rate");
    pDepth = apvts.getRawParameterValue ("depth");
    pDrive = apvts.getRawParameterValue ("drive");
    pMix = apvts.getRawParameterValue ("mix");
    pLevel = apvts.getRawParameterValue ("level");

    midiMap.setDefault ("regen", 1);
    midiMap.setDefault ("cutoff", 11);
    midiMap.setDefault ("hold", 64);
}

bool IC1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void IC1AudioProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare ((float) sampleRate);                               // no latency: the dry sound passes at once
}

void IC1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    midiMap.process (midi);
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0 || buffer.getNumChannels() < 2) return;

    ic1::Params p;
    p.route = juce::jlimit (0, ic1::kRoutes - 1, (int) std::lround (pRoute->load()));
    p.inject = juce::jlimit (0, 2, (int) std::lround (pInject->load()));
    p.filter = juce::jlimit (0, 2, (int) std::lround (pFilter->load()));
    p.timeMs = pTime->load();
    p.spread = pSpread->load() * 0.01f;
    for (int i = 0; i < ic1::kNodes; ++i) p.level[i] = pF[i]->load() * 0.01f;
    p.cross = pCross->load() * 0.01f;
    p.regen = pRegen->load() * 0.01f;
    p.selfOsc = pSelfOsc->load() > 0.5f;
    p.hold = pHold->load() > 0.5f;
    p.cutoff = pCutoff->load();
    p.res = pRes->load() * 0.01f;
    p.tilt = pTilt->load() * 0.01f;
    p.phase = pPhase->load() * 0.01f;
    p.rate = pRate->load();
    p.depth = pDepth->load() * 0.01f;
    p.drive = pDrive->load() * 0.01f;
    engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n, p, pMix->load() * 0.01f, juce::Decibels::decibelsToGain (pLevel->load()), pPower->load() > 0.5f);
    outMeter.measure (buffer, n);
}

juce::AudioProcessorEditor* IC1AudioProcessor::createEditor() { return new IC1AudioProcessorEditor (*this); }


juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new IC1AudioProcessor(); }
