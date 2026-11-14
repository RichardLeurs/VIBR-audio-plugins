// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <functional>

namespace vibr
{
// Averages the intervals between taps (a pause of more than 2 s starts a new sequence; the last 5 taps count).
struct TapAverager
{
    // returns true and the mean interval (ms) once at least two taps are in
    bool add (double nowMs, double& intervalMs)
    {
        if (count > 0 && nowMs - t[(count - 1)] < 60.0) return false;                  // debounce
        if (count > 0 && nowMs - t[count - 1] > 2000.0) count = 0;
        if (count == kMax) { for (int i = 1; i < kMax; ++i) t[i - 1] = t[i]; --count; }
        t[count++] = nowMs;
        if (count < 2) return false;
        intervalMs = (t[count - 1] - t[0]) / (double) (count - 1);
        return true;
    }
private:
    static constexpr int kMax = 5;
    double t[kMax] {};
    int count = 0;
};

// MIDI tap: the audio thread calls hit() (lock-free); a 60 Hz message-thread timer then calls onTap (timestamp in ms,
// from juce::Time::getMillisecondCounterHiRes) so the plugin can set its parameters with a change gesture.
// The editor's TAP button calls tap() directly (message thread). lastMs feeds the tap LED.
class TapTempo : private juce::Timer
{
public:
    explicit TapTempo (std::function<void (double)> handler) : onTap (std::move (handler)) { startTimerHz (60); }
    ~TapTempo() override { stopTimer(); }

    void hit() noexcept { stamp.store (juce::Time::getMillisecondCounterHiRes()); pending.store (true); }   // audio thread
    void tap() { fire (juce::Time::getMillisecondCounterHiRes()); }                                         // message thread
    double lastMs() const noexcept { return last.load(); }

private:
    void fire (double ms) { last.store (ms); if (onTap) onTap (ms); }
    void timerCallback() override { if (pending.exchange (false)) fire (stamp.load()); }

    std::function<void (double)> onTap;
    std::atomic<double> stamp { 0.0 }, last { -1.0e9 };
    std::atomic<bool> pending { false };
};
} // namespace vibr
