// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
// Renders the editor of a plugin to a PNG for the website, with a test signal in the plugin so the meters and displays show something:
//   Shot-<PLUGIN> <out.png> <signal> [seconds] [id=value ...]
// signal: silence | tone (a 220 Hz sine, -14 dBFS) | stepTone (silence, then the tone from the middle) | lissajous (L 220 Hz, R 330 Hz, -14 dBFS) | music (a kick every half second, chords and filtered noise, stereo) | noise
// The plugin is linked in (see tools/screenshot/CMakeLists.txt), its processor made with createPluginFilter(), the editor drawn at its own size with scale 1.
// The editor is not shown on screen: the audio is processed block by block faster than real time, then the timers of the editor are given a moment to fetch it.
#include <JuceHeader.h>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

class ShotApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "Shot"; }
    const juce::String getApplicationVersion() override { return "1.0"; }

    void initialise (const juce::String&) override
    {
        const auto args = getCommandLineParameterArray();
        if (args.size() < 2) { std::printf ("usage: Shot <out.png> <signal> [seconds] [id=value ...]\n"); setApplicationReturnValue (2); quit(); return; }
        const juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile (args[0]);
        const juce::String signal = args[1];
        double seconds = 6.0;
        std::vector<std::pair<juce::String, float>> overrides;
        for (int i = 2; i < args.size(); ++i)
        {
            if (args[i].containsChar ('=')) overrides.push_back ({ args[i].upToFirstOccurrenceOf ("=", false, false), args[i].fromFirstOccurrenceOf ("=", false, false).getFloatValue() });
            else seconds = args[i].getDoubleValue();
        }

        std::unique_ptr<juce::AudioProcessor> proc (createPluginFilter());
        const double fs = 48000.0; const int block = 512;
        proc->setPlayConfigDetails (2, 2, fs, block);
        proc->prepareToPlay (fs, block);
        for (const auto& [id, value] : overrides)
        {
            bool found = false;
            for (auto* par : proc->getParameters())
                if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (par))
                    if (rp->paramID == id) { rp->setValueNotifyingHost (rp->convertTo0to1 (value)); found = true; }
            if (! found) std::printf ("no parameter '%s'\n", id.toRawUTF8());
        }
        juce::MessageManager::getInstance()->runDispatchLoopUntil (300);                  // lets the value tree and the editor pick the parameters up

        std::unique_ptr<juce::AudioProcessorEditor> editor (proc->createEditor());
        const auto amp = juce::Decibels::decibelsToGain (-14.0f);
        juce::Random rng (3);
        double hiss1 = 0.0, hiss2 = 0.0;
        juce::AudioBuffer<float> buf (2, block);
        juce::MidiBuffer midi;
        const long total = (long) (seconds * fs);
        for (long pos = 0; pos < total; pos += block)
        {
            for (int i = 0; i < block; ++i)
            {
                const double t = (double) (pos + i) / fs;
                float l = 0.0f, r = 0.0f;
                if (signal == "tone") l = r = amp * (float) std::sin (juce::MathConstants<double>::twoPi * 220.0 * t);
                else if (signal == "stepTone") { if (pos + i >= total / 2) l = r = amp * (float) std::sin (juce::MathConstants<double>::twoPi * 220.0 * t); }
                else if (signal == "lissajous") { l = amp * (float) std::sin (juce::MathConstants<double>::twoPi * 220.0 * t); r = amp * (float) std::sin (juce::MathConstants<double>::twoPi * 330.0 * t); }
                else if (signal == "music")
                {
                    const double tw = juce::MathConstants<double>::twoPi, beat = std::fmod (t, 0.5), bar = std::fmod (t, 2.0);
                    const double kick = std::sin (tw * (45.0 + 90.0 * std::exp (-beat * 30.0)) * beat) * std::exp (-beat * 9.0);                                    // a kick
                    const double pad = 0.5 * std::sin (tw * 110.0 * t) + 0.35 * std::sin (tw * 164.8 * t) + 0.3 * std::sin (tw * 220.0 * t + 0.4) + 0.2 * std::sin (tw * 329.6 * t);
                    const double hat = (rng.nextDouble() * 2.0 - 1.0) * std::exp (-std::fmod (t + 0.25, 0.5) * 40.0) * 0.5;                                       // a hat on the off beat
                    hiss1 += 0.25 * ((rng.nextDouble() * 2.0 - 1.0) - hiss1); hiss2 += 0.25 * ((rng.nextDouble() * 2.0 - 1.0) - hiss2);
                    const double swell = 0.55 + 0.45 * std::sin (tw * bar / 8.0 * 2.0 + 1.0);
                    l = (float) (0.55 * (0.8 * kick + 0.22 * pad * swell + hat + 0.35 * hiss1));
                    r = (float) (0.55 * (0.8 * kick + 0.22 * (0.6 * std::sin (tw * 110.0 * t) + 0.4 * std::sin (tw * 220.0 * t)) * swell + hat * 0.7 + 0.35 * hiss2));
                }
                else if (signal == "noise") { l = amp * rng.nextFloat() * 2.0f - amp; r = amp * rng.nextFloat() * 2.0f - amp; }
                buf.setSample (0, i, l); buf.setSample (1, i, r);
            }
            proc->processBlock (buf, midi);
            if ((pos / block) % 40 == 0) juce::MessageManager::getInstance()->runDispatchLoopUntil (1);
        }
        juce::MessageManager::getInstance()->runDispatchLoopUntil (400);                  // the 30 Hz timers of the editor read the final state

        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
        out.deleteFile();
        juce::FileOutputStream stream (out);
        juce::PNGImageFormat png;
        const bool ok = stream.openedOk() && png.writeImageToStream (image, stream);
        std::printf ("%s: %dx%d %s\n", out.getFullPathName().toRawUTF8(), image.getWidth(), image.getHeight(), ok ? "written" : "FAILED");
        editor.reset();
        proc.reset();
        setApplicationReturnValue (ok ? 0 : 1);
        quit();
    }
    void shutdown() override {}
};

START_JUCE_APPLICATION (ShotApp)
