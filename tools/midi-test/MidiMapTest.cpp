// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
// Unit test for MidiMap: feeds CC messages and checks the parameter values, the learn binding, soft takeover, tap tempo,
// save / restore of the MIDIMAP state and that process() does not allocate.
#include <JuceHeader.h>
#include "MidiMap.h"
#include <atomic>
#include <cstdlib>
#include <new>

static std::atomic<int> gAllocs { 0 };
static std::atomic<bool> gCount { false };
void* operator new (std::size_t n)
{
    if (gCount.load()) ++gAllocs;
    if (void* p = std::malloc (n)) return p;
    throw std::bad_alloc();
}
void operator delete (void* p) noexcept { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }

static int failures = 0;
#define CHECK(cond) do { if (! (cond)) { ++failures; std::printf ("FAIL line %d: %s\n", __LINE__, #cond); } } while (0)

struct TestProcessor : juce::AudioProcessor
{
    TestProcessor() : AudioProcessor (BusesProperties().withInput ("in", juce::AudioChannelSet::stereo()).withOutput ("out", juce::AudioChannelSet::stereo())),
        apvts (*this, nullptr, "T", layout()) {}
    static juce::AudioProcessorValueTreeState::ParameterLayout layout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout l;
        l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "rate", 1 }, "Rate", 0.0f, 10.0f, 5.0f));
        l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "depth", 1 }, "Depth", 0.0f, 1.0f, 0.0f));
        l.add (std::make_unique<juce::AudioParameterBool>  (juce::ParameterID { "sync", 1 }, "Sync", false));
        l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "division", 1 }, "Division", juce::StringArray { "1/4", "1/8", "1/16", "1/32" }, 0));
        return l;
    }
    const juce::String getName() const override { return "T"; }
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    double getTailLengthSeconds() const override { return 0; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}
    juce::AudioProcessorValueTreeState apvts;
};

static juce::MidiBuffer cc (int num, int val, int ch = 1)
{
    juce::MidiBuffer b; b.addEvent (juce::MidiMessage::controllerEvent (ch, num, val), 0); return b;
}

// let the 60 Hz timer of the map run on the message thread
static void pump (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }

static float norm (TestProcessor& p, const char* id) { return p.apvts.getParameter (id)->getValue(); }

int main()
{
    juce::ScopedJuceInitialiser_GUI init;

    // 1. default binding: CC value 0..127 -> normalised 0..1
    {
        TestProcessor p;
        MidiMap map (p.apvts, { "rate", "depth", "sync", "division" });
        map.setDefault ("depth", 1);
        map.process (cc (1, 127));
        pump (80);
        CHECK (std::abs (norm (p, "depth") - 1.0f) < 1.0e-4f);
        map.process (cc (1, 0));
        pump (80);
        CHECK (std::abs (norm (p, "depth") - 0.0f) < 1.0e-4f);
        map.process (cc (1, 64));
        pump (80);
        CHECK (std::abs (norm (p, "depth") - 64.0f / 127.0f) < 1.0e-3f);
        map.process (cc (2, 127));                                       // unbound CC: nothing moves
        pump (80);
        CHECK (std::abs (norm (p, "depth") - 64.0f / 127.0f) < 1.0e-3f);
        map.process (cc (120, 0));                                       // channel mode message ignored
        CHECK (map.getCC (map.indexOf ("depth")) == 1);
    }

    // 2. learn: the next CC binds, a second parameter on the same CC steals it, clear removes it
    {
        TestProcessor p;
        MidiMap map (p.apvts, { "rate", "depth" });
        map.startLearn (map.indexOf ("rate"));
        CHECK (map.getLearning() == 0);
        map.process (cc (74, 100));
        CHECK (map.getLearning() == -1);
        CHECK (map.getCC (0) == 74);
        pump (80);
        CHECK (std::abs (norm (p, "rate") - 100.0f / 127.0f) < 1.0e-3f);
        map.startLearn (1);
        map.process (cc (74, 10));                                       // same CC: moves to depth
        CHECK (map.getCC (1) == 74 && map.getCC (0) == -1);
        map.startLearn (1);
        map.startLearn (1);                                              // second request cancels the learn
        CHECK (map.getLearning() == -1);
        map.clear (1);
        CHECK (map.getCC (1) == -1);
    }

    // 3. sync alias: with SYNC on, the rate CC moves the division
    {
        TestProcessor p;
        MidiMap map (p.apvts, { "rate" });
        map.setDefault ("rate", 11);
        map.setSyncAlias ("rate", "sync", "division");
        map.process (cc (11, 127));
        pump (80);
        CHECK (std::abs (norm (p, "rate") - 1.0f) < 1.0e-4f);
        p.apvts.getParameter ("sync")->setValueNotifyingHost (1.0f);
        const float rateBefore = norm (p, "rate");
        map.process (cc (11, 0));
        pump (80);
        CHECK (norm (p, "division") < 1.0e-4f);
        CHECK (std::abs (norm (p, "rate") - rateBefore) < 1.0e-6f);
    }

    // 4. soft takeover: ignored until the controller passes the knob
    {
        TestProcessor p;
        MidiMap map (p.apvts, { "depth" });
        map.setDefault ("depth", 1);
        p.apvts.getParameter ("depth")->setValueNotifyingHost (0.5f);
        map.setSoftTakeover (true);
        map.process (cc (1, 0));   pump (80);
        CHECK (std::abs (norm (p, "depth") - 0.5f) < 1.0e-4f);           // far below the knob: ignored
        map.process (cc (1, 127)); pump (80);                            // jumps over the knob: picked up
        CHECK (std::abs (norm (p, "depth") - 1.0f) < 1.0e-4f);
        map.process (cc (1, 20));  pump (80);                            // once picked up it follows freely
        CHECK (std::abs (norm (p, "depth") - 20.0f / 127.0f) < 1.0e-3f);
    }

    // 5. tap tempo: note-on and a rising edge of the tap CC
    {
        TestProcessor p;
        MidiMap map (p.apvts, { "depth" });
        CHECK (! map.consumeTap());
        juce::MidiBuffer n; n.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        map.process (n);
        CHECK (! map.consumeTap());                                      // not enabled yet
        map.enableTap (85);
        map.process (n);
        CHECK (map.consumeTap() && ! map.consumeTap());
        map.process (cc (85, 127));
        CHECK (map.consumeTap());
        map.process (cc (85, 127));                                      // held high: no second tap
        CHECK (! map.consumeTap());
        map.process (cc (85, 0));
        map.process (cc (85, 127));
        CHECK (map.consumeTap());
    }

    // 6. state: bindings and soft takeover survive toTree / fromTree; a missing tree keeps the factory map
    {
        TestProcessor p;
        MidiMap a (p.apvts, { "rate", "depth" });
        a.setDefault ("depth", 1);
        a.startLearn (0); a.process (cc (33, 5));
        a.setSoftTakeover (true);
        const auto t = a.toTree();

        TestProcessor q;
        MidiMap b (q.apvts, { "rate", "depth" });
        b.setDefault ("depth", 1);
        b.fromTree (juce::ValueTree());                                  // old session
        CHECK (b.getCC (1) == 1 && b.getCC (0) == -1);
        b.fromTree (t);
        CHECK (b.getCC (0) == 33 && b.getCC (1) == 1 && b.isSoftTakeover());

        auto xml = t.createXml();                                        // round trip through the XML the plugins save
        CHECK (xml != nullptr);
        TestProcessor r;
        MidiMap c (r.apvts, { "rate", "depth" });
        c.fromTree (juce::ValueTree::fromXml (*xml));
        CHECK (c.getCC (0) == 33 && c.getCC (1) == 1);
    }

    // 7. no allocation in process (audio thread), also while learning and with many events
    {
        TestProcessor p;
        MidiMap map (p.apvts, { "rate", "depth" });
        map.setDefault ("depth", 1);
        map.enableTap (85);
        juce::MidiBuffer big;
        for (int i = 0; i < 64; ++i) { big.addEvent (juce::MidiMessage::controllerEvent (1, 1, i * 2), i); big.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 90), i); }
        map.startLearn (0);
        gAllocs = 0; gCount = true;
        map.process (big);
        map.process (big);
        (void) map.consumeTap();
        gCount = false;
        CHECK (gAllocs.load() == 0);
    }

    // 8. the buffer is not changed
    {
        TestProcessor p;
        MidiMap map (p.apvts, { "depth" });
        map.setDefault ("depth", 1);
        auto b = cc (1, 99);
        const int before = b.getNumEvents();
        map.process (b);
        CHECK (b.getNumEvents() == before);
    }

    std::printf (failures == 0 ? "MidiMapTest: all checks passed\n" : "MidiMapTest: %d check(s) failed\n", failures);
    return failures == 0 ? 0 : 1;
}
