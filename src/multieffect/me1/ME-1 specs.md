# ME-1 Multi Effect: specs

ME-1 is a chain of basic effect modules. It does **not** use the pedals in `src/effects/`: every module is new, small and self-contained (`Modules.cpp`). Each module has **2 or 3 knobs**. You add **any number of modules** to the chain, in any order, and the signal runs through them from left to right.

Keep this file and `Modules.cpp` (the table `kModules`) in step: a module is added by writing its class, adding one row to the table, and adding one row here.

## Plugin
| | |
|---|---|
| Name / bundle | ME-1 Multi Effect, `com.richardleurs.me1`, code `Me01`, formats VST3, AU, Standalone |
| Colour | body `#91BB36` (palette "Existing 04"), print `(24, 33, 10)`; everything else as in `style.md` |
| Audio | stereo in / stereo out, no latency, no MIDI |
| Host parameters | `power` (on / off, crossfaded), `inGain` and `outGain` (-18 .. +18 dB). The module knobs are not host parameters (they are saved in the session, not automatable) |
| Session | `chain` child: per slot the module id, bypass and 3 knob values (`p0 p1 p2`); an old session of the pedal-chain ME-1 is ignored (the starting chain stays) |
| Starting chain | OVERDRIVE, CHORUS, DELAY, REVERB |
| Slots | unlimited; shown as a strip of tiles, the last tile is the add button |

## Window
- 1200 x 574 layout units, resizable with a fixed aspect ratio (min 720 wide), drawn in code on a satin body (no background PNG).
- Header (as the other plugins): name `ME-1` (heavy italic), two description lines `MULTI EFFECT` / `ENDLESS COMBINATIONS`, INPUT and OUTPUT knobs with a stereo level meter beside each (the input meter left of the INPUT knob, the output meter right of the OUTPUT knob: 16 segments for -60 to 0 dB, green / amber above -12 dB / red above -3 dB, a peak-hold tick for 1.2 s; the input meter shows the level after the INPUT knob, the output meter the final output), POWER slide switch (`OFF / ON`), a rule under the header.
- Module strip: one tile per module, a charcoal tile with a bar in the colour of the module group (delay blue, modulation green, drive orange, dynamics yellow, filter & EQ light blue, pitch pink, space purple), the module name, its group, a bypass lamp with the slot number (top left) and a remove cross (top right). Arrows show the signal direction. The tiles shrink down to 84 wide when the chain grows; a longer strip scrolls with the mouse wheel / trackpad. The last tile (+) adds a module.
- The selected tile (outlined in its group colour) shows its panel below: a charcoal panel with the module name in a pill in the group colour, its description, and 2-3 large black knobs (scale 0 - 10, name below, live value; title in the group colour, value in the body colour; knobs step in whole numbers, or 0.1 where the value shows a decimal).
- Click a tile: select. Click the lamp: bypass (10 ms-ish crossfade). Click the cross: remove. Drag a tile over another tile: reorder. Click the panel's title pill: menu (Bypass, Move left, Move right, Remove, Replace with). Double-click a knob: back to its default.

## Modules (39)
Ranges are real units; "centre" = the value in the middle of the knob travel where the knob is not linear.

### Delay & echo
| Module | Knobs (range, default) | What it does |
|---|---|---|
| DELAY | TIME 20 - 1000 ms (380, can sync), FEEDBACK 0 - 95 % (35), MIX 0 - 100 % (30) | Clean digital stereo delay. Time changes glide. Dry stays at full level, MIX sets the repeats. |
| ECHO | TIME 40 - 1200 ms (320), REPEATS 0 - 100 % (40), LEVEL 0 - 100 % (50) | Tape-style echo: every repeat is darker (low-pass 3.2 kHz, high-pass 140 Hz, soft saturation in the loop) and the tape wobbles (0.55 Hz and 6 Hz). |
| MOD DELAY | TIME 20 - 1000 ms (350, can sync), FEEDBACK 0 - 92 % (40), MOD 0 - 100 % (40) | Delay with a modulated time and a 4.5 kHz low-pass in the loop: the repeats chorus and wobble. MOD sets the depth (up to +/- 3.5 ms) and the speed (0.4 - 2 Hz); left and right are 90 degrees apart. The wet level is fixed (0.6). |
| PING-PONG | TIME 20 - 1000 ms (400, can sync), FEEDBACK 0 - 92 % (40), MIX 0 - 100 % (35) | Stereo delay where the echoes bounce: the first echo is on the left, the second on the right, and so on. The input is summed to mono for the echoes; the dry signal stays stereo. |
| REVERSE | TIME 50 - 1000 ms (400, can sync), FEEDBACK 0 - 90 % (20), MIX 0 - 100 % (50) | Plays the last TIME of sound backwards: two overlapping read heads with Hann windows. The reversed sound comes out one TIME later (inherent delay, no latency reported to the host). |

### Modulation
| Module | Knobs | What it does |
|---|---|---|
| CHORUS | RATE 0.1 - 8 Hz (0.8), DEPTH 0 - 100 % (50), MIX 0 - 100 % (50) | Delay of about 12 ms modulated by a sine, the right side 90 degrees later. |
| VIBRATO | RATE 0.5 - 10 Hz (5), DEPTH 0 - 100 % (40) | Modulated delay, 100 % wet: the pitch wobbles. |
| FLANGER | RATE 0.05 - 5 Hz (0.35), DEPTH 0 - 100 % (70), REGEN -90 - +90 % (45) | Swept delay of 0.5 - 4 ms with feedback (negative = hollow, hollower sweep). |
| PHASER | RATE 0.05 - 8 Hz (0.5), DEPTH 0 - 100 % (70), FEEDBACK 0 - 90 % (40) | Four all-pass stages swept between about 250 Hz and 3.5 kHz, with feedback. |
| ROTARY | SPEED 0 - 100 % (30), DEPTH 0 - 100 % (70), MIX 0 - 100 % (100) | Rotary speaker: a split at 800 Hz sends the highs to a fast horn (0.7 - 6.7 Hz) and the lows to a slower drum (0.6 - 5 Hz). Each pulses in level through two "microphones" 90 degrees apart and has a Doppler pitch wobble. The speeds ramp slowly (0.6 s horn, 2.2 s drum) like the real thing: turn SPEED to go from chorale to tremolo. |
| UNI-VIBE | RATE 0.1 - 10 Hz (1.5, can sync), INTENSITY 0 - 100 % (70), MODE CHORUS / VIBRATO (stepped) | Four all-pass stages (centres 130 - 5200 Hz) swept unevenly by a lamp-style LFO (nonlinear, like a photocell). CHORUS mixes the phase-shifted sound with the dry one (throbbing phase), VIBRATO is the phase-shifted sound alone (pitch wobble). |
| RING MOD | FREQ 20 - 5000 Hz (400, centre 400), MIX 0 - 100 % (50), WAVE SINE / TRIANGLE / SQUARE (stepped) | The input multiplied by an oscillator: metallic, bell-like tones. Low FREQ gives a tremolo-like effect. |
| TREMOLO | RATE 0.5 - 15 Hz (5), DEPTH 0 - 100 % (60), SHAPE 0 - 100 % (0) | Volume modulation; SHAPE bends the sine to a square. |
| SLICER | RATE 0.5 - 16 Hz (4), DUTY 10 - 90 % (50), SMOOTH 0 - 100 % (25) | Rhythmic gate: the sound is on for DUTY of every cycle. SMOOTH rounds the edges (0.5 - 14.5 ms). Free running, no tempo sync yet. |

### Drive
| Module | Knobs | What it does |
|---|---|---|
| OVERDRIVE | DRIVE 0 - 100 % (40), TONE 0 - 100 % (50), LEVEL -30 - +6 dB (-6) | Soft clipping (tanh) with a mid lift in front, slightly asymmetric. |
| DISTORTION | DRIVE 0 - 100 % (55), TONE 0 - 100 % (50), LEVEL -30 - +6 dB (-9) | Tight (high-passed at 110 Hz), high gain, hard clipping. |
| FUZZ | FUZZ 0 - 100 % (60), TONE 0 - 100 % (50), LEVEL -30 - +6 dB (-9) | Very high gain, diode-style squared-off wave with a bias that gates the tail. |
| TAPE | DRIVE 0 - 100 % (40), WARMTH 0 - 100 % (50), LEVEL -12 - +6 dB (0) | Gentle saturation (tanh divided by the gain, so quiet sounds keep their level and only peaks are rounded). WARMTH adds a low shelf "head bump" (+4 dB at 90 Hz), rolls the highs off (16 kHz down to 5 kHz) and a little wow. Softer than OVERDRIVE, DISTORTION and FUZZ. |
| TUBE | DRIVE 0 - 100 % (40), BIAS 0 - 100 % (40), LEVEL -12 - +6 dB (0) | Smooth asymmetric saturation: BIAS shifts the signal on the curve, which adds even harmonics (the "glow"); the DC this causes is removed. |
| BIT CRUSHER | BITS 1 - 16 (8), RATE 500 - 48000 Hz (12000, centre 8000), MIX 0 - 100 % (100) | Rounds the signal to fewer bits and holds each sample for longer (sample-rate reduction, no filtering: the aliasing is the effect). |
| BOOST | GAIN 0 - 24 dB (10), TONE 0 - 100 % (50) | Clean gain; TONE tilts the highs (+/- 8 dB shelf at 3 kHz). |

TONE on the drive modules is a two-pole low-pass sweeping 800 Hz - 12 kHz. No oversampling yet, so very high gain on bright material can alias a little.

### Dynamics
| Module | Knobs | What it does |
|---|---|---|
| COMPRESSOR | THRESH -50 - 0 dB (-20), RATIO 1 - 20 :1 (4, centre 4), GAIN 0 - 24 dB (6) | Stereo-linked feed-forward compressor, soft knee (6 dB), attack 8 ms, release 120 ms; GAIN is the make-up gain. |
| SUSTAINER | SUSTAIN 0 - 100 % (60), ATTACK 1 - 50 ms (10), LEVEL -12 - +12 dB (0) | Heavy compression (12:1, threshold -6 .. -52 dB) with automatic make-up gain, so notes ring on; the noise floor is not lifted; the output is kept under 0 dBFS. |
| LIMITER | DRIVE 0 - 24 dB (0), RELEASE 10 - 500 ms (80), CEILING -12 - 0 dB (-1) | Instant-attack peak limiter, stereo-linked, hard ceiling. No look-ahead (no latency). |
| NOISE GATE | THRESH -80 - -10 dB (-50), RELEASE 10 - 800 ms (120) | Opens in 1 ms above the threshold, closes 3 dB lower over RELEASE. |
| TRANSIENT SHAPER | ATTACK -100 - +100 % (0), SUSTAIN -100 - +100 % (0), LEVEL -12 - +12 dB (0) | Compares a fast and a slow level follower. At the start of a note ATTACK turns the level up (+, more punch, up to +10 dB) or down (-, softer, up to -15 dB); in the tail SUSTAIN does the same. It works on the shape of the sound, so it does not depend on how loud you play. Quiet noise is left alone. |
| EXPANDER | THRESH -80 - -10 dB (-45), RATIO 1 - 10 :1 (2, centre 2), RELEASE 10 - 800 ms (150) | Downward expander: below THRESH the level is turned further down (RATIO 2:1 = every dB below becomes 2 dB, up to 60 dB), with a 6 dB soft knee. Gentler than the NOISE GATE, which closes completely. RELEASE is how slowly it closes. |
| VOLUME | LEVEL -60 - +12 dB (0), BALANCE -100 - +100 (0) | Level (-60 = silence) and left / right balance. |

### Filter & EQ
| Module | Knobs | What it does |
|---|---|---|
| FILTER | FREQ 20 Hz - 20 kHz (2000, centre 1000), RES 0 - 100 % (20), TYPE LOW PASS / BAND PASS / HIGH PASS (stepped) | Resonant state-variable filter, 12 dB per octave; RES raises the peak at FREQ (Q 0.7 - 14.7). |
| EQ | LOW, MID, HIGH, each -12 - +12 dB (0) | Low shelf 120 Hz, peak 900 Hz (Q 0.8), high shelf 5 kHz. |
| ENHANCER | FREQ 1 - 8 kHz (3000, centre 3000), AMOUNT 0 - 100 % (40) | Exciter: the part above FREQ is saturated and its harmonics are added. |
| AUTO WAH | FREQ 200 - 1000 Hz (350), SENS 0 - 100 % (60), PEAK 0 - 100 % (60) | Band-pass filter that opens when you play harder (envelope follower, state-variable filter, Q 1.5 - 11.5). |

### Pitch
| Module | Knobs | What it does |
|---|---|---|
| PITCH SHIFTER | SHIFT -12 - +12 semitones (+7), FINE -50 - +50 cents (0), MIX 0 - 100 % (50) | Shifts the notes with two overlapping Hann-windowed read heads (60 ms grains), so it works on chords too. MIX adds the shifted sound to the dry one, giving a harmony; the dry level drops to 50 % at full MIX so the sum does not get too loud. The shifted sound is delayed by up to 60 ms and has a little grain flutter; no latency is reported to the host. |
| OCTAVE | DRY 0 - 100 % (70), DOWN 0 - 100 % (60), UP 0 - 100 % (0) | One octave down (flip-flop divider following the input level) and up (full-wave rectifier). Best with single notes. |

### Space
| Module | Knobs | What it does |
|---|---|---|
| REVERB | SIZE 0 - 100 % (50), DAMP 0 - 100 % (50), MIX 0 - 100 % (25) | Algorithmic room / hall (`juce::Reverb`). |
| SPRING | DECAY 0 - 100 % (55), TONE 0 - 100 % (50), MIX 0 - 100 % (30) | Spring tank: a 37 / 41 ms delay loop (left / right) with 30 all-pass filters in the loop for the dispersion ("drip", "boing"), low-pass damping (TONE 1.5 - 7.5 kHz) and a slow wobble. |
| SHIMMER | SIZE 0 - 100 % (60), SHIMMER 0 - 100 % (50), MIX 0 - 100 % (30) | `juce::Reverb` whose tail is pitch-shifted one octave up (a two-head granular shifter, 50 ms grains) and fed back into the reverb (32-sample feedback blocks). SHIMMER sets how much octave-up goes back in. |
| PAN | POSITION -100 - +100 (0), RATE 0.1 - 10 Hz (1), DEPTH 0 - 100 % (0) | Left / right position plus an auto-pan LFO (DEPTH 0 = no movement). The louder side stays at full level, the other side is turned down. RATE can sync. |
| STEREO SPREADER | WIDTH 0 - 200 % (130), SPREAD 0 - 100 % (30), BASS 0 - 300 Hz (120) | Mid / side width, a short decorrelating delay on the side signal (SPREAD), and the bass below BASS is kept in the middle. |

## Tempo sync
DELAY, ECHO, MOD DELAY, PING-PONG, REVERSE, CHORUS, VIBRATO, UNI-VIBE, FLANGER, PHASER, TREMOLO, SLICER and PAN (RATE knob) have a **SYNC** button at the top left of their panel. With SYNC on, the TIME / RATE knob becomes a note-length knob (12 steps: 1/1, 1/2., 1/2, 1/4., 1/4, 1/8., 1/8, 1/16., 1/8T, 1/16, 1/16T, 1/32) and its readout shows the note length. A TIME knob then gives that length in ms at the host tempo (limited to 2 s); a RATE knob gives one LFO cycle per note length (so 1/4 at 120 bpm = 2 Hz). The tempo comes from the host (120 bpm when the host gives none, e.g. the standalone). The knob keeps its ms / Hz value while SYNC is on, and switching SYNC off brings it back; the SYNC state is saved in the session.

## Ideas for later
Amp / cabinet simulator; oversampling in the drive modules; automatable module knobs; presets.

## Check
`cmake --build ~/Developer/VIBR --target ME1ModuleTest` builds a small offline test (`ModuleTest.cpp`) that runs every module at default, minimum, maximum and middle knob settings and reports non-finite or too loud output; it also checks that PING-PONG alternates left / right and that REVERSE plays backwards that PITCH SHIFTER moves 440 Hz up and down an octave, that ROTARY pulses in level and that TUBE adds a second harmonic, that TRANSIENT SHAPER raises the start and lowers the tail of a note and that EXPANDER lowers only quiet sounds.
