// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
// The real MS-1 processor, driven like a host would: the reported latency equals the measured delay of an impulse in every mode, NaN / inf / denormal /
// DC input never gives a non-finite output, block sizes 1 .. 8192 at every sample rate, randomised parameters and repeated prepareToPlay stay finite and
// bounded, silence in gives silence out, the state survives a save / load round trip.
#include <cstdio>
#include <random>
#include "PluginProcessor.h"

namespace
{
int problems = 0;
void check (bool ok, const juce::String& what) { std::printf ("  %-86s %s\n", what.toRawUTF8(), ok ? "ok" : "FAILED"); if (! ok) ++problems; }

void prepare (MS1AudioProcessor& p, double fs, int block)
{
    p.setPlayConfigDetails (2, 2, fs, block);
    p.prepareToPlay (fs, block);
}

// runs `l` / `r` through the processor in blocks of `block` samples, in place
void run (MS1AudioProcessor& p, std::vector<float>& l, std::vector<float>& r, int block)
{
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;
    for (size_t pos = 0; pos < l.size(); pos += (size_t) block)
    {
        const int n = (int) std::min<size_t> ((size_t) block, l.size() - pos);
        buf.setSize (2, n, false, false, true);
        for (int i = 0; i < n; ++i) { buf.setSample (0, i, l[pos + (size_t) i]); buf.setSample (1, i, r[pos + (size_t) i]); }
        p.processBlock (buf, midi);
        for (int i = 0; i < n; ++i) { l[pos + (size_t) i] = buf.getSample (0, i); r[pos + (size_t) i] = buf.getSample (1, i); }
    }
}

bool finiteAndBounded (const std::vector<float>& v, float bound)
{
    for (float x : v) if (! std::isfinite (x) || std::abs (x) > bound) return false;
    return true;
}

void setParam (MS1AudioProcessor& p, const char* id, float plain)
{
    if (auto* par = p.apvts.getParameter (id)) par->setValueNotifyingHost (par->convertTo0to1 (plain));
}

void pump (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }      // lets the message-thread timers and the convolution threads work
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const bool perfOnly = argc > 1 && juce::String (argv[1]) == "perf";                      // MS1HostTest perf: only the CPU measurement (for profiling)
    std::mt19937 rng (42);
    std::uniform_real_distribution<float> uni (-1.0f, 1.0f), u01 (0.0f, 1.0f);

    if (! perfOnly)
    {
    std::printf ("latency: the reported value equals the measured delay of an impulse\n");
    struct Config { const char* name; std::vector<std::pair<const char*, float>> values; double fs; };
    const std::vector<Config> configs = {
        { "default, 48 kHz", {}, 48000.0 }, { "default, 44.1 kHz", {}, 44100.0 }, { "default, 96 kHz", {}, 96000.0 }, { "default, 192 kHz", {}, 192000.0 },
        { "lookahead 5 ms", { { "limLook", 5.0f } }, 48000.0 },
        { "EQ linear phase", { { "eqPhase", 1.0f } }, 48000.0 }, { "multiband linear phase", { { "mbPhase", 1.0f } }, 48000.0 },
        { "EQ and multiband linear, 96 kHz", { { "eqPhase", 1.0f }, { "mbPhase", 1.0f } }, 96000.0 },
        { "every stage on, minimum phase", { { "mbOn", 1.0f }, { "imgOn", 1.0f }, { "colourOn", 1.0f }, { "busOn", 1.0f }, { "colourDrive", 1.0f } }, 48000.0 },
        { "power off", { { "power", 0.0f } }, 48000.0 } };
    for (const auto& c : configs)
    {
        auto proc = std::unique_ptr<MS1AudioProcessor> (new MS1AudioProcessor());
        for (const auto& [id, v] : c.values) setParam (*proc, id, v);
        prepare (*proc, c.fs, 512);
        pump (1500);                                                                          // the convolutions install their impulse responses
        const int total = (int) (c.fs * 2.0);
        std::vector<float> l ((size_t) total, 0.0f), r ((size_t) total, 0.0f);
        const int at = 5000; l[(size_t) at] = r[(size_t) at] = 0.4f;
        run (*proc, l, r, 512);
        size_t pk = 0; for (size_t i = 0; i < l.size(); ++i) if (std::abs (l[i]) > std::abs (l[pk])) pk = i;
        const int measured = (int) pk - at, reported = proc->getLatencySamples();
        const bool stagesOn = juce::String (c.name).contains ("every stage");                // the allpass phase of the crossovers and the filters of the stages move the peak of an impulse by a sample or two
        check (stagesOn ? std::abs (measured - reported) <= 2 : measured == reported, juce::String (c.name) + ": reported " + juce::String (reported) + ", measured " + juce::String (measured) + " samples");
    }

    std::printf ("robustness\n");
    {   // block sizes at every sample rate: finite and bounded
        bool ok = true;
        for (double fs : { 44100.0, 48000.0, 96000.0, 192000.0 })
            for (int block : { 1, 7, 64, 512, 4096, 8192 })
            {
                auto proc = std::unique_ptr<MS1AudioProcessor> (new MS1AudioProcessor());
                prepare (*proc, fs, block);
                const int total = block == 1 ? (int) (fs * 0.1) : (int) (fs * 0.5);
                std::vector<float> l ((size_t) total), r ((size_t) total);
                for (int i = 0; i < total; ++i) { l[(size_t) i] = 0.4f * uni (rng); r[(size_t) i] = 0.4f * uni (rng); }
                run (*proc, l, r, block);
                if (! finiteAndBounded (l, 4.0f) || ! finiteAndBounded (r, 4.0f)) { ok = false; std::printf ("    not finite at %.0f Hz, block %d\n", fs, block); }
            }
        check (ok, "block sizes 1 .. 8192 at 44.1 / 48 / 96 / 192 kHz: finite and bounded");
    }
    {   // NaN, inf, huge values, denormals, DC
        auto proc = std::unique_ptr<MS1AudioProcessor> (new MS1AudioProcessor());
        setParam (*proc, "mbOn", 1.0f); setParam (*proc, "imgOn", 1.0f); setParam (*proc, "colourOn", 1.0f); setParam (*proc, "colourDrive", 80.0f); setParam (*proc, "busOn", 1.0f);
        prepare (*proc, 48000.0, 512);
        const int total = 48000; std::vector<float> l ((size_t) total), r ((size_t) total);
        for (int i = 0; i < total; ++i)
        {
            const int k = i % 5;
            l[(size_t) i] = k == 0 ? std::numeric_limits<float>::quiet_NaN() : k == 1 ? std::numeric_limits<float>::infinity() : k == 2 ? 1.0e30f : k == 3 ? -std::numeric_limits<float>::infinity() : 0.3f;
            r[(size_t) i] = k == 0 ? -std::numeric_limits<float>::infinity() : 0.3f;
        }
        run (*proc, l, r, 512);
        check (finiteAndBounded (l, 4.0f) && finiteAndBounded (r, 4.0f), "NaN, inf and 1e30 input: the output is finite and bounded");
        std::fill (l.begin(), l.end(), 1.0e-40f); std::fill (r.begin(), r.end(), 1.0e-40f);
        run (*proc, l, r, 512);
        check (finiteAndBounded (l, 4.0f), "denormal input: finite");
        std::fill (l.begin(), l.end(), 0.5f); std::fill (r.begin(), r.end(), 0.5f);
        run (*proc, l, r, 512);
        check (finiteAndBounded (l, 4.0f) && finiteAndBounded (r, 4.0f), "DC input: finite and bounded");
        std::fill (l.begin(), l.end(), 0.0f); std::fill (r.begin(), r.end(), 0.0f);
        for (int rep = 0; rep < 4; ++rep) run (*proc, l, r, 512);
        double e = 0; for (int i = total / 2; i < total; ++i) e = std::max (e, (double) std::abs (l[(size_t) i]));
        check (e < 1.0e-3, "silence in after all of that: silence out (the tails have died)");
    }
    {   // randomised parameters, repeated prepareToPlay, changing sample rate
        bool ok = true;
        auto proc = std::unique_ptr<MS1AudioProcessor> (new MS1AudioProcessor());
        const double rates[4] = { 44100.0, 48000.0, 96000.0, 192000.0 };
        for (int trial = 0; trial < 60 && ok; ++trial)
        {
            for (auto* par : proc->getParameters())
                if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (par))
                {
                    const auto id = rp->paramID;
                    if (id == "power" || id == "refPlay" || id == "refAB") continue;
                    rp->setValueNotifyingHost (u01 (rng));
                }
            const double fs = rates[trial % 4]; const int block = (trial % 3 == 0) ? 64 : (trial % 3 == 1) ? 512 : 2048;
            prepare (*proc, fs, block);
            const int total = (int) (fs * 0.4);
            std::vector<float> l ((size_t) total), r ((size_t) total);
            for (int i = 0; i < total; ++i) { const float s = 0.7f * std::sin (6.2831853f * 220.0f * (float) i / (float) fs); l[(size_t) i] = s + 0.2f * uni (rng); r[(size_t) i] = -s + 0.2f * uni (rng); }
            run (*proc, l, r, block);
            if (trial % 10 == 0) pump (200);
            if (! finiteAndBounded (l, 8.0f) || ! finiteAndBounded (r, 8.0f)) { ok = false; std::printf ("    trial %d (%.0f Hz, block %d) not finite or too loud\n", trial, fs, block); }
        }
        check (ok, "60 random parameter sets with repeated prepareToPlay and changing rates: finite and bounded");
    }

    std::printf ("state\n");
    {
        auto a = std::unique_ptr<MS1AudioProcessor> (new MS1AudioProcessor());
        prepare (*a, 48000.0, 512);
        a->presets.applyFactory (5);
        a->setSpecOption (MS1AudioProcessor::SpecMode, 1); a->setSpecOption (MS1AudioProcessor::SpecSource, 2);
        setParam (*a, "eq3Gain", 4.5f); setParam (*a, "mb2Ratio", 7.0f);
        juce::MemoryBlock block; a->getStateInformation (block);
        auto b = std::unique_ptr<MS1AudioProcessor> (new MS1AudioProcessor());
        prepare (*b, 48000.0, 512);
        b->setStateInformation (block.getData(), (int) block.getSize());
        bool same = true;
        for (auto* par : a->getParameters())
            if (auto* pa = dynamic_cast<juce::RangedAudioParameter*> (par))
                if (auto* pb = b->apvts.getParameter (pa->paramID)) if (std::abs (pa->getValue() - pb->getValue()) > 1.0e-6f) { same = false; std::printf ("    %s differs\n", pa->paramID.toRawUTF8()); }
        check (same, "every parameter survives a state save / load");
        check (b->presets.name() == a->presets.name() && b->specOption (MS1AudioProcessor::SpecMode) == 1 && b->specOption (MS1AudioProcessor::SpecSource) == 2, "the preset name and the spectrum options survive");
        juce::MemoryBlock junk; junk.append ("not xml", 7);
        b->setStateInformation (junk.getData(), (int) junk.getSize());
        check (true, "garbage state data is ignored without a crash");
    }
    {   // presets and undo
        auto p = std::unique_ptr<MS1AudioProcessor> (new MS1AudioProcessor());
        prepare (*p, 48000.0, 512);
        bool all = true;
        for (int i = 0; i < (int) p->presets.factory().size(); ++i)
        {
            p->presets.applyFactory (i);
            std::vector<float> l (4800, 0.3f), r (4800, 0.3f); run (*p, l, r, 512);
            if (! finiteAndBounded (l, 4.0f)) all = false;
        }
        check (all, "every factory preset applies and renders finite audio");
        p->presets.applyFactory (0);
        pump (300);                                                                          // APVTS writes the parameters into its (undoable) tree on a timer of the message thread
        const float before = p->apvts.getRawParameterValue ("busOn")->load();
        p->undoManager.beginNewTransaction();
        p->presets.applyFactory (1);
        pump (300);
        const float after = p->apvts.getRawParameterValue ("busOn")->load();
        p->undoManager.undo();
        pump (300);
        check (before < 0.5f && after > 0.5f && p->apvts.getRawParameterValue ("busOn")->load() < 0.5f, "undo takes a preset back");
    }

    }
    std::printf ("performance (48 kHz, 512 samples, a release build; the spec: chain under 12 %% of one core, with linear phase under 20 %%)\n");
    {
        const auto cpu = [&] (std::initializer_list<std::pair<const char*, float>> values, const char* name, double limit)
        {
            auto proc = std::unique_ptr<MS1AudioProcessor> (new MS1AudioProcessor());
            for (const auto& [id, v] : values) setParam (*proc, id, v);
            prepare (*proc, 48000.0, 512);
            pump (1500);
            juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer midi;
            for (int i = 0; i < 512; ++i) { buf.setSample (0, i, 0.3f * uni (rng)); buf.setSample (1, i, 0.3f * uni (rng)); }
            for (int w = 0; w < 100; ++w) { auto copy = buf; proc->processBlock (copy, midi); }                            // warm up
            const int blocks = 2000; const auto t0 = juce::Time::getHighResolutionTicks();
            for (int b = 0; b < blocks; ++b) { auto copy = buf; proc->processBlock (copy, midi); }
            const double sec = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
            const double percent = 100.0 * (sec / blocks) / (512.0 / 48000.0);
            std::printf ("  %-58s %5.2f %% of one core\n", name, percent);
            check (percent < limit, juce::String (name) + " under " + juce::String (limit, 0) + " %");
        };
        cpu ({}, "default (limiter, loudness, spectrum, meters)", 12.0);
        cpu ({ { "mbOn", 1.0f }, { "imgOn", 1.0f }, { "colourOn", 1.0f }, { "colourDrive", 60.0f }, { "busOn", 1.0f }, { "eq4Gain", 3.0f }, { "eq4Dyn", 1.0f } }, "every stage on, minimum phase", 12.0);
        cpu ({ { "mbOn", 1.0f }, { "imgOn", 1.0f }, { "colourOn", 1.0f }, { "colourDrive", 60.0f }, { "busOn", 1.0f }, { "eq4Gain", 3.0f }, { "eqPhase", 1.0f }, { "mbPhase", 1.0f } }, "every stage on, linear-phase EQ and crossovers", 20.0);
    }

    {   // A / B: the copy button puts the current settings in the other slot, switching swaps the slots
        auto p = std::unique_ptr<MS1AudioProcessor> (new MS1AudioProcessor());
        prepare (*p, 48000.0, 512);
        const auto gain = [&] { return p->apvts.getRawParameterValue ("eq3Gain")->load(); };
        setParam (*p, "eq3Gain", 4.5f); pump (200);
        p->presets.copyToOtherSlot();                                                      // A = 4.5 dB, copied to B
        setParam (*p, "eq3Gain", -3.0f); pump (200);                                       // A is now -3 dB
        p->presets.switchAB (1); pump (300);
        check (std::abs (gain() - 4.5f) < 0.05f, "TO B: after switching to B the copied settings (+4.5 dB) are there, got " + juce::String (gain(), 2));
        setParam (*p, "eq3Gain", 9.0f); pump (200);                                        // B = 9 dB
        p->presets.switchAB (0); pump (300);
        check (std::abs (gain() + 3.0f) < 0.05f, "back on A: A kept its own settings (-3 dB), got " + juce::String (gain(), 2));
        p->presets.copyToOtherSlot(); pump (100);                                          // TO B again: B = A
        p->presets.switchAB (1); pump (300);
        check (std::abs (gain() + 3.0f) < 0.05f, "TO B copies A over B (B reads -3 dB), got " + juce::String (gain(), 2));
    }
    std::printf ("%d problems\n", problems);
    return problems == 0 ? 0 : 1;
}
