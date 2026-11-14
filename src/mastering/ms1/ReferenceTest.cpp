// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
// Check of the MS-1 reference player: a WAV file at 44.1 kHz is loaded at a host rate of 48 kHz (resampled), analysed (a -23 dBFS 997 Hz stereo sine reads
// -23 LUFS and its spectrum peaks at 1 kHz), played back with the right level and length, looped, seeked and synchronised to a host position.
#include <JuceHeader.h>
#include <array>
#include <cstdio>
#include <random>
#include "ReferencePlayer.h"
#include "MasteringStages.h"

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    int problems = 0;
    const auto check = [&problems] (bool ok, const char* what) { std::printf ("  %-82s %s\n", what, ok ? "ok" : "FAILED"); if (! ok) ++problems; };

    const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ms1_reference_test.wav");
    file.deleteFile();
    {
        const double rate = 44100.0; const int len = (int) (rate * 6.0);
        juce::AudioBuffer<float> buf (2, len);
        for (int i = 0; i < len; ++i) { const float s = juce::Decibels::decibelsToGain (-23.0f) * std::sin (6.283185307f * 997.0f * (float) i / (float) rate); buf.setSample (0, i, s); buf.setSample (1, i, s); }
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::FileOutputStream> out (file.createOutputStream());
        std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (out.get(), rate, 2, 24, {}, 0));
        out.release();
        writer->writeFromAudioSampleBuffer (buf, 0, len);
    }

    ReferencePlayer ref;
    ref.prepare (48000.0);
    ref.loadAsync (file);
    for (int i = 0; i < 400 && ref.isLoading(); ++i) juce::Thread::sleep (50);
    check (! ref.isLoading() && ref.hasData() && ! ref.hasFailed(), "the file loads in the background");
    std::printf ("  name '%s', %.2f s, %.2f LUFS\n", ref.name().toRawUTF8(), ref.seconds(), (double) ref.integratedLufs());
    check (std::abs (ref.seconds() - 6.0) < 0.01, "it is resampled to the host rate (6.00 s stays 6.00 s)");
    check (std::abs (ref.integratedLufs() + 23.0f) < 0.3f, "the integrated loudness of a -23 dBFS sine reads -23 LUFS");
    int peakBand = 0; const float* sp = ref.spectrum();
    for (int b = 0; b < ms1::kBands; ++b) if (sp[b] > sp[peakBand]) peakBand = b;
    const float peakHz = 20.0f * std::pow (1000.0f, ((float) peakBand + 0.5f) / (float) ms1::kBands);
    std::printf ("  spectrum peak at %.0f Hz, %.1f dB\n", peakHz, (double) sp[peakBand]);
    check (peakHz > 900.0f && peakHz < 1100.0f && std::abs (sp[peakBand] + 23.0f) < 3.0f, "the long-term spectrum peaks at 1 kHz at about -23 dB");

    std::vector<float> l (4800), r (4800);
    check (ref.process (l.data(), r.data(), 4800, true, -1), "playing returns audio");
    float peak = 0; for (int i = 600; i < 4800; ++i) peak = std::max (peak, std::abs (l[(size_t) i]));
    check (std::abs (juce::Decibels::gainToDecibels (peak) + 23.0f) < 0.2f, "the level after resampling is -23 dBFS");
    check (std::abs (ref.positionFraction() - 0.1f / 6.0f) < 1.0e-3f, "the position advances by the number of samples played");
    ref.process (l.data(), r.data(), 4800, false, -1);
    check (l[100] == 0.0f && std::abs (ref.positionFraction() - 0.1f / 6.0f) < 1.0e-3f, "paused: silence, the position stays");
    ref.seek (0.5f);
    check (std::abs (ref.positionFraction() - 0.5f) < 1.0e-3f, "seek moves the position");
    ref.process (l.data(), r.data(), 4800, true, 48000 * 6 + 24000);                       // host position 1 bar past the end: it loops
    check (std::abs (ref.positionFraction() - ((24000.0f + 4800.0f) / (48000.0f * 6.0f))) < 1.0e-3f, "following the host: the position is the host position, looped");
    ref.seek (0.9999f);
    std::vector<float> big (48000), big2 (48000);
    ref.process (big.data(), big2.data(), 48000, true, -1);
    check (ref.positionFraction() < 0.2f, "the playback loops at the end of the file");

    // the linear-phase EQ through juce::dsp::Convolution: +6 dB bell at 1 kHz, the delay is N / 2 samples, flat elsewhere
    {
        std::printf ("linear-phase EQ (convolution)\n");
        const float fs = 48000.0f; const int n = ms1::linearIrLength (fs);
        std::array<ms1::EqBand, ms1::kEqBands> bands; for (auto& b : bands) b.on = false;
        bands[3].on = true; bands[3].freq = 1000.0f; bands[3].gainDb = 6.0f; bands[3].q = 1.0f;
        const auto ir = ms1::designLinearIr (bands, fs, n);
        juce::AudioBuffer<float> irBuf (2, n);
        std::copy (ir.a.begin(), ir.a.end(), irBuf.getWritePointer (0)); std::copy (ir.b.begin(), ir.b.end(), irBuf.getWritePointer (1));
        juce::dsp::Convolution conv;
        conv.prepare ({ (double) fs, 512, 2 });
        conv.loadImpulseResponse (std::move (irBuf), (double) fs, juce::dsp::Convolution::Stereo::yes, juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
        juce::Thread::sleep (1500);                                                       // the impulse response is installed by a background thread
        { std::vector<float> a (512, 0.0f), b (512, 0.0f); for (int k = 0; k < 8; ++k) { float* ch[2] = { a.data(), b.data() }; juce::dsp::AudioBlock<float> bl (ch, 2, 512); conv.process (juce::dsp::ProcessContextReplacing<float> (bl)); } conv.reset(); }
        const int total = 48000;
        std::vector<float> l ((size_t) total), r ((size_t) total), imp ((size_t) total, 0.0f);
        for (int i = 0; i < total; ++i) { l[(size_t) i] = r[(size_t) i] = 0.2f * std::sin (6.283185307f * 1000.0f * (float) i / fs); }
        imp[100] = 1.0f;
        for (int pos = 0; pos < total; pos += 512) { float* ch[2] = { l.data() + pos, r.data() + pos }; juce::dsp::AudioBlock<float> bl (ch, 2, (size_t) std::min (512, total - pos)); conv.process (juce::dsp::ProcessContextReplacing<float> (bl)); }
        double e = 0, e0 = 0; for (int i = 24000; i < total; ++i) { e += (double) l[(size_t) i] * l[(size_t) i]; e0 += 0.04 * 0.5; }
        const double gain = 10.0 * std::log10 (e / e0);
        std::printf ("  1 kHz tone through the convolution: %+.2f dB\n", gain);
        check (std::abs (gain - 6.0) < 0.3, "a 1 kHz tone is raised by the +6 dB bell");
        conv.reset();
        std::vector<float> ir2 = imp, ir2r = imp;
        for (int pos = 0; pos < total; pos += 512) { float* ch[2] = { ir2.data() + pos, ir2r.data() + pos }; juce::dsp::AudioBlock<float> bl (ch, 2, (size_t) std::min (512, total - pos)); conv.process (juce::dsp::ProcessContextReplacing<float> (bl)); }
        size_t pk = 0; for (size_t i = 0; i < ir2.size(); ++i) if (std::abs (ir2[i]) > std::abs (ir2[pk])) pk = i;
        std::printf ("  an impulse at sample 100 peaks at sample %zu (delay %zu, expected %d)\n", pk, pk - 100, n / 2);
        check (pk - 100 == (size_t) n / 2 && conv.getLatency() == 0, "the delay is exactly N / 2 samples and the engine itself has no latency");
    }

    // the multiband's linear-phase crossovers through four juce::dsp::Convolution (one mono FIR each on a stereo block): the bands add up to the input delayed by N / 2
    {
        std::printf ("linear-phase crossovers (convolution)\n");
        const float fs = 48000.0f; const int n = ms1::linearIrLength (fs);
        const float freq[3] = { 120.0f, 1200.0f, 6000.0f };
        const auto bands = ms1::designLinearBands (freq, fs, n);
        std::array<juce::dsp::Convolution, 4> conv;
        for (size_t k = 0; k < 4; ++k)
        {
            conv[k].prepare ({ (double) fs, 512, 2 });
            juce::AudioBuffer<float> ir (1, n); std::copy (bands[k].begin(), bands[k].end(), ir.getWritePointer (0));
            conv[k].loadImpulseResponse (std::move (ir), (double) fs, juce::dsp::Convolution::Stereo::no, juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
        }
        juce::Thread::sleep (1500);
        for (auto& c : conv) { std::vector<float> a (512, 0.0f), b (512, 0.0f); for (int k = 0; k < 8; ++k) { float* ch[2] = { a.data(), b.data() }; juce::dsp::AudioBlock<float> bl (ch, 2, 512); c.process (juce::dsp::ProcessContextReplacing<float> (bl)); } c.reset(); }
        const int total = 48000; std::mt19937 rng (9); std::uniform_real_distribution<float> u (-0.5f, 0.5f);
        std::vector<float> in ((size_t) total), sum ((size_t) total, 0.0f), bandOne ((size_t) total);
        for (auto& v : in) v = u (rng);
        for (size_t k = 0; k < 4; ++k)
        {
            std::vector<float> l = in, r = in;
            for (int pos = 0; pos < total; pos += 512) { float* ch[2] = { l.data() + pos, r.data() + pos }; juce::dsp::AudioBlock<float> bl (ch, 2, (size_t) std::min (512, total - pos)); conv[k].process (juce::dsp::ProcessContextReplacing<float> (bl)); }
            for (int i = 0; i < total; ++i) sum[(size_t) i] += l[(size_t) i];
            if (k == 3) bandOne = l;
        }
        double err = 0, ref = 0; for (int i = n; i < total; ++i) { const double d = sum[(size_t) i] - in[(size_t) (i - n / 2)]; err += d * d; ref += (double) in[(size_t) (i - n / 2)] * in[(size_t) (i - n / 2)]; }
        std::printf ("  the sum of the four bands against the input delayed by %d samples: %.1f dB\n", n / 2, 10.0 * std::log10 (err / ref));
        check (10.0 * std::log10 (err / ref) < -60.0, "the four convolved bands add up to the delayed input (below -60 dB error)");
    }

    ReferencePlayer bad; bad.prepare (48000.0);
    bad.loadAsync (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("does_not_exist.wav"));
    for (int i = 0; i < 100 && bad.isLoading(); ++i) juce::Thread::sleep (20);
    check (bad.hasFailed() && ! bad.hasData(), "a missing file fails cleanly");

    file.deleteFile();
    std::printf ("%d problems\n", problems);
    return problems == 0 ? 0 : 1;
}
