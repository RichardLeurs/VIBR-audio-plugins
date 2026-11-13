Are you interested in audio plugins and want to help improving and expanding the plugins then send me a message.

# VIBR Audio Plugins

A collection of Vintage effects pedals (and some new) redesigned as audio plugins (VST3, Audio Unit and Standalone), vibe coded with JUCE 8 and C++20.

![DM-1 Delay Machine front panel](src/effects/dm1/assets/dm-1.png)

### Delay & Echo

| Plugin | What it is |
|---|---|
| DD-1 Dual Delay | two delays + shimmer reverb, boost, chorus, hall reverb, 4 panel memories; compact two-tab window (DELAY: tempo, delay A, stereo, delay B / EFFECTS: boost, ambience, shimmer) |
| DM-1 Delay Machine | 12-mode modulation delay, tap division, 4 panel memories |
| RE-2 Space Echo | tape echo + spring reverb, saturation, guitar / line, new / aged tape, 4 panel memories |
| MM-1 Analog Delay | analog-style delay, chorus and vibrato |
| SD-1 Digital Delay | digital delay: time, factor, regeneration, hi-cut, modulation, headroom meter |
| RL-1 Reverse Lab | creative time manipulation: reverse, tape scrub, stutter, glitch and repeat modes, freeze; length (free or synced to the host tempo), speed, smooth, random, repeats, mix, level |

### Modulation

| Plugin | What it is |
|---|---|
| CE-1 Chorus Ensemble | chorus / vibrato |
| BF-3 Flanger | stereo flanger: manual, res, depth, rate, 3 modes (standard, ultra, gate / pan) |
| PH-2 Phaser | phaser: rate, depth, res, 2 modes (4 or 8 stages) |
| TR-2 Tremolo | tremolo: rate, depth, wave (sine to square) |
| VC-1 Voltage Controller | modulation matrix: envelope follower, two LFOs, random and MIDI (mod wheel / expression) sources routed to filter, amp, vibrato and pan with cross-modulation; the rates can sync to the host tempo |
| VR-1 Vibe and Rotary | three swirling modulation effects: uni-vibe (chorus / vibrato type, tempo sync), a rotary speaker (slow / fast horn and drum) and ring mod (sine / triangle / square carrier), with a live picture of the effect |

### Drive

| Plugin | What it is |
|---|---|
| DS-1 Distortion | stompbox distortion: tone, level, dist |
| OD-2 Hybrid Drive | 12 drive types, 3-band tone, boost, noise gate, 4 panel memories |

### Dynamics

| Plugin | What it is |
|---|---|
| CS-3 Compression Sustainer | compressor / sustainer: level, tone, attack, sustain |
| LM-2 Limiter | peak limiter: level, tone, release, threshold |

### EQ

| Plugin | What it is |
|---|---|
| GE-7 Equalizer | graphic equalizer: 7 bands (100 Hz - 6.4 kHz, +-15 dB) and a level fader |
| EH-2 Enhancer | treble enhancer: mix (inverting), freq, sens |
| HM-2 Harmonic Machine | adds and reshapes harmonics: tube / tape / transistor / fold saturation, odd / even balance, octave up and sub octave, frequency-dependent drive |
| MF-1 Motion Filter | a filter moved by an LFO, an envelope follower or an 8-step sequencer: low / band / high pass, notch and vowel (formant) filters, tempo sync, glide |

### Amp & Cabinet

| Plugin | What it is |
|---|---|
| IR-2 Amp & Cabinet | amp and cabinet: 11 amp types, gain, level, bass / middle / treble, ambience |
| MA-1 Metal Amp and Cabinet | modern-metal rig: noise gate, tight boost, 4 amp voicings (8x oversampled), two impulse-response cabinets (load your own WAVs), EQ and limiter |

### Pitch

| Plugin | What it is |
|---|---|
| OC-5 Octave | vintage (monophonic) and poly octave: direct, +1 oct, -1 oct, -2 oct range |
| GR-1 Grain | granular processing of the playing: grain size, density, position, spray, pitch (with random), reverse, spread, freeze, tempo sync for the density, live grain-cloud display (scrolls with time) |
| PT-1 Phantom | four-voice harmonizer: interval, detune, level per voice; intelligent mode keeps the harmonies in key and scale (pitch detection); chord presets |

### Reverb

| Plugin | What it is |
|---|---|
| BS-1 Reverb | 12 reverb types (room, hall, plate, spring, swell, bloom, cloud, chorale, shimmer, magneto, nonlinear, reflections) |
| SP-1 Space Engine | experimental spaces: shimmer, infinite, spectral, modulated and rooms modes, freeze; size, decay, diffusion, tone, motion, texture, mix, level |
| SR-1 Sympathetic Resonator | up to 12 virtual strings tuned to a scale that ring along with the playing; wood / metal / glass textures, decay, damping, tension, motion |

### Rhythm

| Plugin | What it is |
|---|---|
| SL-2 Slicer | rhythmic slicer: 11 patterns, 5 modes, tempo from the knob or the host |
| TR-8 Drum Machine | instrument: 11 voices or sampled kits, 16-step A/B sequencer, host sync |

### Multi effect

| Plugin | What it is |
|---|---|
| ME-1 Multi Effect | build your own chain from 39 basic effect modules with 2-3 knobs each: any number in a chain, drag to reorder, bypass, replace, tempo sync on the time and rate knobs; self-contained (it does not use the pedals above) |

**ME-1 modules** (full list with knob ranges and DSP notes: [`src/multieffect/me1/ME-1 specs.md`](src/multieffect/me1/ME-1%20specs.md)):

| Group | Modules |
|---|---|
| Delay & Echo | delay, echo, mod delay, ping-pong, reverse |
| Modulation | chorus, vibrato, uni-vibe, rotary, flanger, phaser, ring mod, tremolo, slicer |
| Drive | overdrive, distortion, fuzz, tape, tube, bit crusher, boost |
| Dynamics | compressor, sustainer, limiter, noise gate, expander, transient shaper, volume |
| Filter & EQ | filter, EQ, enhancer, auto wah |
| Pitch | octave, pitch shifter |
| Space | reverb, spring, shimmer, pan, stereo spreader |

The window has a strip of tiles at the top (one per module, coloured by group, with a bypass lamp and a remove cross; `+` adds a module, drag a tile to reorder, the wheel scrolls a long chain) and the selected module's knobs below. Delay and LFO modules have a SYNC button that makes the time or rate knob follow the host tempo (120 bpm when the host gives none). Input, output and power are host parameters; the module settings are saved in the session. No presets yet.


You can find the web page for the project on this [`website`](https://vibr.richardleurs.nl).

## Build

### What you need

The build is set up for macOS (the copy and signing steps use `codesign`, `xattr` and `~/Library/Audio/Plug-Ins`).

| Tool | Version | How to get it |
|---|---|---|
| Xcode Command Line Tools (compiler, `git`, `codesign`) | recent | `xcode-select --install` |
| CMake | 3.22 or newer | `brew install cmake` ([Homebrew](https://brew.sh)), or the installer from [cmake.org/download](https://cmake.org/download/) |
| JUCE | 8.0.3 | see below |
| Python 3 with `numpy` and `Pillow` | only to re-render the front-panel artwork (`make_background.py`) | `pip install numpy pillow` |

**JUCE.** The plugins are written for JUCE 8.0.3 (C++20). The easiest way is to clone it once into `~/Developer/JUCE`, where the build finds it automatically:

```bash
git clone --depth 1 --branch 8.0.3 https://github.com/juce-framework/JUCE.git ~/Developer/JUCE
```

Keep it outside iCloud-synced folders such as `~/Documents`, where files can be evicted to the cloud and reads stall. If `~/Developer/JUCE` does not exist, CMake downloads JUCE 8.0.3 from GitHub by itself (`FetchContent`), which needs an internet connection and takes longer on a fresh build. You can point to another checkout with `cmake -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE ...`.

**Signing (optional).** After each build the bundles are signed with a self-signed certificate if it exists in your login keychain; otherwise they get an ad-hoc signature, which is fine for your own machine.

### Building

The build on macOS is placed outside the iCloud synced folders in `~/Developer/VIBR`.

```bash
./build.sh                  # everything, for this Mac's architecture
./build.sh --arch x86_64    # everything for Intel Macs (arm64 and x86_64 are separate builds, so each bundle stays small)
./build.sh --arch all       # both, one after the other
./build.sh DD-1 SD-1        # only these plugins
./build.sh --clean          # rebuild from scratch
```

The signed bundles of each architecture are collected in `~/Developer/VIBR/releases/<arch>/{effects,drumcomputer,multieffect}/{VST3,AU,Standalone}`, with `<arch>` = `arm64` or `x86_64`. Only this Mac's own architecture is installed in `~/Library/Audio/Plug-Ins`; the other one is just collected, so copy it to a Mac of that type.

`build.sh` configures and builds into `~/Developer/VIBR` (outside iCloud; the x86_64 build tree is `~/Developer/VIBR-x86_64`); it is a thin wrapper around:

```bash
cmake -S . -B ~/Developer/VIBR -DRL_ARCH=arm64 -DRELEASES_DIR=~/Developer/VIBR/releases/arm64
cmake --build ~/Developer/VIBR --config Release -j8
```

One plugin: `--target DD-1_All` (or `DM-1_All`, `CE-1_All`, `RE-2_All`, `DS-1_All`, `BF-3_All`, `CS-3_All`, `LM-2_All`, `PH-2_All`, `TR-2_All`, `GE-7_All`, `EH-2_All`, `IR-2_All`, `MA-1_All`, `ME-1_All`, `TR-8_All`, `SL-2_All`, `OD-2_All`, `OC-5_All`, `BS-1_All`, `MM-1_All`, `SD-1_All`, `SP-1_All`, `RL-1_All`, `MF-1_All`, `HM-2_All`, `GR-1_All`, `SR-1_All`, `VC-1_All`, `PT-1_All`, `VR-1_All`, ...).

## License

Copyright © 2026 Richard Leurs

This program is free software: you can redistribute it and/or modify it under the terms of the **GNU General Public License, version 3**, as published by the Free Software Foundation. It is distributed in the hope that it will be useful, but **without any warranty**; without even the implied warranty of merchantability or fitness for a particular purpose. See the [`LICENSE`](LICENSE) file for the full text.

The plugins are built with [JUCE 8](https://juce.com), used under its open-source licence (AGPLv3). The designs are inspired by classic hardware but are independent and carry no third-party branding; product names mentioned belong to their respective owners. The TR-08 sample kits are not part of this repository.
