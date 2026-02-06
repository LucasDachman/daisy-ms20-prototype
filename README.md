# daisy-ms20

A monophonic synthesizer for the [Electrosmith Daisy Seed](https://www.electro-smith.com/daisy/daisy) featuring an emulation of the Korg MS-20 lowpass filter. Designed as a dark, industrial-sounding voice controlled via hardware MIDI from an Elektron Syntakt (or any MIDI controller).

**One voice. One oscillator. One filter. Two effects. Eight knobs.**

```
SAW OSC ──┐
           ├──→ Wavefolder → MS-20 Filter (LP) → Amp Env → Chorus → Reverb → Mono Out
SUB OSC ──┘
```

## Controls

8 MIDI CCs — nothing else:

| CC | Parameter | Range |
|----|-----------|-------|
| 1 | Filter Cutoff | 20 Hz – 18 kHz (exponential) |
| 2 | Drive + Resonance | Drive leads, resonance follows (see below) |
| 3 | Sub Oscillator Level | 0–100% (sine, -1 octave) |
| 4 | Wavefolder Amount | 0–100% (symmetric) |
| 5 | Envelope Decay/Release | 30 ms – 5 s (shared) |
| 6 | Envelope → Amp Depth | 0% = organ sustain, 100% = full decay |
| 7 | Envelope → Filter Depth | 0% = static, 100% = full sweep |
| 8 | FX Crossfade | 0% = dry, 50% = chorus, 100% = reverb |

### Drive + Resonance Link (CC 2)

A single knob controls both, with drive ramping up first:

```
CC value:    0% ──────────── 50% ──────────── 100%
Drive:       0% ──────────── 80% ──────────── 100%   (fast:  x^0.6)
Resonance:   0% ──────────── 20% ──────────── 85%    (slow:  x^1.8 × 0.85)
```

Low values give grit without screaming. Past halfway, resonance catches up and the filter starts to sing.

## MS-20 Filter

The core of this project. A virtual analog emulation of the Korg MS-20 lowpass filter using the Topology-Preserving Transform (TPT) method:

- Two one-pole lowpass filters in series (12 dB/oct)
- Zero-delay feedback — resonance peak is consistent across all frequencies
- `tanh` saturation in the feedback path models the MS-20's diode clippers
- Aggressive, harmonically-rich self-oscillation character

Based on Will Pirkle's analysis (AN-5) and Vadim Zavalishin's "The Art of VA Filter Design."

## Hardware

### Parts List (~$35–45)

| Part | Qty |
|------|-----|
| Daisy Seed (pre-soldered headers) | 1 |
| Breadboard (830-point) | 1 |
| 5-pin DIN socket | 1 |
| 6N138 optocoupler | 1 |
| 1N4148 diode | 1 |
| 220Ω, 470Ω, 10kΩ resistors | 1 each |
| 1/4" mono jack socket | 1 |
| MIDI cable + 1/4" TS cable | 1 each |

### Wiring

**MIDI input** (5-pin DIN → 6N138 optoisolator → Daisy pin D14):

```
DIN Pin 4 → 220Ω → 6N138 pin 1 (Anode)
DIN Pin 5 → 6N138 pin 2 (Cathode)
1N4148 across pins 1-2 (band toward pin 2)
6N138 pin 8 (Vcc) → Daisy 3V3
6N138 pin 5 (GND) → Daisy GND
470Ω between 6N138 pins 5-6
10kΩ from 6N138 pin 7 → Daisy 3V3
6N138 pin 7 (Out) → Daisy D14
```

**Audio output** (2 wires):

```
Daisy Pin 18 (Audio Out L) → 1/4" jack tip
Daisy GND → 1/4" jack sleeve
```

**Power:** USB-C during development. Any 5V USB source for standalone.

## Building

### Prerequisites

- [arm-none-eabi-gcc](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm) (10.3-2021.10 recommended)
- [DaisyExamples](https://github.com/electro-smith/DaisyExamples) cloned with submodules:
  ```bash
  git clone --recurse-submodules https://github.com/electro-smith/DaisyExamples
  cd DaisyExamples
  ./ci/build_libs.sh
  ```
- [dfu-util](http://dfu-util.sourceforge.net/) for flashing

### Setup

1. Clone this repo **next to** (not inside) `DaisyExamples`:
   ```
   your-workspace/
   ├── DaisyExamples/
   │   ├── libDaisy/
   │   └── DaisySP/
   └── daisy-ms20/        ← this repo
   ```

2. If your `DaisyExamples` is elsewhere, edit `DAISYEXAMPLES_DIR` in the Makefile.

### Build & Flash

```bash
make              # build firmware → build/daisy-ms20.bin
make program-dfu  # flash via USB (hold BOOT, press RESET, release BOOT first)
```

### VS Code

Open the project folder. Build with `Ctrl+Shift+B`. The `.vscode/` folder includes tasks for build and flash.

## Project Structure

```
src/
├── main.cpp           Daisy init, audio callback, MIDI handling
├── voice.h/.cpp       Saw + sub + wavefolder + filter + envelope
├── ms20_filter.h      Zero-delay-feedback Korg 35 LPF (header-only)
├── fx_chain.h/.cpp    Chorus → Reverb with serial crossfade
└── params.h           8 CC values, scaling curves, hardcoded defaults
```

The `ms20_filter.h` module is self-contained and portable — it has no Daisy dependencies and can be dropped into any C++ project or unit-tested offline.

## Hardcoded Defaults

| Parameter | Value | Why |
|-----------|-------|-----|
| Waveform | Saw | Full harmonic series for maximum filter interaction |
| Filter Mode | Lowpass | Classic subtractive starting point |
| Key Tracking | 50% | Bass stays dark, treble opens up |
| Envelope Attack | 5 ms | Always snappy |
| Envelope Sustain | 0% | Percussive AD shape |
| Chorus | 0.8 Hz / 40% depth / 20% feedback | Gentle movement |
| Reverb | 70% size / 4 kHz LP | Dark, warm tail |

## Signal Flow

```
MIDI IN (5-pin DIN → 6N138 → UART RX D14)
    │
    ▼
SAW OSC (PolyBLEP, concert pitch)
    + SUB OSC (sine, -1 oct, level = CC 3)
    │
    ▼
WAVEFOLDER (amount = CC 4)
    │
    ▼
MS-20 FILTER (cutoff = CC 1, drive+reso = CC 2, env = CC 7, 50% key tracking)
    │
    ▼
AMP ENVELOPE (5ms attack, decay = CC 5, 0% sustain, depth = CC 6)
    │
    ▼
FX CROSSFADE (CC 8: dry → chorus → reverb)
    │
    ▼
MONO OUT (pin 18 → 1/4" jack)
```

## License

MIT
