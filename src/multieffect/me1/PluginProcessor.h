// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include <JuceHeader.h>
#include "Modules.h"

// ME-1: a chain of basic effect modules (any number) (see Modules.h and "ME-1 specs.md"), edited in the plugin window:
// add, remove, replace, reorder and bypass. Every module has 2-3 knobs. The session stores the chain and the knob values.
// Host parameters: Power, Input and Output gain.
class ME1AudioProcessor : public juce::AudioProcessor,
                          public juce::ChangeBroadcaster
{
public:
    ME1AudioProcessor();
    ~ME1AudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ME-1"; }
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

    // ---- chain editing (message thread only). The change broadcast fires when the chain's structure changes. ----
    int numSlots() const { return (int) slots.size(); }
    int slotEffect (int i) const { return slots[(size_t) i]->effect; }
    bool slotBypassed (int i) const { return slots[(size_t) i]->bypass.load(); }
    void setSlotBypassed (int i, bool b);
    float slotParam (int i, int k) const { return slots[(size_t) i]->p[k].load(); }
    void setSlotParam (int i, int k, float v);
    bool slotSync (int i) const { return slots[(size_t) i]->sync.load(); }
    void setSlotSync (int i, bool on);             // tempo sync of the module's TIME / RATE knob (message thread); rebuilds the editor knobs
    bool addSlot (int effect);                     // appends; false when the chain is full
    void replaceSlot (int i, int effect);          // new module in the same place, default knob values
    void removeSlot (int i);
    void moveSlot (int from, int to);
    int selectedSlot = 0;                          // the module whose knobs the editor shows

    juce::AudioProcessorValueTreeState apvts;      // power, inGain, outGain

    // level meters: the peak (left, right) since the editor last read it; the editor takes it with exchange (0)
    std::atomic<float> meterIn[2] { 0.0f, 0.0f }, meterOut[2] { 0.0f, 0.0f };

private:
    struct Slot
    {
        std::unique_ptr<me1::Module> module;
        int effect = 0;
        std::atomic<bool> bypass { false };
        std::atomic<float> p[me1::kMaxParams];
        std::atomic<bool> sync { false };           // the sync knob holds a note length (index) instead of ms / Hz
        float altValue = (float) me1::kDefaultDivision;   // the sync knob's value in the other mode (kept while switching)
        float mix = 1.0f;                           // 0 = bypassed, 1 = on (ramped over a block)
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    std::unique_ptr<Slot> makeSlot (int effect, const float* values, bool wasBypassed, bool sync = false, float alt = -1.0f);

    std::vector<std::unique_ptr<Slot>> slots;
    juce::CriticalSection chainLock;               // held by the message thread while editing, tried by the audio thread
    std::atomic<float>* pIn = nullptr, *pOut = nullptr, *pPower = nullptr;
    juce::SmoothedValue<float> inSmooth, outSmooth, powerSmooth;
    double sr = 44100.0;
    int block = 512;
    juce::AudioBuffer<float> dryBuf, inBuf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ME1AudioProcessor)
};
