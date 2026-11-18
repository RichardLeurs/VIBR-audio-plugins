// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "plugin/ParamLayout.h"
#include "PluginEditor.h"

namespace
{
using vibr::unitAttrs;
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout SF1AudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    const auto pct = [&layout] (const char* id, const char* name, float def) { vibr::addPercent (layout, id, name, def); };
    const auto skewed = [] (float lo, float hi, float step, float centre) { return vibr::skewedRange (lo, hi, step, centre); };
    vibr::addPower (layout);
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "freeze", 1 }, "Freeze (all bands)", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "fzLow", 1 }, "Freeze Low", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "fzLowMid", 1 }, "Freeze Low Mid", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "fzHighMid", 1 }, "Freeze High Mid", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "fzHigh", 1 }, "Freeze High", false));
    pct ("smear", "Smear", 0.0f);
    pct ("blur", "Blur", 0.0f);
    pct ("feedback", "Feedback", 0.0f);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "shift", 1 }, "Shift", NormalisableRange<float> (-1000.0f, 1000.0f, 1.0f), 0.0f, unitAttrs ("Hz", 0)));
    pct ("spread", "Spread", 0.0f);
    pct ("ring", "Ring", 0.0f);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "ringFreq", 1 }, "Ring Frequency", skewed (20.0f, 2000.0f, 1.0f, 200.0f), 200.0f, unitAttrs ("Hz", 0)));
    pct ("drift", "Drift", 0.0f);
    pct ("morph", "Morph", 0.0f);
    pct ("win", "Window", 0.0f);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "winCentre", 1 }, "Window Centre", skewed (40.0f, 12000.0f, 1.0f, 800.0f), 800.0f, unitAttrs ("Hz", 0)));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "winWidth", 1 }, "Window Width", NormalisableRange<float> (0.15f, 3.0f, 0.01f), 1.0f, unitAttrs ("oct", 2)));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "scan", 1 }, "Scan", skewed (0.0f, 4.0f, 0.01f, 0.5f), 0.0f, unitAttrs ("Hz", 2)));
    pct ("mix", "Mix", 60.0f);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "level", 1 }, "Level", NormalisableRange<float> (-24.0f, 12.0f, 1.0f), 0.0f, unitAttrs ("dB", 0)));
    // the mode HARMONY (the PT-1 harmonizer: four voices, key and scale aware); added after the first release, version hint 2. MIX and LEVEL are the shared ones.
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 2 }, "Mode", StringArray { "Fold", "Harmony" }, 0));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "hIntel", 2 }, "Harmony: Intelligent", true));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "hRoot", 2 }, "Harmony: Key", StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "hScale", 2 }, "Harmony: Scale", StringArray { "Major", "Minor", "Major pentatonic", "Minor pentatonic", "Dorian", "Whole tone" }, 0));
    static const float hLvl[pt1::kNumVoices] = { 70, 60, 50, 50 }, hDet[pt1::kNumVoices] = { 56, 44, 58, 42 };
    for (int v = 0; v < pt1::kNumVoices; ++v)
    {
        const String n (v + 1);
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "hInt" + n, 2 }, "Harmony: Interval " + n, NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f * pt1::intervalValue (pt1::chordSemis (0, v))));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "hDet" + n, 2 }, "Harmony: Detune " + n, NormalisableRange<float> (0.0f, 100.0f, 1.0f), hDet[v]));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "hLvl" + n, 2 }, "Harmony: Level " + n, NormalisableRange<float> (0.0f, 100.0f, 1.0f), hLvl[v], unitAttrs ("%", 0)));
    }
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "hSpread", 2 }, "Harmony: Spread", NormalisableRange<float> (0.0f, 100.0f, 1.0f), 70.0f, unitAttrs ("%", 0)));
    return layout;
}

namespace
{
// the factory presets (values are plain host values)
std::vector<vibr::PresetManager::Factory> sf1FactoryPresets()
{
    using Values = std::vector<std::pair<juce::String, float>>;
    const auto harmony = [] (int chord, Values extra)                                       // the mode HARMONY with the four intervals of a PT-1 chord
    {
        Values v { { "mode", 1 } };
        for (int i = 0; i < pt1::kNumVoices; ++i) v.push_back ({ "hInt" + juce::String (i + 1), 100.0f * pt1::intervalValue (pt1::chordSemis (chord, i)) });
        for (auto& e : extra) v.push_back (e);
        bool hasMix = false; for (auto& e : v) hasMix = hasMix || e.first == "mix";
        if (! hasMix) v.push_back ({ "mix", 50 });
        return v;
    };
    return {
        { "Init", "Everything neutral: the dry sound, one frame late", { { "mix", 100 } } },
        { "Frozen Chord", "Play a chord, press FREEZE and it holds", { { "freeze", 1 }, { "smear", 25 }, { "mix", 100 } } },
        { "Glass Smear", "A bright, washed out copy of what you play", { { "smear", 70 }, { "blur", 45 }, { "mix", 65 } } },
        { "Metal Shift", "Every partial moves up by a fixed number of Hz", { { "shift", 180 }, { "spread", 40 }, { "feedback", 30 }, { "mix", 70 } } },
        { "Alien Ring", "Inharmonic sidebands from a low ring modulator", { { "ring", 70 }, { "ringFreq", 130 }, { "shift", -60 }, { "mix", 75 } } },
        { "Spectral Echo", "The output comes back in, shifted a little each time", { { "feedback", 62 }, { "shift", 35 }, { "smear", 30 }, { "mix", 60 } } },
        { "Moving Window", "A narrow window scans through the spectrum", { { "win", 85 }, { "winCentre", 900 }, { "winWidth", 0.5f }, { "scan", 0.45f }, { "spread", 60 }, { "mix", 100 } } },
        { "Low Hold", "Only the low band is frozen, the highs stay live", { { "fzLow", 1 }, { "smear", 20 }, { "mix", 100 } } },
        { "Drift Cloud", "A frozen spectrum whose partials wander", { { "freeze", 1 }, { "drift", 70 }, { "blur", 40 }, { "spread", 50 }, { "mix", 100 } } },
        { "Morph Pad", "Capture a sound, then morph the live input towards it", { { "morph", 60 }, { "smear", 50 }, { "blur", 25 }, { "mix", 90 } } },
        // the mode HARMONY (the PT-1 presets; the PT-1 default MIX was 50)
        { "Open Fifths", "Stacked fifths, wide and hollow", harmony (1, { { "mix", 45 }, { "hSpread", 85 } }) },
        { "Suspended Air", "Suspended chord, light and open", harmony (2, { { "mix", 40 }, { "hSpread", 90 } }) },
        { "Tight Cluster", "Close notes, free of the key", harmony (3, { { "hIntel", 0 }, { "mix", 35 }, { "hSpread", 60 } }) },
        { "Octave Choir", "Octaves up and down", harmony (4, { { "mix", 55 }, { "hSpread", 95 }, { "hDet1", 54 }, { "hDet2", 46 }, { "hDet3", 53 }, { "hDet4", 47 } }) },
        { "Minor Triad", "A minor triad in A minor", harmony (0, { { "hRoot", 9 }, { "hScale", 1 }, { "mix", 55 }, { "hSpread", 80 } }) },
        { "Dorian Stack", "A suspended chord in D dorian", harmony (2, { { "hRoot", 2 }, { "hScale", 4 }, { "mix", 50 } }) },
        { "Pentatonic Shimmer", "Octaves in a pentatonic key", harmony (4, { { "hScale", 2 }, { "mix", 45 }, { "hSpread", 100 }, { "hLvl3", 40 }, { "hLvl4", 40 } }) },
    };
}
} // namespace

SF1AudioProcessor::SF1AudioProcessor()
    : vibr::LabProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true),
                          "SF1", createLayout(), "SF-1 Presets", sf1FactoryPresets(),
                          { "smear", "blur", "feedback", "shift", "spread", "ring", "ringFreq", "drift", "morph", "win", "winCentre", "winWidth", "scan", "mix", "level", "freeze", "hInt1", "hInt2", "hInt3", "hInt4", "hDet1", "hDet2", "hDet3", "hDet4", "hLvl1", "hLvl2", "hLvl3", "hLvl4", "hSpread" })
{
    pMode = apvts.getRawParameterValue ("mode"); pHIntel = apvts.getRawParameterValue ("hIntel"); pHRoot = apvts.getRawParameterValue ("hRoot"); pHScale = apvts.getRawParameterValue ("hScale");
    pHSpread = apvts.getRawParameterValue ("hSpread");
    for (int v = 0; v < pt1::kNumVoices; ++v)
    {
        const juce::String n (v + 1);
        pHInt[v] = apvts.getRawParameterValue ("hInt" + n); pHDet[v] = apvts.getRawParameterValue ("hDet" + n); pHLvl[v] = apvts.getRawParameterValue ("hLvl" + n);
    }
    pPower = apvts.getRawParameterValue ("power");
    pFreeze = apvts.getRawParameterValue ("freeze");
    pFz[0] = apvts.getRawParameterValue ("fzLow");
    pFz[1] = apvts.getRawParameterValue ("fzLowMid");
    pFz[2] = apvts.getRawParameterValue ("fzHighMid");
    pFz[3] = apvts.getRawParameterValue ("fzHigh");
    pSmear = apvts.getRawParameterValue ("smear");
    pBlur = apvts.getRawParameterValue ("blur");
    pFeedback = apvts.getRawParameterValue ("feedback");
    pShift = apvts.getRawParameterValue ("shift");
    pSpread = apvts.getRawParameterValue ("spread");
    pRing = apvts.getRawParameterValue ("ring");
    pRingFreq = apvts.getRawParameterValue ("ringFreq");
    pDrift = apvts.getRawParameterValue ("drift");
    pMorph = apvts.getRawParameterValue ("morph");
    pWin = apvts.getRawParameterValue ("win");
    pWinCentre = apvts.getRawParameterValue ("winCentre");
    pWinWidth = apvts.getRawParameterValue ("winWidth");
    pScan = apvts.getRawParameterValue ("scan");
    pMix = apvts.getRawParameterValue ("mix");
    pLevel = apvts.getRawParameterValue ("level");

    midiMap.setDefault ("morph", 1);
    midiMap.setDefault ("smear", 11);
    midiMap.setDefault ("freeze", 64);
}

bool SF1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void SF1AudioProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare ((float) sampleRate);
    setLatencySamples (engine.latency());                              // one frame, the same in every setting and with the power switch off
}

void SF1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    midiMap.process (midi);
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0 || buffer.getNumChannels() < 2) return;

    sf1::Params p;
    const bool all = pFreeze->load() > 0.5f;
    for (int b = 0; b < sf1::kBands; ++b) p.freeze[b] = all || pFz[b]->load() > 0.5f;
    p.smear = pSmear->load() * 0.01f;
    p.blur = pBlur->load() * 0.01f;
    p.feedback = pFeedback->load() * 0.01f;
    p.shiftHz = pShift->load();
    p.spread = pSpread->load() * 0.01f;
    p.ring = pRing->load() * 0.01f;
    p.ringHz = pRingFreq->load();
    p.drift = pDrift->load() * 0.01f;
    p.morph = pMorph->load() * 0.01f;
    p.window = pWin->load() * 0.01f;
    p.windowHz = pWinCentre->load();
    p.windowWidthOct = pWinWidth->load();
    p.scanHz = pScan->load();
    p.mode = juce::jlimit (0, 1, (int) std::lround (pMode->load()));
    p.harm.intelligent = pHIntel->load() > 0.5f; p.harm.root = juce::jlimit (0, 11, (int) std::lround (pHRoot->load())); p.harm.scale = juce::jlimit (0, 5, (int) std::lround (pHScale->load()));
    p.harm.spread = pHSpread->load() * 0.01f;
    for (int v = 0; v < pt1::kNumVoices; ++v)
    {
        p.harm.interval[v] = pt1::intervalSemis (pHInt[v]->load() * 0.01f); p.harm.detune[v] = pt1::detuneCents (pHDet[v]->load() * 0.01f); p.harm.level[v] = pHLvl[v]->load() * 0.01f;
    }
    engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n, p, pMix->load() * 0.01f, juce::Decibels::decibelsToGain (pLevel->load()), pPower->load() > 0.5f);
    outMeter.measure (buffer, n);
}

juce::AudioProcessorEditor* SF1AudioProcessor::createEditor() { return new SF1AudioProcessorEditor (*this); }


juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SF1AudioProcessor(); }
