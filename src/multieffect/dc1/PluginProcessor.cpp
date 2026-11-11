// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout DC1AudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "inGain", 1 },  "Input",  NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "outGain", 1 }, "Output", NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));
    return layout;
}

DC1AudioProcessor::DC1AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "DC1", createLayout())
{
    pIn  = apvts.getRawParameterValue ("inGain");
    pOut = apvts.getRawParameterValue ("outGain");
    // a starting chain: drive, amp and delay
    for (auto id : { "OD-2", "IR-2", "DD-1" })
        addSlot (dc1::indexOf (id));
    selectedSlot = 0;
}

DC1AudioProcessor::~DC1AudioProcessor()
{
    const juce::ScopedLock sl (chainLock);
    slots.clear();
}

bool DC1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

std::unique_ptr<DC1AudioProcessor::Slot> DC1AudioProcessor::makeSlot (int effect, const juce::MemoryBlock* state, bool wasBypassed)
{
    int n = 0;
    const auto* fx = dc1::effects (n);
    if (effect < 0 || effect >= n) return nullptr;
    auto s = std::make_unique<Slot>();
    s->effect = effect;
    s->proc.reset (fx[effect].make());
    if (s->proc == nullptr) return nullptr;
    s->proc->setPlayHead (getPlayHead());
    if (state != nullptr && state->getSize() > 0)
        s->proc->setStateInformation (state->getData(), (int) state->getSize());
    s->proc->setPlayConfigDetails (2, 2, sr, block);
    s->proc->prepareToPlay (sr, block);
    s->bypass = wasBypassed;
    s->mix = wasBypassed ? 0.0f : 1.0f;
    return s;
}

void DC1AudioProcessor::chainChanged()
{
    int latency = 0;
    for (auto& s : slots) latency += s->proc->getLatencySamples();
    setLatencySamples (latency);
    sendChangeMessage();
}

bool DC1AudioProcessor::addSlot (int effect)
{
    if (numSlots() >= kMaxSlots) return false;
    auto s = makeSlot (effect, nullptr, false);
    if (s == nullptr) return false;
    {
        const juce::ScopedLock sl (chainLock);
        slots.push_back (std::move (s));
    }
    selectedSlot = numSlots() - 1;
    chainChanged();
    return true;
}

void DC1AudioProcessor::removeSlot (int i)
{
    if (i < 0 || i >= numSlots()) return;
    std::unique_ptr<Slot> gone;
    {
        const juce::ScopedLock sl (chainLock);
        gone = std::move (slots[(size_t) i]);
        slots.erase (slots.begin() + i);
    }
    gone.reset();                                  // destroyed outside the lock (the editor has dropped its view first)
    selectedSlot = juce::jlimit (0, juce::jmax (0, numSlots() - 1), selectedSlot);
    chainChanged();
}

void DC1AudioProcessor::moveSlot (int from, int to)
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
    chainChanged();
}

void DC1AudioProcessor::setSlotBypassed (int i, bool b)
{
    if (i >= 0 && i < numSlots()) { slots[(size_t) i]->bypass = b; sendChangeMessage(); }
}

double DC1AudioProcessor::getTailLengthSeconds() const
{
    double t = 0.0;
    for (auto& s : slots) t += s->proc->getTailLengthSeconds();
    return t;
}

void DC1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    block = juce::jmax (1, samplesPerBlock);
    dryBuf.setSize (2, block, false, false, true);
    inSmooth.reset (sampleRate, 0.03);  inSmooth.setCurrentAndTargetValue (pIn->load());
    outSmooth.reset (sampleRate, 0.03); outSmooth.setCurrentAndTargetValue (pOut->load());
    {
        const juce::ScopedLock sl (chainLock);
        for (auto& s : slots)
        {
            s->proc->setPlayHead (getPlayHead());
            s->proc->setPlayConfigDetails (2, 2, sr, block);
            s->proc->prepareToPlay (sr, block);
        }
    }
    prepared = true;
    int latency = 0;
    for (auto& s : slots) latency += s->proc->getLatencySamples();
    setLatencySamples (latency);
}

void DC1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0 || buffer.getNumChannels() < 2) return;

    inSmooth.setTargetValue (pIn->load());
    outSmooth.setTargetValue (pOut->load());
    buffer.applyGainRamp (0, n, juce::Decibels::decibelsToGain (inSmooth.getCurrentValue()), juce::Decibels::decibelsToGain (inSmooth.skip (n)));

    const juce::ScopedTryLock tl (chainLock);
    if (tl.isLocked() && n <= dryBuf.getNumSamples())
    {
        for (auto& s : slots)
        {
            const float target = s->bypass.load() ? 0.0f : 1.0f;
            if (target == 0.0f && s->mix == 0.0f) continue;
            for (int c = 0; c < 2; ++c) dryBuf.copyFrom (c, 0, buffer, c, 0, n);
            s->proc->setPlayHead (getPlayHead());
            noMidi.clear();
            s->proc->processBlock (buffer, noMidi);
            if (s->mix != 1.0f || target != 1.0f)
            {
                // crossfade dry -> wet over the block when a slot is switched
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
}

juce::AudioProcessorEditor* DC1AudioProcessor::createEditor() { return new DC1AudioProcessorEditor (*this); }

void DC1AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    juce::ValueTree chain ("chain");
    chain.setProperty ("selected", selectedSlot, nullptr);
    for (auto& s : slots)
    {
        int n = 0;
        juce::ValueTree v ("slot");
        v.setProperty ("effect", dc1::effects (n)[s->effect].id, nullptr);
        v.setProperty ("bypass", s->bypass.load(), nullptr);
        juce::MemoryBlock mb;
        s->proc->getStateInformation (mb);
        v.setProperty ("state", mb.toBase64Encoding(), nullptr);
        chain.appendChild (v, nullptr);
    }
    state.removeChild (state.getChildWithName ("chain"), nullptr);
    state.appendChild (chain, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void DC1AudioProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto state = juce::ValueTree::fromXml (*xml);
    auto chain = state.getChildWithName ("chain");
    apvts.replaceState (state);
    if (chain.isValid()) rebuildFromState (chain);
}

void DC1AudioProcessor::rebuildFromState (const juce::ValueTree& chain)
{
    releaseSignal.sendSynchronousChangeMessage();  // the editor lets go of the old pedals first (message thread)
    std::vector<std::unique_ptr<Slot>> fresh;
    for (int i = 0; i < chain.getNumChildren() && (int) fresh.size() < kMaxSlots; ++i)
    {
        const auto v = chain.getChild (i);
        juce::MemoryBlock mb;
        mb.fromBase64Encoding (v.getProperty ("state").toString());
        if (auto s = makeSlot (dc1::indexOf (v.getProperty ("effect").toString()), &mb, (bool) v.getProperty ("bypass")))
            fresh.push_back (std::move (s));
    }
    {
        const juce::ScopedLock sl (chainLock);
        slots.swap (fresh);
    }
    fresh.clear();
    selectedSlot = juce::jlimit (0, juce::jmax (0, numSlots() - 1), (int) chain.getProperty ("selected"));
    chainChanged();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DC1AudioProcessor(); }
