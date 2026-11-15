// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr const char* kLevelIds[tr08::kNumInst] = { "bdLevel", "sdLevel", "ltLevel", "mtLevel", "htLevel", "rsLevel",
                                                    "cpLevel", "cbLevel", "cyLevel", "ohLevel", "chLevel" };
constexpr const char* kTuneIds[3] = { "ltTune", "mtTune", "htTune" };
constexpr const char* kAltIds[5]  = { "ltConga", "mtConga", "htConga", "rsClaves", "cpMaracas" };

constexpr const char* kVarIds[tr08::kNumInst] = { "bdVar", "sdVar", "ltVar", "mtVar", "htVar", "rsVar", "cpVar", "cbVar", "cyVar", "ohVar", "chVar" };
constexpr const char* kVarNames[tr08::kNumInst] = { "BD", "SD", "LT", "MT", "HT", "RS", "CP", "CB", "CY", "OH", "CH" };

inline float taper (float knob) { const float v = knob * 0.1f; return v * v * 1.3f; }   // 0-10 knob -> gain

// GM-style drum map -> instrument index, -1 if unmapped
int noteToInst (int note)
{
    switch (note)
    {
        case 35: case 36:                 return tr08::BD;
        case 37:                          return tr08::RS;
        case 38: case 40:                 return tr08::SD;
        case 39:                          return tr08::CP;
        case 41: case 43:                 return tr08::LT;
        case 45: case 47:                 return tr08::MT;
        case 48: case 50:                 return tr08::HT;
        case 42: case 44:                 return tr08::CH;
        case 46:                          return tr08::OH;
        case 49: case 51: case 52: case 55: case 57: return tr08::CY;
        case 56:                          return tr08::CB;
        default:                          return -1;
    }
}

juce::PropertiesFile::Options settingsOptions()
{
    juce::PropertiesFile::Options o;
    o.applicationName = "TR-08 Drum Machine";
    o.folderName = "TR-08 Drum Machine";
    o.filenameSuffix = ".settings";
    o.osxLibrarySubFolder = "Application Support";
    return o;
}

// Decode the files a kit maps to into mono, peak-normalised samples (at most 5 s each).
std::unique_ptr<tr08::SampleSet> decodeKit (const tr08kit::Kit& kit)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    auto set = std::make_unique<tr08::SampleSet>();

    for (int slot = 0; slot < tr08kit::kNumSlots; ++slot)
    {
      for (const auto& path : kit.files[(size_t) slot])
      {
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (juce::File (juce::String::fromUTF8 (path.string().c_str()))));
        if (reader == nullptr || reader->sampleRate <= 0.0) continue;

        const auto length = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 5.0));
        if (length < 2) continue;

        juce::AudioBuffer<float> buf ((int) reader->numChannels, length);
        reader->read (&buf, 0, length, 0, true, true);

        set->slot[slot].emplace_back();
        auto& out = set->slot[slot].back();
        out.rate = (float) reader->sampleRate;
        out.data.assign ((size_t) length, 0.0f);
        float peak = 0.0f;
        for (int i = 0; i < length; ++i)
        {
            float s = 0.0f;
            for (int ch = 0; ch < buf.getNumChannels(); ++ch) s += buf.getSample (ch, i);
            s /= (float) buf.getNumChannels();
            out.data[(size_t) i] = s;
            peak = std::max (peak, std::abs (s));
        }
        if (peak > 1.0e-5f)
            for (auto& v : out.data) v *= 0.9f / peak;
        else
            set->slot[slot].pop_back();
      }
    }
    return set;
}
} // namespace

//==============================================================================
// Scans the samples folder and decodes the selected kit on its own thread, so the message and audio threads never wait on disk.
class TR08AudioProcessor::KitLoader : public juce::Thread
{
public:
    explicit KitLoader (TR08AudioProcessor& o) : juce::Thread ("TR-08 kit loader"), owner (o) {}

    void request (bool rescan)
    {
        { const juce::ScopedLock sl (lock); pendingRescan = pendingRescan || rescan; hasWork = true; }
        notify();
    }

    void run() override
    {
        while (! threadShouldExit())
        {
            bool work, rescan;
            { const juce::ScopedLock sl (lock); work = hasWork; rescan = pendingRescan; hasWork = pendingRescan = false; }
            if (work) owner.runJob (rescan);
            else      wait (-1);
        }
    }

private:
    TR08AudioProcessor& owner;
    juce::CriticalSection lock;
    bool hasWork = false, pendingRescan = false;
};

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout TR08AudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    const auto knob = [&] (const char* id, const char* name, float def)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 10.0f, 0.01f), def));
    };
    const auto toggle = [&] (const char* id, const char* name, bool def)
    {
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };
    const auto choice = [&] (const char* id, const char* name, int hi, int def)
    {
        layout.add (std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, name, 0, hi, def));
    };

    knob ("accent", "Accent Level", 7.0f);
    knob ("bdLevel", "Bass Drum Level", 8.0f);  knob ("bdTone", "Bass Drum Tone", 4.0f);  knob ("bdDecay", "Bass Drum Decay", 4.0f);
    knob ("sdLevel", "Snare Level", 7.0f);      knob ("sdTone", "Snare Tone", 5.0f);      knob ("sdSnappy", "Snare Snappy", 6.0f);
    knob ("ltLevel", "Low Tom/Conga Level", 6.0f); knob ("ltTune", "Low Tom/Conga Tuning", 5.0f);
    knob ("mtLevel", "Mid Tom/Conga Level", 6.0f); knob ("mtTune", "Mid Tom/Conga Tuning", 5.0f);
    knob ("htLevel", "Hi Tom/Conga Level", 6.0f);  knob ("htTune", "Hi Tom/Conga Tuning", 5.0f);
    knob ("rsLevel", "Rimshot/Claves Level", 6.0f);
    knob ("cpLevel", "Hand Clap/Maracas Level", 6.0f);
    knob ("cbLevel", "Cowbell Level", 5.0f);
    knob ("cyLevel", "Cymbal Level", 5.0f); knob ("cyTone", "Cymbal Tone", 5.0f); knob ("cyDecay", "Cymbal Decay", 5.0f);
    knob ("ohLevel", "Open Hi-hat Level", 5.0f); knob ("ohDecay", "Open Hi-hat Decay", 4.0f);
    knob ("chLevel", "Closed Hi-hat Level", 6.0f);

    toggle ("ltConga", "Low Conga", false);  toggle ("mtConga", "Mid Conga", false);  toggle ("htConga", "Hi Conga", false);
    toggle ("rsClaves", "Claves", false);    toggle ("cpMaracas", "Maracas", false);

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "tempo", 1 }, "Tempo", NormalisableRange<float> (40.0f, 300.0f, 0.1f), 120.0f, AudioParameterFloatAttributes().withLabel ("bpm")));
    knob ("fine", "Fine", 5.0f);
    toggle ("run", "Start/Stop", false);
    choice ("variation", "Basic Variation (A/AB/B)", 2, 0);
    toggle ("shuffle", "Shuffle", false);
    choice ("mode", "Pattern Write", 5, 1);
    choice ("inst", "Instrument Select", 11, 1);
    choice ("measures", "Measures / Auto Fill In (Manual,16,12,8,4,2)", 5, 0);
    choice ("fillPart", "I/F Variation (A/B)", 1, 1);
    choice ("preScale", "Pre-Scale (8th triplet, 16th triplet, 16th, 32nd)", 3, 2);
    for (int i = 0; i < tr08::kNumInst; ++i)
        layout.add (std::make_unique<AudioParameterInt> (ParameterID { kVarIds[i], 1 }, juce::String (kVarNames[i]) + " Sample Variant", 0, 127, 1));
    return layout;
}

TR08AudioProcessor::TR08AudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TR08", createLayout())
{
    for (int i = 0; i < tr08::kNumInst; ++i) pLevel[i] = apvts.getRawParameterValue (kLevelIds[i]);
    for (int i = 0; i < 3; ++i) pTune[i] = apvts.getRawParameterValue (kTuneIds[i]);
    for (int i = 0; i < tr08::kNumInst; ++i) pVar[i] = apvts.getRawParameterValue (kVarIds[i]);
    for (int i = 0; i < 5; ++i) pAlt[i] = apvts.getRawParameterValue (kAltIds[i]);
    pBdTone = apvts.getRawParameterValue ("bdTone");   pBdDecay = apvts.getRawParameterValue ("bdDecay");
    pSdTone = apvts.getRawParameterValue ("sdTone");   pSdSnappy = apvts.getRawParameterValue ("sdSnappy");
    pCyTone = apvts.getRawParameterValue ("cyTone");   pCyDecay = apvts.getRawParameterValue ("cyDecay");
    pOhDecay = apvts.getRawParameterValue ("ohDecay"); pAccent = apvts.getRawParameterValue ("accent");
    pTempo = apvts.getRawParameterValue ("tempo");     pFine = apvts.getRawParameterValue ("fine");
    pRun = apvts.getRawParameterValue ("run");         pVariation = apvts.getRawParameterValue ("variation");
    pShuffle = apvts.getRawParameterValue ("shuffle");
    pMeasures = apvts.getRawParameterValue ("measures"); pFillPart = apvts.getRawParameterValue ("fillPart");
    pPreScale = apvts.getRawParameterValue ("preScale");
    loadDefaultPattern();

    {
        juce::PropertiesFile props (settingsOptions());
        samplesRoot = props.getValue ("samplesRoot");
    }
    if (samplesRoot.isEmpty())
    {
        const auto fallback = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                                  .getChildFile ("TR-08 Drum Machine").getChildFile ("samples");
        if (fallback.isDirectory()) samplesRoot = fallback.getFullPathName();
    }

    loader = std::make_unique<KitLoader> (*this);
    loader->startThread (juce::Thread::Priority::low);
    if (samplesRoot.isNotEmpty())
        loader->request (true);
}

TR08AudioProcessor::~TR08AudioProcessor()
{
    loader->stopThread (20000);
}

//==============================================================================
juce::StringArray TR08AudioProcessor::getKitNames() const
{
    const juce::ScopedLock sl (kitLock);
    juce::StringArray names;
    for (auto& k : kits) names.add (juce::String::fromUTF8 (k.name.c_str()));
    return names;
}

juce::String TR08AudioProcessor::getKitName() const
{
    const juce::ScopedLock sl (kitLock);
    return kitName;
}

juce::File TR08AudioProcessor::getSamplesRoot() const
{
    const juce::ScopedLock sl (kitLock);
    return juce::File (samplesRoot);
}

void TR08AudioProcessor::setSamplesRoot (const juce::File& dir)
{
    {
        const juce::ScopedLock sl (kitLock);
        samplesRoot = dir.getFullPathName();
    }
    {
        juce::PropertiesFile props (settingsOptions());
        props.setValue ("samplesRoot", dir.getFullPathName());
        props.saveIfNeeded();
    }
    loader->request (true);
}

void TR08AudioProcessor::selectKit (const juce::String& name)
{
    {
        const juce::ScopedLock sl (kitLock);
        kitName = name;
    }
    loader->request (false);
}

void TR08AudioProcessor::rescanKits() { loader->request (true); }

void TR08AudioProcessor::runJob (bool rescan)
{
    loading = true;
    juce::String root, want;
    {
        const juce::ScopedLock sl (kitLock);
        root = samplesRoot;
        want = kitName;
    }

    if (rescan)
    {
        std::vector<tr08kit::Kit> found;
        if (root.isNotEmpty() && juce::File (root).isDirectory())
            found = tr08kit::scanKits (std::filesystem::path (root.toStdString()));
        const juce::ScopedLock sl (kitLock);
        kits = std::move (found);
    }

    std::unique_ptr<tr08::SampleSet> set;
    if (want.isNotEmpty())
    {
        tr08kit::Kit chosen;
        bool found = false;
        {
            const juce::ScopedLock sl (kitLock);
            for (auto& k : kits)
                if (juce::String::fromUTF8 (k.name.c_str()) == want) { chosen = k; found = true; break; }
        }
        if (found) set = decodeKit (chosen);
    }
    install (std::move (set));      // nullptr = built-in synth
    loading = false;
}

void TR08AudioProcessor::install (std::unique_ptr<tr08::SampleSet> set)
{
    const juce::ScopedLock sl (kitLock);
    for (int i = 0; i < tr08::kNumInst; ++i)
        variantCounts[i].store (set ? (int) set->slot[i].size() : 0);
    currentSet.store (set.get());                      // the audio thread switches over at its next block
    if (activeSet) retiredSets.push_back (std::move (activeSet));
    activeSet = std::move (set);
    while (retiredSets.size() > 4) retiredSets.erase (retiredSets.begin());
}

void TR08AudioProcessor::loadDefaultPattern()
{
    for (auto& v : patterns) for (auto& h : v) for (auto& t : h) t.store (0);
    // part A: four on the floor with off-beat open hat; part B: a busier variation
    patterns[0][0][1 + tr08::BD].store (0x1111);
    patterns[0][0][1 + tr08::SD].store (0x1010);
    patterns[0][0][1 + tr08::CH].store (0x5555);
    patterns[0][0][1 + tr08::OH].store (0x4444);
    patterns[0][0][0].store (0x1010);
    patterns[1][0][1 + tr08::BD].store (0x1451);
    patterns[1][0][1 + tr08::SD].store (0x1010);
    patterns[1][0][1 + tr08::CH].store (0xDDDD);
    patterns[1][0][1 + tr08::CP].store (0x1000);
}

void TR08AudioProcessor::clearAll()
{
    clearVariation (0);
    clearVariation (1);
}

void TR08AudioProcessor::clearVariation (int var)
{
    for (auto& h : patterns[juce::jlimit (0, kParts - 1, var)]) for (auto& t : h) t.store (0);
    steps1.store (kSteps);                    // "The Clear operation automatically sets this to 16 steps" (2nd part: 0)
    steps2.store (0);
}

void TR08AudioProcessor::setSteps (int half, int steps)
{
    if (half == 0) { steps1.store (juce::jlimit (1, kSteps, steps)); }
    else           { steps2.store (juce::jlimit (0, kSteps, steps)); }
}

//==============================================================================
bool TR08AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono()) && layouts.getMainInputChannelSet().isDisabled();
}

void TR08AudioProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    for (int i = 0; i < tr08::kNumInst; ++i) kit.params.level[i] = taper (pLevel[i]->load());
    kit.prepare (sampleRate);
    stepPos = 0.0;
    wasRunning = false;
    currentStep.store (-1);
}

void TR08AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();

    kit.setSamples (currentSet.load());

    // ---- panel snapshot
    auto& P = kit.params;
    for (int i = 0; i < tr08::kNumInst; ++i) P.level[i] = taper (pLevel[i]->load());
    P.bdTone = pBdTone->load() * 0.1f;   P.bdDecay = pBdDecay->load() * 0.1f;
    P.sdTone = pSdTone->load() * 0.1f;   P.sdSnappy = pSdSnappy->load() * 0.1f;
    for (int i = 0; i < 3; ++i) P.tune[i] = pTune[i]->load() * 0.1f;
    for (int i = 0; i < 5; ++i) P.alt[i] = pAlt[i]->load() > 0.5f;
    for (int i = 0; i < tr08::kNumInst; ++i) P.variant[i] = (int) pVar[i]->load();
    P.cyTone = pCyTone->load() * 0.1f;   P.cyDecay = pCyDecay->load() * 0.1f;
    P.ohDecay = pOhDecay->load() * 0.1f; P.accent = pAccent->load() * 0.1f;

    // ---- transport: follow the host when it is playing, otherwise the Start/Stop button
    double bpm = (double) pTempo->load() + ((double) pFine->load() - 5.0);
    bool hostPlaying = false;
    double hostStep = 0.0;
    if (auto* head = getPlayHead())
        if (auto pos = head->getPosition())
            if (pos->getIsPlaying())
                if (auto ppq = pos->getPpqPosition())
                {
                    hostPlaying = true;
                    hostStep = *ppq * 4.0;
                    if (auto hb = pos->getBpm()) bpm = *hb;
                }
    bpm = juce::jlimit (20.0, 400.0, bpm);
    displayBpm.store ((float) bpm);

    const bool running = hostPlaying || pRun->load() > 0.5f;
    if (hostPlaying)        stepPos = hostStep;
    else if (running && ! wasRunning) stepPos = 0.0;
    wasRunning = running;

    int numEvents = 0;
    const auto addEvent = [&] (int offset, int inst, float vel, bool accent)
    {
        if (numEvents < kMaxEvents)
            events[numEvents++] = { juce::jlimit (0, juce::jmax (0, n - 1), offset), inst, vel, accent };
    };

    // ---- sequencer: trigger every step whose (shuffled) position falls inside this block
    const double sps = bpm / 60.0 * 4.0 / sr;       // steps per sample
    const double p0 = stepPos, p1 = stepPos + (double) n * sps;
    if (running)
    {
        const bool shuffle = pShuffle->load() > 0.5f;
        const int variation = juce::roundToInt (pVariation->load());
        static constexpr int kMeasures[6] = { 0, 16, 12, 8, 4, 2 };       // Auto Fill In knob: Manual, 16, 12, 8, 4, 2 measures
        const int fillEvery = kMeasures[juce::jlimit (0, 5, juce::roundToInt (pMeasures->load()))];
        const int fillPart = juce::jlimit (0, 1, juce::roundToInt (pFillPart->load()));
        // Pre-scale (applied with [CLEAR]) = note value of one step: 1 = 8th-note triplet, 2 = 16th-note triplet, 3 = 16th, 4 = 32nd.
        static constexpr double kStepLen[4] = { 4.0 / 3.0, 2.0 / 3.0, 1.0, 0.5 };     // in sixteenths
        const double len = kStepLen[juce::jlimit (0, 3, scaleApplied.load())];
        const int n1 = juce::jlimit (1, kSteps, steps1.load()), n2 = juce::jlimit (0, kSteps, steps2.load());
        const int total = n1 + n2;                    // 1st part followed by the 2nd part make one pattern
        const auto k0 = (int64_t) std::floor (p0 / len) - 1, k1 = (int64_t) std::floor (p1 / len);
        for (int64_t k = juce::jmax ((int64_t) 0, k0); k <= k1; ++k)
        {
            const double pos = ((double) k + ((shuffle && (k & 1)) ? 0.33 : 0.0)) * len;
            if (pos < p0 || pos >= p1) continue;

            const int idx = (int) (k % total);
            const int half = idx < n1 ? 0 : 1;
            const int step = half == 0 ? idx : idx - n1;
            const int cycle = (int) (k / total);
            int var = variation == 0 ? 0 : (variation == 2 ? 1 : (cycle & 1));          // BASIC-VARIATION A / AB / B
            if (fillEvery > 0 && cycle % fillEvery == fillEvery - 1)                     // last pattern of each group: the fill-in variation
                var = fillPart;
            const bool accent = (patterns[var][half][0].load() >> step) & 1;
            const int offset = (int) ((pos - p0) / sps);
            for (int inst = 0; inst < tr08::kNumInst; ++inst)
                if ((patterns[var][half][1 + inst].load() >> step) & 1)
                    addEvent (offset, inst, 0.85f, accent);
            currentStep.store (idx);
        }
        if (! hostPlaying) stepPos = p1;
    }
    else
    {
        currentStep.store (-1);
    }

    // ---- MIDI notes and manual triggers from the panel
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn())
            if (const int inst = noteToInst (msg.getNoteNumber()); inst >= 0)
                addEvent (meta.samplePosition, inst, 0.5f + 0.5f * msg.getFloatVelocity(), msg.getVelocity() >= 112);
    }
    const auto manual = manualMask.exchange (0);
    for (int i = 0; i < tr08::kNumInst; ++i)
        if ((manual >> i) & 1u)
            addEvent (0, i, 0.9f, false);

    std::sort (events, events + numEvents, [] (const Event& a, const Event& b) { return a.offset < b.offset; });

    // ---- render
    float* left = buffer.getWritePointer (0);
    float* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    int e = 0;
    for (int i = 0; i < n; ++i)
    {
        while (e < numEvents && events[e].offset <= i)
        {
            kit.trigger (events[e].inst, events[e].vel, events[e].accent);
            ++e;
        }
        const float s = kit.process();
        left[i] = s;
        if (right != nullptr) right[i] = s;
    }
    midi.clear();
}

//==============================================================================
juce::AudioProcessorEditor* TR08AudioProcessor::createEditor() { return new TR08AudioProcessorEditor (*this); }

void TR08AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
    {
        juce::String pat;
        for (auto& v : patterns) for (auto& h : v) for (auto& t : h) pat << (int) t.load() << " ";
        xml->setAttribute ("patterns", pat.trim());
        xml->setAttribute ("kit", getKitName());
        xml->setAttribute ("steps1", getSteps (0));
        xml->setAttribute ("steps2", getSteps (1));
        xml->setAttribute ("scale", getAppliedPreScale());
        copyXmlToBinary (*xml, dest);
    }
}

void TR08AudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            selectKit (xml->getStringAttribute ("kit"));
            setSteps (0, xml->getIntAttribute ("steps1", kSteps));
            setSteps (1, xml->getIntAttribute ("steps2", 0));
            scaleApplied.store (juce::jlimit (0, 3, xml->getIntAttribute ("scale", 2)));
            const auto tokens = juce::StringArray::fromTokens (xml->getStringAttribute ("patterns"), " ", "");
            if (tokens.size() == kParts * 2 * kTracks)
            {
                int i = 0;
                for (auto& v : patterns) for (auto& h : v) for (auto& t : h) t.store ((uint16_t) tokens[i++].getIntValue());
            }
            else if (tokens.size() == kParts * kTracks)          // older sessions: one part per variation
            {
                int i = 0;
                for (auto& v : patterns) for (auto& t : v[0]) t.store ((uint16_t) tokens[i++].getIntValue());
                for (auto& v : patterns) for (auto& t : v[1]) t.store (0);
            }
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TR08AudioProcessor(); }
