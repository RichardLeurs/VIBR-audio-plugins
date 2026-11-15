// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
using P = std::vector<std::pair<juce::String, float>>;
std::vector<vibr::PresetManager::Factory> ax1FactoryPresets()
{
    return {
        { "Init", "BRIT at its defaults", {} },
        { "Clean Glass", "A bright, open clean", P { { "ampType", 0 }, { "gain", 4 }, { "treble", 6 }, { "presence", 6 }, { "bright", 1 }, { "ambience", 2 }, { "gateOn", 0 } } },
        { "Tweed Edge", "A small amp just at the edge", P { { "ampType", 2 }, { "gain", 6 }, { "bass", 4 }, { "middle", 6 }, { "sag", 5 }, { "highCut", 6 } } },
        { "Blues Breakup", "Touch sensitive breakup", P { { "ampType", 1 }, { "gain", 6.5f }, { "middle", 6 }, { "presence", 4 }, { "sag", 5 }, { "ambience", 1 }, { "gateOn", 0 } } },
        { "British Crunch", "The BRIT voicing, medium gain", P { { "ampType", 5 }, { "gain", 6 }, { "middle", 6 }, { "presence", 6 } } },
        { "Classic Rock", "A warm, singing crunch", P { { "ampType", 6 }, { "gain", 7 }, { "middle", 6 }, { "presence", 6 }, { "sag", 4 } } },
        { "Hot Rod", "A hot-rodded high-gain rhythm", P { { "ampType", 9 }, { "gain", 7 }, { "treble", 6 }, { "middle", 6 }, { "bright", 1 }, { "gateThresh", 4.5f } } },
        { "Modern Lead", "A smooth modern lead with a boost", P { { "ampType", 11 }, { "gain", 7 }, { "middle", 6.5f }, { "boostOn", 1 }, { "boostDrive", 3 }, { "boostLevel", 6 }, { "ambience", 2 } } },
        { "Tight Rhythm", "Tight, fast palm mutes", P { { "ampType", 7 }, { "gain", 7 }, { "bass", 4 }, { "boostOn", 1 }, { "boostDrive", 2 }, { "boostTight", 8 }, { "gateThresh", 5 } } },
        { "Brutal Chug", "Low, tight and heavy", P { { "ampType", 12 }, { "gain", 7 }, { "bass", 6 }, { "middle", 3 }, { "deep", 1 }, { "boostOn", 1 }, { "boostTight", 8 }, { "gateThresh", 5 }, { "mic", 2 } } },
        { "Ultra Saw", "Maximum gain, buzzing and tight", P { { "ampType", 13 }, { "gain", 8 }, { "presence", 6 }, { "boostOn", 1 }, { "boostTight", 7 }, { "gateThresh", 6 }, { "mic", 2 } } },
        { "Doom Wall", "Loose, huge and slow", P { { "ampType", 10 }, { "gain", 8 }, { "bass", 8 }, { "middle", 4 }, { "treble", 4 }, { "resonance", 7 }, { "deep", 1 }, { "sag", 7 }, { "lowCut", 1 } } },
        { "Ambient Clean", "A clean amp in a big room", P { { "ampType", 0 }, { "gain", 3 }, { "presence", 6 }, { "bright", 1 }, { "ambience", 7 }, { "gateOn", 0 } } },
        { "Fat Bass Amp", "A bass guitar through a gritty amp", P { { "ampType", 4 }, { "gain", 4 }, { "bass", 8 }, { "treble", 4 }, { "resonance", 7 }, { "deep", 1 }, { "lowCut", 0 }, { "highCut", 5 }, { "gateOn", 0 } } },
        { "Boosted Solo", "A singing solo sound", P { { "ampType", 8 }, { "gain", 6 }, { "middle", 7 }, { "boostOn", 1 }, { "boostDrive", 5 }, { "boostLevel", 6 }, { "ambience", 3 } } },
    };
}

// host label and readable text for a float parameter
juce::AudioParameterFloatAttributes unitAttrs (const juce::String& unit, std::function<juce::String (float)> text)
{
    return juce::AudioParameterFloatAttributes()
        .withLabel (unit)
        .withStringFromValueFunction ([text] (float v, int) { return text (v); })
        .withValueFromStringFunction ([] (const juce::String& s) { return s.retainCharacters ("0123456789.-+").getFloatValue(); });
}
juce::String hzText (float hz) { return hz >= 1000.0f ? juce::String (hz / 1000.0f, 1) + " k" : juce::String ((int) std::lround (hz)); }
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout AX1AudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    const auto knob = [&layout] (const char* id, const char* name, float def, const String& unit = {}, std::function<String (float)> text = {})
    {
        if (text) layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 10.0f, 0.01f), def, unitAttrs (unit, text)));
        else layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 10.0f, 0.01f), def));
    };
    const auto flag = [&layout] (const char* id, const char* name, bool def)
    {
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def, AudioParameterBoolAttributes().withMeta (String (id) == "power")));
    };
    const auto dbText = [] (float v) { return String (v, 1); };
    flag ("power", "Power", true);
    layout.add (vibr::makeBypassParam());
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "inGain", 1 }, "Input", NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f, unitAttrs ("dB", dbText)));
    flag ("gateOn", "Gate On", true);
    knob ("gateThresh", "Gate Threshold", 4.0f, "dB", [] (float k) { return String (ax1::gateThresholdDb (k), 0); });
    knob ("gateRelease", "Gate Release", 3.0f, "ms", [] (float k) { return String ((int) std::lround (ax1::gateReleaseMs (k))); });
    flag ("boostOn", "Boost On", false);
    knob ("boostDrive", "Boost Drive", 2.0f);
    knob ("boostLevel", "Boost Level", 5.0f, "dB", [] (float k) { return String (ax1::bipolarDb (k), 1); });
    knob ("boostTight", "Boost Tight", 5.0f, "Hz", [] (float k) { return hzText (ax1::tightHz (k)); });
    StringArray names;
    for (int i = 0; i < ax1::kNumTypes; ++i) names.add (ax1::kTypes[i].name);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "ampType", 1 }, "Amp Type", names, 5));
    knob ("gain", "Gain", 5.0f);
    knob ("bass", "Bass", 5.0f);
    knob ("middle", "Middle", 5.0f);
    knob ("treble", "Treble", 5.0f);
    knob ("presence", "Presence", 5.0f, "dB", [] (float k) { return String (ax1::presenceDb (k), 1); });
    knob ("resonance", "Resonance", 5.0f, "dB", [] (float k) { return String (ax1::presenceDb (k), 1); });
    knob ("sag", "Sag", 3.0f);
    knob ("master", "Master", 5.0f, "dB", [] (float k) { return String (ax1::masterDb (k), 1); });
    flag ("bright", "Bright", false);
    flag ("deep", "Deep", false);
    flag ("cabOn", "Cab On", true);
    knob ("irALevel", "IR A Level", 7.0f, "dB", [] (float k) { return k <= 0.01f ? String ("-inf") : String ((k - 7.0f) * 3.0f, 1); });
    knob ("irBLevel", "IR B Level", 7.0f, "dB", [] (float k) { return k <= 0.01f ? String ("-inf") : String ((k - 7.0f) * 3.0f, 1); });
    knob ("lowCut", "Low Cut", 2.0f, "Hz", [] (float k) { return hzText (ax1::lowCutHz (k)); });
    knob ("highCut", "High Cut", 7.0f, "Hz", [] (float k) { return hzText (ax1::highCutHz (k)); });
    knob ("mic", "Mic", 3.0f);
    knob ("ambience", "Ambience", 0.0f);
    flag ("eqOn", "EQ On", false);
    knob ("eqLow", "EQ Low", 5.0f, "dB", [] (float k) { return String (ax1::bipolarDb (k), 1); });
    knob ("eqMid", "EQ Mid", 5.0f, "dB", [] (float k) { return String (ax1::bipolarDb (k), 1); });
    knob ("eqFreq", "EQ Mid Freq", 5.0f, "Hz", [] (float k) { return hzText (ax1::eqFreqHz (k)); });
    knob ("eqHigh", "EQ High", 5.0f, "dB", [] (float k) { return String (ax1::bipolarDb (k), 1); });
    knob ("out", "Output", 5.0f, "dB", [] (float k) { return k <= 0.01f ? String ("-inf") : String (ax1::outputDb (k), 1); });
    flag ("limiter", "Limiter", true);
    return layout;
}

AX1AudioProcessor::AX1AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "AX1", createLayout()),
      presets (*this, apvts, undoManager, "AX-1 Presets", ax1FactoryPresets(), { "power", "bypass" })
{
    const auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pPower = get ("power"); pInGain = get ("inGain"); pGateOn = get ("gateOn"); pGateThresh = get ("gateThresh"); pGateRelease = get ("gateRelease");
    pBoostOn = get ("boostOn"); pBoostDrive = get ("boostDrive"); pBoostLevel = get ("boostLevel"); pBoostTight = get ("boostTight"); pAmpType = get ("ampType");
    pGain = get ("gain"); pBass = get ("bass"); pMiddle = get ("middle"); pTreble = get ("treble"); pPresence = get ("presence"); pResonance = get ("resonance");
    pSag = get ("sag"); pMaster = get ("master"); pBright = get ("bright"); pDeep = get ("deep"); pCabOn = get ("cabOn"); pIrALevel = get ("irALevel"); pIrBLevel = get ("irBLevel");
    pLowCut = get ("lowCut"); pHighCut = get ("highCut"); pMic = get ("mic"); pAmbience = get ("ambience"); pEqOn = get ("eqOn"); pEqLow = get ("eqLow"); pEqMid = get ("eqMid");
    pEqFreq = get ("eqFreq"); pEqHigh = get ("eqHigh"); pOut = get ("out"); pLimiter = get ("limiter");
    midiMap.setDefault ("gain", 11);
    midiMap.setDefault ("presence", 1);
}

ax1::Params AX1AudioProcessor::readParams() const
{
    ax1::Params p;
    p.inGainDb = pInGain->load();
    p.gateOn = pGateOn->load() > 0.5f; p.gateThresh = pGateThresh->load(); p.gateRelease = pGateRelease->load();
    p.boostOn = pBoostOn->load() > 0.5f; p.boostDrive = pBoostDrive->load(); p.boostLevel = pBoostLevel->load(); p.boostTight = pBoostTight->load();
    p.type = juce::jlimit (0, ax1::kNumTypes - 1, (int) std::lround (pAmpType->load()));
    p.gain = pGain->load(); p.bass = pBass->load(); p.middle = pMiddle->load(); p.treble = pTreble->load(); p.presence = pPresence->load();
    p.resonance = pResonance->load(); p.sag = pSag->load(); p.master = pMaster->load();
    p.bright = pBright->load() > 0.5f; p.deep = pDeep->load() > 0.5f;
    p.cabOn = pCabOn->load() > 0.5f; p.irALevel = pIrALevel->load(); p.irBLevel = pIrBLevel->load();
    p.lowCut = pLowCut->load(); p.highCut = pHighCut->load(); p.mic = pMic->load(); p.ambience = pAmbience->load();
    p.eqOn = pEqOn->load() > 0.5f; p.eqLow = pEqLow->load(); p.eqMid = pEqMid->load(); p.eqFreq = pEqFreq->load(); p.eqHigh = pEqHigh->load();
    p.out = pOut->load(); p.limiter = pLimiter->load() > 0.5f;
    return p;
}

bool AX1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) && out == layouts.getMainInputChannelSet();
}

void AX1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    engine.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (engine.latency());
    powerSmooth.reset (sampleRate, 0.02);
    powerSmooth.setCurrentAndTargetValue (pPower->load() > 0.5f ? 1.0f : 0.0f);
    dryBuf.setSize (2, juce::jmax (samplesPerBlock, 8192));
    for (auto& d : powerDelay) d.assign (8192, 0.0f);
    delayPos = 0;
}

void AX1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    midiMap.process (midi);
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int channels = juce::jmin (buffer.getNumChannels(), 2);
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, n);
    if (n == 0 || channels == 0 || n > dryBuf.getNumSamples()) return;
    for (int c = 0; c < channels; ++c)
    {
        auto* d = buffer.getWritePointer (c);
        for (int i = 0; i < n; ++i) if (! (std::abs (d[i]) < 1.0e4f)) d[i] = 0.0f;
    }

    const int lat = engine.latency();                                              // the power switch crosses to the input delayed by the latency
    for (int c = 0; c < channels; ++c)
    {
        const auto* s = buffer.getReadPointer (c);
        for (int i = 0; i < n; ++i)
        {
            powerDelay[c][(size_t) ((delayPos + i) & 8191)] = s[i];
            dryBuf.setSample (c, i, powerDelay[c][(size_t) ((delayPos + i - lat) & 8191)]);
        }
    }
    delayPos = (delayPos + n) & 8191;

    const float from = powerSmooth.getCurrentValue();
    powerSmooth.setTargetValue (pPower->load() > 0.5f ? 1.0f : 0.0f);
    const float to = powerSmooth.skip (n);
    if (from == 0.0f && to == 0.0f) { for (int c = 0; c < channels; ++c) buffer.copyFrom (c, 0, dryBuf, c, 0, n); return; }

    engine.process (buffer.getArrayOfWritePointers(), channels, n, readParams());

    if (from != 1.0f || to != 1.0f)
        for (int c = 0; c < channels; ++c)
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

//==============================================================================
// the magnitude of an impulse response at 160 log spaced frequencies (dB, 0 dB = the mean level between 200 Hz and 4 kHz), for the CABINET display
bool AX1AudioProcessor::loadIrData (int slot, const juce::MemoryBlock& data, const juce::String& name, juce::String* error)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (std::make_unique<juce::MemoryInputStream> (data, false)));
    if (reader == nullptr || reader->lengthInSamples <= 0) { if (error) *error = "This file is not a WAV or AIFF impulse response."; return false; }
    const double seconds = (double) reader->lengthInSamples / reader->sampleRate;
    if (seconds > (double) kMaxIrSeconds + 0.05) { if (error) *error = "The impulse response is longer than 3 seconds."; return false; }

    const int len = (int) juce::jmin<juce::int64> (reader->lengthInSamples, 16384);
    juce::AudioBuffer<float> b ((int) juce::jmax<unsigned> (1u, reader->numChannels), len);
    reader->read (&b, 0, len, 0, true, true);
    std::array<float, kCurvePoints> curve {};
    double mean = 0.0; int meanN = 0;
    for (int k = 0; k < kCurvePoints; ++k)
    {
        const double f = 20.0 * std::pow (1000.0, (double) k / (double) (kCurvePoints - 1)), w = juce::MathConstants<double>::twoPi * f / reader->sampleRate;
        double power = 0.0;
        for (int c = 0; c < b.getNumChannels(); ++c)
        {
            double re = 0.0, im = 0.0; const auto* x = b.getReadPointer (c);
            for (int i = 0; i < len; ++i) { re += x[i] * std::cos (w * i); im -= x[i] * std::sin (w * i); }
            power += re * re + im * im;
        }
        const double d = 10.0 * std::log10 (power / (double) b.getNumChannels() + 1.0e-12);
        curve[(size_t) k] = (float) d;
        if (f >= 200.0 && f <= 4000.0) { mean += d; ++meanN; }
    }
    for (auto& v : curve) v -= (float) (mean / juce::jmax (1, meanN));

    {
        const juce::ScopedLock sl (irLock);
        irData[slot] = data; irName[slot] = name;
    }
    { const juce::SpinLock::ScopedLockType sl (curveLock); irCurve[slot] = curve; }
    irSeconds[slot].store ((float) seconds);
    engine.loadIr (slot, data.getData(), data.getSize());
    return true;
}

bool AX1AudioProcessor::loadImpulseResponse (int slot, const juce::File& file, juce::String* error)
{
    juce::MemoryBlock mb;
    if (slot < 0 || slot > 1 || ! file.existsAsFile() || ! file.loadFileAsData (mb) || mb.getSize() == 0)
    {
        if (error) *error = "The file could not be read.";
        return false;
    }
    return loadIrData (slot, mb, file.getFileNameWithoutExtension(), error);
}

void AX1AudioProcessor::clearImpulseResponse (int slot)
{
    if (slot < 0 || slot > 1) return;
    engine.clearIr (slot);
    irSeconds[slot].store (0.0f);
    const juce::ScopedLock sl (irLock);
    irData[slot].setSize (0);
    irName[slot] = {};
}

juce::AudioProcessorEditor* AX1AudioProcessor::createEditor() { return new AX1AudioProcessorEditor (*this); }

void AX1AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.appendChild (midiMap.toTree(), nullptr);
    {
        const juce::ScopedLock sl (irLock);
        for (int s = 0; s < 2; ++s)
            if (irData[s].getSize() > 0)
            {
                juce::ValueTree t (s == 0 ? "IR_A" : "IR_B");
                t.setProperty ("name", irName[s], nullptr);
                t.setProperty ("data", irData[s].toBase64Encoding(), nullptr);
                state.appendChild (t, nullptr);
            }
    }
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void AX1AudioProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto st = juce::ValueTree::fromXml (*xml);
    midiMap.fromTree (st.getChildWithName ("MIDIMAP"));
    st.removeChild (st.getChildWithName ("MIDIMAP"), nullptr);
    juce::ValueTree irTree[2] = { st.getChildWithName ("IR_A"), st.getChildWithName ("IR_B") };
    st.removeChild (irTree[0], nullptr);
    st.removeChild (irTree[1], nullptr);
    vibr::BypassLink::normalise (st);
    apvts.replaceState (st);
    for (int s = 0; s < 2; ++s)                                          // a state without IR children clears the slots
    {
        juce::MemoryBlock mb;
        if (irTree[s].isValid() && mb.fromBase64Encoding (irTree[s].getProperty ("data").toString()) && mb.getSize() > 0
            && loadIrData (s, mb, irTree[s].getProperty ("name").toString(), nullptr))
            continue;
        clearImpulseResponse (s);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AX1AudioProcessor(); }
