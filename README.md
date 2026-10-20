# VectorFM

A wavetable / phase-modulation oscillator for the **Korg NTS-1 digital kit mkII** and the **Korg microKORG 2**, built with the logue SDK.

Six procedural wavetables you can scan continuously, phase-modulated by a sine modulator, across two detuned oscillators you can morph between, followed by a wavefolder and soft saturation. A built-in slow movement LFO sweeps the wavetable, as does the NTS-1's shape LFO, or the virtual patch on the microKORG 2.

## Download & install

Go to [**Releases**](https://github.com/TwistedGenetics/Vector_Synth/releases/latest) and download the file for your synth.

### NTS-1 mkII — `VectorFM.nts1mkiiunit`

1. Open **KORG KONTROL Editor** with the NTS-1 mkII connected (firmware 1.3 or later recommended).
2. Drag the file onto the **oscillator** list, then press **SEND**.
3. Select VectorFM with the OSC button.

### microKORG 2 — `VectorFM.mk2unit`

Needs microKORG 2 firmware **2.0.0 or later** (the version that added logue SDK support).

- **KORG Sound Librarian** (Korg's official app for v2.0): connect the microKORG 2 by USB, add `VectorFM.mk2unit` as a user oscillator and send it to the synth.
- **Or USB mass storage:** hold the first option button while powering on, then copy the file into `Units/Oscs/` (into a `SlotN` folder) on the drive that appears.

VectorFM then appears at the end of the oscillator list. The microKORG 2 version is polyphonic: every voice runs its own VectorFM.

## Controls

The parameters are the same on both synths, in the same order.

| NTS-1 mkII | microKORG 2 | Parameter | Range | What it does |
|---|---|---|---|---|
| Knob A | OSC page 1 | SCAN | 0–1023 | Sweeps through the 6 wavetables: sine → triangle → saw → square → formant → harmonic stack |
| Knob B | OSC page 1 | FM | 0–1023 | FM intensity (squared curve, so the first half of the knob is subtle) |
| Edit 1 | OSC page 1 | FM RATIO | 0.5, 1, 1.5, 2, 3, 4, 5, 7, 8 | Modulator frequency relative to the note. Whole numbers stay harmonic; 1.5 gives bell/metallic tones |
| Edit 2 | OSC page 2 | FM INDEX | 0–100 % | FM depth multiplier (in FDBK mode: modulator feedback amount) |
| Edit 3 | OSC page 2 | WAVEFOLD | 0–100 % | Sine wavefolder |
| Edit 4 | OSC page 2 | MORPH | 0–100 % | Crossfade osc 1 → osc 2. Osc 2 reads the wavetable 40 % further along, so it's a different timbre |
| Edit 5 | OSC page 2 | DETUNE | 0–100 % | Detunes the oscillators apart, up to ±25 cents (Reese territory near 100 % with MORPH at 50 %) |
| Edit 6 | OSC page 2 | SATURATE | 0–100 % | Soft (tanh-style) saturation, level compensated |
| Edit 7 | OSC page 3 | MOVEMENT | 0–100 % | Built-in LFO on the wavetable scan: rate 0.05–2 Hz and depth rise together |
| Edit 8 | OSC page 3 | MODE | CLSC, FDBK, CROS, FOLD | Synthesis variation (below) |

**NTS-1 mkII:** hold **OSC** and turn the TYPE encoder to choose an Edit parameter, then use Knob B to set the value. The NTS-1's **shape LFO** also modulates the wavetable scan.

**microKORG 2:** the parameters are on the OSC pages 1–3. In the **virtual patch**, VectorFM adds four modulation destinations: **Scan**, **FM**, **Morph** and **Fold**. Modulation is per voice, so an envelope or velocity on Scan gives each note its own sweep.

### Modes

- **CLSC (classic)** — sine modulator phase-modulates the wavetable. Clean, DX-meets-wavetable.
- **FDBK (feedback)** — the modulator feeds back on itself (amount = FM INDEX), giving gritty, noisy, saw-like FM. FM knob sets depth on its own.
- **CROS (cross-mod)** — osc 2 modulates osc 1 (and a little back again). FM RATIO sets osc 2's pitch relative to osc 1. Turn MORPH to hear osc 2 itself.
- **FOLD (fold-FM)** — classic FM pushed into a harder, always-on wavefolder. For aggressive basses.

## Test checklist (by ear on the hardware)

Start from the default settings (everything 0, FM INDEX 50 %, RATIO 1, MODE CLSC).

- [ ] **Tuning** — play A4 against a tuner: should read 440 Hz.
- [ ] **SCAN** — smooth sweep sine → triangle → saw → square → nasal formant → organ-like stack, no clicks or steps.
- [ ] **FM** — nothing at 0, gradual brightening, metallic/clangy at full.
- [ ] **FM RATIO** — with FM up, 1/2/3/4… stay pitched; 1.5 and 0.5 sound inharmonic/sub-heavy.
- [ ] **WAVEFOLD** — adds buzzy upper harmonics, stays pitched.
- [ ] **MORPH + DETUNE** — MORPH 50 %, DETUNE up: slow beating that becomes a wide, chorused/Reese sound.
- [ ] **SATURATE** — thickens and compresses; volume roughly steady.
- [ ] **MOVEMENT** — timbre slowly sweeps on its own; faster and deeper as you raise it.
- [ ] **NTS-1 shape LFO** — route the LFO to shape: the wavetable scan wobbles.
- [ ] **microKORG 2 virtual patch** — route an LFO or envelope to Scan / FM / Morph / Fold and hear each move.
- [ ] **microKORG 2 chords** — hold a chord: all notes in tune, no crackles; voices at the same level as the built-in oscillators (roughly).
- [ ] **MODE** — each of CLSC / FDBK / CROS / FOLD sounds distinctly different with FM around halfway.
- [ ] **High notes** — top octave sounds clean (no whistling aliases) with FM at 0.
- [ ] **Everything at max** — loud and nasty, but no dropouts, crackles, or the unit stopping (on the microKORG 2, try it with full polyphony).

## Building

GitHub Actions builds both units with Korg's own toolchains and runs the DSP tests on every push (each run's files are under the **Actions** tab). Releases are automatic: when `.version` in `nts1mkii/header.c` and `microkorg2/header.c` changes (e.g. `0x00010101U` = 1.1.1) and is pushed to main, Actions publishes a new Release with both files. Pushes that keep the same version refresh that Release's files.

To build locally (no Docker needed):

```sh
./build.sh          # both units into dist/
./build.sh nts1     # NTS-1 mkII only
./build.sh mk2      # microKORG 2 only (Linux: Korg only publishes its toolchain for Linux)
```

This downloads the toolchains and the logue SDK into `.deps/` on first run.

Desktop DSP tests (renders audio, checks tuning, levels, aliasing and zipper noise, writes demo WAVs to `test/out/`):

```sh
./test/run.sh
```

The wavetables in `core/tables.h` are generated by `tools/gen_tables.py` (6 shapes × 7 band-limited mip levels × 256 samples, int16).

## Layout

```
core/         synth.h (DSP, shared by both units), tables.h
nts1mkii/     NTS-1 mkII unit: header.c (parameters), unit.cc (SDK callbacks), Makefile, config.mk
microkorg2/   microKORG 2 unit: header.c, unit.cc (per-voice rendering, virtual patch), Makefile, config.mk
tools/        gen_tables.py
test/         render.cc, run.sh
build.sh
.github/workflows/build.yml   CI build + release
```

## Notes

- Developer ID `0x5477476E` ('TwGn'), unit ID `1`. If you register an ID with Korg (pull request to the logue SDK's `developer_ids.md`), set it in both `header.c` files.
- Built against logue SDK commit `e78b16c`.
- Size: ~28 KB of the 48 KB oscillator budget (NTS-1 mkII).
