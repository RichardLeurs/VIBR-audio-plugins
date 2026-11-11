# Richard Leurs Audio Plugins

A collection of Vintage effects pedals redesigned as audio plugins (VST3, Audio Unit and Standalone), built with JUCE 8 and C++20.

| Plugin | What it is |
|---|---|
| DD-1 Dual Delay | two delays + shimmer reverb, boost, chorus, hall reverb, 4 panel memories |
| DM-1 Delay Machine | 12-mode modulation delay, tap division, 4 panel memories |
| CE-1 Chorus Ensemble | chorus / vibrato |
| RE-1 Space Echo | tape echo + spring reverb, 12-position mode selector |
| RE-2 Space Echo | RE-1 engine + saturation, guitar / line, new / aged tape, 4 panel memories |
| OD-2 Hybrid Drive | 12 drive types, 3-band tone, boost, noise gate, 4 panel memories |
| SL-2 Slicer | rhythmic slicer: 11 patterns, 5 modes, tempo from the knob or the host |
| BF-3 Flanger | stereo flanger: manual, res, depth, rate, 3 modes (standard, ultra, gate / pan) |
| CS-3 Compression Sustainer | compressor / sustainer: level, tone, attack, sustain |
| LM-2 Limiter | peak limiter: level, tone, release, threshold |
| PH-2 Phaser | phaser: rate, depth, res, 2 modes (4 or 8 stages) |
| GE-7 Equalizer | graphic equalizer: 7 bands (100 Hz - 6.4 kHz, +-15 dB) and a level fader |
| EH-2 Enhancer | treble enhancer: mix (inverting), freq, sens |
| IR-2 Amp & Cabinet | amp and cabinet: 11 amp types, gain, level, bass / middle / treble, ambience |
| DC-1 Multi Effect | chain up to 8 of the pedals above: add, remove, reorder, bypass; each pedal keeps its own front panel |
| TR-8 Drum Machine | drum machine: 11 voices or sampled kits, 16-step A/B sequencer, host sync (instrument) |
| BS-1 Reverb | 12 reverb types (room, hall, plate, spring, swell, bloom, cloud, chorale, shimmer, magneto, nonlinear, reflections) |
| OC-5 Octave | vintage (monophonic) and poly octave: direct, +1 oct, -1 oct, -2 oct range |
| DS-1 Distortion | stompbox distortion: tone, level, dist |
| SD-3 Digital Delay | programmable digital delay: 9 programs, filter, 5 waveforms, level balance |
| SD-1 Digital Delay | digital delay: time, factor, regeneration, hi-cut, modulation, hold, headroom meter |
| MM-1 Analog Delay | analog-style delay, chorus and vibrato, overload lamp |


You can find the web page for the project on this [`website`](https://audio-plugins.richardleurs.nl).

## Build

The build on macOS is placed outside the iCloud synced folders in `~/Developer/Plugins-build`.

```bash
./build.sh              # everything
./build.sh DD-1 SD-3     # only these plugins
./build.sh --clean       # rebuild from scratch
```

`build.sh` configures and builds into `~/Developer/Plugins-build` (outside iCloud); it is a thin wrapper around:

```bash
cmake -S . -B ~/Developer/Plugins-build
cmake --build ~/Developer/Plugins-build --config Release -j8
```

JUCE 8 is expected in `~/Developer/JUCE` (see `CMakeLists.txt`). One plugin: `--target DD-1_All` (or `DM-1_All`, `CE-1_All`, `RE-1_All`, `RE-2_All`, `DS-1_All`, `BF-3_All`, `CS-3_All`, `LM-2_All`, `PH-2_All`, `GE-7_All`, `EH-2_All`, `IR-2_All`, `DC-1_All`, `TR-8_All`, `SL-2_All`, `OD-2_All`, `OC-5_All`, `BS-1_All`, `MM-1_All`, `SD-1_All`, `SD-3_All`, ...).

## License

Copyright (C) 2026 Richard Leurs

This program is free software: you can redistribute it and/or modify it under the terms of the **GNU General Public License, version 3**, as published by the Free Software Foundation. It is distributed in the hope that it will be useful, but **without any warranty**; without even the implied warranty of merchantability or fitness for a particular purpose. See the [`LICENSE`](LICENSE) file for the full text.

The plugins are built with [JUCE 8](https://juce.com), used under its open-source licence (AGPLv3). The designs are inspired by classic hardware but are independent and carry no third-party branding; product names mentioned belong to their respective owners. The TR-08 sample kits are not part of this repository.
