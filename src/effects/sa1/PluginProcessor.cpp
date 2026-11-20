// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout SA1AudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    const auto flag = [&layout] (const char* id, const char* name, bool def)
    {
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def, AudioParameterBoolAttributes().withMeta (String (id) == "power" || String (id) == "bypass")));
    };
    const auto pick = [&layout] (const char* id, const char* name, StringArray items, int def)
    {
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def));
    };
    flag ("power", "Power", true);
    flag ("bypass", "Bypass", false);
    pick ("fftSize", "FFT Size", { "1024", "2048", "4096", "8192", "16384" }, 2);
    pick ("channel", "Channel", { "Left", "Right", "Sum" }, 2);
    pick ("range", "Range", { "60 dB", "90 dB", "120 dB" }, 1);
    pick ("smooth", "Smoothing", { "Off", "Low", "Medium", "High" }, 1);
    pick ("avg", "Averaging", { "1x", "2x", "4x", "8x" }, 1);
    pick ("display", "Display", { "Filled", "Line" }, 0);
    pick ("scale", "Frequency Scale", { "Log", "Linear" }, 0);
    flag ("hold", "Peak Hold", false);
    flag ("freeze", "Freeze", false);
    flag ("harmonics", "Harmonic Markers", true);
    pick ("mode", "Analysis Mode", { "Spectrum", "Spectrogram", "Octave Bands" }, 0);
    pick ("bands", "Octave Bands Width", { "1/3 Octave", "1 Octave" }, 0);
    pick ("speed", "Spectrogram Speed", { "Fast", "Medium", "Slow" }, 0);
    flag ("compare", "Compare With Snapshot", false);
    return layout;
}

namespace
{
std::vector<vibr::PresetManager::Factory> sa1FactoryPresets()
{
    return {
        { "Init", "A balanced view: 4K FFT, light smoothing and averaging", {} },
        { "Guitar Detail", "A 16K FFT with 4x averaging: the low strings and the harmonics resolve", { { "fftSize", 4 }, { "avg", 2 }, { "smooth", 1 }, { "range", 1 }, { "harmonics", 1 } } },
        { "Fast Look", "A 2K FFT with no averaging: it follows picking and palm mutes", { { "fftSize", 1 }, { "avg", 0 }, { "smooth", 0 }, { "range", 1 } } },
        { "Tonal Balance", "An 8K FFT, 8x averaging and heavy smoothing: the broad shape of the sound", { { "fftSize", 3 }, { "avg", 3 }, { "smooth", 3 }, { "range", 0 }, { "harmonics", 0 } } },
        { "Octave Balance", "1/3-octave bands with 8x averaging: the tonal balance at a glance", { { "mode", 2 }, { "bands", 0 }, { "avg", 3 }, { "fftSize", 3 }, { "range", 0 } } },
        { "Spectrogram", "Frequency over time: watch the harmonics, the decay and the modulation", { { "mode", 1 }, { "fftSize", 3 }, { "avg", 1 }, { "smooth", 0 }, { "range", 1 } } },
        { "Peak Hunt", "A 16K FFT with the peak hold on, as a line: find the resonances", { { "fftSize", 4 }, { "hold", 1 }, { "display", 1 }, { "avg", 1 }, { "smooth", 0 }, { "range", 2 } } },
    };
}
}

SA1AudioProcessor::SA1AudioProcessor()
    : vibr::LabProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true),
                          "SA1", createLayout(), "SA-1 Presets", sa1FactoryPresets(), {})
{
    for (auto& s : ringL) s.store (0.0f);
    for (auto& s : ringR) s.store (0.0f);
}

bool SA1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono()) && layouts.getMainInputChannelSet() == out;
}

void SA1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const int n = buffer.getNumSamples(), channels = buffer.getNumChannels();
    if (n == 0 || channels == 0) return;
    const float* l = buffer.getReadPointer (0);
    const float* r = buffer.getReadPointer (channels > 1 ? 1 : 0);
    int pos = ringPos.load (std::memory_order_relaxed);
    for (int i = 0; i < n; ++i)                                          // the signal is only read: the audio passes untouched
    {
        ringL[pos].store (std::isfinite (l[i]) ? l[i] : 0.0f, std::memory_order_relaxed);
        ringR[pos].store (std::isfinite (r[i]) ? r[i] : 0.0f, std::memory_order_relaxed);
        pos = (pos + 1) & (kRing - 1);
    }
    ringPos.store (pos, std::memory_order_release);
}

juce::AudioProcessorEditor* SA1AudioProcessor::createEditor() { return new SA1AudioProcessorEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SA1AudioProcessor(); }
