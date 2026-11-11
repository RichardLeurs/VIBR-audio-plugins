// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include <JuceHeader.h>
#include "DC1Registry.h"

// DC-1: a chain of up to 8 pedals (the processors of src/effects/), edited in the plugin window: add, remove, reorder, bypass.
// The session stores the chain with every pedal's own state. Only the input / output gain are host parameters.
class DC1AudioProcessor : public juce::AudioProcessor,
                          public juce::ChangeBroadcaster
{
public:
    static constexpr int kMaxSlots = 8;

    DC1AudioProcessor();
    ~DC1AudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "DC-1"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- chain editing (message thread only) ----
    int numSlots() const { return (int) slots.size(); }
    juce::AudioProcessor* slotProcessor (int i) const { return i >= 0 && i < numSlots() ? slots[(size_t) i]->proc.get() : nullptr; }
    int slotEffect (int i) const { return slots[(size_t) i]->effect; }
    bool slotBypassed (int i) const { return slots[(size_t) i]->bypass.load(); }
    void setSlotBypassed (int i, bool b);
    bool addSlot (int effect);                     // appends; false when the chain is full
    void removeSlot (int i);
    void moveSlot (int from, int to);
    int selectedSlot = 0;
    juce::ChangeBroadcaster releaseSignal;        // sent synchronously before the pedals are destroyed: the editor drops its pedal view

    juce::AudioProcessorValueTreeState apvts;

private:
    struct Slot
    {
        std::unique_ptr<juce::AudioProcessor> proc;
        int effect = 0;
        std::atomic<bool> bypass { false };
        float mix = 1.0f;                           // 0 = bypassed, 1 = on (ramped over a block)
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    std::unique_ptr<Slot> makeSlot (int effect, const juce::MemoryBlock* state, bool wasBypassed);
    void chainChanged();                           // latency + notify the editor
    void rebuildFromState (const juce::ValueTree& chain);

    std::vector<std::unique_ptr<Slot>> slots;
    juce::CriticalSection chainLock;               // held by the message thread while editing, tried by the audio thread
    std::atomic<float>* pIn = nullptr, *pOut = nullptr;
    juce::SmoothedValue<float> inSmooth, outSmooth;
    double sr = 44100.0;
    int block = 512;
    bool prepared = false;
    juce::AudioBuffer<float> dryBuf;
    juce::MidiBuffer noMidi;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DC1AudioProcessor)
};
