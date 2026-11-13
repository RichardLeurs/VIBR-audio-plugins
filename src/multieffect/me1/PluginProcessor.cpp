// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout ME1AudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "power", 1 }, "Power", true));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "inGain", 1 },  "Input",  NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "outGain", 1 }, "Output", NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));
    return layout;
}

ME1AudioProcessor::ME1AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ME1", createLayout())
{
    pIn    = apvts.getRawParameterValue ("inGain");
    pOut   = apvts.getRawParameterValue ("outGain");
    pPower = apvts.getRawParameterValue ("power");
    // a starting chain: drive, chorus, delay and reverb
    for (auto id : { "OVERDRIVE", "CHORUS", "DELAY", "REVERB" })
        addSlot (me1::indexOfModule (id));
}

ME1AudioProcessor::~ME1AudioProcessor()
{
    const juce::ScopedLock sl (chainLock);
    slots.clear();
}

bool ME1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

std::unique_ptr<ME1AudioProcessor::Slot> ME1AudioProcessor::makeSlot (int effect, const float* values, bool wasBypassed, bool sync, float alt)
{
    if (effect < 0 || effect >= me1::numModules()) return nullptr;
    const auto& info = me1::moduleInfo (effect);
    auto s = std::make_unique<Slot>();
    s->effect = effect;
    s->module = info.make();
    s->sync = sync && info.syncParam >= 0;
    for (int k = 0; k < me1::kMaxParams; ++k)
    {
        const auto& ps = info.params[k];
        const bool isSync = s->sync.load() && k == info.syncParam;
        const float lo = isSync ? 0.0f : ps.min, hi = isSync ? (float) me1::kNumDivisions - 1.0f : ps.max;
        const float def = isSync ? (float) me1::kDefaultDivision : ps.def;
        s->p[k] = k >= info.numParams ? 0.0f : (values != nullptr ? juce::jlimit (lo, hi, values[k]) : def);
    }
    if (info.syncParam >= 0)
        s->altValue = alt >= 0.0f ? alt : (s->sync.load() ? info.params[info.syncParam].def : (float) me1::kDefaultDivision);
    s->module->prepare (sr);
    s->bypass = wasBypassed;
    s->mix = wasBypassed ? 0.0f : 1.0f;
    return s;
}

bool ME1AudioProcessor::addSlot (int effect)
{
    auto s = makeSlot (effect, nullptr, false);
    if (s == nullptr) return false;
    {
        const juce::ScopedLock sl (chainLock);
        slots.push_back (std::move (s));
    }
    selectedSlot = numSlots() - 1;
    sendChangeMessage();
    return true;
}

void ME1AudioProcessor::replaceSlot (int i, int effect)
{
    if (i < 0 || i >= numSlots()) return;
    auto s = makeSlot (effect, nullptr, false);
    if (s == nullptr) return;
    std::unique_ptr<Slot> old;
    {
        const juce::ScopedLock sl (chainLock);
        old = std::move (slots[(size_t) i]);
        slots[(size_t) i] = std::move (s);
    }
    sendChangeMessage();
}

void ME1AudioProcessor::removeSlot (int i)
{
    if (i < 0 || i >= numSlots()) return;
    std::unique_ptr<Slot> gone;
    {
        const juce::ScopedLock sl (chainLock);
        gone = std::move (slots[(size_t) i]);
        slots.erase (slots.begin() + i);
    }
    selectedSlot = juce::jlimit (0, juce::jmax (0, numSlots() - 1), selectedSlot > i ? selectedSlot - 1 : selectedSlot);
    sendChangeMessage();
}

void ME1AudioProcessor::moveSlot (int from, int to)
{
    if (from < 0 || from >= numSlots() || to < 0 || to >= numSlots() || from == to) return;
    {
        const juce::ScopedLock sl (chainLock);
        auto s = std::move (slots[(size_t) from]);
        slots.erase (slots.begin() + from);
        slots.insert (slots.begin() + to, std::move (s));
    }
    if (selectedSlot == from) selectedSlot = to;
    else if (from < selectedSlot && to >= selectedSlot) --selectedSlot;
    else if (from > selectedSlot && to <= selectedSlot) ++selectedSlot;
    sendChangeMessage();
}

void ME1AudioProcessor::setSlotBypassed (int i, bool b)
{
    if (i >= 0 && i < numSlots()) slots[(size_t) i]->bypass = b;
}

void ME1AudioProcessor::setSlotParam (int i, int k, float v)
{
    if (i >= 0 && i < numSlots() && k >= 0 && k < me1::kMaxParams) slots[(size_t) i]->p[k] = v;
}

void ME1AudioProcessor::setSlotSync (int i, bool on)
{
    if (i < 0 || i >= numSlots()) return;
    auto& s = *slots[(size_t) i];
    const int sp = me1::moduleInfo (s.effect).syncParam;
    if (sp < 0 || s.sync.load() == on) return;
    const float current = s.p[sp].load();
    s.p[sp] = s.altValue;                         // swap the knob's value with the one of the other mode
    s.altValue = current;
    s.sync = on;
    sendChangeMessage();
}

double ME1AudioProcessor::getTailLengthSeconds() const
{
    double t = 0.0;
    for (auto& s : slots) t += me1::moduleInfo (s->effect).tailSeconds;
    return t;
}

void ME1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    block = juce::jmax (1, samplesPerBlock);
    dryBuf.setSize (2, block, false, false, true);
    inBuf.setSize (2, block, false, false, true);
    inSmooth.reset (sampleRate, 0.03);  inSmooth.setCurrentAndTargetValue (pIn->load());
    outSmooth.reset (sampleRate, 0.03); outSmooth.setCurrentAndTargetValue (pOut->load());
    powerSmooth.reset (sampleRate, 0.02); powerSmooth.setCurrentAndTargetValue (pPower->load() > 0.5f ? 1.0f : 0.0f);
    const juce::ScopedLock sl (chainLock);
    for (auto& s : slots) s->module->prepare (sr);
}

void ME1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0 || buffer.getNumChannels() < 2 || n > dryBuf.getNumSamples()) return;

    const float powerTarget = pPower->load() > 0.5f ? 1.0f : 0.0f;
    const float pFrom = powerSmooth.getCurrentValue();
    powerSmooth.setTargetValue (powerTarget);
    const float pTo = powerSmooth.skip (n);
    const auto addPeak = [n] (std::atomic<float>* m, const juce::AudioBuffer<float>& b)
    {
        for (int c = 0; c < 2; ++c) { const float v = b.getMagnitude (c, 0, n); if (v > m[c].load()) m[c].store (v); }
    };
    if (pFrom == 0.0f && pTo == 0.0f)                                // off: the signal passes untouched
    {
        addPeak (meterIn, buffer); addPeak (meterOut, buffer);
        return;
    }
    for (int c = 0; c < 2; ++c) inBuf.copyFrom (c, 0, buffer, c, 0, n);

    inSmooth.setTargetValue (pIn->load());
    outSmooth.setTargetValue (pOut->load());
    buffer.applyGainRamp (0, n, juce::Decibels::decibelsToGain (inSmooth.getCurrentValue()), juce::Decibels::decibelsToGain (inSmooth.skip (n)));
    addPeak (meterIn, buffer);

    double bpm = 120.0;                                              // the host tempo (120 when the host gives none)
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) bpm = *b;

    const juce::ScopedTryLock tl (chainLock);
    if (tl.isLocked())
    {
        for (auto& s : slots)
        {
            const float target = s->bypass.load() ? 0.0f : 1.0f;
            if (target == 0.0f && s->mix == 0.0f) continue;
            for (int c = 0; c < 2; ++c) dryBuf.copyFrom (c, 0, buffer, c, 0, n);
            float pv[me1::kMaxParams];
            for (int k = 0; k < me1::kMaxParams; ++k) pv[k] = s->p[k].load();
            const auto& info = me1::moduleInfo (s->effect);
            if (info.syncParam >= 0 && s->sync.load())                         // note length -> ms / Hz at the host tempo
                pv[info.syncParam] = me1::syncedValue (info.params[info.syncParam], (int) std::lround (pv[info.syncParam]), bpm);
            s->module->process (buffer.getWritePointer (0), buffer.getWritePointer (1), n, pv);
            if (s->mix != 1.0f || target != 1.0f)
            {
                // crossfade dry -> wet over the block when a module is switched
                for (int c = 0; c < 2; ++c)
                {
                    auto* w = buffer.getWritePointer (c);
                    const auto* d = dryBuf.getReadPointer (c);
                    for (int i = 0; i < n; ++i)
                    {
                        const float m = s->mix + (target - s->mix) * (float) (i + 1) / (float) n;
                        w[i] = d[i] + (w[i] - d[i]) * m;
                    }
                }
                s->mix = target;
            }
        }
    }

    buffer.applyGainRamp (0, n, juce::Decibels::decibelsToGain (outSmooth.getCurrentValue()), juce::Decibels::decibelsToGain (outSmooth.skip (n)));

    if (pFrom != 1.0f || pTo != 1.0f)                              // power switched: crossfade with the untouched input
        for (int c = 0; c < 2; ++c)
        {
            auto* w = buffer.getWritePointer (c);
            const auto* d = inBuf.getReadPointer (c);
            for (int i = 0; i < n; ++i)
            {
                const float m = pFrom + (pTo - pFrom) * (float) (i + 1) / (float) n;
                w[i] = d[i] + (w[i] - d[i]) * m;
            }
        }
    addPeak (meterOut, buffer);
}

juce::AudioProcessorEditor* ME1AudioProcessor::createEditor() { return new ME1AudioProcessorEditor (*this); }

void ME1AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    juce::ValueTree chain ("chain");
    chain.setProperty ("selected", selectedSlot, nullptr);
    for (auto& s : slots)
    {
        juce::ValueTree v ("slot");
        v.setProperty ("effect", me1::moduleInfo (s->effect).id, nullptr);
        v.setProperty ("bypass", s->bypass.load(), nullptr);
        v.setProperty ("sync", s->sync.load(), nullptr);
        v.setProperty ("alt", s->altValue, nullptr);
        for (int k = 0; k < me1::kMaxParams; ++k) v.setProperty ("p" + juce::String (k), s->p[k].load(), nullptr);
        chain.appendChild (v, nullptr);
    }
    state.removeChild (state.getChildWithName ("chain"), nullptr);
    state.appendChild (chain, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void ME1AudioProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto state = juce::ValueTree::fromXml (*xml);
    auto chain = state.getChildWithName ("chain");
    apvts.replaceState (state);
    if (! chain.isValid()) return;

    std::vector<std::unique_ptr<Slot>> fresh;
    for (int i = 0; i < chain.getNumChildren(); ++i)
    {
        const auto v = chain.getChild (i);
        float values[me1::kMaxParams];
        for (int k = 0; k < me1::kMaxParams; ++k) values[k] = (float) v.getProperty ("p" + juce::String (k));
        if (auto s = makeSlot (me1::indexOfModule (v.getProperty ("effect").toString()), values, (bool) v.getProperty ("bypass"), (bool) v.getProperty ("sync"), v.hasProperty ("alt") ? (float) v.getProperty ("alt") : -1.0f))
            fresh.push_back (std::move (s));
    }
    if (fresh.empty() && chain.getNumChildren() > 0) return;       // a session of the old pedal-chain ME-1: keep the starting chain
    {
        const juce::ScopedLock sl (chainLock);
        slots.swap (fresh);
    }
    fresh.clear();
    selectedSlot = juce::jlimit (0, juce::jmax (0, numSlots() - 1), (int) chain.getProperty ("selected"));
    sendChangeMessage();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ME1AudioProcessor(); }
