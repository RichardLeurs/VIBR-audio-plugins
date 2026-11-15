# TR-8 Drum Machine (formerly TR-08): plan to complete the VST

## Context
The plugin builds and signs (VST3/AU/Standalone), has a synth engine, sampled-kit browser, 16-step A/B sequencer and an image-based GUI. Only the kit menu and kit selection were seen working in the real app; most of the audio, timing and host behaviour has never been verified, and several printed controls do nothing. This plan lists what is left to call it finished.

## Phase 0: housekeeping
- [deferred] Move `src/drumcomputer/tr08/samples` (637 MB, iCloud-synced) to `~/Library/Application Support/TR-08 Drum Machine/samples`, update the settings file `samplesRoot`, and keep it out of the repo (ignore rule).
- [done] Tempo display (98.9 / 69.0): not a bug. The Standalone restores the last panel state from its settings file, so it shows whatever tempo was last set. The default is 120.0.
- [done] Bundle IDs (`com.richardleurs.<name>`) and CMake CMP0175 warning fixed.

## Phase 1: verify what exists (by ear and by measurement)
1. Audio: each of the 11 synth voices and a few kits; check relative loudness, add per-kit normalisation if voices are uneven.
2. Sequencer: pattern toggling by click, A/B/AB variation, shuffle, tempo/fine, start/stop, tap tempo.
3. Host sync: load VST3/AU in a DAW (or AU host), check ppq lock, tempo changes, loop jumps, start/stop.
4. MIDI: GM note triggers and velocity/accent in the DAW.
5. Clicks: sweep every knob during playback; add parameter smoothing where zipper noise or clicks appear (per CLAUDE.md conventions).
6. [auval passed 2025-11-09: `auval -v aumu Tr08 Rlrs`; pluginval not installed] State: save/reload a session (parameters, both patterns, kit name); validate with `pluginval` and `auval -v aumu Tr08` if installed.

## Phase 2: finish the unwired controls
- [done] Measures / auto-fill knob: Manual, 16, 12, 8, 4, 2 measures; the last measure of each group plays the fill pattern. Param `measures`.
- [done] I/F variation switch (A/B): picks which part the fill measure plays. Param `fillPart`. - [done] Pre-scale switch: note value per step (1 = 8th triplet, 2 = 16th triplet, 3 = 16th, 4 = 32nd, default 3), per the TR-08 manual (param `preScale`).
- Trigger-out: no audio equivalent; leave decorative or send MIDI out (decide).
- Tone / snappy on sampled voices: stay inert (user decision).

- [done] Red [CLEAR] button, per the TR-08 manual: PATTERN CLEAR clears the A/B variation chosen (AB = both), back to 16 + 0 steps; 1st PART applies the PRE-SCALE switch; hold it + a step button sets the STEP NUMBER of the shown part (2nd part continues after the 1st, total up to 32 steps, A and B share it); with TAP the 2nd part goes to 0; COMPOSE clears the selected instrument (the plugin has no rhythm track). Pre-scale only takes effect on [CLEAR].

## Phase 3: features worth adding
- Per-instrument output routing (multi-out bus) for DAW mixing.
- Pattern memory: several patterns saved in plugin state, with copy/clear.
- Sample voice polish: velocity layers if kit files provide them, round-robin for repeated names.
- Kit menu: favourites, remembering the last kit per session, progress while decoding.

## Phase 4: robustness and release
- Run the offline harnesses under ASan/UBSan again after any DSP change.
- Check CPU use at 44.1/48/96 kHz and small buffers; no allocation or locks in `processBlock`.
- Graceful handling of missing/evicted/corrupt sample files and an empty samples folder.
- Resizable window: check scaling at min/max size.
- Final clean build of `TR-08_All` from `~/Developer/VIBR`, signed copies to the plug-in folders, update `.claude/agents/CLAUDE.md`.

## Verification
Build `TR-08_All`; run the Standalone app; run the Phase 1 checklist and record pass/fail for each item here; run pluginval/auval if available.

## Critical files
`src/drumcomputer/tr08/{TR08DSP.h, KitMapper.h, PluginProcessor.cpp/.h, PluginEditor.cpp/.h, CMakeLists.txt}`, `.claude/agents/CLAUDE.md`.
