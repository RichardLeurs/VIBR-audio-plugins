// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include <JuceHeader.h>
#include "MasteringEngine.h"

// MS-1 reference track (see .claude/mastering_specs.md, "Reference A/B"). A file (WAV / AIFF / FLAC / whatever the platform decodes) is read in a background
// thread into memory at the host sample rate and analysed once: integrated loudness (ITU-R BS.1770-4) and the long-term average spectrum (the same 128
// bands as the spectrum display). The audio thread plays it (looping) through its own transport; it can follow the host position. Nothing here allocates
// on the audio thread.
class ReferencePlayer : private juce::Thread
{
public:
    struct Data
    {
        juce::AudioBuffer<float> audio;                     // stereo, at the host sample rate
        float integratedLufs = -120.0f;
        float spectrum[ms1::kBands] {};                     // average level per band (dB), like the spectrum display
        juce::String name;
        double seconds = 0.0;
    };

    ReferencePlayer() : juce::Thread ("MS-1 reference loader") { formats.registerBasicFormats(); }
    ~ReferencePlayer() override { stopThread (5000); }

    void prepare (double sampleRate)
    {
        const bool changed = std::abs (sampleRate - fs) > 0.5;
        fs = sampleRate;
        if (changed && lastFile.existsAsFile()) loadAsync (lastFile);          // the file is stored at the host rate: read it again
    }

    void loadAsync (const juce::File& file)
    {
        stopThread (5000);
        lastFile = file;
        loading.store (true);
        failed.store (false);
        startThread();
    }

    void clear()
    {
        const juce::ScopedLock sl (lock);
        current.store (nullptr);
        lastFile = juce::File();
    }

    // the audio thread: the next n samples of the reference into l / r (silence when nothing is loaded or it is not running). hostSamples >= 0: follow the host.
    bool process (float* l, float* r, int n, bool running, juce::int64 hostSamples)
    {
        const Data* d = current.load (std::memory_order_acquire);
        if (d == nullptr || d->audio.getNumSamples() == 0) { std::fill (l, l + n, 0.0f); std::fill (r, r + n, 0.0f); return false; }
        const int len = d->audio.getNumSamples();
        if (hostSamples >= 0) pos.store ((int) (hostSamples % len), std::memory_order_relaxed);
        int p = pos.load (std::memory_order_relaxed);
        if (p >= len) p = 0;
        const float* a = d->audio.getReadPointer (0);
        const float* b = d->audio.getReadPointer (1);
        for (int i = 0; i < n; ++i)
        {
            l[i] = running ? a[p] : 0.0f; r[i] = running ? b[p] : 0.0f;
            if (running && ++p >= len) p = 0;
        }
        pos.store (p, std::memory_order_relaxed);
        return true;
    }

    void seek (float fraction)
    {
        if (const Data* d = current.load()) pos.store ((int) (juce::jlimit (0.0f, 0.9999f, fraction) * (float) d->audio.getNumSamples()));
    }

    // for the editor
    bool hasData() const { return current.load() != nullptr; }
    float positionFraction() const { const Data* d = current.load(); return d && d->audio.getNumSamples() > 0 ? (float) pos.load() / (float) d->audio.getNumSamples() : 0.0f; }
    juce::String name() const { const Data* d = current.load(); return d ? d->name : juce::String(); }
    double seconds() const { const Data* d = current.load(); return d ? d->seconds : 0.0; }
    float integratedLufs() const { const Data* d = current.load(); return d ? d->integratedLufs : -120.0f; }
    const float* spectrum() const { const Data* d = current.load(); return d ? d->spectrum : nullptr; }
    bool isLoading() const { return loading.load(); }
    bool hasFailed() const { return failed.load(); }
    juce::File file() const { return lastFile; }

private:
    void run() override
    {
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (lastFile));
        if (reader == nullptr || reader->lengthInSamples <= 0) { failed.store (true); loading.store (false); return; }

        const double srcRate = reader->sampleRate;
        const juce::int64 maxIn = (juce::int64) (srcRate * 60.0 * 20.0);                         // at most 20 minutes
        const int inLen = (int) std::min<juce::int64> (reader->lengthInSamples, maxIn);
        juce::AudioBuffer<float> in (2, inLen);
        if (! reader->read (&in, 0, inLen, 0, true, true)) { failed.store (true); loading.store (false); return; }
        if (reader->numChannels < 2) in.copyFrom (1, 0, in, 0, 0, inLen);                         // mono: both channels

        auto d = std::make_unique<Data>();
        d->name = lastFile.getFileNameWithoutExtension();
        if (std::abs (srcRate - fs) < 0.5) d->audio = std::move (in);
        else
        {
            const double ratio = srcRate / fs;
            const int outLen = (int) std::floor ((double) inLen / ratio);
            d->audio.setSize (2, outLen);
            for (int c = 0; c < 2; ++c)
            {
                juce::LagrangeInterpolator resampler;
                resampler.process (ratio, in.getReadPointer (c), d->audio.getWritePointer (c), outLen);
                if (threadShouldExit()) return;
            }
        }
        const int len = d->audio.getNumSamples();
        d->seconds = (double) len / fs;

        // analysis: loudness and the long-term average spectrum
        auto loud = std::make_unique<ms1::Loudness>();
        auto spec = std::make_unique<ms1::Spectrum>();
        loud->prepare ((float) fs); spec->prepare ((float) fs);
        std::array<double, ms1::kBands> sum {};
        int frames = 0;
        const float* a = d->audio.getReadPointer (0);
        const float* b = d->audio.getReadPointer (1);
        for (int pos0 = 0; pos0 < len; pos0 += 2048)
        {
            if (threadShouldExit()) return;
            const int n = std::min (2048, len - pos0);
            loud->process (a + pos0, b + pos0, n);
            for (int i = 0; i < n; ++i) spec->push (0.5f * (a[pos0 + i] + b[pos0 + i]));
            if (n == 2048 && pos0 > 8192)
            {
                for (int k = 0; k < ms1::kBands; ++k) sum[(size_t) k] += std::pow (10.0, (double) spec->bands[k].load() / 10.0);
                ++frames;
            }
        }
        d->integratedLufs = loud->integrated.load();
        for (int k = 0; k < ms1::kBands; ++k) d->spectrum[k] = frames > 0 ? (float) (10.0 * std::log10 (std::max (1.0e-12, sum[(size_t) k] / frames))) : -120.0f;

        {
            const juce::ScopedLock sl (lock);
            owned.push_back (std::move (d));
            pos.store (0);
            current.store (owned.back().get(), std::memory_order_release);
            while (owned.size() > 3) owned.erase (owned.begin());                                // older data is certainly no longer read by the audio thread
        }
        loading.store (false);
    }

    juce::AudioFormatManager formats;
    juce::CriticalSection lock;
    std::vector<std::unique_ptr<Data>> owned;
    std::atomic<const Data*> current { nullptr };
    std::atomic<int> pos { 0 };
    std::atomic<bool> loading { false }, failed { false };
    juce::File lastFile;
    double fs = 48000.0;
};
